#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/WorldModulation/WorldModulation.h"
#include "../Features/LagRecords/LagRecords.h"
#include "../Features/MiscVisuals/MiscVisuals.h"
#include "../Features/MovementSimulation/MovementSimulation.h"
#include "../Features/SkinChanger/SkinChanger.h"

MAKE_HOOK(IBaseClientDLL_FrameStageNotify, Memory::GetVFunc(I::BaseClientDLL, 35), void, __fastcall,
	void* ecx, ClientFrameStage_t curStage)
{
	if (curStage == FRAME_NET_UPDATE_POSTDATAUPDATE_END)
		F::SkinChanger->Run();

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

			if (H::Entities->GetLocal())
			{
				for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
				{
					if (!pEntity)
						continue;

					const auto pPlayer = pEntity->As<C_TFPlayer>();
					if (pPlayer->deadflag()
						|| pPlayer->m_flSimulationTime() <= pPlayer->m_flOldSimulationTime())
						continue;

					if (CFG::Aimbot_Projectile_Hitchance_Enabled)
						F::MovementSimulation->StoreMoveRecord(pPlayer);
				}
			}

			for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
			{
				if (!pEntity)
					continue;

				const auto pPlayer = pEntity->As<C_TFPlayer>();

				const int nIndex = pPlayer->entindex();

				if (nIndex < 1 || nIndex > MAX_PLAYERS)
					continue;

				if (pPlayer->deadflag())
				{
					G::arrVelFixRecords[nIndex].m_pOwner = nullptr;
					continue;
				}

				G::arrVelFixRecords[nIndex] = { pPlayer, pPlayer->m_vecOrigin(), pPlayer->m_fFlags(), pPlayer->m_flSimulationTime() };
			}

			break;
		}

		case FRAME_RENDER_START:
		{
			// The original frame-stage handler has now applied interpolation and
			// client animation. Capture that coherent visible pose without advancing
			// or rewinding the live animation state a second time.
			if (const auto pLocal = H::Entities->GetLocal(); pLocal && I::GlobalVars)
			{
				const float flPoseTime = I::GlobalVars->curtime - SDKUtils::GetLerp();
				if (std::isfinite(flPoseTime) && flPoseTime > 0.0f)
				{
					for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
					{
						if (!pEntity)
							continue;

						const auto pPlayer = pEntity->As<C_TFPlayer>();
						// The interpolation target can sit past the latest network
						// sample, at which point the visible pose is extrapolated.
						// Tolerate a bounded amount of that rather than dropping the
						// frame: with a tight interp (cl_interp 0 + cl_interp_ratio 1
						// puts lerp at roughly one snapshot interval) the target is
						// beyond the sample on a large share of frames, and jitter on
						// an unstable connection pushes it further - so a hard reject
						// here starved the ring and left manual shots with nothing to
						// backtrack to at all, intermittently and in step with ping.
						// One tick of extrapolation still falls inside a tick the
						// server holds history for. Note the pose time recorded stays
						// curtime-based either way; clamping it onto
						// m_flSimulationTime would put a server-clock value in a field
						// every age and tick calculation reads as client-clock.
						if (flPoseTime > pPlayer->m_flSimulationTime() + CLagRecords::GetMaxExtrapolationTime())
							continue;

						if (CLagRecords::ShouldCaptureRecord(pLocal, pPlayer))
							F::LagRecords->AddRenderRecord(pPlayer, flPoseTime);
					}
				}
			}

			F::LagRecords->UpdateRecords();
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

			break;
		}

		default: break;
	}
}
