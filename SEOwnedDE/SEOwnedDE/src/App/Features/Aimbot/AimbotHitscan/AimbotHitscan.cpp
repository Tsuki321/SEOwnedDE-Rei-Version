#include "AimbotHitscan.h"

#include "../../CFG.h"

namespace
{
	mstudiohitboxset_t* GetHitboxSet(C_BaseAnimating* pAnimating)
	{
		if (!pAnimating)
			return nullptr;

		const auto pModel = pAnimating->GetModel();
		if (!pModel)
			return nullptr;

		const auto pHDR = I::ModelInfoClient->GetStudiomodel(pModel);
		return pHDR ? pHDR->pHitboxSet(pAnimating->m_nHitboxSet()) : nullptr;
	}

	bool SetupHitboxScan(C_BaseAnimating* pAnimating, mstudiohitboxset_t*& pSet, matrix3x4_t (&boneMatrix)[128])
	{
		pSet = GetHitboxSet(pAnimating);
		return pSet && pAnimating->SetupBones(boneMatrix, 128, BONE_USED_BY_HITBOX, I::GlobalVars->curtime);
	}

	bool HistoricalPoseMayIntersectRay(mstudiohitboxset_t* pSet, const LagRecord_t* pRecord,
		const Vec3& vTraceStart, const Vec3& vForward, float flTraceLength)
	{
		if (!pSet || !pRecord || pRecord->BoneCount <= 0)
			return false;

		for (int n = 0; n < pSet->numhitboxes; ++n)
		{
			const auto pBox = pSet->pHitbox(n);
			if (!pBox || pBox->bone < 0 || pBox->bone >= pRecord->BoneCount)
				continue;

			Vec3 vCenter = {};
			Math::VectorTransform((pBox->bbmin + pBox->bbmax) * 0.5f,
				pRecord->BoneData[pBox->bone], vCenter);

			const Vec3 vToCenter = vCenter - vTraceStart;
			float flAlongRay = vToCenter.Dot(vForward);
			if (flAlongRay < 0.0f)
				flAlongRay = 0.0f;
			else if (flAlongRay > flTraceLength)
				flAlongRay = flTraceLength;

			const Vec3 vClosestPoint = vTraceStart + (vForward * flAlongRay);
			const Vec3 vHalfExtents = (pBox->bbmax - pBox->bbmin) * 0.5f;
			const float flRadius = vHalfExtents.Length() + 8.0f;

			// The padded half-diagonal sphere encloses the rotated hitbox, so this
			// cheaply rejects impossible records before the authoritative trace.
			if (vClosestPoint.DistToSqr(vCenter) <= flRadius * flRadius)
				return true;
		}

		return false;
	}
}

int CAimbotHitscan::GetAimHitbox(C_TFWeaponBase* pWeapon)
{
	switch (CFG::Aimbot_Hitscan_Hitbox)
	{
		case 0: return HITBOX_HEAD;
		case 1: return HITBOX_PELVIS;
		case 2:
		{
			if (pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_CLASSIC)
				return (pWeapon->As<C_TFSniperRifle>()->m_flChargedDamage() >= 150.0f) ? HITBOX_HEAD : HITBOX_PELVIS;

			return H::AimUtils->IsWeaponCapableOfHeadshot(pWeapon) ? HITBOX_HEAD : HITBOX_PELVIS;
		}
		default: return HITBOX_PELVIS;
	}
}

