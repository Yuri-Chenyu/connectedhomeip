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

#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/CommandHandler.h>
#include <app/CommandHandlerInterfaceRegistry.h>
#include <app/ConcreteCommandPath.h>
#include <app/clusters/dynamic-lighting-server/CodegenIntegration.h>
#include <app/clusters/dynamic-lighting-server/dynamic-lighting-server.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters::DynamicLighting;

namespace chip::app::Clusters::DynamicLighting {

void MatterDynamicLightingPluginServerInitCallback()
{
    ReturnOnFailure(CommandHandlerInterfaceRegistry::Instance().RegisterCommandHandler(&DynamicLightingServer::Instance()));
    VerifyOrReturn(AttributeAccessInterfaceRegistry::Instance().Register(&DynamicLightingServer::Instance()),
                   ChipLogError(Zcl, "DynamicLighting: failed to register attribute access"));
}

void MatterDynamicLightingPluginServerShutdownCallback()
{
    TEMPORARY_RETURN_IGNORED CommandHandlerInterfaceRegistry::Instance().UnregisterCommandHandler(&DynamicLightingServer::Instance());
    AttributeAccessInterfaceRegistry::Instance().Unregister(&DynamicLightingServer::Instance());
}

void MatterDynamicLightingClusterInitCallback(EndpointId endpointId)
{
    TEMPORARY_RETURN_IGNORED DynamicLightingServer::Instance().RegisterEndpoint(endpointId);
}

void MatterDynamicLightingClusterShutdownCallback(EndpointId endpointId, MatterClusterShutdownType)
{
    TEMPORARY_RETURN_IGNORED DynamicLightingServer::Instance().UnregisterEndpoint(endpointId);
}

void emberAfDynamicLightingClusterInitCallback(EndpointId endpoint)
{
    MatterDynamicLightingClusterInitCallback(endpoint);
}

void emberAfDynamicLightingClusterShutdownCallback(EndpointId endpoint)
{
    TEMPORARY_RETURN_IGNORED DynamicLightingServer::Instance().UnregisterEndpoint(endpoint);
}

void emberAfDynamicLightingClusterServerInitCallback(EndpointId endpoint)
{
    emberAfDynamicLightingClusterInitCallback(endpoint);
}

bool emberAfDynamicLightingClusterStartEffectCallback(CommandHandler * commandHandler, const ConcreteCommandPath & commandPath,
                                                      const Commands::StartEffect::DecodableType & commandData)
{
    commandHandler->AddStatus(commandPath, DynamicLightingServer::Instance().HandleStartEffect(commandPath.mEndpointId, commandData));
    return true;
}

bool emberAfDynamicLightingClusterStopEffectCallback(CommandHandler * commandHandler, const ConcreteCommandPath & commandPath,
                                                     const Commands::StopEffect::DecodableType &)
{
    commandHandler->AddStatus(commandPath, DynamicLightingServer::Instance().HandleStopEffect(commandPath.mEndpointId));
    return true;
}

void MatterDynamicLightingClusterServerShutdownCallback(EndpointId endpoint)
{
    emberAfDynamicLightingClusterShutdownCallback(endpoint);
}

void MatterDynamicLightingHandleAttributeChange(const ConcreteAttributePath & attributePath, uint16_t size, uint8_t * value)
{
    DynamicLightingServer::Instance().HandleExternalAttributeChange(attributePath, size, value);
}

} // namespace chip::app::Clusters::DynamicLighting

void MatterDynamicLightingPluginServerInitCallback()
{
    chip::app::Clusters::DynamicLighting::MatterDynamicLightingPluginServerInitCallback();
}

void MatterDynamicLightingPluginServerShutdownCallback()
{
    chip::app::Clusters::DynamicLighting::MatterDynamicLightingPluginServerShutdownCallback();
}

void emberAfDynamicLightingClusterInitCallback(chip::EndpointId endpoint)
{
    chip::app::Clusters::DynamicLighting::emberAfDynamicLightingClusterInitCallback(endpoint);
}

void emberAfDynamicLightingClusterShutdownCallback(chip::EndpointId endpoint)
{
    chip::app::Clusters::DynamicLighting::emberAfDynamicLightingClusterShutdownCallback(endpoint);
}

void emberAfDynamicLightingClusterServerInitCallback(chip::EndpointId endpoint)
{
    chip::app::Clusters::DynamicLighting::emberAfDynamicLightingClusterServerInitCallback(endpoint);
}

bool emberAfDynamicLightingClusterStartEffectCallback(
    chip::app::CommandHandler * commandHandler, const chip::app::ConcreteCommandPath & commandPath,
    const chip::app::Clusters::DynamicLighting::Commands::StartEffect::DecodableType & commandData)
{
    return chip::app::Clusters::DynamicLighting::emberAfDynamicLightingClusterStartEffectCallback(commandHandler, commandPath,
                                                                                                  commandData);
}

bool emberAfDynamicLightingClusterStopEffectCallback(
    chip::app::CommandHandler * commandHandler, const chip::app::ConcreteCommandPath & commandPath,
    const chip::app::Clusters::DynamicLighting::Commands::StopEffect::DecodableType & commandData)
{
    return chip::app::Clusters::DynamicLighting::emberAfDynamicLightingClusterStopEffectCallback(commandHandler, commandPath,
                                                                                                 commandData);
}

void MatterDynamicLightingClusterServerShutdownCallback(chip::EndpointId endpoint)
{
    chip::app::Clusters::DynamicLighting::MatterDynamicLightingClusterServerShutdownCallback(endpoint);
}
