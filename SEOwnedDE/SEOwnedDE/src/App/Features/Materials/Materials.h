#pragma once

#include "../../../SDK/SDK.h"
#include <unordered_set>

class CMaterials
{
	void Initialize();

	std::unordered_set<C_BaseEntity*> m_setDrawnEntities = {};
	bool m_bRendering = false;
	bool m_bRenderingOriginalMat = false;
	bool m_bCleaningUp = false;

	void DrawEntity(C_BaseEntity* pEntity);
	void RunLagRecords();

public:
	IMaterial* m_pFlat = nullptr;
	IMaterial* m_pShaded = nullptr;
	IMaterial* m_pGlossy = nullptr;
	IMaterial* m_pGlow = nullptr;
	IMaterial* m_pPlastic = nullptr;
	IMaterialVar* m_pGlowEnvmapTint = nullptr;
	IMaterialVar* m_pGlowSelfillumTint = nullptr;
	IMaterial* m_pFlatNoInvis = nullptr;
	IMaterial* m_pShadedNoInvis = nullptr;

	void Run();
	void CleanUp();

	bool HasDrawn(C_BaseEntity* pEntity)
	{
		return m_setDrawnEntities.contains(pEntity);
	}

	// Cheap gate for the per-draw hot path: when nothing was drawn this frame the
	// set is empty, so callers can skip the hash lookup entirely.
	bool HasAnyDrawn()
	{
		return !m_setDrawnEntities.empty();
	}

	bool IsRendering()
	{
		return m_bRendering;
	}

	bool IsRenderingOriginalMat()
	{
		return m_bRenderingOriginalMat;
	}

	bool IsUsedMaterial(const IMaterial* pMaterial)
	{
		return pMaterial == m_pFlat
			|| pMaterial == m_pShaded
			|| pMaterial == m_pGlossy
			|| pMaterial == m_pGlow
			|| pMaterial == m_pPlastic
			|| pMaterial == m_pFlatNoInvis
			|| pMaterial == m_pShadedNoInvis;
	}

	bool IsCleaningUp() { return m_bCleaningUp; }
};

MAKE_SINGLETON_SCOPED(CMaterials, Materials, F);