bool CAimbotHitscan::ScanHead(C_TFPlayer* pLocal, HitscanTarget_t& target)
{
	if (!CFG::Aimbot_Hitscan_Scan_Head)
		return false;

	const auto pPlayer = target.Entity->As<C_TFPlayer>();
	if (!pPlayer)
		return false;

	mstudiohitboxset_t* pSet = nullptr;
	matrix3x4_t boneMatrix[128];
	if (!SetupHitboxScan(pPlayer, pSet, boneMatrix))
		return false;

	const auto pBox = pSet->pHitbox(HITBOX_HEAD);
	if (!pBox || pBox->bone < 0 || pBox->bone >= 128)
		return false;

	const Vec3 vMins = pBox->bbmin;
	const Vec3 vMaxs = pBox->bbmax;

	const std::array vPoints = {
		// Center
		Vec3((vMins.x + vMaxs.x) * 0.5f, (vMins.y + vMaxs.y) * 0.5f, (vMins.z + vMaxs.z) * 0.5f),
		// Front, back
		Vec3((vMins.x + vMaxs.x) * 0.5f, vMins.y * 0.7f, (vMins.z + vMaxs.z) * 0.5f),
		Vec3((vMins.x + vMaxs.x) * 0.5f, vMaxs.y * 0.7f, (vMins.z + vMaxs.z) * 0.5f),
		// Top, bottom
		Vec3((vMins.x + vMaxs.x) * 0.5f, (vMins.y + vMaxs.y) * 0.5f, vMaxs.z * 0.7f),
		Vec3((vMins.x + vMaxs.x) * 0.5f, (vMins.y + vMaxs.y) * 0.5f, vMins.z * 0.7f),
		// Left, right
		Vec3(vMins.x * 0.7f, (vMins.y + vMaxs.y) * 0.5f, (vMins.z + vMaxs.z) * 0.5f),
		Vec3(vMaxs.x * 0.7f, (vMins.y + vMaxs.y) * 0.5f, (vMins.z + vMaxs.z) * 0.5f),
	};

	const Vec3 vLocalPos = pLocal->GetShootPos();
	for (const auto& vPoint : vPoints)
	{
		Vec3 vTransformed = {};
		Math::VectorTransform(vPoint, boneMatrix[pBox->bone], vTransformed);

		int nHitHitbox = -1;

		if (!H::AimUtils->TraceEntityBullet(pPlayer, vLocalPos, vTransformed, &nHitHitbox))
			continue;

		if (nHitHitbox != HITBOX_HEAD)
			continue;

		target.Position = vTransformed;
		target.AngleTo = Math::CalcAngle(vLocalPos, vTransformed);
		target.WasMultiPointed = true;

		return true;
	}

	return false;
}

bool CAimbotHitscan::ScanBody(C_TFPlayer* pLocal, HitscanTarget_t& target)
{
	const bool bScanningBody = CFG::Aimbot_Hitscan_Scan_Body;
	const bool bScanningArms = CFG::Aimbot_Hitscan_Scan_Arms;
	const bool bScanningLegs = CFG::Aimbot_Hitscan_Scan_Legs;

	if (!bScanningBody && !bScanningArms && !bScanningLegs)
		return false;

	const auto pPlayer = target.Entity->As<C_TFPlayer>();
	if (!pPlayer)
		return false;

	mstudiohitboxset_t* pSet = nullptr;
	matrix3x4_t boneMatrix[128];
	if (!SetupHitboxScan(pPlayer, pSet, boneMatrix))
		return false;

	const Vec3 vLocalPos = pLocal->GetShootPos();
	for (int n = 1; n < pSet->numhitboxes; n++)
	{
		if (n == target.AimedHitbox)
			continue;

		const auto pBox = pSet->pHitbox(n);
		if (!pBox || pBox->bone < 0 || pBox->bone >= 128)
			continue;

		const int nHitboxGroup = pBox->group;

		if (!bScanningBody && (nHitboxGroup == HITGROUP_CHEST || nHitboxGroup == HITGROUP_STOMACH))
			continue;

		if (!bScanningArms && (nHitboxGroup == HITGROUP_LEFTARM || nHitboxGroup == HITGROUP_RIGHTARM))
			continue;

		if (!bScanningLegs && (nHitboxGroup == HITGROUP_LEFTLEG || nHitboxGroup == HITGROUP_RIGHTLEG))
			continue;

		Vec3 vHitbox = {};
		Math::VectorTransform((pBox->bbmin + pBox->bbmax) * 0.5f, boneMatrix[pBox->bone], vHitbox);

		if (!H::AimUtils->TraceEntityBullet(pPlayer, vLocalPos, vHitbox))
			continue;

		target.Position = vHitbox;
		target.AngleTo = Math::CalcAngle(vLocalPos, vHitbox);

		return true;
	}

	return false;
}

