#include <gtest/gtest.h>
#include "App/Features/CFG.h"
#include "SDK/SDK.h"

// Mock the MAKE_HOOK / MAKE_SIGNATURE machinery so we can pull the hook body in.
#undef MAKE_SIGNATURE
#define MAKE_SIGNATURE(...)
#undef MAKE_HOOK
#define MAKE_HOOK(name, address, ret, calling, ...) \
    ret name(__VA_ARGS__)

static int g_ResetLatched_CalledOriginal = 0;
#undef CALL_ORIGINAL
#define CALL_ORIGINAL(...) g_ResetLatched_CalledOriginal++

#include "App/Hooks/CBaseEntity_ResetLatched.cpp"

// When the prediction-error jitter fix is on, the hook must short-circuit and
// never invoke the original. Stale latched state survives, but jitter is suppressed.
TEST(ResetLatchedTest, JitterFixOnSuppressesOriginal) {
    g_ResetLatched_CalledOriginal = 0;
    CFG::Misc_Pred_Error_Jitter_Fix = true;

    char dummyEcx[256] = {0};
    CBaseEntity_ResetLatched(dummyEcx);

    EXPECT_EQ(g_ResetLatched_CalledOriginal, 0);
}

// When the toggle is off, the hook must call the original engine implementation
// so latched state is reset normally.
TEST(ResetLatchedTest, JitterFixOffInvokesOriginal) {
    g_ResetLatched_CalledOriginal = 0;
    CFG::Misc_Pred_Error_Jitter_Fix = false;

    char dummyEcx[256] = {0};
    CBaseEntity_ResetLatched(dummyEcx);

    EXPECT_EQ(g_ResetLatched_CalledOriginal, 1);

    // Restore default for downstream tests in the same translation unit.
    CFG::Misc_Pred_Error_Jitter_Fix = true;
}

// Multiple invocations with the gate on must all be suppressed (idempotent).
TEST(ResetLatchedTest, RepeatedCallsAreAllSuppressed) {
    g_ResetLatched_CalledOriginal = 0;
    CFG::Misc_Pred_Error_Jitter_Fix = true;

    char dummyEcx[256] = {0};
    for (int i = 0; i < 5; ++i) {
        CBaseEntity_ResetLatched(dummyEcx);
    }

    EXPECT_EQ(g_ResetLatched_CalledOriginal, 0);
}
