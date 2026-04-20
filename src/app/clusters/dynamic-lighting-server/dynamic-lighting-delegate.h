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

#include <app/data-model/Nullable.h>
#include <clusters/DynamicLighting/Commands.h>
#include <clusters/DynamicLighting/Enums.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>

namespace chip::app::Clusters::DynamicLighting {

struct EffectInfo
{
    uint16_t effectID         = 0;
    EffectSourceEnum source   = EffectSourceEnum::kInternal;
    CharSpan label            = CharSpan();
    uint16_t maxSpeed         = 1;
    uint16_t defaultSpeed     = 1;
    bool supportsColorPalette = false;
};

class Delegate
{
public:
    virtual ~Delegate() = default;

    // Return CHIP_ERROR_NOT_FOUND when the index is out of range.
    virtual CHIP_ERROR GetEffectByIndex(EndpointId endpoint, size_t index, EffectInfo & effect) = 0;

    virtual Protocols::InteractionModel::Status HandleStartEffect(EndpointId endpoint,
                                                                  const Commands::StartEffect::DecodableType & commandData,
                                                                  uint16_t resolvedSpeed)
    {
        return Protocols::InteractionModel::Status::Success;
    }

    virtual Protocols::InteractionModel::Status HandleStopEffect(EndpointId endpoint)
    {
        return Protocols::InteractionModel::Status::Success;
    }

    virtual Protocols::InteractionModel::Status HandleSetCurrentSpeed(EndpointId endpoint, uint16_t effectId, uint16_t speed)
    {
        return Protocols::InteractionModel::Status::Success;
    }

    virtual Protocols::InteractionModel::Status HandleLevelChanged(EndpointId endpoint, uint8_t level)
    {
        return Protocols::InteractionModel::Status::Success;
    }
};

void SetDefaultDelegate(Delegate * delegate);
Delegate * GetDefaultDelegate();

} // namespace chip::app::Clusters::DynamicLighting
