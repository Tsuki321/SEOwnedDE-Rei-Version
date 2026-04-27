#include <gtest/gtest.h>
#include "App/Features/CFG.h"
#include "SDK/SDK.h"

// Mock the MAKE_HOOK architecture to inject logic for testing
#undef MAKE_SIGNATURE
#define MAKE_SIGNATURE(...)
#undef MAKE_HOOK
#define MAKE_HOOK(name, address, ret, calling, ...) \
    ret name(__VA_ARGS__)

int g_CalledOriginal = 0;
#undef CALL_ORIGINAL
#define CALL_ORIGINAL(...) g_CalledOriginal++

// Include the target file itself to compile its body within our test scope
#include "App/Hooks/CBaseEntity_AddVar.cpp"

// Mock IInterpolatedVar
class MockInterpolatedVar : public IInterpolatedVar {
public:
    std::string m_debugName;
    MockInterpolatedVar(const char* name) : m_debugName(name) {}

    virtual ~MockInterpolatedVar() {}
    virtual void Setup(void* pValue, int type) {}
    virtual void SetInterpolationAmount(float seconds) {}
    virtual void NoteLastNetworkedValue() {}
    virtual bool NoteChanged(float changetime, bool bUpdateLastNetworkedValue) { return false; }
    virtual void Reset() {}
    virtual int Interpolate(float currentTime) { return 0; }
    virtual int GetType() const { return 0; }
    virtual void RestoreToLastNetworked() {}
    virtual void Copy(IInterpolatedVar* pSrc) {}
    virtual const char* GetDebugName() { return m_debugName.c_str(); }
    virtual void SetDebugName(const char* pName) {}
    virtual void SetDebug(bool bDebug) {}
};

TEST(AnimationSmoothnessTest, ValidatesVelocityBlocked) {
    g_CalledOriginal = 0;
    CFG::Misc_Accuracy_Improvements = true;
    
    char dummyEcx[1024] = {0};
    C_BaseEntity* pEntity = reinterpret_cast<C_BaseEntity*>(dummyEcx);
    
    MockInterpolatedVar watcher("C_BaseEntity::m_iv_vecVelocity");
    
    // Call the hooked function directly
    CBaseEntity_AddVar(pEntity, nullptr, reinterpret_cast<IInterpolatedVar*>(&watcher), 0, false);
    
    // Should NOT have called original, because velocity MUST be blocked
    EXPECT_EQ(g_CalledOriginal, 0);
}

TEST(AnimationSmoothnessTest, ValidatesPoseParametersBlocked) {
    g_CalledOriginal = 0;
    CFG::Misc_Accuracy_Improvements = true;
    char dummyEcx[1024] = {0};
    C_BaseEntity* pEntity = reinterpret_cast<C_BaseEntity*>(dummyEcx);
    
    MockInterpolatedVar watcher("C_BaseAnimating::m_iv_flPoseParameter");
    
    CBaseEntity_AddVar(pEntity, nullptr, reinterpret_cast<IInterpolatedVar*>(&watcher), 0, false);
    
    // Pose params are filtered by the hook when accuracy improvements are enabled.
    EXPECT_EQ(g_CalledOriginal, 0);
}

TEST(AnimationSmoothnessTest, ValidatesCycleBlocked) {
    g_CalledOriginal = 0;
    CFG::Misc_Accuracy_Improvements = true;
    char dummyEcx[1024] = {0};
    C_BaseEntity* pEntity = reinterpret_cast<C_BaseEntity*>(dummyEcx);
    MockInterpolatedVar watcher("C_BaseAnimating::m_iv_flCycle");
    CBaseEntity_AddVar(pEntity, nullptr, reinterpret_cast<IInterpolatedVar*>(&watcher), 0, false);
    EXPECT_EQ(g_CalledOriginal, 0);
}