bool CAimbotHitscan::ScanBuilding(C_TFPlayer* pLocal, HitscanTarget_t& target)
{
	if (!CFG::Aimbot_Hitscan_Scan_Buildings)
		return false;

	const auto pObject = target.Entity->As<C_BaseObject>();
	if (!pObject)
		return false;

	const Vec3 vLocalPos = pLocal->GetShootPos();

	if (pObject->GetClassId() == ETFClassIds::CObjectSentrygun)
	{
		mstudiohitboxset_t* pSet = nullptr;
		matrix3x4_t boneMatrix[128];
		if (!SetupHitboxScan(pObject, pSet, boneMatrix))
			return false;

		for (int n = 0; n < pSet->numhitboxes; n++)
		{
			const auto pBox = pSet->pHitbox(n);
			if (!pBox || pBox->bone < 0 || pBox->bone >= 128)
				continue;

			Vec3 vHitbox = {};
			Math::VectorTransform((pBox->bbmin + pBox->bbmax) * 0.5f, boneMatrix[pBox->bone], vHitbox);

			if (!H::AimUtils->TraceEntityBullet(pObject, vLocalPos, vHitbox))
				continue;

			target.Position = vHitbox;
			target.AngleTo = Math::CalcAngle(vLocalPos, vHitbox);

			return true;
		}
	}

	else
	{
		const Vec3 vMins = pObject->m_vecMins();
		const Vec3 vMaxs = pObject->m_vecMaxs();

		const std::array vPoints = {
			Vec3(vMins.x * 0.9f, ((vMins.y + vMaxs.y) * 0.5f), ((vMins.z + vMaxs.z) * 0.5f)),
			Vec3(vMaxs.x * 0.9f, ((vMins.y + vMaxs.y) * 0.5f), ((vMins.z + vMaxs.z) * 0.5f)),
			Vec3(((vMins.x + vMaxs.x) * 0.5f), vMins.y * 0.9f, ((vMins.z + vMaxs.z) * 0.5f)),
			Vec3(((vMins.x + vMaxs.x) * 0.5f), vMaxs.y * 0.9f, ((vMins.z + vMaxs.z) * 0.5f)),
			Vec3(((vMins.x + vMaxs.x) * 0.5f), ((vMins.y + vMaxs.y) * 0.5f), vMins.z * 0.9f),
			Vec3(((vMins.x + vMaxs.x) * 0.5f), ((vMins.y + vMaxs.y) * 0.5f), vMaxs.z * 0.9f)
		};

		const matrix3x4_t& transform = pObject->RenderableToWorldTransform();
		for (const auto& vPoint : vPoints)
		{
			Vec3 vTransformed = {};
			Math::VectorTransform(vPoint, transform, vTransformed);

			if (!H::AimUtils->TraceEntityBullet(pObject, vLocalPos, vTransformed))
				continue;

			target.Position = vTransformed;
			target.AngleTo = Math::CalcAngle(vLocalPos, vTransformed);

			return true;
		}
	}

	return false;
}

