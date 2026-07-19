#include <gtest/gtest.h>
#include "SDK/SDK.h"

#undef MAKE_SIGNATURE
#define MAKE_SIGNATURE(...)
#undef MAKE_HOOK
#define MAKE_HOOK(name, address, ret, calling, ...) \
    ret name(__VA_ARGS__)

namespace {
int g_ResetLatchedCalledOriginal = 0;
}

#undef CALL_ORIGINAL
#define CALL_ORIGINAL(...) g_ResetLatchedCalledOriginal++

#include "App/Hooks/CBaseEntity_ResetLatched.cpp"

TEST(ResetLatchedTest, AlwaysInvokesEngineReset) {
    g_ResetLatchedCalledOriginal = 0;
    char dummyEcx[256] = {};

    CBaseEntity_ResetLatched(dummyEcx);

    EXPECT_EQ(g_ResetLatchedCalledOriginal, 1);
}

TEST(ResetLatchedTest, RepeatedCallsAreNotSuppressed) {
    g_ResetLatchedCalledOriginal = 0;
    char dummyEcx[256] = {};

    for (int i = 0; i < 5; ++i)
        CBaseEntity_ResetLatched(dummyEcx);

    EXPECT_EQ(g_ResetLatchedCalledOriginal, 5);
}
