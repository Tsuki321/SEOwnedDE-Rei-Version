#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/LagRecords/LagRecords.h"

MAKE_SIGNATURE(CBaseAnimating_SetupBones, "client.dll", "48 8B C4 44 89 40 ? 48 89 50 ? 55 53", 0x0);

//this here belongs to boss
//bless boss

C_BaseEntity* GetRootMoveParent(C_BaseEntity* baseEnt)
{
	auto pEntity = baseEnt;
	auto pParent = baseEnt->GetMoveParent();

	constexpr int MAX_MOVE_PARENT_DEPTH = 32;
	auto its{ 0 };

	while (pParent)
	{
		if (its > MAX_MOVE_PARENT_DEPTH)
		{
			break;
		}

		its++;

		pEntity = pParent;
		pParent = pEntity->GetMoveParent();
	}

	return pEntity;
}

// Phase 2 helper: normalize a yaw delta into [-180, 180] so subsequent sin/cos
// operate on the shortest angular path between the two poses.
static inline float NormalizeYawDelta(float deltaYawDegrees)
{
	deltaYawDegrees = std::fmod(deltaYawDegrees + 540.0f, 360.0f) - 180.0f;
	return deltaYawDegrees;
}

// Phase 2 helper: rotate a single bone matrix's translation + 3x3 basis around
// pivotOrigin by yaw radians (rotation in the world XY plane). This keeps the
// cached skeleton aligned with the live entity yaw without needing to rebuild
// bones from scratch.
static inline void RotateBoneAroundOriginYaw(matrix3x4_t& bone,
                                             const Vec3& pivotOrigin,
                                             float sinYaw, float cosYaw,
                                             const Vec3& liveOrigin)
{
	// Translate position relative to pivot.
	const float px = bone[0][3] - pivotOrigin.x;
	const float py = bone[1][3] - pivotOrigin.y;
	const float pz = bone[2][3] - pivotOrigin.z;

	// Rotate the translation around Z (yaw).
	const float rx = px * cosYaw - py * sinYaw;
	const float ry = px * sinYaw + py * cosYaw;

	// Rotate the basis columns 0..2 around Z.
	for (int col = 0; col < 3; ++col)
	{
		const float bx = bone[0][col];
		const float by = bone[1][col];
		bone[0][col] = bx * cosYaw - by * sinYaw;
		bone[1][col] = bx * sinYaw + by * cosYaw;
	}

	// Re-anchor at the live (engine-interpolated) origin.
	bone[0][3] = rx + liveOrigin.x;
	bone[1][3] = ry + liveOrigin.y;
	bone[2][3] = pz + liveOrigin.z;
}

