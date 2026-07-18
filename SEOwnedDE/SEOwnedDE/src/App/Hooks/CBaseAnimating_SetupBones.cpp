#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/LagRecords/LagRecords.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

MAKE_SIGNATURE(CBaseAnimating_SetupBones, "client.dll", "48 8B C4 44 89 40 ? 48 89 50 ? 55 53", 0x0);

C_BaseEntity* GetRootMoveParent(C_BaseEntity* baseEnt)
{
	auto pEntity = baseEnt;
	auto pParent = baseEnt->GetMoveParent();

	constexpr int MAX_MOVE_PARENT_DEPTH = 32;
	auto its = 0;

	while (pParent)
	{
		if (its >= MAX_MOVE_PARENT_DEPTH)
			break;

		++its;
		pEntity = pParent;
		pParent = pEntity->GetMoveParent();
	}

	return pEntity;
}

static inline float NormalizeYawDelta(float deltaYawDegrees)
{
	return std::fmod(deltaYawDegrees + 540.0f, 360.0f) - 180.0f;
}

static inline void RotateBoneAroundOriginYaw(
	matrix3x4_t& bone,
	const Vec3& pivotOrigin,
	float sinYaw,
	float cosYaw,
	const Vec3& liveOrigin)
{
	const float px = bone[0][3] - pivotOrigin.x;
	const float py = bone[1][3] - pivotOrigin.y;
	const float pz = bone[2][3] - pivotOrigin.z;

	const float rx = px * cosYaw - py * sinYaw;
	const float ry = px * sinYaw + py * cosYaw;

	for (int col = 0; col < 3; ++col)
	{
		const float bx = bone[0][col];
		const float by = bone[1][col];
		bone[0][col] = bx * cosYaw - by * sinYaw;
		bone[1][col] = bx * sinYaw + by * cosYaw;
	}

	bone[0][3] = rx + liveOrigin.x;
	bone[1][3] = ry + liveOrigin.y;
	bone[2][3] = pz + liveOrigin.z;
}

namespace
{
	struct AdjustedBoneCacheEntry_t
	{
		int Frame = -1;
		C_TFPlayer* Player = nullptr;
		int PlayerHandle = -1;
		const matrix3x4_t* SourceBones = nullptr;
		const LagRecord_t* Record = nullptr;
		const LagRecord_t* PreviousRecord = nullptr;
		float RecordSimulationTime = -1.0f;
		float PreviousSimulationTime = -1.0f;
		float LiveYaw = 0.0f;
		float CurrentTime = 0.0f;
		Vec3 LiveOrigin = {};
		int RecordCount = 0;
		int BoneCount = 0;
		int BoneMask = 0;
		bool WithinBlendDistance = false;
		std::array<matrix3x4_t, MAX_BONE_COUNT> BoneData = {};
	};

	std::array<AdjustedBoneCacheEntry_t, MAX_PLAYERS> g_AdjustedBoneCache = {};

	bool SameVector(const Vec3& lhs, const Vec3& rhs)
	{
		return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
	}

	bool IsAdjustedBoneCacheHit(
		const AdjustedBoneCacheEntry_t& cache,
		int frame,
		C_TFPlayer* player,
		int playerHandle,
		const matrix3x4_t* sourceBones,
		int boneCount,
		const LagRecord_t* record,
		const LagRecord_t* previousRecord,
		int recordCount,
		const Vec3& liveOrigin,
		float liveYaw,
		float currentTime,
		int boneMask,
		bool withinBlendDistance)
	{
		return cache.Frame == frame
			&& cache.Player == player
			&& cache.PlayerHandle == playerHandle
			&& cache.SourceBones == sourceBones
			&& cache.BoneCount == boneCount
			&& cache.Record == record
			&& cache.PreviousRecord == previousRecord
			&& cache.RecordSimulationTime == record->SimulationTime
			&& cache.PreviousSimulationTime == (previousRecord ? previousRecord->SimulationTime : -1.0f)
			&& cache.RecordCount == recordCount
			&& SameVector(cache.LiveOrigin, liveOrigin)
			&& cache.LiveYaw == liveYaw
			&& cache.CurrentTime == currentTime
			&& cache.BoneMask == boneMask
			&& cache.WithinBlendDistance == withinBlendDistance;
	}
}

