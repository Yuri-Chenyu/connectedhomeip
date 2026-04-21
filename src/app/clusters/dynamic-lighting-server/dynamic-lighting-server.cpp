/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "dynamic-lighting-server.h"

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/reporting/reporting.h>
#include <app/util/attribute-table.h>
#include <clusters/DynamicLighting/Attributes.h>
#include <clusters/DynamicLighting/Commands.h>
#include <clusters/DynamicLighting/Enums.h>
#include <clusters/DynamicLighting/Structs.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#include <clusters/ColorControl/Enums.h>
#include <clusters/LevelControl/Attributes.h>
#include <clusters/OnOff/Attributes.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::DynamicLighting;
using namespace chip::app::Clusters::DynamicLighting::Attributes;
using chip::Protocols::InteractionModel::Status;

bool emberAfContainsServer(chip::EndpointId endpoint, chip::ClusterId clusterId);

namespace {

Delegate * gDelegate = nullptr;
constexpr size_t kMaxAvailableEffects = 128;

bool IsEndOfListError(CHIP_ERROR err)
{
    return (err == CHIP_ERROR_NOT_FOUND) || (err == CHIP_ERROR_PROVIDER_LIST_EXHAUSTED);
}

template <typename T>
bool NullableHasValue(const DataModel::Nullable<T> & value)
{
    return !value.IsNull();
}

bool ValidatePaletteEntry(const Structs::EffectColorStruct::DecodableType & entry, EffectColorModeEnum colorMode)
{
    const bool hasLevel       = NullableHasValue(entry.level);
    const bool hasX           = NullableHasValue(entry.x);
    const bool hasY           = NullableHasValue(entry.y);
    const bool hasHue         = NullableHasValue(entry.hue);
    const bool hasEnhancedHue = NullableHasValue(entry.enhancedHue);
    const bool hasSaturation  = NullableHasValue(entry.saturation);

    switch (colorMode)
    {
    case EffectColorModeEnum::kLevel:
        return hasLevel && !hasX && !hasY && !hasHue && !hasEnhancedHue && !hasSaturation;
    case EffectColorModeEnum::kXy:
        return !hasLevel && hasX && hasY && !hasHue && !hasEnhancedHue && !hasSaturation;
    case EffectColorModeEnum::kXYAndLevel:
        return hasLevel && hasX && hasY && !hasHue && !hasEnhancedHue && !hasSaturation;
    case EffectColorModeEnum::kHs:
        return !hasLevel && !hasX && !hasY && hasHue && !hasEnhancedHue && hasSaturation;
    case EffectColorModeEnum::kHSAndLevel:
        return hasLevel && !hasX && !hasY && hasHue && !hasEnhancedHue && hasSaturation;
    case EffectColorModeEnum::kEhue:
        return !hasLevel && !hasX && !hasY && !hasHue && hasEnhancedHue && hasSaturation;
    case EffectColorModeEnum::kEHUEAndLevel:
        return hasLevel && !hasX && !hasY && !hasHue && hasEnhancedHue && hasSaturation;
    default:
        return false;
    }
}

bool ShouldStopForColorControlAttribute(AttributeId attributeId)
{
    using namespace ColorControl::Attributes;

    switch (attributeId)
    {
    case CurrentHue::Id:
    case CurrentSaturation::Id:
    case CurrentX::Id:
    case CurrentY::Id:
    case EnhancedCurrentHue::Id:
    case ColorTemperatureMireds::Id:
        return true;
    default:
        return false;
    }
}

} // namespace

