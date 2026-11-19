#include "../../SDK/SDK.h"

#include "../Features/CFG.h"

MAKE_SIGNATURE(CBaseEntity_AddVar, "client.dll", "48 89 5C 24 ? 48 89 6C 24 ? 57 41 56 41 57 48 83 EC ? 33 DB 48 89 74 24", 0x0);

MAKE_HOOK(CBaseEntity_AddVar, Signatures::CBaseEntity_AddVar.Get(), void, __fastcall,
	C_BaseEntity* ecx, void* data, IInterpolatedVar* watcher, int type, bool bSetup)
{
	// Registration is permanent for the entity's lifetime. Keep every watcher
	// so body animations, eye angles, weapons and cosmetics interpolate together.
	CALL_ORIGINAL(ecx, data, watcher, type, bSetup);
}

MAKE_HOOK(CBaseEntity_EstimateAbsVelocity, Signatures::CBaseEntity_EstimateAbsVelocity.Get(), void, __fastcall,
	C_BaseEntity* ecx, Vector& vel)
{
	if (CFG::Misc_Accuracy_Improvements && ecx
		&& ecx->GetClassId() == ETFClassIds::CTFPlayer)
	{
		const auto pLocal = (I::ClientEntityList && I::EngineClient)
			? H::Entities->GetLocal()
			: nullptr;
		if (const auto pPlayer = ecx->As<C_TFPlayer>();
			(!pLocal || ecx != pLocal) && pPlayer)
		{
			vel = pPlayer->m_vecVelocity();
			return;
		}
	}

	CALL_ORIGINAL(ecx, vel);
}
