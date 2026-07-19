#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/Outlines/Outlines.h"
#include "../Features/Paint/Paint.h"
#include "../Features/Rendering/RenderPassState.h"
#include "../Features/SpyCamera/SpyCamera.h"

MAKE_SIGNATURE(CViewRender_DrawViewModels, "client.dll", "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 8B FA", 0x0);

MAKE_HOOK(CViewRender_DrawViewModels, Signatures::CViewRender_DrawViewModels.Get(), void, __fastcall,
	void* ecx, const CViewSetup& viewRender, bool drawViewmodel)
{
	CALL_ORIGINAL(ecx, viewRender, drawViewmodel);

	if (F::SpyCamera->IsRendering())
		return;

	// DrawViewModels also runs for monitors and other nested views. Only the
	// RenderView invocation that actually drew VIEW_MAIN owns the composite.
	if (!RenderPassState::IsCurrentRenderViewMainWorld())
		return;

	if (I::EngineVGui->IsGameUIVisible())
	{
		// The UI path performs point maintenance only; CPaint::Run returns before
		// allocating a render context or touching blur targets.
		F::Paint->Run();
		return;
	}

	const int frame = I::GlobalVars ? I::GlobalVars->framecount : -1;
	const bool bOutlinesEnabled = CFG::Outlines_Active
		&& (CFG::Outlines_Players_Active || CFG::Outlines_Buildings_Active || CFG::Outlines_World_Active);
	const bool bNeedsPreparedMainWorld = bOutlinesEnabled || CFG::Visuals_Paint_Active;
	const bool bMainWorldPassComplete = RenderPassState::IsMainWorldModelPassComplete(frame);

	// Auxiliary views can reach this hook before the main world has prepared
	// stencil/model data. Do not let them consume the frame's composite claim.
	if (bNeedsPreparedMainWorld && !bMainWorldPassComplete)
		return;

	if (!RenderPassState::TryBeginCompositePass(frame))
		return;

	if (bOutlinesEnabled)
		F::Outlines->Run();

	// Keep the disabled path so Paint can release its point storage. An active
	// paint composite requires the main-world stencil clear to have completed.
	if (!CFG::Visuals_Paint_Active || bMainWorldPassComplete)
		F::Paint->Run();
}