namespace chip::app::Clusters::DynamicLighting {

void SetDefaultDelegate(Delegate * delegate)
{
    gDelegate = delegate;
}

Delegate * GetDefaultDelegate()
{
    return gDelegate;
}

DynamicLightingServer::DynamicLightingServer() :
    AttributeAccessInterface(Optional<EndpointId>::Missing(), DynamicLighting::Id),
    CommandHandlerInterface(Optional<EndpointId>::Missing(), DynamicLighting::Id)
{}

DynamicLightingServer & DynamicLightingServer::Instance()
{
    static DynamicLightingServer server;
    return server;
}

DynamicLightingServer::EndpointState * DynamicLightingServer::GetState(EndpointId endpoint)
{
    for (size_t i = 0; i < kNumSupportedEndpoints; ++i)
    {
        if (mStates[i].endpoint == endpoint)
        {
            return &mStates[i];
        }
    }
    return nullptr;
}

const DynamicLightingServer::EndpointState * DynamicLightingServer::GetState(EndpointId endpoint) const
{
    for (size_t i = 0; i < kNumSupportedEndpoints; ++i)
    {
        if (mStates[i].endpoint == endpoint)
        {
            return &mStates[i];
        }
    }
    return nullptr;
}

CHIP_ERROR DynamicLightingServer::RegisterEndpoint(EndpointId endpoint)
{
    if (GetState(endpoint) != nullptr)
    {
        return CHIP_NO_ERROR;
    }

    for (size_t i = 0; i < kNumSupportedEndpoints; ++i)
    {
        if (mStates[i].endpoint == kInvalidEndpointId)
        {
            mStates[i].endpoint = endpoint;
            mStates[i].currentEffectID.SetNull();
            mStates[i].currentSpeed.SetNull();
            return CHIP_NO_ERROR;
        }
    }

    return CHIP_ERROR_NO_MEMORY;
}

CHIP_ERROR DynamicLightingServer::UnregisterEndpoint(EndpointId endpoint)
{
    EndpointState * state = GetState(endpoint);
    VerifyOrReturnError(state != nullptr, CHIP_ERROR_NOT_FOUND);

    state->endpoint = kInvalidEndpointId;
    state->currentEffectID.SetNull();
    state->currentSpeed.SetNull();
    return CHIP_NO_ERROR;
}

void DynamicLightingServer::ReportCurrentStateChange(EndpointId endpoint) const
{
    MatterReportingAttributeChangeCallback(endpoint, DynamicLighting::Id, CurrentEffectID::Id);
    MatterReportingAttributeChangeCallback(endpoint, DynamicLighting::Id, CurrentSpeed::Id);
}

CHIP_ERROR DynamicLightingServer::EncodeAvailableEffects(EndpointId endpoint, AttributeValueEncoder & encoder)
{
    if (gDelegate == nullptr)
    {
        return CHIP_IM_GLOBAL_STATUS(Failure);
    }

    return encoder.EncodeList([endpoint](const auto & listEncoder) -> CHIP_ERROR {
        size_t encodedCount = 0;
        for (size_t index = 0; index < kMaxAvailableEffects; ++index)
        {
            EffectInfo effect;
            CHIP_ERROR err = gDelegate->GetEffectByIndex(endpoint, index, effect);
            if (IsEndOfListError(err))
            {
                return (encodedCount > 0) ? CHIP_NO_ERROR : CHIP_IM_GLOBAL_STATUS(Failure);
            }
            ReturnErrorOnFailure(err);

            VerifyOrReturnError(effect.maxSpeed >= 1, CHIP_IM_GLOBAL_STATUS(Failure));
            VerifyOrReturnError(effect.defaultSpeed >= 1 && effect.defaultSpeed <= effect.maxSpeed, CHIP_IM_GLOBAL_STATUS(Failure));
            VerifyOrReturnError(!effect.label.empty() && effect.label.size() <= 32, CHIP_IM_GLOBAL_STATUS(Failure));

            Structs::EffectStruct::Type encoded;
            encoded.effectID             = effect.effectID;
            encoded.source               = effect.source;
            encoded.label                = effect.label;
            encoded.maxSpeed             = effect.maxSpeed;
            encoded.defaultSpeed         = effect.defaultSpeed;
            encoded.supportsColorPalette = effect.supportsColorPalette;

            ReturnErrorOnFailure(listEncoder.Encode(encoded));
            ++encodedCount;
        }

        return CHIP_NO_ERROR;
    });
}

bool DynamicLightingServer::TryGetEffectById(EndpointId endpoint, uint16_t effectId, EffectInfo & effect) const
{
    if (gDelegate == nullptr)
    {
        return false;
    }

    for (size_t index = 0; index < kMaxAvailableEffects; ++index)
    {
        CHIP_ERROR err = gDelegate->GetEffectByIndex(endpoint, index, effect);
        if (IsEndOfListError(err))
        {
            return false;
        }

        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Zcl, "DynamicLighting: GetEffectByIndex failed: %" CHIP_ERROR_FORMAT, err.Format());
            return false;
        }

