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
			if (I::GlobalVars && H::Entities->GetLocal())
				F::LagRecords->BeginRenderFrame();
			else
				F::LagRecords->ClearRenderPoseTime();
			// Capture the engine-rendered body and the timestamp shared by live shots.
			if (const auto pLocal = H::Entities->GetLocal(); pLocal && I::GlobalVars)
			{
				const float flRenderPoseTime = F::LagRecords->GetLivePoseTime();
				if (std::isfinite(flRenderPoseTime) && flRenderPoseTime > 0.0f)
				{
					for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
					{
						if (!pEntity)
							continue;

						const auto pPlayer = pEntity->As<C_TFPlayer>();
						const float flPoseTime = flRenderPoseTime;
						if (!std::isfinite(flPoseTime) || flPoseTime <= 0.0f)
							continue;

						// Admit at most one tick beyond the latest network sample during jitter.
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
