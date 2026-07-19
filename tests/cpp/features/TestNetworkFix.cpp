#include <gtest/gtest.h>

#include "App/Features/NetworkFix/NetworkFix.h"
#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/NetworkFix";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/NetworkFix/NetworkFix.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/NetworkFix/NetworkFix.h";
constexpr const char* kCLMoveSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/CL_Move.cpp";
constexpr const char* kCreateMoveSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/ClientModeShared_CreateMove.cpp";
constexpr const char* kSendNetMsgSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/INetChannel_SendNetMsg.cpp";
constexpr const char* kLevelInitSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_LevelInitPostEntity.cpp";
constexpr const char* kLevelShutdownSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_LevelShutdown.cpp";
}

TEST(NetworkReadGateBehavior, ConsumesMatchingStateOnce) {
    CReadPacketGate gate;
    int channel = 0;

    EXPECT_FALSE(gate.Consume(10, &channel));

    gate.Arm(10, &channel);
    EXPECT_TRUE(gate.IsArmedFor(10, &channel));
    EXPECT_TRUE(gate.Consume(10, &channel));
    EXPECT_FALSE(gate.Consume(10, &channel));
}

TEST(NetworkReadGateBehavior, RejectsStaleFrameAndChangedChannel) {
    CReadPacketGate gate;
    int channelA = 0;
    int channelB = 0;

    gate.Arm(10, &channelA);
    EXPECT_FALSE(gate.Consume(11, &channelA));
    EXPECT_FALSE(gate.IsArmedFor(10, &channelA));

    gate.Arm(12, &channelA);
    EXPECT_FALSE(gate.Consume(12, &channelB));
    EXPECT_FALSE(gate.IsArmedFor(12, &channelA));
}

TEST(NetworkReadGateBehavior, ResetDisarmsPendingState) {
    CReadPacketGate gate;
    int channel = 0;

    gate.Arm(20, &channel);
    gate.Reset();

    EXPECT_FALSE(gate.IsArmedFor(20, &channel));
    EXPECT_FALSE(gate.Consume(20, &channel));
}

TEST(NetworkShiftCapacityBehavior, RespectsCommandEnvelopeBoundaries) {
    EXPECT_EQ(CNetworkFix::GetShiftCommandCapacity(-1), MAX_COMMANDS);
    EXPECT_EQ(CNetworkFix::GetShiftCommandCapacity(0), MAX_COMMANDS);
    EXPECT_EQ(CNetworkFix::GetShiftCommandCapacity(1), MAX_COMMANDS - 1);
    EXPECT_EQ(CNetworkFix::GetShiftCommandCapacity(14), MAX_COMMANDS - 14);
    EXPECT_EQ(CNetworkFix::GetShiftCommandCapacity(21), 1);
    EXPECT_EQ(CNetworkFix::GetShiftCommandCapacity(22), 0);
    EXPECT_EQ(CNetworkFix::GetShiftCommandCapacity(23), 0);
}

TEST(NetworkFixContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(NetworkFixContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Misc_Ping_Reducer"), std::string::npos);
    EXPECT_NE(mainSource.find("CReadPacketState::Store("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(NetworkFixContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(1));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(1));
}

TEST(NetworkFixContracts, ArmsAndConsumesOnlyCurrentFrameAndChannel) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kHeaderSource);
    const auto source = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(header.find("class CReadPacketGate"), std::string::npos);
    EXPECT_NE(header.find("m_nFrame == nFrame && m_pChannel == pChannel"), std::string::npos);
    EXPECT_NE(header.find("bool Consume(int nFrame, const void* pChannel)"), std::string::npos);

    const auto earlyRead = source.find("Hook.Original<Hooks::CL_ReadPackets::fn>()");
    const auto arm = source.find("m_ReadGate.Arm(nFrame, pNetChannel)");
    const auto consume = source.find("m_ReadGate.Consume(I::GlobalVars->framecount, pNetChannel)");
    const auto restore = source.find("m_State.Restore()", consume);

    ASSERT_NE(earlyRead, std::string::npos);
    ASSERT_NE(arm, std::string::npos);
    ASSERT_NE(consume, std::string::npos);
    ASSERT_NE(restore, std::string::npos);
    EXPECT_LT(earlyRead, arm);
    EXPECT_LT(consume, restore);
}

