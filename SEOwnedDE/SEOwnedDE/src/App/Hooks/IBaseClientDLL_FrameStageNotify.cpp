#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/WorldModulation/WorldModulation.h"
#include "../Features/LagRecords/LagRecords.h"
#include "../Features/MiscVisuals/MiscVisuals.h"

#include <algorithm>

// Restores entity origins and bone caches to their accurate (non-interpolated) state
// after visual offsets were applied for rendering in the previous frame.
static void RestoreVisualOffsets()
{
	for (const auto& entry : G::vecActiveVisualOffsets)
	{
		if (!entry.pPlayer)
			continue;

		entry.pPlayer->SetAbsOrigin(entry.vOriginalAbsOrigin);

		if (const auto pBones = entry.pPlayer->As<C_BaseAnimating>()->GetCachedBoneData())
		{
			const int nCount = pBones->Count();
			auto* pBase = pBones->Base();

			for (int i = 0; i < nCount; i++)
			{
				pBase[i][0][3] -= entry.vDelta.x;
				pBase[i][1][3] -= entry.vDelta.y;
				pBase[i][2][3] -= entry.vDelta.z;
			}
		}
	}

	G::vecActiveVisualOffsets.clear();
}

// Updates the per-player visual interpolation tracking data when new network updates arrive.
static void UpdateVisualInterpData()
{
	if (!CFG::Misc_Accuracy_Improvements)
		return;

	const auto pLocal = H::Entities->GetLocal();

	if (!pLocal)
	{
		G::mapVisualInterpData.clear();
		return;
	}

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
	{
		if (!pEntity || pEntity == pLocal)
			continue;

		const auto pPlayer = pEntity->As<C_TFPlayer>();

		if (!pPlayer || pPlayer->deadflag())
		{
			G::mapVisualInterpData.erase(pPlayer);
			continue;
		}

		auto& data = G::mapVisualInterpData[pPlayer];
		const float flSimTime = pPlayer->m_flSimulationTime();

		if (!data.bInitialized)
		{
			data.vPreviousOrigin = pPlayer->m_vecOrigin();
			data.vCurrentOrigin = pPlayer->m_vecOrigin();
			data.flCurrentSimTime = flSimTime;
			data.flLastUpdateRealTime = I::GlobalVars->realtime;
			data.flInterpDuration = TICK_INTERVAL;
			data.bInitialized = true;
		}
		else if (flSimTime != data.flCurrentSimTime)
		{
			data.vPreviousOrigin = data.vCurrentOrigin;
			data.vCurrentOrigin = pPlayer->m_vecOrigin();
			data.flInterpDuration = std::max(flSimTime - data.flCurrentSimTime, TICK_INTERVAL);
			data.flCurrentSimTime = flSimTime;
			data.flLastUpdateRealTime = I::GlobalVars->realtime;

			// Teleport detection: snap if distance is unreasonably large
			if ((data.vCurrentOrigin - data.vPreviousOrigin).LengthSqr() > 200.0f * 200.0f)
			{
				data.vPreviousOrigin = data.vCurrentOrigin;
			}
		}
	}

	// Clean up stale entries for players no longer tracked
	if (G::mapVisualInterpData.size() > 64)
	{
		G::mapVisualInterpData.clear();
	}
}

// Applies smooth visual position offsets to non-local players right before rendering.
// This offsets both the abs origin and the cached bone matrices so the model
// renders at the interpolated position while internal aim data stays accurate.
static void ApplyVisualOffsets()
{
	if (!CFG::Misc_Accuracy_Improvements)
		return;

	const auto pLocal = H::Entities->GetLocal();

	if (!pLocal)
		return;

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
	{
		if (!pEntity || pEntity == pLocal)
			continue;

		const auto pPlayer = pEntity->As<C_TFPlayer>();

		if (!pPlayer || pPlayer->deadflag())
			continue;

		const auto it = G::mapVisualInterpData.find(pPlayer);

		if (it == G::mapVisualInterpData.end() || !it->second.bInitialized)
			continue;

		const auto& data = it->second;

		// Calculate interpolation fraction based on real elapsed time since last network update
		const float flElapsed = I::GlobalVars->realtime - data.flLastUpdateRealTime;
		float flFraction = data.flInterpDuration > 0.0f ? (flElapsed / data.flInterpDuration) : 1.0f;
		flFraction = std::clamp(flFraction, 0.0f, 1.0f);

		// Lerp between previous and current network origin
		const Vec3 vVisualOrigin = data.vPreviousOrigin + (data.vCurrentOrigin - data.vPreviousOrigin) * flFraction;
		const Vec3 vDelta = vVisualOrigin - pPlayer->GetAbsOrigin();

		// Skip if no meaningful offset
		if (vDelta.LengthSqr() < 0.01f)
			continue;

		// Store restore data
		G::VisualOffsetEntry_t entry = {};
		entry.pPlayer = pPlayer;
		entry.vOriginalAbsOrigin = pPlayer->GetAbsOrigin();
		entry.vDelta = vDelta;
		G::vecActiveVisualOffsets.push_back(entry);

		// Apply visual origin
		pPlayer->SetAbsOrigin(vVisualOrigin);

		// Offset cached bone matrices by the same delta so the skeleton matches the visual origin
		if (const auto pBones = pPlayer->As<C_BaseAnimating>()->GetCachedBoneData())
		{
			const int nCount = pBones->Count();
			auto* pBase = pBones->Base();

			for (int i = 0; i < nCount; i++)
			{
				pBase[i][0][3] += vDelta.x;
				pBase[i][1][3] += vDelta.y;
				pBase[i][2][3] += vDelta.z;
			}
		}
	}
}

