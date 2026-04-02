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

	auto its{ 0 };

	while (pParent)
	{
		if (its > 32) //XD
		{
			break;
		}

		its++;

		pEntity = pParent;
		pParent = pEntity->GetMoveParent();
	}

	return pEntity;
}

MAKE_HOOK(CBaseAnimating_SetupBones, Signatures::CBaseAnimating_SetupBones.Get(), bool, __fastcall,
	C_BaseAnimating* ecx, matrix3x4_t* pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime)
{
	if (CFG::Misc_SetupBones_Optimization && !F::LagRecords->IsSettingUpBones() && !CFG::Misc_Accuracy_Improvements)
	{
		const auto baseEnt = reinterpret_cast<C_BaseEntity*>(reinterpret_cast<uintptr_t>(ecx) - sizeof(uintptr_t));

		if (baseEnt)
		{
			const auto owner = GetRootMoveParent(baseEnt);
			const auto ent = owner ? owner : baseEnt;

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
						const auto pPlayer = ent->As<C_TFPlayer>();
						int nRecords = 0;

						if (pPlayer && F::LagRecords->HasRecords(pPlayer, &nRecords) && nRecords > 0)
						{
							if (const auto pRecord = F::LagRecords->GetRecord(pPlayer, 0, true))
							{
								const Vec3 vDelta = ent->GetAbsOrigin() - pRecord->AbsOrigin;

								if (vDelta.LengthSqr() > 0.01f)
								{
									for (int i = 0; i < nCopyCount; i++)
									{
										pBoneToWorldOut[i][0][3] += vDelta.x;
										pBoneToWorldOut[i][1][3] += vDelta.y;
										pBoneToWorldOut[i][2][3] += vDelta.z;
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
