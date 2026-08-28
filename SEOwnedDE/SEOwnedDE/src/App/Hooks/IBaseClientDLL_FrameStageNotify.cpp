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
			// Animations are engine-owned in both modes. With Accuracy Improvements
			// enabled, BaseInterpolatePart1 keeps the origin at the newest network
			// sample, so pair those bones with m_flSimulationTime. Otherwise capture
			// the vanilla rendered pose at curtime - lerp.
			if (const auto pLocal = H::Entities->GetLocal(); pLocal && I::GlobalVars)
			{
				const float flRenderPoseTime = I::GlobalVars->curtime - SDKUtils::GetLerp();
				if (std::isfinite(flRenderPoseTime) && flRenderPoseTime > 0.0f)
				{
					for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
					{
						if (!pEntity)
							continue;

						const auto pPlayer = pEntity->As<C_TFPlayer>();
						const float flPoseTime = CFG::Misc_Accuracy_Improvements
							? pPlayer->m_flSimulationTime()
							: flRenderPoseTime;
						if (!std::isfinite(flPoseTime) || flPoseTime <= 0.0f)
							continue;

						// Vanilla interpolation can put the render target just beyond
						// the latest network sample. Admit one bounded extrapolation
						// tick in that mode so jitter does not starve the ring. Accuracy
						// mode already selected the exact network simulation time and
						// therefore needs no extrapolation allowance here.
						if (!CFG::Misc_Accuracy_Improvements
							&& flPoseTime > pPlayer->m_flSimulationTime() + CLagRecords::GetMaxExtrapolationTime())
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
