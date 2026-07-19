#include "../../SDK/SDK.h"

MAKE_SIGNATURE(CBaseEntity_AddVar, "client.dll", "48 89 5C 24 ? 48 89 6C 24 ? 57 41 56 41 57 48 83 EC ? 33 DB 48 89 74 24", 0x0);

MAKE_HOOK(CBaseEntity_AddVar, Signatures::CBaseEntity_AddVar.Get(), void, __fastcall,
	C_BaseEntity* ecx, void* data, IInterpolatedVar* watcher, int type, bool bSetup)
{
	CALL_ORIGINAL(ecx, data, watcher, type, bSetup);
}

MAKE_HOOK(CBaseEntity_EstimateAbsVelocity, Signatures::CBaseEntity_EstimateAbsVelocity.Get(), void, __fastcall,
	C_BaseEntity* ecx, Vector& vel)
{
	CALL_ORIGINAL(ecx, vel);
}
