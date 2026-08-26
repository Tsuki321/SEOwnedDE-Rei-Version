#include "AimbotHitscan.h"

#include "../../CFG.h"

#include <algorithm>

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
		if (!pSet || !pRecord || pRecord->BoneCount <= 0 || pRecord->BoneCount > MAX_BONE_COUNT)
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

bool CAimbotHitscan::ScanHead(C_TFPlayer* pLocal, HitscanTarget_t& target, const Vec3& vLocalAngles, float flFOVLimit)
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
	const bool bStablePointOrder = CFG::Aimbot_Hitscan_Aim_Type == 2;

	// A substituted point must clear the same cone the collection loop enforced,
	// otherwise we aim somewhere the on-screen FOV circle never covered. Direct
	// modes prefer the closest point; Smooth keeps the fixed center-out order.
	bool bFoundPoint = false;
	Vec3 vBestPoint = {};
	Vec3 vBestAngle = {};
	float flBestFOV = 0.0f;

	for (const auto& vPoint : vPoints)
	{
		Vec3 vTransformed = {};
		Math::VectorTransform(vPoint, boneMatrix[pBox->bone], vTransformed);

		const Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vTransformed);
		const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

		if (flFOVTo > flFOVLimit)
			continue;

		// Cannot beat the current pick, so skip the trace entirely
		if (bFoundPoint && flFOVTo >= flBestFOV)
			continue;

		int nHitHitbox = -1;

		if (!H::AimUtils->TraceEntityBullet(pPlayer, vLocalPos, vTransformed, &nHitHitbox))
			continue;

		if (nHitHitbox != HITBOX_HEAD)
			continue;

		bFoundPoint = true;
		vBestPoint = vTransformed;
		vBestAngle = vAngleTo;
		flBestFOV = flFOVTo;

		// Smooth aim favors the first point in this fixed center-out order.
		// Re-ranking points against a moving crosshair makes the selected side of
		// a hitbox alternate from tick to tick, which is visible as aim jitter.
		if (bStablePointOrder)
			break;
	}

	if (!bFoundPoint)
		return false;

	target.Position = vBestPoint;
	target.AngleTo = vBestAngle;
	target.WasMultiPointed = true;

	return true;
}

bool CAimbotHitscan::ScanBody(C_TFPlayer* pLocal, HitscanTarget_t& target, const Vec3& vLocalAngles, float flFOVLimit)
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
	const bool bStablePointOrder = CFG::Aimbot_Hitscan_Aim_Type == 2;

	// Same cone requirement as ScanHead. Direct modes pick the closest in-cone
	// hitbox; Smooth retains fixed hitbox order to avoid point hopping.
	bool bFoundPoint = false;
	Vec3 vBestPoint = {};
	Vec3 vBestAngle = {};
	float flBestFOV = 0.0f;

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

		const Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vHitbox);
		const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

		if (flFOVTo > flFOVLimit)
			continue;

		// Cannot beat the current pick, so skip the trace entirely
		if (bFoundPoint && flFOVTo >= flBestFOV)
			continue;

		if (!H::AimUtils->TraceEntityBullet(pPlayer, vLocalPos, vHitbox))
			continue;

		bFoundPoint = true;
		vBestPoint = vHitbox;
		vBestAngle = vAngleTo;
		flBestFOV = flFOVTo;

		if (bStablePointOrder)
			break;
	}

	if (!bFoundPoint)
		return false;

	target.Position = vBestPoint;
	target.AngleTo = vBestAngle;

	return true;
}

