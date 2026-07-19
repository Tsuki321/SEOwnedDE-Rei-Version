#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/Materials/Materials.h"
#include "../Features/Outlines/Outlines.h"
#include "../Features/Rendering/RenderContextScope.h"
#include "../Features/Rendering/RenderPassState.h"
#include "../Features/SpyCamera/SpyCamera.h"
#include "../Features/VisualUtils/VisualUtils.h"

MAKE_SIGNATURE(CBaseWorldView_DrawExecute, "client.dll", "40 53 55 56 41 56 41 57 48 81 EC", 0x0);
MAKE_SIGNATURE(CParticleSystemMgr_DrawRenderCache, "client.dll", "48 8B C4 88 50 ? 48 89 48 ? 55 57", 0x0);

MAKE_HOOK(CBaseWorldView_DrawExecute, Signatures::CBaseWorldView_DrawExecute.Get(), void, __fastcall,
	void* ecx, float waterHeight, view_id_t viewID, float waterZAdjust)
{
	// Feature model passes and stencil preparation belong to the main view;
	// reflections/sky/shadow views would otherwise repeat the full work.
	if (viewID == VIEW_MAIN)
		RenderPassState::MarkCurrentRenderViewMainWorld();

	RenderPassState::CMainWorldScope mainWorldScope(viewID == VIEW_MAIN);
	CALL_ORIGINAL(ecx, waterHeight, viewID, waterZAdjust);
}

MAKE_HOOK(CParticleSystemMgr_DrawRenderCache, Signatures::CParticleSystemMgr_DrawRenderCache.Get(), void, __fastcall,
	void* ecx, bool bShadowDepth)
{
	if (RenderPassState::g_bDrawingMainWorld && !bShadowDepth && !F::SpyCamera->IsRendering())
	{
		const int frame = I::GlobalVars ? I::GlobalVars->framecount : -1;
		const bool bCleanScreenshot = CFG::Misc_Clean_Screenshot
			&& F::VisualUtils->IsTakingScreenshotCached();
		const bool bCanDrawModels = !I::EngineVGui->IsGameUIVisible() && !bCleanScreenshot;
		const bool bRunMaterials = bCanDrawModels
			&& CFG::Materials_Active
			&& (CFG::Materials_Players_Active || CFG::Materials_Buildings_Active || CFG::Materials_World_Active);
		const bool bRunOutlines = bCanDrawModels
			&& CFG::Outlines_Active
			&& (CFG::Outlines_Players_Active || CFG::Outlines_Buildings_Active || CFG::Outlines_World_Active);
		const bool bPreparePaintStencil = bCanDrawModels && CFG::Visuals_Paint_Active;
		const bool bNeedsRenderPass = bRunMaterials || bRunOutlines || bPreparePaintStencil;

		if (bNeedsRenderPass && RenderPassState::TryBeginMainWorldModelPass(frame))
		{
			CRenderContextScope renderContext(I::MaterialSystem);
			if (renderContext)
			{
				if (bRunOutlines || bPreparePaintStencil)
					renderContext->ClearBuffers(false, false, true);

				if (bRunMaterials)
					F::Materials->Run(renderContext.Get());

				if (bRunOutlines)
					F::Outlines->RunModels(renderContext.Get());

				RenderPassState::CompleteMainWorldModelPass(frame);
			}
			else
			{
				RenderPassState::ReleaseMainWorldModelPass(frame);
			}
		}
	}

	CALL_ORIGINAL(ecx, bShadowDepth);
}
