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

#include <app/clusters/dynamic-lighting-server/dynamic-lighting-server.h>
#include <app/MessageDef/ReportDataMessage.h>
#include <app/data-model-provider/tests/ReadTesting.h>
#include <app/data-model-provider/tests/TestConstants.h>
#include <clusters/DynamicLighting/Attributes.h>
#include <pw_unit_test/framework.h>

#include <array>
#include <vector>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::DynamicLighting;

bool emberAfContainsServer(chip::EndpointId endpoint, chip::ClusterId clusterId)
{
    return endpoint == 1 && clusterId == DynamicLighting::Id;
}

namespace {

constexpr EndpointId kTestEndpointId      = 1;
constexpr uint16_t kUnknownEffectId       = 999;
constexpr size_t kMaxAvailableEffects     = 128;
constexpr size_t kReadBufferSize          = 16384;
constexpr uint16_t kNominalMaxSpeed       = 5;
constexpr uint16_t kNominalDefaultSpeed   = 2;
constexpr char kNominalEffectLabel[]      = "Effect";

CHIP_ERROR DecodeAttributeReports(ByteSpan data, std::vector<Testing::DecodedAttributeData> & decodedItems)
{
    TLV::TLVReader reportIBsReader;
    reportIBsReader.Init(data);

    ReturnErrorOnFailure(reportIBsReader.Next());
    VerifyOrReturnError(reportIBsReader.GetType() == TLV::TLVType::kTLVType_Structure, CHIP_ERROR_INVALID_ARGUMENT);

    TLV::TLVType outerStructureType;
    ReturnErrorOnFailure(reportIBsReader.EnterContainer(outerStructureType));

    ReturnErrorOnFailure(reportIBsReader.Next());
    VerifyOrReturnError(reportIBsReader.GetType() == TLV::TLVType::kTLVType_Array, CHIP_ERROR_INVALID_ARGUMENT);

    TLV::TLVType outerArrayType;
    ReturnErrorOnFailure(reportIBsReader.EnterContainer(outerArrayType));

    CHIP_ERROR err = CHIP_NO_ERROR;
    while ((err = reportIBsReader.Next()) == CHIP_NO_ERROR)
    {
        TLV::TLVReader attributeReportReader = reportIBsReader;
        app::AttributeReportIB::Parser attributeReportParser;
        ReturnErrorOnFailure(attributeReportParser.Init(attributeReportReader));

        app::AttributeDataIB::Parser dataParser;
        ReturnErrorOnFailure(attributeReportParser.GetAttributeData(&dataParser));

        Testing::DecodedAttributeData decoded;
        ReturnErrorOnFailure(decoded.DecodeFrom(dataParser));
        decodedItems.push_back(decoded);
    }

    VerifyOrReturnError(err == CHIP_END_OF_TLV, err);
    ReturnErrorOnFailure(reportIBsReader.ExitContainer(outerArrayType));
    ReturnErrorOnFailure(reportIBsReader.ExitContainer(outerStructureType));

    return CHIP_NO_ERROR;
}

class CountingDelegate : public Delegate
{
public:
    size_t getEffectByIndexCallCount = 0;

    CHIP_ERROR GetEffectByIndex(EndpointId endpoint, size_t index, EffectInfo & effect) override
    {
        VerifyOrReturnError(endpoint == kTestEndpointId, CHIP_ERROR_INVALID_ARGUMENT);

        ++getEffectByIndexCallCount;
        if (index >= kMaxAvailableEffects)
        {
            return CHIP_ERROR_INTERNAL;
        }

        effect.effectID             = static_cast<uint16_t>(index + 1);
        effect.source               = EffectSourceEnum::kInternal;
        effect.label                = CharSpan::fromCharString(kNominalEffectLabel);
        effect.maxSpeed             = kNominalMaxSpeed;
        effect.defaultSpeed         = kNominalDefaultSpeed;
        effect.supportsColorPalette = false;
        return CHIP_NO_ERROR;
    }
};

struct TestDynamicLightingCluster : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_EQ(Platform::MemoryInit(), CHIP_NO_ERROR); }
    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    void SetUp() override
    {
        SetDefaultDelegate(nullptr);
        ASSERT_EQ(mServer.RegisterEndpoint(kTestEndpointId), CHIP_NO_ERROR);
    }

    void TearDown() override
    {
        SetDefaultDelegate(nullptr);
        ASSERT_EQ(mServer.UnregisterEndpoint(kTestEndpointId), CHIP_NO_ERROR);
    }

    template <typename T>
    CHIP_ERROR ReadAttribute(AttributeId attributeId, T & out)
    {
        ConcreteReadAttributePath path(kTestEndpointId, DynamicLighting::Id, attributeId);
        std::array<uint8_t, kReadBufferSize> buffer;
        TLV::TLVWriter writer;
        writer.Init(buffer.data(), buffer.size());

        TLV::TLVType outerStructureType;
        ReturnErrorOnFailure(writer.StartContainer(TLV::AnonymousTag(), TLV::kTLVType_Structure, outerStructureType));

        app::AttributeReportIBs::Builder builder;
        ReturnErrorOnFailure(builder.Init(&writer, to_underlying(app::ReportDataMessage::Tag::kAttributeReportIBs)));

        AttributeValueEncoder encoder(builder, Testing::kDenySubjectDescriptor, path, 0x1234, false, app::AttributeEncodeState());
        ReturnErrorOnFailure(mServer.Read(path, encoder));

        builder.EndOfContainer();
        ReturnErrorOnFailure(writer.EndContainer(outerStructureType));
        ReturnErrorOnFailure(writer.Finalize());

        ByteSpan encoded(buffer.data(), writer.GetLengthWritten());
        std::vector<Testing::DecodedAttributeData> attributeData;
        ReturnErrorOnFailure(DecodeAttributeReports(encoded, attributeData));
        VerifyOrReturnError(attributeData.size() == 1u, CHIP_ERROR_INCORRECT_STATE);

        return DataModel::Decode(attributeData[0].dataReader, out);
    }

    DynamicLightingServer & mServer = DynamicLightingServer::Instance();
};

TEST_F(TestDynamicLightingCluster, AvailableEffectsReadStopsAtSpecLimit)
{
    CountingDelegate delegate;
    SetDefaultDelegate(&delegate);

    Attributes::AvailableEffects::TypeInfo::DecodableType effects;
    EXPECT_EQ(ReadAttribute(Attributes::AvailableEffects::Id, effects), CHIP_NO_ERROR);

    size_t effectCount = 0;
    EXPECT_EQ(effects.ComputeSize(&effectCount), CHIP_NO_ERROR);
    EXPECT_EQ(effectCount, kMaxAvailableEffects);
    EXPECT_EQ(delegate.getEffectByIndexCallCount, kMaxAvailableEffects);
}

TEST_F(TestDynamicLightingCluster, StartEffectLookupStopsAtSpecLimit)
{
    CountingDelegate delegate;
    SetDefaultDelegate(&delegate);

    Commands::StartEffect::DecodableType startEffect;
    startEffect.effectID = kUnknownEffectId;
    startEffect.speed    = 1;

    EXPECT_EQ(mServer.HandleStartEffect(kTestEndpointId, startEffect), Protocols::InteractionModel::Status::InvalidCommand);
    EXPECT_EQ(delegate.getEffectByIndexCallCount, kMaxAvailableEffects);
}

} // namespace
