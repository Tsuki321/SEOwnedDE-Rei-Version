#include "SpyCamera.h"

#include "../CFG.h"
#include "../Menu/Menu.h"
#include "../VisualUtils/VisualUtils.h"

void CSpyCamera::Drag()
{
	const int nMouseX = H::Input->GetMouseX();
	const int nMouseY = H::Input->GetMouseY();

	static bool bDragging = false;

	if (!bDragging && F::Menu->IsMenuWindowHovered())
		return;

	static int nDeltaX = 0;
	static int nDeltaY = 0;

	const int nCamX = CFG::Visuals_SpyCamera_Pos_X;
	const int nCamY = CFG::Visuals_SpyCamera_Pos_Y;
	const int nCamW = CFG::Visuals_SpyCamera_Pos_W;

	const bool bHovered = nMouseX > nCamX && nMouseX < nCamX + nCamW && nMouseY > nCamY && nMouseY < nCamY + CFG::Menu_Drag_Bar_Height;

	if (bHovered && H::Input->IsPressed(VK_LBUTTON))
	{
		nDeltaX = nMouseX - nCamX;
		nDeltaY = nMouseY - nCamY;
		bDragging = true;
	}

	if (!H::Input->IsPressed(VK_LBUTTON) && !H::Input->IsHeld(VK_LBUTTON))
		bDragging = false;

	if (bDragging)
	{
		CFG::Visuals_SpyCamera_Pos_X = nMouseX - nDeltaX;
		CFG::Visuals_SpyCamera_Pos_Y = nMouseY - nDeltaY;
	}
}

