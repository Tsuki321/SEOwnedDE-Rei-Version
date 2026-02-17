#include "../../SDK/SDK.h"

#include "../Features/CFG.h"

MAKE_SIGNATURE(CSequenceTransitioner_CheckForSequenceChange, "client.dll", "48 85 D2 0F 84 ? ? ? ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24", 0x0);

MAKE_HOOK(CSequenceTransitioner_CheckForSequenceChange, Signatures::CSequenceTransitioner_CheckForSequenceChange.Get(), void, __fastcall,
	void* ecx, CStudioHdr* hdr, int nCurSequence, bool bForceNewSequence, bool bInterpolate)
{
	// Only force-disable sequence transition interpolation when SetupBones_Optimization is active,
	// since cached bones can't reflect interpolated transitions anyway. When the optimization is
	// off, allow transition blending for smoother animation visuals.
	if (CFG::Misc_Accuracy_Improvements && CFG::Misc_SetupBones_Optimization)
	{
		bInterpolate = false;
	}

	CALL_ORIGINAL(ecx, hdr, nCurSequence, bForceNewSequence, bInterpolate);
}
