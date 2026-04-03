#include "SDK/SDK.h"
#include "App/Features/CFG.h"
#include "Utils/Hash/Hash.h"

class CMockBaseClientDLL : public IBaseClientDLL {
public:
    virtual int Init(CreateInterfaceFn appSystemFactory, CreateInterfaceFn physicsFactory, CGlobalVarsBase *pGlobals) { return 0; }
    virtual void PostInit() {}
    virtual void Shutdown(void) {}
    virtual bool ReplayInit(CreateInterfaceFn replayFactory) { return false; }
    virtual bool ReplayPostInit() { return false; }
    virtual void LevelInitPreEntity(char const *pMapName) {}
    virtual void LevelInitPostEntity() {}
    virtual void LevelShutdown(void) {}
    virtual ClientClass *GetAllClasses(void) { return nullptr; }
    virtual int HudVidInit(void) { return 0; }
    virtual void HudProcessInput(bool bActive) {}
    virtual void HudUpdate(bool bActive) {}
    virtual void HudReset(void) {}
};

CMockBaseClientDLL g_mockClient;

namespace I {
  IBaseClientDLL* BaseClientDLL = &g_mockClient;
  IVEngineClient* EngineClient = nullptr;
  IVModelInfoClient* ModelInfoClient = nullptr;
  CGlobalVarsBase* GlobalVars = nullptr;
  IMaterialSystem* MaterialSystem = nullptr;
}

