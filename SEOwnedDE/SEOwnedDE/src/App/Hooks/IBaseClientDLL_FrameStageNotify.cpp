#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/WorldModulation/WorldModulation.h"
#include "../Features/LagRecords/LagRecords.h"
#include "../Features/MiscVisuals/MiscVisuals.h"
#include "../Features/MovementSimulation/MovementSimulation.h"
#include "../Features/SkinChanger/SkinChanger.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace
{
	struct VisualInterpState_t
	{
		C_TFPlayer* Player = nullptr;
		Vec3 PreviousOrigin = {};
		Vec3 CurrentOrigin = {};
		float CurrentSimulationTime = 0.0f;
		float LastUpdateRealTime = 0.0f;
		float InterpolationDuration = 0.0f;
		bool Initialized = false;
	};

	struct ActiveVisualOffset_t
	{
		C_TFPlayer* Player = nullptr;
		Vec3 OriginalAbsOrigin = {};
		CUtlVector<matrix3x4_t>* CachedBones = nullptr;
		int BoneCount = 0;
		std::array<matrix3x4_t, MAX_BONE_COUNT> OriginalBones = {};
		bool HasBoneSnapshot = false;
	};

	std::array<VisualInterpState_t, MAX_PLAYERS + 1> g_VisualInterpStates = {};
	std::array<ActiveVisualOffset_t, MAX_PLAYERS + 1> g_ActiveVisualOffsets = {};

	void RestoreAccuracyVisualOffsets()
	{
		for (int nIndex = 1; nIndex <= MAX_PLAYERS; ++nIndex)
		{
			auto& entry = g_ActiveVisualOffsets[nIndex];
			if (!entry.Player)
				continue;

			C_TFPlayer* pPlayer = nullptr;
			if (I::ClientEntityList)
			{
				if (const auto pEntity = I::ClientEntityList->GetClientEntity(nIndex))
					pPlayer = pEntity->As<C_TFPlayer>();
			}

			// A recycled entity index makes the stored pointer unsafe to touch.
			if (pPlayer == entry.Player)
			{
				pPlayer->SetAbsOrigin(entry.OriginalAbsOrigin);
				bool bRestoredBones = false;

				if (entry.HasBoneSnapshot && entry.CachedBones == pPlayer->GetCachedBoneData())
				{
					const auto pBones = entry.CachedBones;
					const int nCount = pBones ? pBones->Count() : 0;
					if (nCount == entry.BoneCount && nCount > 0 && nCount <= MAX_BONE_COUNT)
					{
						auto* pBase = pBones->Base();
						if (pBase)
						{
							memcpy(pBase, entry.OriginalBones.data(), sizeof(matrix3x4_t) * nCount);
							bRestoredBones = true;
						}
					}
				}

				// Rendering may rebuild or reallocate the cache while the visual origin
				// is active. Such matrices were generated in the offset coordinate
				// space and cannot be restored from the old snapshot safely. Invalidate
				// them after restoring the accurate origin so the next request rebuilds
				// against the network pose.
				if (!bRestoredBones)
					pPlayer->InvalidateBoneCache();
			}

			entry = {};
		}
	}

	void UpdateAccuracyVisualInterpolation()
	{
		if (!CFG::Misc_Accuracy_Improvements || !I::GlobalVars)
		{
			g_VisualInterpStates = {};
			return;
		}

		const auto pLocal = H::Entities->GetLocal();
		if (!pLocal)
		{
			g_VisualInterpStates = {};
			return;
		}

		std::array<bool, MAX_PLAYERS + 1> seen = {};

		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
		{
			if (!pEntity || pEntity == pLocal)
				continue;

			const auto pPlayer = pEntity->As<C_TFPlayer>();
			const int nIndex = pPlayer->entindex();
			if (nIndex < 1 || nIndex > MAX_PLAYERS || pPlayer->deadflag() || pPlayer->IsDormant())
				continue;

			seen[nIndex] = true;
			auto& state = g_VisualInterpStates[nIndex];
			const float flSimulationTime = pPlayer->m_flSimulationTime();
			const Vec3 vNetworkOrigin = pPlayer->m_vecOrigin();

			if (!state.Initialized || state.Player != pPlayer)
			{
				state.Player = pPlayer;
				state.PreviousOrigin = vNetworkOrigin;
				state.CurrentOrigin = vNetworkOrigin;
				state.CurrentSimulationTime = flSimulationTime;
				state.LastUpdateRealTime = I::GlobalVars->realtime;
				state.InterpolationDuration = TICK_INTERVAL;
				state.Initialized = true;
				continue;
			}

			if (flSimulationTime == state.CurrentSimulationTime)
				continue;

			state.PreviousOrigin = state.CurrentOrigin;
			state.CurrentOrigin = vNetworkOrigin;
			state.InterpolationDuration = std::max(flSimulationTime - state.CurrentSimulationTime, TICK_INTERVAL);
			state.CurrentSimulationTime = flSimulationTime;
			state.LastUpdateRealTime = I::GlobalVars->realtime;

			// Do not visually sweep across teleports or entity discontinuities.
			if ((state.CurrentOrigin - state.PreviousOrigin).LengthSqr() > 200.0f * 200.0f)
				state.PreviousOrigin = state.CurrentOrigin;
		}

		for (int nIndex = 1; nIndex <= MAX_PLAYERS; ++nIndex)
		{
			if (!seen[nIndex])
				g_VisualInterpStates[nIndex] = {};
		}
	}

	void ApplyAccuracyVisualOffsets()
	{
		if (!CFG::Misc_Accuracy_Improvements || !I::GlobalVars)
			return;

		const auto pLocal = H::Entities->GetLocal();
		if (!pLocal)
			return;

		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
		{
			if (!pEntity || pEntity == pLocal)
				continue;

			const auto pPlayer = pEntity->As<C_TFPlayer>();
			const int nIndex = pPlayer->entindex();
			if (nIndex < 1 || nIndex > MAX_PLAYERS || pPlayer->deadflag() || pPlayer->IsDormant())
				continue;

			const auto& state = g_VisualInterpStates[nIndex];
			if (!state.Initialized || state.Player != pPlayer)
				continue;

			const float flElapsed = I::GlobalVars->realtime - state.LastUpdateRealTime;
			const float flFraction = state.InterpolationDuration > 0.0f
				? std::clamp(flElapsed / state.InterpolationDuration, 0.0f, 1.0f)
				: 1.0f;
			const Vec3 vVisualOrigin = state.PreviousOrigin
				+ (state.CurrentOrigin - state.PreviousOrigin) * flFraction;
			const Vec3 vDelta = vVisualOrigin - pPlayer->GetAbsOrigin();

			if (vDelta.LengthSqr() < 0.01f)
				continue;

			auto& entry = g_ActiveVisualOffsets[nIndex];
			entry.Player = pPlayer;
			entry.OriginalAbsOrigin = pPlayer->GetAbsOrigin();

			pPlayer->SetAbsOrigin(vVisualOrigin);

			if (const auto pBones = pPlayer->GetCachedBoneData())
			{
				const int nCount = pBones->Count();
				auto* pBase = pBones->Base();
				if (pBase && nCount > 0 && nCount <= MAX_BONE_COUNT)
				{
					entry.CachedBones = pBones;
					entry.BoneCount = nCount;
					memcpy(entry.OriginalBones.data(), pBase, sizeof(matrix3x4_t) * nCount);
					entry.HasBoneSnapshot = true;

					for (int n = 0; n < nCount; ++n)
					{
						pBase[n][0][3] += vDelta.x;
						pBase[n][1][3] += vDelta.y;
						pBase[n][2][3] += vDelta.z;
					}
				}
			}
		}
	}
}

MAKE_HOOK(IBaseClientDLL_FrameStageNotify, Memory::GetVFunc(I::BaseClientDLL, 35), void, __fastcall,
	void* ecx, ClientFrameStage_t curStage)
{
	// Visual offsets exist only between FRAME_RENDER_START and the next frame
	// stage. Restore the accurate network pose before the engine, CreateMove, or
	// lag-record code can observe the entity again.
	RestoreAccuracyVisualOffsets();

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
			UpdateAccuracyVisualInterpolation();

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

			// Lag records and all gameplay consumers have already observed the
			// accurate network pose. Smooth only the render origin/bones from here
			// until the next frame stage.
			ApplyAccuracyVisualOffsets();

			break;
		}

		default: break;
	}
}