bool CAimbotHitscan::ScanBuilding(C_TFPlayer* pLocal, HitscanTarget_t& target, const Vec3& vLocalAngles, float flFOVLimit)
{
	if (!CFG::Aimbot_Hitscan_Scan_Buildings)
		return false;

	const auto pObject = target.Entity->As<C_BaseObject>();
	if (!pObject)
		return false;

	const Vec3 vLocalPos = pLocal->GetShootPos();
	const bool bStablePointOrder = CFG::Aimbot_Hitscan_Aim_Type == 2;

	// Substituted points are held to the same cone here as on players
	bool bFoundPoint = false;
	Vec3 vBestPoint = {};
	Vec3 vBestAngle = {};
	float flBestFOV = 0.0f;

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

			const Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vHitbox);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > flFOVLimit)
				continue;

			// Cannot beat the current pick, so skip the trace entirely
			if (bFoundPoint && flFOVTo >= flBestFOV)
				continue;

			if (!H::AimUtils->TraceEntityBullet(pObject, vLocalPos, vHitbox))
				continue;

			bFoundPoint = true;
			vBestPoint = vHitbox;
			vBestAngle = vAngleTo;
			flBestFOV = flFOVTo;

			if (bStablePointOrder)
				break;
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

			const Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vTransformed);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (flFOVTo > flFOVLimit)
				continue;

			// Cannot beat the current pick, so skip the trace entirely
			if (bFoundPoint && flFOVTo >= flBestFOV)
				continue;

			if (!H::AimUtils->TraceEntityBullet(pObject, vLocalPos, vTransformed))
				continue;

			bFoundPoint = true;
			vBestPoint = vTransformed;
			vBestAngle = vAngleTo;
			flBestFOV = flFOVTo;

			if (bStablePointOrder)
				break;
		}
	}

	if (!bFoundPoint)
		return false;

	target.Position = vBestPoint;
	target.AngleTo = vBestAngle;

	return true;
}

