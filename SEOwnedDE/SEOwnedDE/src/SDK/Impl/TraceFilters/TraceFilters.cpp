#include "TraceFilters.h"
#include "../../SDK.h"

CTraceFilterHitscan::CTraceFilterHitscan()
{
	// A trace can visit dozens of entities.  Snapshot the invariant inputs once
	// when the filter is created instead of resolving them for every candidate.
	const auto pLocal = H::Entities->GetLocal();
	const auto pWeaponEntity = pLocal ? pLocal->m_hActiveWeapon().Get() : nullptr;
	const auto pWeapon = pWeaponEntity ? pWeaponEntity->As<C_TFWeaponBase>() : nullptr;

	if (pLocal && pWeapon)
	{
		m_pLocal = pLocal;
		m_nLocalTeam = pLocal->m_iTeamNum();
		m_nWeaponID = pWeapon->GetWeaponID();
		m_bValid = true;
	}
}

bool CTraceFilterHitscan::ShouldHitEntity(IHandleEntity *pServerEntity, int contentsMask)
{
	if (!m_bValid)
		return false;

	if (!pServerEntity || pServerEntity == m_pIgnore || pServerEntity == m_pLocal)
		return false;

	if (auto pEntity = static_cast<IClientEntity *>(pServerEntity)->As<C_BaseEntity>())
	{
		switch (pEntity->GetClassId())
		{
			case ETFClassIds::CFuncAreaPortalWindow:
			case ETFClassIds::CFuncRespawnRoomVisualizer:
			case ETFClassIds::CSniperDot:
			case ETFClassIds::CTFAmmoPack: return false;

			case ETFClassIds::CTFMedigunShield:
			{
				if (pEntity->m_iTeamNum() == m_nLocalTeam)
					return false;

				break;
			}

			case ETFClassIds::CTFPlayer:
			case ETFClassIds::CObjectSentrygun:
			case ETFClassIds::CObjectDispenser:
			case ETFClassIds::CObjectTeleporter:
			{
				switch (m_nWeaponID)
				{
					case TF_WEAPON_SNIPERRIFLE:
					case TF_WEAPON_SNIPERRIFLE_CLASSIC:
					case TF_WEAPON_SNIPERRIFLE_DECAP:
					{
						if (pEntity->m_iTeamNum() == m_nLocalTeam)
							return false;

						break;
					}

					default: break;
				}

				break;
			}

			default: break;
		}

	}

	return true;
}

bool CTraceFilterWorldCustom::ShouldHitEntity(IHandleEntity *pServerEntity, int contentsMask)
{
	if (auto pEntity = static_cast<IClientEntity *>(pServerEntity)->As<C_BaseEntity>())
	{
		switch (pEntity->GetClassId())
		{
			case ETFClassIds::CTFPlayer:
			case ETFClassIds::CObjectSentrygun:
			case ETFClassIds::CObjectDispenser:
			case ETFClassIds::CObjectTeleporter: return pEntity == m_pTarget;

			case ETFClassIds::CObjectCartDispenser:
			case ETFClassIds::CBaseDoor:
			case ETFClassIds::CPhysicsProp:
			case ETFClassIds::CDynamicProp:
			case ETFClassIds::CBaseEntity:
			case ETFClassIds::CFuncTrackTrain: return true;

			default: return false;
		}
	}

	return false;
}

bool CTraceFilterArc::ShouldHitEntity(IHandleEntity* pServerEntity, int contentsMask)
{
	if (!pServerEntity || pServerEntity == m_pIgnore || pServerEntity == m_pIgnore2)
		return false;

	if (const auto pEntity = static_cast<IClientEntity*>(pServerEntity)->As<C_BaseEntity>())
	{
		switch (pEntity->GetClassId())
		{
			case ETFClassIds::CTFPlayer:
			case ETFClassIds::CObjectSentrygun:
			case ETFClassIds::CObjectDispenser:
			case ETFClassIds::CObjectTeleporter:
			case ETFClassIds::CObjectCartDispenser:
			case ETFClassIds::CBaseDoor:
			case ETFClassIds::CPhysicsProp:
			case ETFClassIds::CDynamicProp:
			case ETFClassIds::CBaseEntity:
			case ETFClassIds::CFuncTrackTrain:
			{
				return true;
			}

			default:
			{
				return false;
			}
		}
	}

	return false;
}
