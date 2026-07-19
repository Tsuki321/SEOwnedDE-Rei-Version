#include <gtest/gtest.h>
#include "SDK/SDK.h"

#undef MAKE_SIGNATURE
#define MAKE_SIGNATURE(...)
#undef MAKE_HOOK
#define MAKE_HOOK(name, address, ret, calling, ...) \
    ret name(__VA_ARGS__)

namespace {
int g_CalledOriginal = 0;
}

#undef CALL_ORIGINAL
#define CALL_ORIGINAL(...) g_CalledOriginal++

#include "App/Hooks/CBaseEntity_AddVar.cpp"

class MockInterpolatedVar : public IInterpolatedVar {
public:
    explicit MockInterpolatedVar(const char* name) : m_debugName(name) {}

    virtual ~MockInterpolatedVar() {}
    virtual void Setup(void*, int) {}
    virtual void SetInterpolationAmount(float) {}
    virtual void NoteLastNetworkedValue() {}
    virtual bool NoteChanged(float, bool) { return false; }
    virtual void Reset() {}
    virtual int Interpolate(float) { return 0; }
    virtual int GetType() const { return 0; }
    virtual void RestoreToLastNetworked() {}
    virtual void Copy(IInterpolatedVar*) {}
    virtual const char* GetDebugName() { return m_debugName.c_str(); }
    virtual void SetDebugName(const char*) {}
    virtual void SetDebug(bool) {}

private:
    std::string m_debugName;
};

TEST(AnimationSmoothnessTest, VelocityInterpolationPassesThrough) {
    g_CalledOriginal = 0;
    char dummyEcx[1024] = {};
    MockInterpolatedVar watcher("C_BaseEntity::m_iv_vecVelocity");

    CBaseEntity_AddVar(reinterpret_cast<C_BaseEntity*>(dummyEcx), nullptr, &watcher, 0, false);

    EXPECT_EQ(g_CalledOriginal, 1);
}

TEST(AnimationSmoothnessTest, AnimationInputsPassThrough) {
    char dummyEcx[1024] = {};
    C_BaseEntity* entity = reinterpret_cast<C_BaseEntity*>(dummyEcx);
    MockInterpolatedVar maxSpeed("CMultiPlayerAnimState::m_iv_flMaxGroundSpeed");
    MockInterpolatedVar pose("C_BaseAnimating::m_iv_flPoseParameter");
    MockInterpolatedVar cycle("C_BaseAnimating::m_iv_flCycle");

    g_CalledOriginal = 0;
    CBaseEntity_AddVar(entity, nullptr, &maxSpeed, 0, false);
    CBaseEntity_AddVar(entity, nullptr, &pose, 0, false);
    CBaseEntity_AddVar(entity, nullptr, &cycle, 0, false);

    EXPECT_EQ(g_CalledOriginal, 3);
}

TEST(AnimationSmoothnessTest, NullWatcherPassesThrough) {
    g_CalledOriginal = 0;
    char dummyEcx[1024] = {};

    CBaseEntity_AddVar(reinterpret_cast<C_BaseEntity*>(dummyEcx), nullptr, nullptr, 0, false);

    EXPECT_EQ(g_CalledOriginal, 1);
}

TEST(AnimationSmoothnessTest, VelocityEstimationPassesThrough) {
    g_CalledOriginal = 0;
    char dummyEcx[1024] = {};
    Vector velocity = {};

    CBaseEntity_EstimateAbsVelocity(reinterpret_cast<C_BaseEntity*>(dummyEcx), velocity);

    EXPECT_EQ(g_CalledOriginal, 1);
}
