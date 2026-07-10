#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

// End-to-end integration contract for the animation smoothness pipeline.
//
// The pipeline spans multiple files. Rather than reconstructing the engine in a unit
// test, this suite verifies that the *call chain contracts* hold across the
// participating sources:
//
//   FRAME_NET_UPDATE_END (FrameStageNotify)
//     -> per remote player: UpdateClientSideAnimation x N (tick-locked)
//     -> per remote player: LagRecords::AddRecord
//       -> SetupBones writes into LagRecord_t::BoneData
//     -> arrVelFixRecords refreshed
//   FRAME_RENDER_START
//     -> CBaseAnimating::SetupBones consumes cached bones + delta correction
//   CPrediction::RunCommand
//     -> drives local player anim state
//   AddVar
//     -> filters interpolated vars used by remote pose
//
// Each link must remain wired together; this test catches accidental decoupling.

namespace {
struct PipelineFile {
    const char* path;
    std::vector<const char*> required_tokens;
};

const PipelineFile kPipeline[] = {
    {
        "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_FrameStageNotify.cpp",
        {
            "FRAME_NET_UPDATE_END",
            "G::bUpdatingAnims",
            "pPlayer->UpdateClientSideAnimation()",
            "F::LagRecords->AddRecord(pPlayer)",
            "G::arrVelFixRecords",
        },
    },
    {
        "SEOwnedDE/SEOwnedDE/src/App/Hooks/CTFPlayer_UpdateClientSideAnimation.cpp",
        {
            "G::bUpdatingAnims",
            "UpdateAllViewmodelAddons()",
        },
    },
    {
        "SEOwnedDE/SEOwnedDE/src/App/Hooks/CPrediction_RunCommand.cpp",
        {
            "pAnimState->Update",
            "FrameAdvance",
            "TICK_INTERVAL",
        },
    },
    {
        "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.cpp",
        {
            "newRecord.BoneData",
            "newRecord.AbsOrigin",
            "newRecord.AbsAngles",
            "newRecord.SimulationTime",
            "records[newHead]",
        },
    },
    {
        "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseAnimating_SetupBones.cpp",
        {
            "GetCachedBoneData()",
            "F::LagRecords->",
            "pRecord->AbsOrigin",
            "pBoneToWorldOut",
        },
    },
    {
        "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_SetAbsVelocity.cpp",
        {
            "G::arrVelFixRecords",
            "FL_DUCKING",
        },
    },
    {
        "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_AddVar.cpp",
        {
            "m_iv_vecVelocity",
            "m_iv_flPoseParameter",
            "m_iv_flCycle",
            "m_iv_flMaxGroundSpeed",
            "m_iv_angEyeAngles",
        },
    },
};
}  // namespace

TEST(AnimationPipelineIntegration, AllParticipatingFilesExist) {
    const auto root = testhelpers::FindRepoRoot();
    for (const auto& entry : kPipeline) {
        const auto fullPath = root / entry.path;
        EXPECT_TRUE(std::filesystem::exists(fullPath))
            << "Pipeline source missing: " << fullPath.string();
    }
}

TEST(AnimationPipelineIntegration, AllPipelineLinksRetainContractTokens) {
    const auto root = testhelpers::FindRepoRoot();
    for (const auto& entry : kPipeline) {
        const auto fullPath = root / entry.path;
        ASSERT_TRUE(std::filesystem::exists(fullPath));
        const auto src = testhelpers::ReadTextFile(fullPath);

        for (const auto* token : entry.required_tokens) {
            EXPECT_NE(src.find(token), std::string::npos)
                << "File " << entry.path << " missing required pipeline token: " << token;
        }
    }
}

TEST(AnimationPipelineIntegration, CatchUpLoopAndLagRecordCallSitesAreColocated) {
    const auto root = testhelpers::FindRepoRoot();
    const auto frameStage = testhelpers::ReadTextFile(
        root / "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_FrameStageNotify.cpp");

    // Anim catch-up loop and LagRecord capture must occur within the same frame stage,
    // so the catch-up loop and AddRecord call must both be present.
    const auto loopPos = frameStage.find("UpdateClientSideAnimation()");
    const auto addPos = frameStage.find("AddRecord(pPlayer)");
    ASSERT_NE(loopPos, std::string::npos);
    ASSERT_NE(addPos, std::string::npos);

    // Anim catch-up should precede the lag record capture so bones reflect the
    // freshest pose for the recorded snapshot.
    EXPECT_LT(loopPos, addPos);
}

TEST(AnimationPipelineIntegration, AimbotTickRemapUsesInterpAmount) {
    const auto root = testhelpers::FindRepoRoot();
    const auto hitscan = testhelpers::ReadTextFile(
        root / "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp");

    // The aimbot must remap tick_count using the client interp amount.
	EXPECT_NE(hitscan.find("SDKUtils::GetLerp()"), std::string::npos);
    EXPECT_NE(hitscan.find("TIME_TO_TICKS"), std::string::npos);
    EXPECT_NE(hitscan.find("SimulationTime"), std::string::npos);
}