TEST(NetworkFixContracts, ReadsEarlyOnceOutsideSyntheticMoveLoop) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kCLMoveSource);

    EXPECT_EQ(testhelpers::CountOccurrences(source, "F::NetworkFix->FixInputDelay("), 1u);

    const auto earlyRead = source.find("F::NetworkFix->FixInputDelay(bFinalTick)");
    const auto syntheticMoveLambda = source.find("auto callOriginal");
    ASSERT_NE(earlyRead, std::string::npos);
    ASSERT_NE(syntheticMoveLambda, std::string::npos);
    EXPECT_LT(earlyRead, syntheticMoveLambda);
}

TEST(NetworkFixContracts, ResetsAcrossToggleAndLevelTransitions) {
    const auto root = testhelpers::FindRepoRoot();
    const auto networkFix = testhelpers::ReadTextFile(root / kMainSource);
    const auto clMove = testhelpers::ReadTextFile(root / kCLMoveSource);
    const auto levelInit = testhelpers::ReadTextFile(root / kLevelInitSource);
    const auto levelShutdown = testhelpers::ReadTextFile(root / kLevelShutdownSource);

    EXPECT_NE(networkFix.find("if (!CFG::Misc_Ping_Reducer"), std::string::npos);
    EXPECT_NE(clMove.find("F::NetworkFix->Reset()"), std::string::npos);
    EXPECT_NE(levelInit.find("F::NetworkFix->Reset()"), std::string::npos);
    EXPECT_NE(levelShutdown.find("F::NetworkFix->Reset()"), std::string::npos);
}

TEST(NetworkIntegrityContracts, CapsOrdinaryCommandChokeBeforeOverflow) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kCreateMoveSource);

    EXPECT_NE(source.find("!Shifting::bShifting"), std::string::npos);
    EXPECT_NE(source.find("chokedcommands >= (MAX_NEW_COMMANDS - 1)"), std::string::npos);
    EXPECT_EQ(source.find("chokedcommands > 22"), std::string::npos);
}

TEST(NetworkIntegrityContracts, LeavesOrdinaryMoveMessagesUntouched) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSendNetMsgSource);

    EXPECT_NE(source.find("msg.GetType() != clc_Move || !Shifting::bShifting"), std::string::npos);
    EXPECT_NE(source.find("nChokedCommands < MAX_NEW_COMMANDS || nChokedCommands >= MAX_COMMANDS"), std::string::npos);
    EXPECT_EQ(source.find("pMsg->m_DataOut.StartWriting"), std::string::npos);
    EXPECT_EQ(source.find("pMsg->m_nNewCommands ="), std::string::npos);
    EXPECT_EQ(source.find("pMsg->m_nBackupCommands ="), std::string::npos);
}

TEST(NetworkIntegrityContracts, ShiftSerializationIsTransactionalAndPropagatesResult) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSendNetMsgSource);

    EXPECT_NE(source.find("CLC_Move shiftedMsg = *pMsg"), std::string::npos);
    EXPECT_NE(source.find("alignas(4) std::array<unsigned char, 4000> shiftedData"), std::string::npos);
    EXPECT_NE(source.find("return CALL_ORIGINAL(pNet, msg, bForceReliable, bVoice)"), std::string::npos);
    EXPECT_NE(source.find("const bool bSent = CALL_ORIGINAL"), std::string::npos);
    EXPECT_NE(source.find("if (!bSent)"), std::string::npos);
    EXPECT_NE(source.find("return bSent"), std::string::npos);
	EXPECT_NE(source.find("pNet->m_nChokedPackets < nExtraCommands"), std::string::npos);
	EXPECT_EQ(source.find("std::max(0, nOldChokedPackets - nExtraCommands)"), std::string::npos);
    EXPECT_EQ(source.find("\n\t\treturn true;"), std::string::npos);
}

TEST(NetworkIntegrityContracts, BoundsEverySyntheticBurstByRemainingCapacity) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kHeaderSource);
    const auto source = testhelpers::ReadTextFile(root / kCLMoveSource);

    EXPECT_NE(header.find("GetShiftCommandCapacity(int nChokedCommands)"), std::string::npos);
    EXPECT_NE(source.find("getShiftCommandCapacity()"), std::string::npos);
    EXPECT_NE(source.find("nCommandCapacity < 2"), std::string::npos);
    EXPECT_NE(source.find("std::min(Shifting::nAvailableTicks, nCommandCapacity)"),
              std::string::npos);
    EXPECT_GE(testhelpers::CountOccurrences(source, "callOriginal(bFinalTick);"), 3u);
}
