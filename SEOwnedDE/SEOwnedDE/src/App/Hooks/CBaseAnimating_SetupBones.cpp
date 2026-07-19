#include "../../SDK/SDK.h"

#include "../Features/LagRecords/LagRecords.h"

MAKE_SIGNATURE(CBaseAnimating_SetupBones, "client.dll", "48 8B C4 44 89 40 ? 48 89 50 ? 55 53", 0x0);

MAKE_HOOK(CBaseAnimating_SetupBones, Signatures::CBaseAnimating_SetupBones.Get(), bool, __fastcall,
	C_BaseAnimating* ecx, matrix3x4_t* pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime)
{
	// Historical poses are installed only inside an explicit CLagRecordScope.
	// Every ordinary render/prediction request stays on the engine SetupBones path.
	if (F::LagRecordMatrixHelper->IsActive() && ecx)
	{
		const auto pEntity = reinterpret_cast<C_BaseEntity*>(reinterpret_cast<uintptr_t>(ecx) - sizeof(uintptr_t));
		if (F::LagRecordMatrixHelper->IsActiveFor(pEntity))
		{
			// The scope already installed the historical matrices in the player's
			// internal cache. A null output request must not recompute over them.
			if (!pBoneToWorldOut)
				return true;

			if (nMaxBones > 0 && F::LagRecordMatrixHelper->CopyActiveBones(pEntity, pBoneToWorldOut, nMaxBones))
				return true;
		}
	}

	return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);
}
