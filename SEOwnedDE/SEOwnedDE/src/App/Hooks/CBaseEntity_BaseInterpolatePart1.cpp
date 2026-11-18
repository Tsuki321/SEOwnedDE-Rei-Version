#include "../../SDK/SDK.h"

#include "../Features/CFG.h"

MAKE_SIGNATURE(CBaseEntity_BaseInterpolatePart1, "client.dll", "48 89 5C 24 ? 56 57 41 55 41 56 41 57 48 83 EC ? 4C 8B BC 24", 0x0);

MAKE_HOOK(CBaseEntity_BaseInterpolatePart1, Signatures::CBaseEntity_BaseInterpolatePart1.Get(), int, __fastcall,
	void* ecx, float& currentTime, Vector& oldOrigin, QAngle& oldAngles, Vector& oldVel, int& bNoMoreChanges)
{
	const auto pLocal = H::Entities->GetLocal();
	const auto pEntity = static_cast<C_BaseEntity*>(ecx);
	if (pLocal && pEntity && ((pEntity == pLocal && Shifting::bRecharging)
		|| (CFG::Misc_Accuracy_Improvements && pEntity->GetClassId() == ETFClassIds::CBaseDoor)))
	{
		bNoMoreChanges = 1;
		return 0;
	}

	// Players, wearables and weapons must share the engine's interpolated pose.
	// Accuracy Improvements resolves shots against that pose and its history.
	return CALL_ORIGINAL(ecx, currentTime, oldOrigin, oldAngles, oldVel, bNoMoreChanges);
}