        if (effect.effectID == effectId)
        {
            return true;
        }
    }

    return false;
}

CHIP_ERROR DynamicLightingServer::Read(const ConcreteReadAttributePath & path, AttributeValueEncoder & encoder)
{
    VerifyOrReturnError(path.mClusterId == DynamicLighting::Id, CHIP_ERROR_INVALID_ARGUMENT);

    const EndpointState * state = GetState(path.mEndpointId);
    VerifyOrReturnError(state != nullptr, CHIP_IM_GLOBAL_STATUS(UnsupportedEndpoint));

    switch (path.mAttributeId)
    {
    case AvailableEffects::Id:
        return EncodeAvailableEffects(path.mEndpointId, encoder);
    case CurrentEffectID::Id:
        return encoder.Encode(state->currentEffectID);
    case CurrentSpeed::Id:
        return encoder.Encode(state->currentSpeed);
    case Globals::Attributes::FeatureMap::Id:
        return encoder.Encode(static_cast<uint32_t>(0));
    case Globals::Attributes::ClusterRevision::Id:
        return encoder.Encode(static_cast<uint16_t>(1));
    default:
        return CHIP_IM_GLOBAL_STATUS(UnsupportedAttribute);
    }
}

Status DynamicLightingServer::HandleCurrentSpeedWrite(EndpointId endpoint, const DataModel::Nullable<uint16_t> & requestedSpeed)
{
    EndpointState * state = GetState(endpoint);
    VerifyOrReturnValue(state != nullptr, Status::UnsupportedEndpoint);
    VerifyOrReturnValue(!state->currentEffectID.IsNull() && !state->currentSpeed.IsNull(), Status::InvalidInState);
    VerifyOrReturnValue(!requestedSpeed.IsNull(), Status::ConstraintError);

    EffectInfo effect;
    VerifyOrReturnValue(TryGetEffectById(endpoint, state->currentEffectID.Value(), effect), Status::ConstraintError);
    const uint16_t resolvedSpeed = (requestedSpeed.Value() == 0) ? effect.defaultSpeed : requestedSpeed.Value();
    VerifyOrReturnValue(resolvedSpeed <= effect.maxSpeed, Status::ConstraintError);

    if (gDelegate != nullptr)
    {
        const Status delegateStatus = gDelegate->HandleSetCurrentSpeed(endpoint, state->currentEffectID.Value(), resolvedSpeed);
        VerifyOrReturnValue(delegateStatus == Status::Success, delegateStatus);
    }

    state->currentSpeed.SetNonNull(resolvedSpeed);
    MatterReportingAttributeChangeCallback(endpoint, DynamicLighting::Id, CurrentSpeed::Id);
    return Status::Success;
}

CHIP_ERROR DynamicLightingServer::Write(const ConcreteDataAttributePath & path, AttributeValueDecoder & decoder)
{
    VerifyOrReturnError(path.mClusterId == DynamicLighting::Id, CHIP_ERROR_INVALID_ARGUMENT);

    switch (path.mAttributeId)
    {
    case CurrentSpeed::Id: {
        DataModel::Nullable<uint16_t> requestedSpeed;
        ReturnErrorOnFailure(decoder.Decode(requestedSpeed));
        return StatusIB(HandleCurrentSpeedWrite(path.mEndpointId, requestedSpeed)).ToChipError();
    }
    default:
        return CHIP_IM_GLOBAL_STATUS(UnsupportedWrite);
    }
}

