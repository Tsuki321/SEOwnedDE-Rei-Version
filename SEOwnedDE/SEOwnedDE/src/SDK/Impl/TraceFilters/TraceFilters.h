#pragma once
#include "../../TF2/IEngineTrace.h"

class CTraceFilterHitscan : public CTraceFilter
{
public:
	CTraceFilterHitscan();
	bool ShouldHitEntity(IHandleEntity *pServerEntity, int contentsMask) override;

	TraceType_t GetTraceType() const override
	{
		return TRACE_EVERYTHING;
	}

	C_BaseEntity *m_pIgnore = nullptr;

private:
	C_BaseEntity *m_pLocal = nullptr;
	int m_nLocalTeam = 0;
	int m_nWeaponID = 0;
	bool m_bValid = false;
};

class CTraceFilterWorldCustom : public CTraceFilter
{
public:
	bool ShouldHitEntity(IHandleEntity *pServerEntity, int contentsMask) override;

	TraceType_t GetTraceType() const override
	{
		return TRACE_EVERYTHING;
	}

	C_BaseEntity *m_pTarget = nullptr;
};

class CTraceFilterArc : public CTraceFilter
{
public:
	bool ShouldHitEntity(IHandleEntity* pServerEntity, int contentsMask) override;

	TraceType_t GetTraceType() const override
	{
		return TRACE_EVERYTHING;
	}

	C_BaseEntity* m_pIgnore = nullptr;
	C_BaseEntity* m_pIgnore2 = nullptr;
};
