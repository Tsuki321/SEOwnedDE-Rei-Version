#pragma once

#include "../../../SDK/SDK.h"
#include <array>
#include <cstdint>

class CMaterials
{
	void Initialize();

	std::array<uint32_t, MAX_EDICTS> m_arrDrawnGenerations = {};
	std::array<int, MAX_EDICTS> m_arrDrawnHandles = {};
	uint32_t m_nDrawGeneration = 1;
	int m_nDrawFrame = -1;
	bool m_bHasAnyDrawn = false;
	bool m_bRendering = false;
	bool m_bRenderingOriginalMat = false;
	bool m_bCleaningUp = false;

	void BeginDrawPass();
	void MarkDrawn(C_BaseEntity* pEntity);
	void DrawEntity(C_BaseEntity* pEntity, IMatRenderContext* pRenderContext, C_TFPlayer* pPlayerOwner = nullptr);
	void RunLagRecords(IMatRenderContext* pRenderContext);

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

	void Run(IMatRenderContext* pRenderContext);
	void CleanUp();

	bool HasDrawn(C_BaseEntity* pEntity)
	{
		if (!pEntity)
			return false;

		const int nEntityIndex = pEntity->entindex();
		const int nFrame = I::GlobalVars ? I::GlobalVars->framecount : -1;
		return m_nDrawFrame == nFrame
			&& nEntityIndex >= 0 && nEntityIndex < MAX_EDICTS
			&& m_arrDrawnGenerations[nEntityIndex] == m_nDrawGeneration
			&& m_arrDrawnHandles[nEntityIndex] == pEntity->GetRefEHandle().ToInt();
	}

	// Cheap gate for the per-draw hot path: when nothing was drawn this pass the
	// boolean is false, so callers can skip the indexed lookup entirely.
	bool HasAnyDrawn()
	{
		const int nFrame = I::GlobalVars ? I::GlobalVars->framecount : -1;
		return m_bHasAnyDrawn && m_nDrawFrame == nFrame;
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
		return pMaterial && (pMaterial == m_pFlat
			|| pMaterial == m_pShaded
			|| pMaterial == m_pGlossy
			|| pMaterial == m_pGlow
			|| pMaterial == m_pPlastic
			|| pMaterial == m_pFlatNoInvis
			|| pMaterial == m_pShadedNoInvis);
	}

	bool IsCleaningUp() { return m_bCleaningUp; }
};

MAKE_SINGLETON_SCOPED(CMaterials, Materials, F);