bool DynamicLightingServer::EndpointSupportsColorControlFeature(EndpointId endpoint, ColorControl::Feature feature) const
{
    if (!emberAfContainsServer(endpoint, ColorControl::Id))
    {
        return false;
    }

    uint32_t featureMap                                = 0;
    Protocols::InteractionModel::Status readStatus = emberAfReadAttribute(endpoint, ColorControl::Id,
                                                                           Globals::Attributes::FeatureMap::Id,
                                                                           reinterpret_cast<uint8_t *>(&featureMap),
                                                                           sizeof(featureMap));
    if (readStatus != Status::Success)
    {
        return false;
    }

    return (featureMap & to_underlying(feature)) != 0;
}

Status DynamicLightingServer::ValidateColorSettings(EndpointId endpoint, const EffectInfo & effect,
                                                    const Commands::StartEffect::DecodableType & commandData) const
{
    if (!effect.supportsColorPalette)
    {
        return (commandData.colorMode.HasValue() || commandData.colorPalette.HasValue()) ? Status::InvalidCommand : Status::Success;
    }

    if (commandData.colorMode.HasValue() != commandData.colorPalette.HasValue())
    {
        return Status::InvalidCommand;
    }

    if (!commandData.colorPalette.HasValue())
    {
        return Status::Success;
    }

    const EffectColorModeEnum colorMode = commandData.colorMode.Value();
    switch (colorMode)
    {
    case EffectColorModeEnum::kXy:
    case EffectColorModeEnum::kXYAndLevel:
        VerifyOrReturnValue(EndpointSupportsColorControlFeature(endpoint, ColorControl::Feature::kXy), Status::InvalidCommand);
        break;
    case EffectColorModeEnum::kHs:
    case EffectColorModeEnum::kHSAndLevel:
        VerifyOrReturnValue(EndpointSupportsColorControlFeature(endpoint, ColorControl::Feature::kHueAndSaturation),
                            Status::InvalidCommand);
        break;
    case EffectColorModeEnum::kEhue:
    case EffectColorModeEnum::kEHUEAndLevel:
        VerifyOrReturnValue(EndpointSupportsColorControlFeature(endpoint, ColorControl::Feature::kEnhancedHue),
                            Status::InvalidCommand);
        break;
    case EffectColorModeEnum::kLevel:
        break;
    default:
        return Status::InvalidCommand;
    }

    auto iter = commandData.colorPalette.Value().begin();
    bool sawPaletteEntry = false;
    while (iter.Next())
    {
        sawPaletteEntry = true;
        VerifyOrReturnValue(ValidatePaletteEntry(iter.GetValue(), colorMode), Status::InvalidCommand);
    }
    VerifyOrReturnValue(iter.GetStatus() == CHIP_NO_ERROR, Status::InvalidCommand);
    VerifyOrReturnValue(sawPaletteEntry, Status::InvalidCommand);

    return Status::Success;
}

Status DynamicLightingServer::HandleStartEffect(EndpointId endpoint, const Commands::StartEffect::DecodableType & commandData)
{
    EndpointState * state = GetState(endpoint);
    VerifyOrReturnValue(state != nullptr, Status::UnsupportedEndpoint);

    EffectInfo effect;
    VerifyOrReturnValue(TryGetEffectById(endpoint, commandData.effectID, effect), Status::InvalidCommand);

    const uint16_t resolvedSpeed = (commandData.speed == 0) ? effect.defaultSpeed : commandData.speed;
    VerifyOrReturnValue(resolvedSpeed <= effect.maxSpeed, Status::InvalidCommand);
    VerifyOrReturnValue(ValidateColorSettings(endpoint, effect, commandData) == Status::Success, Status::InvalidCommand);

    if (gDelegate != nullptr)
    {
        const Status delegateStatus = gDelegate->HandleStartEffect(endpoint, commandData, resolvedSpeed);
        VerifyOrReturnValue(delegateStatus == Status::Success, delegateStatus);
    }

    state->currentEffectID.SetNonNull(commandData.effectID);
    state->currentSpeed.SetNonNull(resolvedSpeed);
    ReportCurrentStateChange(endpoint);
    return Status::Success;
}

