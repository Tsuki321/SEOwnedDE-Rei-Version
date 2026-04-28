#include "../../SDK/SDK.h"

#include "../Features/CFG.h"

MAKE_SIGNATURE(CSequenceTransitioner_CheckForSequenceChange, "client.dll", "48 85 D2 0F 84 ? ? ? ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24", 0x0);

MAKE_HOOK(CSequenceTransitioner_CheckForSequenceChange, Signatures::CSequenceTransitioner_CheckForSequenceChange.Get(), void, __fastcall,
	void* ecx, CStudioHdr* hdr, int nCurSequence, bool bForceNewSequence, bool bInterpolate)
{
	// Phase 2: sequence transition interpolation is now allowed to run unmodified.
	// The bone-fidelity work in CBaseAnimating_SetupBones (yaw delta correction +
	// two-record translation lerp) keeps cached bones path-aware, so transition
	// blending no longer fights the cached pose. The hook is retained as scaffolding
	// for future overrides without re-introducing a registration.
	CALL_ORIGINAL(ecx, hdr, nCurSequence, bForceNewSequence, bInterpolate);
}
