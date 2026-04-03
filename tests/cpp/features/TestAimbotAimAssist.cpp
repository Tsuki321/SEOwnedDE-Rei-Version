#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "App/Features/Aimbot/AimbotHitscan/AimbotHitscan.h"
#include "App/Features/CFG.h"
#include "SDK/SDK.h"

class MockAimUtils {
public:
    MOCK_METHOD(void, FixMovement, (CUserCmd* pCmd, const Vec3& vAngles));
};

// Safe mock subclass for C_TFPlayer
class MockTFPlayer : public C_TFPlayer {
public:
    // Overriding methods if necessary, ensures vtable exists correctly
};

TEST(AimbotHitscanTest, AimAssistCalculatesCorrectStrength) {
    MockAimUtils mockAimUtils;
    // Assume H::AimUtils exists or is mocked if testing code calls it
    
    CUserCmd cmd = {};
    cmd.viewangles = { 10.f, 20.f, 0.f };

    MockTFPlayer localPlayer;
    // Initialize mock player state safely via NETVAR overrides or offset mapping if needed
    // But since NetVar returns 0 locally, m_vecPunchAngle is read from localPlayer class beginning.
    
    CFG::Aimbot_Hitscan_Aim_Type = 3;
    CFG::Aimbot_Hitscan_AimAssist_Strength = 2.0f;

    Vec3 targetAngle = { 20.f, 40.f, 0.f };
    CAimbotHitscan hitscan;
    
    hitscan.Aim(&cmd, &localPlayer, targetAngle);

    EXPECT_FLOAT_EQ(cmd.viewangles.x, 15.f);
    EXPECT_FLOAT_EQ(cmd.viewangles.y, 30.f);
    EXPECT_FLOAT_EQ(cmd.viewangles.z, 0.f);
}