void CSpyCamera::Run()
{
	if (!CFG::Visuals_SpyCamera_Active)
	{
		m_nCachedSpyIndex = -1;
		m_nCachedSpyHandle = -1;
		m_nNextSpyScanTick = -1;
		return;
	}

	// Anti screenshot?
	if (CFG::Misc_Clean_Screenshot && F::VisualUtils->IsTakingScreenshotCached())
	{
		return;
	}

	// Is the game menu open?
	if (!F::Menu->IsOpen() && (I::EngineVGui->IsGameUIVisible() || SDKUtils::BInEndOfMatch()))
		return;

	if (F::Menu->IsOpen())
		Drag();

	const auto bgColor = F::VisualUtils->GetAlphaColor(CFG::Menu_Background, CFG::Visuals_SpyCamera_Background_Alpha);

	// Background
	H::Draw->Rect(
		CFG::Visuals_SpyCamera_Pos_X,
		CFG::Visuals_SpyCamera_Pos_Y,
		CFG::Visuals_SpyCamera_Pos_W,
		CFG::Menu_Drag_Bar_Height,
		bgColor
	);

	// Outline
	H::Draw->OutlinedRect(
		CFG::Visuals_SpyCamera_Pos_X,
		CFG::Visuals_SpyCamera_Pos_Y,
		CFG::Visuals_SpyCamera_Pos_W,
		CFG::Menu_Drag_Bar_Height,
		CFG::Menu_Accent_Secondary
	);

	// Title
	H::Draw->String(
		H::Fonts->Get(EFonts::Menu),
		CFG::Visuals_SpyCamera_Pos_X + (CFG::Visuals_SpyCamera_Pos_W / 2),
		CFG::Visuals_SpyCamera_Pos_Y + (CFG::Menu_Drag_Bar_Height / 2),
		CFG::Menu_Text,
		POS_CENTERXY,
		"Spy Camera"
	);

	const auto pLocal = H::Entities->GetLocal();
	if (!pLocal || pLocal->deadflag() || !I::ViewRender)
	{
		m_nCachedSpyIndex = -1;
		m_nCachedSpyHandle = -1;
		m_nNextSpyScanTick = -1;
		return;
	}

	const Vec3 vLocalCenter = pLocal->GetCenter();
	const Vec3 vLocalShootPos = pLocal->GetShootPos();
	const Vec3 vEngineAngles = I::EngineClient->GetViewAngles();
	const int nCurrentTick = I::GlobalVars ? I::GlobalVars->tickcount : 0;

	auto IsCandidate = [&](C_TFPlayer* pPlayer)
	{
		if (!pPlayer
			|| pPlayer == pLocal
			|| pPlayer->m_iTeamNum() == pLocal->m_iTeamNum()
			|| pPlayer->IsDormant()
			|| pPlayer->deadflag()
			|| pPlayer->m_iClass() != TF_CLASS_SPY
			|| pPlayer->InCond(TF_COND_STEALTHED))
			return false;

		const Vec3 vSpyCenter = pPlayer->GetCenter();
		if (vSpyCenter.DistToSqr(vLocalCenter) > 400.0f * 400.0f)
			return false;

		return Math::CalcFov(
			{ 0.0f, vEngineAngles.y, 0.0f },
			Math::CalcAngle(vLocalShootPos, vSpyCenter)) >= 80.0f;
	};

	C_TFPlayer* pSpy = nullptr;
	if (m_nCachedSpyIndex > 0)
	{
		if (const auto pEntity = I::ClientEntityList->GetClientEntity(m_nCachedSpyIndex))
		{
			if (pEntity->GetClassId() == ETFClassIds::CTFPlayer
				&& pEntity->GetRefEHandle().ToInt() == m_nCachedSpyHandle)
			{
				auto pCachedSpy = pEntity->As<C_TFPlayer>();
				if (IsCandidate(pCachedSpy)
					&& H::AimUtils->TraceEntityAutoDet(pCachedSpy, vLocalShootPos, pCachedSpy->GetShootPos()))
					pSpy = pCachedSpy;
			}
		}
	}

	constexpr int SPY_SCAN_INTERVAL_TICKS = 3;
	const bool bTickRolledBack = m_nNextSpyScanTick >= 0
		&& nCurrentTick + SPY_SCAN_INTERVAL_TICKS < m_nNextSpyScanTick;
	const bool bCachedCandidateInvalid = m_nCachedSpyIndex > 0 && !pSpy;
	if (bTickRolledBack || bCachedCandidateInvalid || nCurrentTick >= m_nNextSpyScanTick || (!pSpy && m_nNextSpyScanTick < 0))
	{
		pSpy = nullptr;
		float flBestDistanceSqr = 400.0f * 400.0f;

		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
		{
			if (!pEntity)
				continue;

			auto pPlayer = pEntity->As<C_TFPlayer>();
			if (!IsCandidate(pPlayer))
				continue;

			const float flDistanceSqr = pPlayer->GetCenter().DistToSqr(vLocalCenter);
			if (flDistanceSqr >= flBestDistanceSqr)
				continue;

			if (!H::AimUtils->TraceEntityAutoDet(pPlayer, vLocalShootPos, pPlayer->GetShootPos()))
				continue;

			flBestDistanceSqr = flDistanceSqr;
			pSpy = pPlayer;
		}

		m_nCachedSpyIndex = pSpy ? pSpy->entindex() : -1;
		m_nCachedSpyHandle = pSpy ? pSpy->GetRefEHandle().ToInt() : -1;
		m_nNextSpyScanTick = nCurrentTick + SPY_SCAN_INTERVAL_TICKS;
	}

	if (!pSpy)
		return;

	// Draw the target spy.
	{
		auto& setup = m_ViewSetup;

		// Set the camera rect
		setup.x = CFG::Visuals_SpyCamera_Pos_X + 1;
		setup.y = CFG::Visuals_SpyCamera_Pos_Y + CFG::Menu_Drag_Bar_Height;
		setup.width = CFG::Visuals_SpyCamera_Pos_W - 2;
		setup.height = CFG::Visuals_SpyCamera_Pos_H;

		const Vec3 vSpyPos = pSpy->GetAbsOrigin() + Vec3(0.0f, 0.0f, pSpy->m_vecMaxs().z);

		const Vec3 vAngles = Math::CalcAngle(vSpyPos, pLocal->GetCenter());
		Vec3 vForward = {};
		Math::AngleVectors(vAngles, &vForward);

		trace_t trace = {};
		CTraceFilterWorldCustom filter = {};
		H::AimUtils->Trace(vSpyPos, vSpyPos - (vForward * 80.0f), MASK_SOLID, &filter, &trace);

		// Set the camera properties
		setup.origin = vSpyPos - ((vForward * 80.0f) * trace.fraction);
		setup.angles = vAngles;
		setup.fov = CFG::Visuals_SpyCamera_FOV;
		setup.m_flAspectRatio = static_cast<float>(setup.width) / static_cast<float>(setup.height);

		// Draw local player if in first-person
		if (!I::Input->CAM_IsThirdPerson())
		{
			I::Input->CAM_ToThirdPerson();

			pLocal->UpdateVisibility();
			pLocal->CreateShadow();

			auto pAttachment = pLocal->FirstMoveChild();

			for (int n = 0; n < 32; n++)
			{
				if (!pAttachment)
					break;

				pAttachment->UpdateVisibility();
				pAttachment->CreateShadow();

				pAttachment = pAttachment->NextMovePeer();
			}
		}

		// Render the camera
		m_IsRendering = true;
		I::ViewRender->RenderView(setup, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL, RENDERVIEW_UNSPECIFIED);
		m_IsRendering = false;

		// Outline
		H::Draw->OutlinedRect(
			CFG::Visuals_SpyCamera_Pos_X,
			CFG::Visuals_SpyCamera_Pos_Y,
			CFG::Visuals_SpyCamera_Pos_W,
			CFG::Visuals_SpyCamera_Pos_H + CFG::Menu_Drag_Bar_Height,
			CFG::Menu_Accent_Secondary
		);
	}
}
