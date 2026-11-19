#include "../../SDK/SDK.h"

#include "../Features/Aimbot/AimbotHitscan/AimbotHitscan.h"
#include "../Features/Aimbot/AimbotProjectile/AimbotProjectile.h"
#include "../Features/Materials/Materials.h"
#include "../Features/Outlines/Outlines.h"
#include "../Features/WorldModulation/WorldModulation.h"
#include "../Features/Paint/Paint.h"
#include "../Features/SeedPred/SeedPred.h"
#include "../Features/Killstreak/Killstreak.h"
#include "../Features/NetworkFix/NetworkFix.h"
#include "../Features/SkinChanger/SkinChanger.h"
#include "../Features/SkinChanger/LocalCosmetics.h"
#include "../Features/LagRecords/LagRecords.h"
#include "../Features/EnginePrediction/EnginePrediction.h"

MAKE_HOOK(IBaseClientDLL_LevelShutdown, Memory::GetVFunc(I::BaseClientDLL, 7), void, __fastcall,
	void* ecx)
{
	F::NetworkFix->Reset();
	F::EnginePrediction->Reset();
	F::LagRecords->ClearRenderPoseTime();
	F::LocalCosmetics->LevelShutdown();
	F::ProjectileSim->CleanUp(); // release bodies before model collision data unloads
	CALL_ORIGINAL(ecx);

	H::Entities->ClearCache();
	H::Entities->ClearModelIndexes();

	F::AimbotHitscan->Reset();
	F::AimbotProjectile->Reset();
	F::Materials->CleanUp();
	F::Outlines->CleanUp();
	F::Paint->CleanUp();
	F::WorldModulation->LevelShutdown();

	F::SeedPred->Reset();
	F::Killstreak->Reset();
	F::SkinChanger->ResetRuntimeState();

	G::arrVelFixRecords = {};

	Shifting::Reset();
}
