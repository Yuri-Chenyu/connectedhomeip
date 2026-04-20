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

#pragma once

#include "dynamic-lighting-delegate.h"

#include <app-common/zap-generated/cluster-enums.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/AttributeAccessInterface.h>
#include <app/CommandHandlerInterface.h>
#include <app/ConcreteAttributePath.h>
#include <app/data-model/Nullable.h>
#include <app/util/basic-types.h>
#include <clusters/DynamicLighting/Attributes.h>
#include <clusters/DynamicLighting/Commands.h>
#include <clusters/DynamicLighting/Enums.h>
#include <clusters/DynamicLighting/Structs.h>
#include <lib/core/CHIPError.h>
#include <platform/CHIPDeviceConfig.h>

namespace chip::app::Clusters::DynamicLighting {

#ifndef MATTER_DM_DYNAMIC_LIGHTING_CLUSTER_SERVER_ENDPOINT_COUNT
#define MATTER_DM_DYNAMIC_LIGHTING_CLUSTER_SERVER_ENDPOINT_COUNT 0
#endif

#define DYNAMIC_LIGHTING_NUM_SUPPORTED_ENDPOINTS                                                                                  \
    (MATTER_DM_DYNAMIC_LIGHTING_CLUSTER_SERVER_ENDPOINT_COUNT + CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT)

static constexpr size_t kNumSupportedEndpoints = DYNAMIC_LIGHTING_NUM_SUPPORTED_ENDPOINTS;

class DynamicLightingServer : public AttributeAccessInterface, public CommandHandlerInterface
{
public:
    DynamicLightingServer();

    static DynamicLightingServer & Instance();

    CHIP_ERROR Read(const ConcreteReadAttributePath & path, AttributeValueEncoder & encoder) override;
    CHIP_ERROR Write(const ConcreteDataAttributePath & path, AttributeValueDecoder & decoder) override;
    void InvokeCommand(HandlerContext & ctx) override;

    CHIP_ERROR RegisterEndpoint(EndpointId endpoint);
    CHIP_ERROR UnregisterEndpoint(EndpointId endpoint);
    Protocols::InteractionModel::Status HandleStartEffect(EndpointId endpoint,
                                                          const Commands::StartEffect::DecodableType & commandData);
    Protocols::InteractionModel::Status HandleStopEffect(EndpointId endpoint);
    void HandleExternalAttributeChange(const ConcreteAttributePath & attributePath, uint16_t size, uint8_t * value);

private:
    struct EndpointState
    {
        EndpointId endpoint = kInvalidEndpointId;
        DataModel::Nullable<uint16_t> currentEffectID;
        DataModel::Nullable<uint16_t> currentSpeed;
    };

    EndpointState * GetState(EndpointId endpoint);
    const EndpointState * GetState(EndpointId endpoint) const;

    CHIP_ERROR EncodeAvailableEffects(EndpointId endpoint, AttributeValueEncoder & encoder);
    bool TryGetEffectById(EndpointId endpoint, uint16_t effectId, EffectInfo & effect) const;
    Protocols::InteractionModel::Status HandleCurrentSpeedWrite(EndpointId endpoint,
                                                                const DataModel::Nullable<uint16_t> & requestedSpeed);
    Protocols::InteractionModel::Status ValidateColorSettings(EndpointId endpoint, const EffectInfo & effect,
                                                              const Commands::StartEffect::DecodableType & commandData) const;

    bool EndpointSupportsColorControlFeature(EndpointId endpoint, ColorControl::Feature feature) const;
    void ReportCurrentStateChange(EndpointId endpoint) const;

#if DYNAMIC_LIGHTING_NUM_SUPPORTED_ENDPOINTS > 0
    EndpointState mStates[kNumSupportedEndpoints];
#else
    EndpointState * mStates = nullptr;
#endif
};

} // namespace chip::app::Clusters::DynamicLighting