bool CAimbotHitscan::ResolveManualShot(CUserCmd* pCmd, C_TFPlayer* pLocal)
{
	if (!pCmd || !pLocal || !CFG::Aimbot_Target_Players || !CFG::Aimbot_Hitscan_Target_LagRecords)
		return false;

	const Vec3 vTraceStart = pLocal->GetShootPos();
	const Vec3 vShotAngles = pCmd->viewangles + pLocal->m_vecPunchAngle();
	Vec3 vForward = {};
	Math::AngleVectors(vShotAngles, &vForward);
	constexpr float flTraceLength = 8192.0f;
	const Vec3 vTraceEnd = vTraceStart + (vForward * flTraceLength);

	const LagRecord_t* pBestRecord = nullptr;
	C_TFPlayer* pBestPlayer = nullptr;

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
	{
		if (!pEntity)
			continue;

		const auto pPlayer = pEntity->As<C_TFPlayer>();
		if (!pPlayer || pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
			continue;

		if (CFG::Aimbot_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
			continue;

		if (CFG::Aimbot_Ignore_Invisible && pPlayer->IsInvisible())
			continue;

		if (CFG::Aimbot_Ignore_Invulnerable && pPlayer->IsInvulnerable())
			continue;

		if (CFG::Aimbot_Ignore_Taunting && pPlayer->InCond(TF_COND_TAUNTING))
			continue;

		const auto pHitboxSet = GetHitboxSet(pPlayer);
		if (!pHitboxSet)
			continue;

		int nRecords = 0;
		if (!F::LagRecords->HasRecords(pPlayer, &nRecords))
			continue;

		const auto& cachedState = F::LagRecords->GetCachedState(pPlayer->entindex());
		for (int n = 0; n < nRecords; ++n)
		{
			const auto pRecord = F::LagRecords->GetRecord(pPlayer, n);
			if (!pRecord)
				continue;

			// Records are newest-first. Once this player's record cannot beat the
			// best global hit, none of its older records can beat it either.
			if (pBestRecord && pRecord->SimulationTime <= pBestRecord->SimulationTime)
				break;

			if (!CLagRecords::IsRecordUsable(pRecord, cachedState))
				continue;

			if (!HistoricalPoseMayIntersectRay(pHitboxSet, pRecord, vTraceStart, vForward, flTraceLength))
				continue;

			CLagRecordScope scope(pRecord);
			if (!scope.IsActive())
				continue;

			if (!H::AimUtils->TraceEntityBullet(pPlayer, vTraceStart, vTraceEnd))
				continue;

			pBestRecord = pRecord;
			pBestPlayer = pPlayer;
			break;
		}
	}

	if (!pBestRecord || !pBestPlayer)
		return false;

	pCmd->tick_count = CLagRecords::GetCommandTick(pBestRecord->SimulationTime);
	G::nTargetIndexEarly = pBestPlayer->entindex();
	G::nTargetIndex = pBestPlayer->entindex();
	return true;
}

bool CAimbotHitscan::GetTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, HitscanTarget_t& outTarget)
{
	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	// Hoist invariant reads out of the per-target loops. CFG::Aimbot_Hitscan_FOV
	// was being read per target in both the FOV ternary and the continue check.
	const float flFOVLimit = CFG::Aimbot_Hitscan_FOV;

	m_vecTargets.clear();

	// Find player targets
	if (CFG::Aimbot_Target_Players)
	{
		const int nAimHitbox = GetAimHitbox(pWeapon);

		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
		{
			if (!pEntity)
				continue;

			const auto pPlayer = pEntity->As<C_TFPlayer>();
			if (pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
				continue;

			if (CFG::Aimbot_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
				continue;

			if (CFG::Aimbot_Ignore_Invisible && pPlayer->IsInvisible())
				continue;

			if (CFG::Aimbot_Ignore_Invulnerable && pPlayer->IsInvulnerable())
				continue;

			if (CFG::Aimbot_Ignore_Taunting && pPlayer->InCond(TF_COND_TAUNTING))
				continue;

			if (CFG::Aimbot_Hitscan_Target_LagRecords)
			{
				int nRecords = 0;

				if (!F::LagRecords->HasRecords(pPlayer, &nRecords))
					continue;

				const auto& cachedState = F::LagRecords->GetCachedState(pPlayer->entindex());

				for (int n = 0; n < nRecords; n++)
				{
					const auto pRecord = F::LagRecords->GetRecord(pPlayer, n);

					if (!CLagRecords::IsRecordUsable(pRecord, cachedState))
						continue;

					Vec3 vPos = SDKUtils::GetHitboxPosFromMatrix(pPlayer, nAimHitbox, pRecord->BoneData.data());
					Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
					const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
					const float flDistTo = vLocalPos.DistTo(vPos);

					if (flFOVTo > flFOVLimit)
						continue;

					m_vecTargets.emplace_back(AimTarget_t {
						pPlayer, vPos, vAngleTo, flFOVTo, flDistTo
					}, nAimHitbox, pRecord->SimulationTime, pRecord);
				}
			}

			/*if (CFG::Aimbot_Hitscan_Aim_Type != 2)
			{
				if (TIME_TO_TICKS(pPlayer->m_flSimulationTime() - pPlayer->m_flOldSimulationTime()) < 1)
					continue;
			}*/

			Vec3 vPos = pPlayer->GetHitboxPos(nAimHitbox);
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			const float flDistTo = vLocalPos.DistTo(vPos);

			if (flFOVTo > flFOVLimit)
				continue;

			m_vecTargets.emplace_back(AimTarget_t { pPlayer, vPos, vAngleTo, flFOVTo, flDistTo}, nAimHitbox, pPlayer->m_flSimulationTime());
		}
	}

	// Find Building targets
	if (CFG::Aimbot_Target_Buildings)
	{
		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::BUILDINGS_ENEMIES))
		{
			if (!pEntity)
				continue;

			const auto pBuilding = pEntity->As<C_BaseObject>();
			if (pBuilding->m_bPlacing())
				continue;

			Vec3 vPos = pBuilding->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			const float flDistTo = vLocalPos.DistTo(vPos);

			if (flFOVTo > flFOVLimit)
				continue;

			m_vecTargets.emplace_back(AimTarget_t { pBuilding, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	// Find stickybomb targets
	if (CFG::Aimbot_Hitscan_Target_Stickies)
	{
		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PROJECTILES_ENEMIES))
		{
			if (!pEntity || pEntity->GetClassId() != ETFClassIds::CTFGrenadePipebombProjectile)
			{
				continue;
			}

			const auto pipe = pEntity->As<C_TFGrenadePipebombProjectile>();
			if (!pipe || !pipe->m_bTouched() || !pipe->HasStickyEffects() || pipe->m_iType() == TF_GL_MODE_REMOTE_DETONATE_PRACTICE)
			{
				continue;
			}

			Vec3 vPos = pipe->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			const float flDistTo = vLocalPos.DistTo(vPos);

			if (flFOVTo > flFOVLimit)
				continue;

			m_vecTargets.emplace_back(AimTarget_t {pipe, vPos, vAngleTo, flFOVTo, flDistTo});
		}
	}

	if (m_vecTargets.empty())
		return false;

	// Sort by target priority
	F::AimbotCommon->Sort(m_vecTargets, CFG::Aimbot_Hitscan_Sort);

	// Find and return the first valid target
	for (auto& target : m_vecTargets)
	{
		switch (target.Entity->GetClassId())
		{
			case ETFClassIds::CTFPlayer:
			{
				if (!target.LagRecord)
				{
					int nHitHitbox = -1;

					if (!H::AimUtils->TraceEntityBullet(target.Entity, vLocalPos, target.Position, &nHitHitbox))
					{
						if (target.AimedHitbox == HITBOX_HEAD)
						{
							if (!ScanHead(pLocal, target))
								continue;
						}

						else if (target.AimedHitbox == HITBOX_PELVIS)
						{
							if (!ScanBody(pLocal, target))
								continue;
						}

						else
						{
							continue;
						}
					}

					else
					{
						if (nHitHitbox != target.AimedHitbox && target.AimedHitbox == HITBOX_HEAD)
						{
							if (!ScanHead(pLocal, target))
								continue;
						}
					}
				}

				else
				{
					CLagRecordScope scope(target.LagRecord);
					if (!scope.IsActive())
						continue;

					int nHitHitbox = -1;
					const bool bTraceResult = H::AimUtils->TraceEntityBullet(target.Entity, vLocalPos, target.Position, &nHitHitbox);

					if (!bTraceResult)
					{
						if (target.AimedHitbox == HITBOX_HEAD)
						{
							if (!ScanHead(pLocal, target))
								continue;
						}

						else if (target.AimedHitbox == HITBOX_PELVIS)
						{
							if (!ScanBody(pLocal, target))
								continue;
						}

						else
						{
							continue;
						}
					}
				}

				break;
			}

			case ETFClassIds::CObjectSentrygun:
			case ETFClassIds::CObjectDispenser:
			case ETFClassIds::CObjectTeleporter:
			{
				if (!H::AimUtils->TraceEntityBullet(target.Entity, vLocalPos, target.Position))
				{
					if (!ScanBuilding(pLocal, target))
						continue;
				}

				break;
			}

			case ETFClassIds::CTFGrenadePipebombProjectile:
			{
				if (!H::AimUtils->TraceEntityBullet(target.Entity, vLocalPos, target.Position))
				{
					continue;
				}

				break;
			}

			default: continue;
		}

		outTarget = target;
		return true;
	}

	return false;
}

bool CAimbotHitscan::ShouldAim(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	if (CFG::Aimbot_Hitscan_Aim_Type == 1 && (!IsFiring(pCmd, pWeapon) || !pWeapon->HasPrimaryAmmoForShot()))
		return false;

	if (CFG::Aimbot_Hitscan_Aim_Type == 2 || CFG::Aimbot_Hitscan_Aim_Type == 3)
	{
		const int nWeaponID = pWeapon->GetWeaponID();
		if (nWeaponID == TF_WEAPON_SNIPERRIFLE || nWeaponID == TF_WEAPON_SNIPERRIFLE_CLASSIC || nWeaponID == TF_WEAPON_SNIPERRIFLE_DECAP)
		{
			if (!G::bCanPrimaryAttack)
				return false;
		}
	}

	if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN && pWeapon->As<C_TFMinigun>()->m_iWeaponState() == AC_STATE_DRYFIRE)
		return false;

	return true;
}

void CAimbotHitscan::Aim(CUserCmd* pCmd, C_TFPlayer* pLocal, const Vec3& vAngles)
{
	Vec3 vAngleTo = vAngles - pLocal->m_vecPunchAngle();
	Math::ClampAngles(vAngleTo);

	switch (CFG::Aimbot_Hitscan_Aim_Type)
	{
		// Plain
		case 0:
		{
			pCmd->viewangles = vAngleTo;
			break;
		}
		
		// Silent
		case 1:
		{
			if (G::bCanPrimaryAttack)
			{
				H::AimUtils->FixMovement(pCmd, vAngleTo);
				pCmd->viewangles = vAngleTo;
				G::bSilentAngles = true;
			}

			break;
		}

		// Smooth
		case 2:
		{
			Vec3 vDelta = vAngleTo - pCmd->viewangles;
			Math::ClampAngles(vDelta);

			// Apply smoothing
			if (vDelta.Length() > 0.0f && CFG::Aimbot_Hitscan_Smoothing > 0.f)
				pCmd->viewangles += vDelta / CFG::Aimbot_Hitscan_Smoothing;

			break;
		}

		// Aim Assist
		case 3:
		{
			Vec3 vDelta = vAngleTo - pCmd->viewangles;
			Math::ClampAngles(vDelta);

			if (vDelta.Length() > 0.0f && CFG::Aimbot_Hitscan_AimAssist_Strength > 0.0f)
			{
				if (!CFG::Aimbot_Hitscan_AimAssist_Stabilization)
				{
					pCmd->viewangles += vDelta / CFG::Aimbot_Hitscan_AimAssist_Strength;
					Math::ClampAngles(pCmd->viewangles);
					break;
				}

				Vec3 vAssistStep = vDelta / CFG::Aimbot_Hitscan_AimAssist_Strength;

				// Keep legacy feel: tiny deadzone and per-tick step clamp only.
				if (vAssistStep.LengthSqr() < (0.02f * 0.02f))
					break;

				constexpr float flMaxAssistStep = 4.0f;
				if (vAssistStep.x > flMaxAssistStep) vAssistStep.x = flMaxAssistStep;
				if (vAssistStep.x < -flMaxAssistStep) vAssistStep.x = -flMaxAssistStep;
				if (vAssistStep.y > flMaxAssistStep) vAssistStep.y = flMaxAssistStep;
				if (vAssistStep.y < -flMaxAssistStep) vAssistStep.y = -flMaxAssistStep;

				pCmd->viewangles += vAssistStep;
				Math::ClampAngles(pCmd->viewangles);
			}

			break;
		}

		default: break;
	}
}

bool CAimbotHitscan::ShouldFire(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const HitscanTarget_t& target)
{
	if (!CFG::Aimbot_AutoShoot)
		return false;

	const bool bIsMachina = pWeapon->m_iItemDefinitionIndex() == Sniper_m_TheMachina || pWeapon->m_iItemDefinitionIndex() == Sniper_m_ShootingStar;
	const bool bCapableOfHeadshot = H::AimUtils->IsWeaponCapableOfHeadshot(pWeapon);
	const bool bIsSydneySleeper = pWeapon->m_iItemDefinitionIndex() == Sniper_m_TheSydneySleeper;
	const bool bIsSniper = pLocal->m_iClass() == TF_CLASS_SNIPER;

	if (bIsMachina && !pLocal->IsZoomed())
		return false;

	if (CFG::Aimbot_Hitscan_Wait_For_Headshot)
	{
		if (target.Entity->GetClassId() == ETFClassIds::CTFPlayer && bCapableOfHeadshot && !G::bCanHeadshot)
			return false;
	}

	if (CFG::Aimbot_Hitscan_Wait_For_Charge)
	{
		if (target.Entity->GetClassId() == ETFClassIds::CTFPlayer && bIsSniper && (bCapableOfHeadshot || bIsSydneySleeper))
		{
			const auto pPlayer = target.Entity->As<C_TFPlayer>();
			const auto pSniperRifle = pWeapon->As<C_TFSniperRifle>();

			const int nHealth = pPlayer->m_iHealth();
			const bool bIsCritBoosted = pLocal->IsCritBoosted();

			if (target.AimedHitbox == HITBOX_HEAD && !bIsSydneySleeper)
			{
				if (nHealth > 150)
				{
					const float flDamage = Math::RemapValClamped(pSniperRifle->m_flChargedDamage(), 0.0f, 150.0f, 0.0f, 450.0f);

					if (flDamage < static_cast<float>(nHealth) && flDamage < 449.5f)
						return false;
				}

				else
				{
					if (!bIsCritBoosted && !G::bCanHeadshot)
						return false;
				}
			}

			else
			{
				if (nHealth > (bIsCritBoosted ? 150 : 50))
				{
					float flMult = pPlayer->IsMarked() ? 1.36f : 1.0f;

					if (bIsCritBoosted)
						flMult = 3.0f;

					const float flMax = 150.0f * flMult;
					const float flDamage = pSniperRifle->m_flChargedDamage() * flMult;

					if (flDamage < static_cast<float>(nHealth) && flDamage < flMax - 0.5f)
						return false;
				}
			}
		}
	}

	if (CFG::Aimbot_Hitscan_Minigun_TapFire)
	{
		if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN)
		{
			if (pLocal->GetAbsOrigin().DistTo(target.Position) >= 900.0f)
			{
				if ((pLocal->m_nTickBase() * TICK_INTERVAL) - pWeapon->m_flLastFireTime() <= 0.25f)
					return false;
			}
		}
	}

	if (CFG::Aimbot_Hitscan_Advanced_Smooth_AutoShoot && (CFG::Aimbot_Hitscan_Aim_Type == 2 || CFG::Aimbot_Hitscan_Aim_Type == 3))
	{
		Vec3 vForward = {};
		Math::AngleVectors(pCmd->viewangles, &vForward);
		const Vec3 vTraceStart = pLocal->GetShootPos();
		const Vec3 vTraceEnd = vTraceStart + (vForward * 8192.0f);

		if (target.Entity->GetClassId() == ETFClassIds::CTFPlayer)
		{
			const auto pPlayer = target.Entity->As<C_TFPlayer>();

			if (!target.LagRecord)
			{
				int nHitHitbox = -1;

				if (!H::AimUtils->TraceEntityBullet(pPlayer, vTraceStart, vTraceEnd, &nHitHitbox))
					return false;

				if (target.AimedHitbox == HITBOX_HEAD)
				{
					if (nHitHitbox != HITBOX_HEAD)
						return false;

					if (!target.WasMultiPointed)
					{
						Vec3 vMins = {}, vMaxs = {}, vCenter = {};
						matrix3x4_t matrix = {};
						pPlayer->GetHitboxInfo(nHitHitbox, &vCenter, &vMins, &vMaxs, &matrix);

						vMins *= 0.5f;
						vMaxs *= 0.5f;

						if (!Math::RayToOBB(vTraceStart, vForward, vCenter, vMins, vMaxs, matrix))
							return false;
					}
				}
			}

			else
			{
				CLagRecordScope scope(target.LagRecord);
				if (!scope.IsActive())
					return false;

				int nHitHitbox = -1;

				if (!H::AimUtils->TraceEntityBullet(pPlayer, vTraceStart, vTraceEnd, &nHitHitbox))
					return false;

				if (target.AimedHitbox == HITBOX_HEAD)
				{
					if (nHitHitbox != HITBOX_HEAD)
						return false;

					Vec3 vMins = {}, vMaxs = {}, vCenter = {};
					SDKUtils::GetHitboxInfoFromMatrix(pPlayer, nHitHitbox, target.LagRecord->BoneData.data(), &vCenter, &vMins, &vMaxs);

					vMins *= 0.5f;
					vMaxs *= 0.5f;

					if (!Math::RayToOBB(vTraceStart, vForward, vCenter, vMins, vMaxs, *target.LagRecord->BoneData.data()))
						return false;
				}
			}
		}

		else
		{
			if (!H::AimUtils->TraceEntityBullet(target.Entity, vTraceStart, vTraceEnd, nullptr))
			{
				return false;
			}
		}
	}

	return true;
}

// Handles and updated the IN_ATTACK state
void CAimbotHitscan::HandleFire(CUserCmd* pCmd, C_TFWeaponBase* pWeapon)
{
	if (!pWeapon->HasPrimaryAmmoForShot())
		return;

	if (pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_CLASSIC)
	{
		if (G::nOldButtons & IN_ATTACK)
		{
			pCmd->buttons &= ~IN_ATTACK;
		}
		else
		{
			pCmd->buttons |= IN_ATTACK;
		}
	}

	else
	{
		pCmd->buttons |= IN_ATTACK;
	}
}

bool CAimbotHitscan::IsFiring(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon)
{
	if (!pWeapon->HasPrimaryAmmoForShot())
		return false;

	if (pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_CLASSIC)
		return !(pCmd->buttons & IN_ATTACK) && (G::nOldButtons & IN_ATTACK);

	return (pCmd->buttons & IN_ATTACK) && G::bCanPrimaryAttack;
}

void CAimbotHitscan::Run(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	const bool bManualFiring = IsFiring(pCmd, pWeapon);
	G::bManualHitscanFiring = bManualFiring;

	if (!CFG::Aimbot_Hitscan_Active)
		return;

	if (CFG::Aimbot_Hitscan_Sort == 0)
		G::flAimbotFOV = CFG::Aimbot_Hitscan_FOV;

	if (Shifting::bShifting && !Shifting::bShiftingWarp)
		return;

	// Delay check - prevents snap aiming
	if (CFG::Aimbot_Hitscan_Delay_Fire && I::GlobalVars->curtime < m_flDelayFireEndTime)
	{
		if (bManualFiring)
			ResolveManualShot(pCmd, pLocal);

		return;
	}

	const bool aimKeyDown = H::Input->IsDown(CFG::Aimbot_Key);
	const bool manualFireIntent = pCmd->buttons & IN_ATTACK;
	const bool rapidFirePretracking = CFG::Exploits_RapidFire_Key && H::Input->IsDown(CFG::Exploits_RapidFire_Key);
	const bool needsTargetScan = aimKeyDown || manualFireIntent || bManualFiring || rapidFirePretracking;
	if (!needsTargetScan)
		return;

	HitscanTarget_t target = {};
	if (GetTarget(pLocal, pWeapon, target) && target.Entity)
	{
		G::nTargetIndexEarly = target.Entity->entindex();

		if (aimKeyDown || bManualFiring)
		{
			G::nTargetIndex = target.Entity->entindex();

			// Auto Scope
			if (CFG::Aimbot_Hitscan_Auto_Scope
				&& !pLocal->IsZoomed() && pLocal->m_iClass() == TF_CLASS_SNIPER && pWeapon->GetSlot() == WEAPON_SLOT_PRIMARY && G::bCanPrimaryAttack)
			{
				pCmd->buttons |= IN_ATTACK2;
				if (bManualFiring)
					ResolveManualShot(pCmd, pLocal);

				return;
			}

			// Auto Shoot
			if (CFG::Aimbot_AutoShoot && pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_CLASSIC)
				pCmd->buttons |= IN_ATTACK;

			// Spin up minigun
			if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN)
			{
				const int nState = pWeapon->As<C_TFMinigun>()->m_iWeaponState();
				if (nState == AC_STATE_IDLE || nState == AC_STATE_STARTFIRING)
					G::bCanPrimaryAttack = false; // TODO: hack

				pCmd->buttons |= IN_ATTACK2;
			}

			// Update attack state
			if (ShouldFire(pCmd, pLocal, pWeapon, target))
			{
				HandleFire(pCmd, pWeapon);
			}

			const bool bIsFiring = IsFiring(pCmd, pWeapon);
			G::bFiring = bIsFiring;

			// Reset delay timer after firing
			if (CFG::Aimbot_Hitscan_Delay_Fire && bIsFiring)
				m_flDelayFireEndTime = I::GlobalVars->curtime + CFG::Aimbot_Hitscan_Delay_Fire_Time;

			// Are we ready to aim?
			if (ShouldAim(pCmd, pLocal, pWeapon) || bIsFiring)
			{
				if (aimKeyDown)
				{
					Aim(pCmd, pLocal, target.AngleTo);
				}

				if (bIsFiring && !bManualFiring && target.Entity->GetClassId() == ETFClassIds::CTFPlayer)
				{
					pCmd->tick_count = CLagRecords::GetCommandTick(target.SimulationTime);
				}
			}
		}
	}

	if (bManualFiring)
		ResolveManualShot(pCmd, pLocal);
}
