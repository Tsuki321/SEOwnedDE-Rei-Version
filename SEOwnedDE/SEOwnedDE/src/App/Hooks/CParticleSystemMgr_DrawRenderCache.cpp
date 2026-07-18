#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/Materials/Materials.h"
#include "../Features/Outlines/Outlines.h"
#include "../Features/Rendering/RenderContextScope.h"
#include "../Features/Rendering/RenderPassState.h"
#include "../Features/SpyCamera/SpyCamera.h"

MAKE_SIGNATURE(CBaseWorldView_DrawExecute, "client.dll", "40 53 55 56 41 56 41 57 48 81 EC", 0x0);
MAKE_SIGNATURE(CParticleSystemMgr_DrawRenderCache, "client.dll", "48 8B C4 88 50 ? 48 89 48 ? 55 57", 0x0);

bool RenderPassState::g_bDrawingMainWorld = false;

MAKE_HOOK(CBaseWorldView_DrawExecute, Signatures::CBaseWorldView_DrawExecute.Get(), void, __fastcall,
	void* ecx, float waterHeight, view_id_t viewID, float waterZAdjust)
{
	// Feature model passes and stencil preparation belong to the main view;
	// reflections/sky/shadow views would otherwise repeat the full work.
	RenderPassState::CMainWorldScope mainWorldScope(viewID == VIEW_MAIN);
	CALL_ORIGINAL(ecx, waterHeight, viewID, waterZAdjust);
}

MAKE_HOOK(CParticleSystemMgr_DrawRenderCache, Signatures::CParticleSystemMgr_DrawRenderCache.Get(), void, __fastcall,
	void* ecx, bool bShadowDepth)
{
	if (RenderPassState::g_bDrawingMainWorld && !bShadowDepth && !F::SpyCamera->IsRendering())
	{
		const bool bNeedsRenderContext = CFG::Materials_Active
			|| CFG::Outlines_Active
			|| CFG::Visuals_Paint_Active;
		CRenderContextScope renderContext(bNeedsRenderContext ? I::MaterialSystem : nullptr);

		// Stencil contents are only consumed by outlines/paint. Avoid a context
		// acquisition and full stencil clear in the default disabled path.
		if (renderContext && (CFG::Outlines_Active || CFG::Visuals_Paint_Active))
			renderContext->ClearBuffers(false, false, true);

		F::Materials->Run(renderContext.Get());
		F::Outlines->RunModels(renderContext.Get());
	}

	CALL_ORIGINAL(ecx, bShadowDepth);
}