bool CAimbotHitscan::ResolveManualShot(CUserCmd* pCmd, C_TFPlayer* pLocal)
{
	if (!pCmd || !pLocal || !CFG::Aimbot_Hitscan_Manual_Backtrack
		|| !CFG::Aimbot_Target_Players || !CFG::Aimbot_Hitscan_Target_LagRecords)
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

		int nRecords = 0;
		if (!F::LagRecords->HasRecords(pPlayer, &nRecords))
			continue;

		const auto& cachedState = F::LagRecords->GetCachedState(pPlayer->entindex());
		for (int n = 0; n < nRecords; ++n)
		{
			const auto pRecord = F::LagRecords->GetRecord(pPlayer, n);
			if (!pRecord)
				continue;

			if (!CLagRecords::IsRecordUsable(pRecord, cachedState))
				continue;

			CLagRecordScope scope(pRecord);
			if (!scope.IsActive())
				continue;

			// Get hitbox set after activating historical pose to ensure model index matches
			const auto pHitboxSet = GetHitboxSet(pPlayer);
			if (!pHitboxSet)
				continue;

			if (!HistoricalPoseMayIntersectRay(pHitboxSet, pRecord, vTraceStart, vForward, flTraceLength))
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
	G::bCommandTickResolved = true;
	G::nTargetIndexEarly = pBestPlayer->entindex();
	G::nTargetIndex = pBestPlayer->entindex();
	return true;
}

bool CAimbotHitscan::ValidateTarget(C_TFPlayer* pLocal, HitscanTarget_t& target,
	const Vec3& vLocalPos, const Vec3& vLocalAngles, float flFOVLimit)
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
						if (!ScanHead(pLocal, target, vLocalAngles, flFOVLimit))
							return false;
					}

					else if (target.AimedHitbox == HITBOX_PELVIS)
					{
						if (!ScanBody(pLocal, target, vLocalAngles, flFOVLimit))
							return false;
					}

					else
					{
						return false;
					}
				}

				else if (nHitHitbox != target.AimedHitbox && target.AimedHitbox == HITBOX_HEAD)
				{
					if (!ScanHead(pLocal, target, vLocalAngles, flFOVLimit))
						return false;
				}
			}

			else
			{
				CLagRecordScope scope(target.LagRecord);
				if (!scope.IsActive())
					return false;

				int nHitHitbox = -1;
				const bool bTraceResult = H::AimUtils->TraceEntityBullet(target.Entity, vLocalPos, target.Position, &nHitHitbox);

				if (!bTraceResult)
				{
					if (target.AimedHitbox == HITBOX_HEAD)
					{
						if (!ScanHead(pLocal, target, vLocalAngles, flFOVLimit))
							return false;
					}

					else if (target.AimedHitbox == HITBOX_PELVIS)
					{
						if (!ScanBody(pLocal, target, vLocalAngles, flFOVLimit))
							return false;
					}

					else
					{
						return false;
					}
				}
			}

			return true;
		}

		case ETFClassIds::CObjectSentrygun:
		case ETFClassIds::CObjectDispenser:
		case ETFClassIds::CObjectTeleporter:
		{
			if (H::AimUtils->TraceEntityBullet(target.Entity, vLocalPos, target.Position))
				return true;

			return ScanBuilding(pLocal, target, vLocalAngles, flFOVLimit);
		}

		case ETFClassIds::CTFGrenadePipebombProjectile:
			return H::AimUtils->TraceEntityBullet(target.Entity, vLocalPos, target.Position);

		default: return false;
	}
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

	if (CFG::Aimbot_Hitscan_Aim_Type == 2)
	{
		// Scan priority in allocation-free passes: keep the locked entity first,
		// and within each group prefer its continuously animated live pose over
		// discrete historical samples. The original FOV/distance order is retained
		// inside every pass.
		const bool bHasSmoothLock = m_nSmoothTargetIndex > 0;
		auto FindSmoothTarget = [&](bool bLocked, bool bHistorical)
		{
			for (auto& target : m_vecTargets)
			{
				const bool bIsLocked = bHasSmoothLock && target.Entity
					&& target.Entity->entindex() == m_nSmoothTargetIndex;
				if (bIsLocked != bLocked || (target.LagRecord != nullptr) != bHistorical)
					continue;

				if (!ValidateTarget(pLocal, target, vLocalPos, vLocalAngles, flFOVLimit))
					continue;

				outTarget = target;
				return true;
			}

			return false;
		};

		if (bHasSmoothLock
			&& (FindSmoothTarget(true, false) || FindSmoothTarget(true, true)))
			return true;

		return FindSmoothTarget(false, false) || FindSmoothTarget(false, true);
	}

	// Other aim modes retain the single sorted pass.
	for (auto& target : m_vecTargets)
	{
		if (!ValidateTarget(pLocal, target, vLocalPos, vLocalAngles, flFOVLimit))
			continue;

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

			// Ignore sub-pixel corrections so tiny animation and punch changes do
			// not make the crosshair buzz after it has settled on the target.
			constexpr float flSmoothDeadzone = 0.01f;
			const float flDeltaLengthSqr = vDelta.LengthSqr();
			if (flDeltaLengthSqr <= flSmoothDeadzone * flSmoothDeadzone)
			{
				ResetSmoothMotion();
				break;
			}

			// Values below one used to overshoot the target every command. Keep the
			// slider responsive at the low end without allowing that oscillation.
			const float flSmoothing = std::max(CFG::Aimbot_Hitscan_Smoothing, 1.0f);
			const Vec3 vDesiredStep = vDelta / flSmoothing;

			// Ease the angular velocity as well as the angle. The old direct
			// vDelta/smoothing step instantly inherited every hitbox or record jump.
			constexpr float flSmoothStepResponse = 0.35f;
			m_vSmoothAimStep += (vDesiredStep - m_vSmoothAimStep) * flSmoothStepResponse;

			// Never retain momentum opposite the new target direction and never
			// travel farther than the remaining angular error.
			if (m_vSmoothAimStep.Dot(vDelta) <= 0.0f)
				m_vSmoothAimStep = vDesiredStep * flSmoothStepResponse;

			const float flStepLengthSqr = m_vSmoothAimStep.LengthSqr();
			if (flStepLengthSqr > flDeltaLengthSqr)
				m_vSmoothAimStep *= sqrtf(flDeltaLengthSqr / flStepLengthSqr);

			pCmd->viewangles += m_vSmoothAimStep;
			Math::ClampAngles(pCmd->viewangles);

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

					// Mirror the live branch: multi-point targets already resolved
					// a specific surface point, so re-testing them against the
					// shrunk box rejects shots that are legitimately on target.
					if (!target.WasMultiPointed)
					{
						Vec3 vMins = {}, vMaxs = {}, vCenter = {};
						matrix3x4_t matrix = {};

						// The head hitbox's own bone matrix, not BoneData[0]. The
						// root matrix was orienting a head-sized box by the pelvis,
						// so the test passed or failed on an unrelated rotation.
						if (!SDKUtils::GetHitboxInfoFromMatrix(pPlayer, nHitHitbox, target.LagRecord->BoneData.data(), &vCenter, &vMins, &vMaxs, &matrix))
							return false;

						vMins *= 0.5f;
						vMaxs *= 0.5f;

						if (!Math::RayToOBB(vTraceStart, vForward, vCenter, vMins, vMaxs, matrix))
							return false;
					}
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
	if (!CFG::Aimbot_Hitscan_Active)
	{
		ResetSmoothState();
		return;
	}

	const bool aimKeyDown = H::Input->IsDown(CFG::Aimbot_Key);
	const bool bSmoothAimActive = aimKeyDown && CFG::Aimbot_Hitscan_Aim_Type == 2;
	if (bSmoothAimActive)
	{
		// A skipped command means this feature stopped running (weapon switch,
		// recharge, or another CreateMove early-out). Do not carry stale target
		// lock or angular momentum back into a later hitscan command.
		if (m_nLastSmoothCommandNumber <= 0
			|| pCmd->command_number != m_nLastSmoothCommandNumber + 1)
			ResetSmoothState();

		m_nLastSmoothCommandNumber = pCmd->command_number;
	}
	else
		ResetSmoothState();

	const bool bManualFiring = IsFiring(pCmd, pWeapon);
	G::bManualHitscanFiring = bManualFiring;

	// The cone constrains both sort modes, so the indicator applies to both.
	G::flAimbotFOV = CFG::Aimbot_Hitscan_FOV;

	if (Shifting::bShifting && !Shifting::bShiftingWarp)
	{
		if (bSmoothAimActive)
			ResetSmoothMotion();

		return;
	}

	// Delay check - prevents snap aiming. The delay governs aimbot-initiated fire
	// only; the user's own manual shot is resolved by CAimbot::Run after this
	// feature returns, so the window no longer strips its historical backtracking.
	if (CFG::Aimbot_Hitscan_Delay_Fire && I::GlobalVars->curtime < m_flDelayFireEndTime)
	{
		G::bAimbotFireDelayed = true;
		if (bSmoothAimActive)
			ResetSmoothMotion();

		return;
	}

	const bool manualFireIntent = pCmd->buttons & IN_ATTACK;
	const bool rapidFirePretracking = CFG::Exploits_RapidFire_Key && H::Input->IsDown(CFG::Exploits_RapidFire_Key);
	const bool needsTargetScan = aimKeyDown || manualFireIntent || bManualFiring || rapidFirePretracking;
	if (!needsTargetScan)
		return;

	HitscanTarget_t target = {};
	const bool bFoundTarget = GetTarget(pLocal, pWeapon, target) && target.Entity;

	// Target switch settle - crossing the fire delay must not license an instant
	// snap onto a *different* player, which in Silent mode is a full view jump.
	// Re-acquiring the same target is unaffected.
	if (bFoundTarget && CFG::Aimbot_Hitscan_Delay_Fire && CFG::Aimbot_Hitscan_Delay_Fire_Switch_Time > 0.0f
		&& m_nLastFiredTargetIndex > 0 && target.Entity->entindex() != m_nLastFiredTargetIndex
		&& I::GlobalVars->curtime < m_flTargetSwitchEndTime)
	{
		G::bAimbotFireDelayed = true;
		if (bSmoothAimActive)
			ResetSmoothMotion();

		return;
	}

	if (bFoundTarget)
	{
		const int nTargetIndex = target.Entity->entindex();
		if (bSmoothAimActive && nTargetIndex != m_nSmoothTargetIndex)
		{
			m_nSmoothTargetIndex = nTargetIndex;
			ResetSmoothMotion();
		}

		G::nTargetIndexEarly = nTargetIndex;

		if (aimKeyDown || bManualFiring)
		{
			G::nTargetIndex = nTargetIndex;

			// Auto Scope
			if (CFG::Aimbot_Hitscan_Auto_Scope
				&& !pLocal->IsZoomed() && pLocal->m_iClass() == TF_CLASS_SNIPER && pWeapon->GetSlot() == WEAPON_SLOT_PRIMARY && G::bCanPrimaryAttack)
			{
				pCmd->buttons |= IN_ATTACK2;
				if (bSmoothAimActive)
					ResetSmoothMotion();

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
			{
				m_flDelayFireEndTime = I::GlobalVars->curtime + CFG::Aimbot_Hitscan_Delay_Fire_Time;

				// Record who we shot so a later switch to a different player has
				// to settle past the fire delay before it can be acquired
				m_nLastFiredTargetIndex = target.Entity->entindex();
				m_flTargetSwitchEndTime = m_flDelayFireEndTime + CFG::Aimbot_Hitscan_Delay_Fire_Switch_Time;
			}

			// Are we ready to aim?
			if (ShouldAim(pCmd, pLocal, pWeapon) || bIsFiring)
			{
				if (aimKeyDown)
				{
					Aim(pCmd, pLocal, target.AngleTo);
				}

				if (bIsFiring && target.Entity->GetClassId() == ETFClassIds::CTFPlayer)
				{
					// A record's tick is only correct for a shot that is actually
					// pointing at that record. Verify rather than assume: Aim()
					// above snaps exactly onto target.AngleTo for Plain/Silent,
					// only walks toward it for Smooth/Aim Assist, and does not run
					// at all when the aim key is up - and Aimbot_Key is unbound by
					// default, so the common case is a hand-aimed shot that the
					// aimbot merely happened to scan a target for.
					//
					// Stamping unconditionally is what made manual shots miss: the
					// tick came from whichever candidate won the FOV sort inside
					// Aimbot_Hitscan_FOV (45 deg by default), rewinding every
					// player to a pose the crosshair was never on - sometimes a
					// pose belonging to someone the user was not even shooting at.
					//
					// A shot this branch declines to claim is left to
					// CAimbot::Run, which resolves it against the ray the user
					// actually fired down once this feature has returned.
					Vec3 vAimError = target.AngleTo - pLocal->m_vecPunchAngle() - pCmd->viewangles;
					Math::ClampAngles(vAimError);

					constexpr float flAimedEpsilon = 0.01f;
					const bool bAimbotDirectedShot = vAimError.LengthSqr() <= flAimedEpsilon * flAimedEpsilon;

					if (bAimbotDirectedShot)
					{
						// Only rewind when the winning candidate actually is a
						// historical pose. A live-pose candidate carries
						// m_flSimulationTime while its position is the
						// interpolated present, so stamping it fabricates a
						// historical tick for a pose that was never recorded;
						// leaving tick_count alone lets the server apply its own
						// latency correction, which is what that shot was aimed
						// with.
						if (target.LagRecord)
						{
							pCmd->tick_count = CLagRecords::GetCommandTick(target.SimulationTime);
						}

						// Claim the command either way. The live-pose case is a
						// deliberate decision to keep the incoming tick, and it is
						// every bit as much a decision as writing one - without the
						// flag, AutoBackstab and the manual resolver both read
						// "nobody owns this" and retarget a shot the aimbot had
						// already aimed at the present.
						G::bCommandTickResolved = true;
					}
				}
			}
			else if (bSmoothAimActive)
			{
				ResetSmoothMotion();
			}
		}
	}
	else if (bSmoothAimActive)
	{
		// No aimbot target found. The user's own shot, if any, is resolved by
		// CAimbot::Run once this feature returns.
		ResetSmoothMotion();
	}
}