MAKE_HOOK(CBaseAnimating_SetupBones, Signatures::CBaseAnimating_SetupBones.Get(), bool, __fastcall,
	C_BaseAnimating* ecx, matrix3x4_t* pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime)
{
	if (CFG::Misc_SetupBones_Optimization && !F::LagRecords->IsSettingUpBones())
	{
		const auto baseEnt = reinterpret_cast<C_BaseEntity*>(reinterpret_cast<uintptr_t>(ecx) - sizeof(uintptr_t));

		if (baseEnt)
		{
			const auto owner = GetRootMoveParent(baseEnt);
			const auto ent = owner ? owner : baseEnt;

			// Move-child entities such as cosmetics and weapons have their own model
			// setup. Do not serve the root player's cached body bones for them.
			if (baseEnt != ent)
			{
				return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);
			}

			// Phase 2: a wearable whose own SetupBones failed during lag-record
			// capture must fall through to the engine path so that frame's pose is
			// rebuilt from scratch instead of using stale cached data.
			if (F::LagRecords->HasFailedBones(baseEnt) || F::LagRecords->HasFailedBones(ent))
			{
				return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);
			}

			if (ent->GetClassId() == ETFClassIds::CTFPlayer && ent != H::Entities->GetLocal())
			{
				if (pBoneToWorldOut)
				{
					if (const auto bones = ent->As<C_BaseAnimating>()->GetCachedBoneData())
					{
						const int nCopyCount = std::min(nMaxBones, bones->Count());
						std::memcpy(pBoneToWorldOut, bones->Base(), sizeof(matrix3x4_t) * nCopyCount);

						// Offset cached bones to match the engine-interpolated visual origin.
						// Bones were computed at server-tick origin (stored in the latest lag record).
						// After engine interpolation runs, GetAbsOrigin() returns a smooth visual
						// position that may differ. Apply the delta so the skeleton follows smoothly.
						// Skip correction when LagRecordMatrixHelper::Set() is active — the caller
						// has already swapped in a specific lag record's bones and origin, and
						// applying delta correction from record 0 would corrupt that pose.
						const auto pPlayer = ent->As<C_TFPlayer>();
						int nRecords = 0;

						if (pPlayer && !F::LagRecordMatrixHelper->IsActive() && F::LagRecords->HasRecords(pPlayer, &nRecords) && nRecords > 0)
						{
							if (const auto pRecord = F::LagRecords->GetRecord(pPlayer, 0))
							{
								const Vec3 vLiveOrigin = ent->GetAbsOrigin();
								const Vec3 vDelta = vLiveOrigin - pRecord->AbsOrigin;

								// Phase 2: yaw rotational delta correction. When the live
								// AbsAngles yaw differs meaningfully from the recorded yaw
								// (e.g. fast spin), translation-only correction leaves the
								// skeleton facing the wrong way. Apply a yaw rotation about
								// the recorded origin to track the live yaw.
								const float deltaYaw = NormalizeYawDelta(
									ent->GetAbsAngles().y - pRecord->AbsAngles.y);

								constexpr float kYawEpsilonDeg = 0.05f;

								if (std::fabs(deltaYaw) > kYawEpsilonDeg)
								{
									float sinYaw = 0.0f, cosYaw = 1.0f;
									Math::SinCos(DEG2RAD(deltaYaw), &sinYaw, &cosYaw);

									for (int i = 0; i < nCopyCount; ++i)
									{
										RotateBoneAroundOriginYaw(
											pBoneToWorldOut[i],
											pRecord->AbsOrigin,
											sinYaw, cosYaw,
											vLiveOrigin);
									}
								}
								else if (vDelta.LengthSqr() > 0.01f)
								{
									// Fast path: translation-only correction.
									for (int i = 0; i < nCopyCount; i++)
									{
										pBoneToWorldOut[i][0][3] += vDelta.x;
										pBoneToWorldOut[i][1][3] += vDelta.y;
										pBoneToWorldOut[i][2][3] += vDelta.z;
									}
								}

								// Phase 2: when two consecutive lag records exist within
								// the validity window, lerp the cached bone translation
								// component toward the previous record's pose to soften
								// per-tick snaps. Rotational components keep the freshest
								// (rec0) data which already reflects the live yaw above.
								if (nRecords >= 2)
								{
									if (const auto pPrev = F::LagRecords->GetRecord(pPlayer, 1))
									{
										const float dt =
											pRecord->SimulationTime - pPrev->SimulationTime;

										if (dt > 1e-4f)
										{
											const float t = std::clamp(
												(currentTime - pPrev->SimulationTime) / dt,
												0.0f, 1.0f);

											// Blend translation from rec1 toward rec0 by t.
											// We already wrote rec0 + delta into pBoneToWorldOut,
											// so we only need to adjust toward rec1 by (1 - t).
											const float oneMinusT = 1.0f - t;

											if (oneMinusT > 0.001f)
											{
												for (int i = 0; i < nCopyCount; ++i)
												{
													const float dx = pPrev->BoneMatrix[i][0][3] -
														pRecord->BoneMatrix[i][0][3];
													const float dy = pPrev->BoneMatrix[i][1][3] -
														pRecord->BoneMatrix[i][1][3];
													const float dz = pPrev->BoneMatrix[i][2][3] -
														pRecord->BoneMatrix[i][2][3];

													pBoneToWorldOut[i][0][3] += dx * oneMinusT;
													pBoneToWorldOut[i][1][3] += dy * oneMinusT;
													pBoneToWorldOut[i][2][3] += dz * oneMinusT;
												}
											}
										}
									}
								}
							}
						}
					}
				}

				return true;
			}
		}
	}

	return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);
}
