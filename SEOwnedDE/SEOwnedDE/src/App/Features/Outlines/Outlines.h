#pragma once

#include "../../../SDK/SDK.h"
#include <array>
#include <cstdint>

class COutlines
{
	IMaterial *m_pMatGlowColor = nullptr, *m_pMatHaloAddToScreen = nullptr;
	ITexture *m_pRtFullFrame = nullptr, *m_pRenderBuffer0 = nullptr, *m_pRenderBuffer1 = nullptr;
	IMaterial *m_pMatBlurX = nullptr, *m_pMatBlurY = nullptr;
	IMaterialVar* m_pBloomAmount = nullptr;

	void Initialize(bool bCreateBloomResources);

	std::array<uint32_t, MAX_EDICTS> m_arrDrawnGenerations = {};
	std::array<int, MAX_EDICTS> m_arrDrawnHandles = {};
	uint32_t m_nDrawGeneration = 1;
	int m_nDrawFrame = -1;
	bool m_bHasAnyDrawn = false;
	bool m_bRendering = false;
	bool m_bRenderingOutlines = false;
	bool m_bCleaningUp = false;

	void BeginDrawPass();
	void MarkDrawn(C_BaseEntity* pEntity);
	void DrawEntity(C_BaseEntity* pEntity, bool bModel);

	struct OutlineEntity_t
	{
		C_BaseEntity* m_pEntity = nullptr;
		Color_t m_Color = {};
		float m_flAlpha = 0.0f;
	};

	std::vector<OutlineEntity_t> m_vecOutlineEntities = {};

public:
	void RunModels(IMatRenderContext* pRenderContext);
	void Run();
	void CleanUp();
	void SetModelStencil(IMatRenderContext* pRenderContext);

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

	bool IsRenderingOutlines()
	{
		return m_bRenderingOutlines;
	}

	bool IsUsedMaterial(const IMaterial* pMaterial)
	{
		return pMaterial && (pMaterial == m_pMatGlowColor
			|| pMaterial == m_pMatBlurX
			|| pMaterial == m_pMatBlurY
			|| pMaterial == m_pMatHaloAddToScreen);
	}

	bool IsCleaningUp() { return m_bCleaningUp; }
};

MAKE_SINGLETON_SCOPED(COutlines, Outlines, F);