Status DynamicLightingServer::HandleStopEffect(EndpointId endpoint)
{
    EndpointState * state = GetState(endpoint);
    VerifyOrReturnValue(state != nullptr, Status::UnsupportedEndpoint);
    VerifyOrReturnValue(!state->currentEffectID.IsNull(), Status::InvalidCommand);

    if (gDelegate != nullptr)
    {
        const Status delegateStatus = gDelegate->HandleStopEffect(endpoint);
        VerifyOrReturnValue(delegateStatus == Status::Success, delegateStatus);
    }

    state->currentEffectID.SetNull();
    state->currentSpeed.SetNull();
    ReportCurrentStateChange(endpoint);
    return Status::Success;
}

void DynamicLightingServer::HandleExternalAttributeChange(const ConcreteAttributePath & attributePath, uint16_t size, uint8_t * value)
{
    const EndpointState * state = GetState(attributePath.mEndpointId);
    VerifyOrReturn((state != nullptr) && !state->currentEffectID.IsNull() && !state->currentSpeed.IsNull());

    switch (attributePath.mClusterId)
    {
    case OnOff::Id:
        VerifyOrReturn(attributePath.mAttributeId == OnOff::Attributes::OnOff::Id);
        VerifyOrReturn(value != nullptr && size >= sizeof(uint8_t));
        if (value[0] == 0)
        {
            const Status status = HandleStopEffect(attributePath.mEndpointId);
            VerifyOrReturn(status == Status::Success,
                           ChipLogError(Zcl, "DynamicLighting: stop on OnOff change failed: 0x%02x", to_underlying(status)));
        }
        return;
    case LevelControl::Id:
        VerifyOrReturn(attributePath.mAttributeId == LevelControl::Attributes::CurrentLevel::Id);
        VerifyOrReturn(value != nullptr && size >= sizeof(uint8_t));
        if (gDelegate != nullptr)
        {
            const Status status = gDelegate->HandleLevelChanged(attributePath.mEndpointId, value[0]);
            VerifyOrReturn(status == Status::Success,
                           ChipLogError(Zcl, "DynamicLighting: level change handling failed: 0x%02x", to_underlying(status)));
        }
        return;
    case ColorControl::Id:
        VerifyOrReturn(ShouldStopForColorControlAttribute(attributePath.mAttributeId));
        {
            const Status status = HandleStopEffect(attributePath.mEndpointId);
            VerifyOrReturn(status == Status::Success,
                           ChipLogError(Zcl, "DynamicLighting: stop on color change failed: 0x%02x", to_underlying(status)));
        }
        return;
    default:
        return;
    }
}

void DynamicLightingServer::InvokeCommand(HandlerContext & ctx)
{
    switch (ctx.mRequestPath.mCommandId)
    {
    case Commands::StartEffect::Id:
        HandleCommand<Commands::StartEffect::DecodableType>(ctx, [this](HandlerContext & commandCtx, const auto & commandData) {
            commandCtx.mCommandHandler.AddStatus(commandCtx.mRequestPath,
                                                 HandleStartEffect(commandCtx.mRequestPath.mEndpointId, commandData));
        });
        return;
    case Commands::StopEffect::Id:
        HandleCommand<Commands::StopEffect::DecodableType>(ctx, [this](HandlerContext & commandCtx, const auto &) {
            commandCtx.mCommandHandler.AddStatus(commandCtx.mRequestPath,
                                                 HandleStopEffect(commandCtx.mRequestPath.mEndpointId));
        });
        return;
    default:
        ctx.mCommandHandler.AddStatus(ctx.mRequestPath, Status::UnsupportedCommand);
        return;
    }
}

} // namespace chip::app::Clusters::DynamicLighting