MAKE_HOOK(CBaseAnimating_SetupBones, Signatures::CBaseAnimating_SetupBones.Get(), bool, __fastcall,
	C_BaseAnimating* ecx, matrix3x4_t* pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime)
{
	if (CFG::Misc_SetupBones_Optimization && !F::LagRecords->IsSettingUpBones())
	{
		if (!ecx)
			return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

		const auto baseEnt = reinterpret_cast<C_BaseEntity*>(reinterpret_cast<uintptr_t>(ecx) - sizeof(uintptr_t));
		if (!baseEnt)
			return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

		const auto owner = GetRootMoveParent(baseEnt);
		const auto ent = owner ? owner : baseEnt;

		// Move-child entities such as cosmetics and weapons need their own model
		// setup; the root player's body cache is not valid for them.
		if (baseEnt != ent)
			return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

		if (F::LagRecords->HasFailedBones(baseEnt))
			return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

		const auto pLocal = H::Entities->GetLocal();
		if (ent->GetClassId() == ETFClassIds::CTFPlayer && ent != pLocal)
		{
			if (!pBoneToWorldOut)
				return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

			const auto bones = ent->As<C_BaseAnimating>()->GetCachedBoneData();
			if (!bones)
				return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

			const int nCachedCount = bones->Count();
			if (nCachedCount <= 0 || nCachedCount > MAX_BONE_COUNT)
				return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

			const int nCopyCount = std::min(nMaxBones, nCachedCount);
			if (nCopyCount <= 0)
				return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

			const auto pSourceBones = bones->Base();
			if (!pSourceBones)
				return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

			// A matrix-helper scope has already installed a specific historical pose.
			// Serve it verbatim and never let it populate the live adjusted cache.
			if (F::LagRecordMatrixHelper->IsActive())
			{
				std::memcpy(pBoneToWorldOut, pSourceBones, sizeof(matrix3x4_t) * nCopyCount);
				return true;
			}

			const auto pPlayer = ent->As<C_TFPlayer>();
			int nRecords = 0;
			const LagRecord_t* pRecord = nullptr;
			if (pPlayer && F::LagRecords->HasRecords(pPlayer, &nRecords) && nRecords > 0)
				pRecord = F::LagRecords->GetRecord(pPlayer, 0);

			if (!pRecord)
			{
				std::memcpy(pBoneToWorldOut, pSourceBones, sizeof(matrix3x4_t) * nCopyCount);
				return true;
			}

			const int nPlayerIndex = pPlayer->entindex();
			if (nPlayerIndex < 1 || nPlayerIndex >= MAX_PLAYERS)
				return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);

			const Vec3 vLiveOrigin = ent->GetAbsOrigin();
			const float flLiveYaw = ent->GetAbsAngles().y;
			const float flDistSqr = pLocal ? vLiveOrigin.DistToSqr(pLocal->GetAbsOrigin()) : 0.0f;
			constexpr float kLerpDistThresholdSqr = 2250000.0f;
			const bool bWithinBlendDistance = flDistSqr < kLerpDistThresholdSqr;

			const LagRecord_t* pPrev = nullptr;
			if (!pRecord->bTeleported && nRecords >= 2 && bWithinBlendDistance)
				pPrev = F::LagRecords->GetRecord(pPlayer, 1);

			const int nFrame = I::GlobalVars ? I::GlobalVars->framecount : -1;
			const int nPlayerHandle = pPlayer->GetRefEHandle().ToInt();
			auto& cache = g_AdjustedBoneCache[nPlayerIndex];

			if (!IsAdjustedBoneCacheHit(
				cache,
				nFrame,
				pPlayer,
				nPlayerHandle,
				pSourceBones,
				nCachedCount,
				pRecord,
				pPrev,
				nRecords,
				vLiveOrigin,
				flLiveYaw,
				currentTime,
				boneMask,
				bWithinBlendDistance))
			{
				cache.Frame = nFrame;
				cache.Player = pPlayer;
				cache.PlayerHandle = nPlayerHandle;
				cache.SourceBones = pSourceBones;
				cache.Record = pRecord;
				cache.PreviousRecord = pPrev;
				cache.RecordSimulationTime = pRecord->SimulationTime;
				cache.PreviousSimulationTime = pPrev ? pPrev->SimulationTime : -1.0f;
				cache.LiveOrigin = vLiveOrigin;
				cache.LiveYaw = flLiveYaw;
				cache.CurrentTime = currentTime;
				cache.RecordCount = nRecords;
				cache.BoneCount = nCachedCount;
				cache.BoneMask = boneMask;
				cache.WithinBlendDistance = bWithinBlendDistance;

				std::memcpy(cache.BoneData.data(), pSourceBones, sizeof(matrix3x4_t) * nCachedCount);

				const Vec3 vDelta = vLiveOrigin - pRecord->AbsOrigin;
				const float deltaYaw = NormalizeYawDelta(
					flLiveYaw - pRecord->AbsAngles.y);
				constexpr float kYawEpsilonDeg = 0.05f;

				if (std::fabs(deltaYaw) > kYawEpsilonDeg)
				{
					float sinYaw = 0.0f;
					float cosYaw = 1.0f;
					Math::SinCos(DEG2RAD(deltaYaw), &sinYaw, &cosYaw);

					for (int i = 0; i < nCachedCount; ++i)
					{
						RotateBoneAroundOriginYaw(
							cache.BoneData[i],
							pRecord->AbsOrigin,
							sinYaw,
							cosYaw,
							vLiveOrigin);
					}
				}
				else if (vDelta.LengthSqr() > 0.01f)
				{
					for (int i = 0; i < nCachedCount; ++i)
					{
						cache.BoneData[i][0][3] += vDelta.x;
						cache.BoneData[i][1][3] += vDelta.y;
						cache.BoneData[i][2][3] += vDelta.z;
					}
				}

				if (std::fabs(deltaYaw) <= kYawEpsilonDeg && !pRecord->bTeleported && nRecords >= 2 && bWithinBlendDistance && pPrev)
				{
					constexpr float kMinLerpDistSqr = 16.0f;
					if ((pRecord->AbsOrigin - pPrev->AbsOrigin).LengthSqr() >= kMinLerpDistSqr)
					{
						const float dt = pRecord->SimulationTime - pPrev->SimulationTime;
						if (dt > 1e-4f)
						{
							const float t = std::clamp(
								(currentTime - pPrev->SimulationTime) / dt,
								0.0f,
								1.0f);
							const float oneMinusT = 1.0f - t;

							if (oneMinusT > 0.001f)
							{
								const int nBlendCount = std::min({ nCachedCount, pRecord->BoneCount, pPrev->BoneCount });
								const auto* pPrevBones = pPrev->BoneData.data();
								const auto* pRecBones = pRecord->BoneData.data();
								const float odx = pPrev->AbsOrigin.x - pRecord->AbsOrigin.x;
								const float ody = pPrev->AbsOrigin.y - pRecord->AbsOrigin.y;
								const float odz = pPrev->AbsOrigin.z - pRecord->AbsOrigin.z;

								for (int i = 0; i < nBlendCount; ++i)
								{
									const float dx = (pPrevBones[i][0][3] - pRecBones[i][0][3]) - odx;
									const float dy = (pPrevBones[i][1][3] - pRecBones[i][1][3]) - ody;
									const float dz = (pPrevBones[i][2][3] - pRecBones[i][2][3]) - odz;

									cache.BoneData[i][0][3] += dx * oneMinusT;
									cache.BoneData[i][1][3] += dy * oneMinusT;
									cache.BoneData[i][2][3] += dz * oneMinusT;
								}
							}
						}
					}
				}
			}

			std::memcpy(pBoneToWorldOut, cache.BoneData.data(), sizeof(matrix3x4_t) * nCopyCount);
			return true;
		}
	}

	return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);
}