MAKE_HOOK(IBaseClientDLL_FrameStageNotify, Memory::GetVFunc(I::BaseClientDLL, 35), void, __fastcall,
	void* ecx, ClientFrameStage_t curStage)
{
	// Restore visual offsets from the previous render frame before the engine processes
	// any new stage. This ensures CreateMove / aimbot always sees accurate (non-interpolated)
	// entity data, while visual smoothing is only active during rendering.
	RestoreVisualOffsets();

	CALL_ORIGINAL(ecx, curStage);

	switch (curStage)
	{
		case FRAME_NET_UPDATE_START:
		{
			H::Entities->ClearCache();

			break;
		}

		case FRAME_NET_UPDATE_END:
		{
			H::Entities->UpdateCache();

			if (const auto pLocal = H::Entities->GetLocal())
			{
				for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
				{
					if (!pEntity || pEntity == pLocal)
						continue;

					const auto pPlayer = pEntity->As<C_TFPlayer>();

					if (const auto nDifference = std::clamp(TIME_TO_TICKS(pPlayer->m_flSimulationTime() - pPlayer->m_flOldSimulationTime()), 0, 22))
					{
						//deal with animations, local player is dealt with in RunCommand
						if (CFG::Misc_Accuracy_Improvements)
						{
							const float flOldFrameTime = I::GlobalVars->frametime;

							I::GlobalVars->frametime = I::Prediction->m_bEnginePaused ? 0.0f : TICK_INTERVAL;

							for (int n = 0; n < nDifference; n++)
							{
								G::bUpdatingAnims = true;
								pPlayer->UpdateClientSideAnimation();
								G::bUpdatingAnims = false;
							}

							I::GlobalVars->frametime = flOldFrameTime;
						}

						//add the lag record
						if (CFG::Misc_SetupBones_Optimization)
						{
							if (!pPlayer->deadflag())
							{
								F::LagRecords->AddRecord(pPlayer);
							}
						}

						else
						{
							if (pPlayer->m_iTeamNum() != pLocal->m_iTeamNum() && !pPlayer->deadflag())
							{
								F::LagRecords->AddRecord(pPlayer);
							}
						}
					}
				}
			}

			F::LagRecords->UpdateRecords();

			if (G::mapVelFixRecords.size() > 64)
			{
				G::mapVelFixRecords.clear();
			}

			for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
			{
				if (!pEntity)
					continue;

				const auto pPlayer = pEntity->As<C_TFPlayer>();

				if (pPlayer->deadflag())
					continue;

				G::mapVelFixRecords[pPlayer] = { pPlayer->m_vecOrigin(), pPlayer->m_fFlags(), pPlayer->m_flSimulationTime() };
			}

			// Update per-player visual interpolation tracking after all network data is processed
			UpdateVisualInterpData();

			break;
		}

		case FRAME_RENDER_START:
		{
			H::Input->Update();

			F::WorldModulation->UpdateWorldModulation();
			F::MiscVisuals->ViewModelSway();
			F::MiscVisuals->DetailProps();

			//fake taunt stuff
			{
				static bool bWasEnabled = false;

				if (CFG::Misc_Fake_Taunt)
				{
					bWasEnabled = true;

					if (G::bStartedFakeTaunt)
					{
						if (const auto pLocal = H::Entities->GetLocal())
						{
							if (const auto pAnimState = pLocal->GetAnimState())
							{
								const auto& gs = pAnimState->m_aGestureSlots[GESTURE_SLOT_VCD];

								if (gs.m_pAnimLayer && (gs.m_pAnimLayer->m_flCycle >= 1.0f || gs.m_pAnimLayer->m_nSequence <= 0))
								{
									G::bStartedFakeTaunt = false;
									pLocal->m_nForceTauntCam() = 0;
								}
							}
						}
					}
				}
				else
				{
					G::bStartedFakeTaunt = false;

					if (bWasEnabled)
					{
						bWasEnabled = false;

						if (const auto pLocal = H::Entities->GetLocal())
						{
							pLocal->m_nForceTauntCam() = 0;
						}
					}
				}
			}

			// Apply smooth visual position offsets right before the engine renders
			ApplyVisualOffsets();

			break;
		}

		default: break;
	}
}
