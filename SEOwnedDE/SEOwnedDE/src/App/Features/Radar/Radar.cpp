#include "Radar.h"

#include "../CFG.h"
#include "../Menu/Menu.h"
#include "../VisualUtils/VisualUtils.h"

void CRadar::UpdateRadarCache(C_TFPlayer* pLocal)
{
	m_pCachedLocal = pLocal;
	m_vCachedLocalCenter = pLocal ? pLocal->GetCenter() : Vec3{};
	m_nCachedRadarSize = CFG::Radar_Size;
	m_nCachedRadarStyle = CFG::Radar_Style;
	m_nCachedRadarX = CFG::Radar_Pos_X + (m_nCachedRadarSize / 2);
	m_nCachedRadarY = CFG::Radar_Pos_Y + (m_nCachedRadarSize / 2);
	m_flCachedRadius = std::max(CFG::Radar_Radius, 1.0f);

	const float flYaw = I::EngineClient->GetViewAngles().y * (static_cast<float>(PI) / 180.0f);
	m_flCachedCos = std::cosf(flYaw);
	m_flCachedSin = std::sinf(flYaw);
}

void CRadar::Drag()
{
	const int nMouseX = H::Input->GetMouseX();
	const int nMouseY = H::Input->GetMouseY();
	const int nRadarSize = CFG::Radar_Size;

	bool bHovered = false;
	static bool bDragging = false;

	if (!bDragging && F::Menu->IsMenuWindowHovered())
		return;

	static int nDeltaX = 0;
	static int nDeltaY = 0;

	switch (CFG::Radar_Style)
	{
		case 0:
		{
			const int nRadarX = CFG::Radar_Pos_X;
			const int nRadarY = CFG::Radar_Pos_Y;

			bHovered = nMouseX > nRadarX && nMouseX < nRadarX + nRadarSize && nMouseY > nRadarY && nMouseY < nRadarY + nRadarSize;

			if (bHovered && H::Input->IsPressed(VK_LBUTTON))
			{
				nDeltaX = nMouseX - nRadarX;
				nDeltaY = nMouseY - nRadarY;
				bDragging = true;
			}

			break;
		}

		case 1:
		{
			const int nRadarX = CFG::Radar_Pos_X + nRadarSize / 2;
			const int nRadarY = CFG::Radar_Pos_Y + nRadarSize / 2;

			const auto vRadar = Vec2(static_cast<float>(nRadarX), static_cast<float>(nRadarY));
			const auto vMouse = Vec2(static_cast<float>(nMouseX), static_cast<float>(nMouseY));

			bHovered = static_cast<int>(vRadar.DistTo(vMouse)) < (CFG::Radar_Size / 2);

			if (bHovered && H::Input->IsPressed(VK_LBUTTON))
			{
				nDeltaX = nMouseX - CFG::Radar_Pos_X;
				nDeltaY = nMouseY - CFG::Radar_Pos_Y;
				bDragging = true;
			}

			break;
		}

		default: return;
	}

	if (!H::Input->IsPressed(VK_LBUTTON) && !H::Input->IsHeld(VK_LBUTTON))
		bDragging = false;

	if (bDragging)
	{
		CFG::Radar_Pos_X = nMouseX - nDeltaX;
		CFG::Radar_Pos_Y = nMouseY - nDeltaY;
	}
}

bool CRadar::GetDrawPosition(int& x, int& y, const Vec3& vWorld)
{
	if (!m_pCachedLocal)
		return false;

	const Vec3 vDelta = vWorld - m_vCachedLocalCenter;
	Vec2 vPos = {
		(vDelta.y * (-m_flCachedCos) + vDelta.x * m_flCachedSin),
		(vDelta.x * (-m_flCachedCos) - vDelta.y * m_flCachedSin)
	};

	switch (m_nCachedRadarStyle)
	{
		// Rectangle
		case 0:
		{
			if (fabsf(vPos.x) > m_flCachedRadius || fabsf(vPos.y) > m_flCachedRadius)
			{
				if (vPos.y > vPos.x)
				{
					if (vPos.y > -vPos.x)
					{
						vPos.x = m_flCachedRadius * vPos.x / vPos.y;
						vPos.y = m_flCachedRadius;
					}

					else
					{
						vPos.y = -m_flCachedRadius * vPos.y / vPos.x;
						vPos.x = -m_flCachedRadius;
					}
				}

				else
				{
					if (vPos.y > -vPos.x)
					{
						vPos.y = m_flCachedRadius * vPos.y / vPos.x;
						vPos.x = m_flCachedRadius;
					}

					else
					{
						vPos.x = -m_flCachedRadius * vPos.x / vPos.y;
						vPos.y = -m_flCachedRadius;
					}
				}
			}

			x = m_nCachedRadarX + static_cast<int>(vPos.x / m_flCachedRadius * static_cast<float>(m_nCachedRadarSize / 2));
			y = m_nCachedRadarY + static_cast<int>(vPos.y / m_flCachedRadius * static_cast<float>(m_nCachedRadarSize / 2));

			break;
		}

		// Circle
		case 1:
		{
			const int nPosX = m_nCachedRadarX + static_cast<int>(vPos.x / m_flCachedRadius * static_cast<float>(m_nCachedRadarSize / 2));
			const int nPosY = m_nCachedRadarY + static_cast<int>(vPos.y / m_flCachedRadius * static_cast<float>(m_nCachedRadarSize / 2));

			const Vec2 vRadar = { static_cast<float>(m_nCachedRadarX), static_cast<float>(m_nCachedRadarY) };
			Vec2 vPoint = { static_cast<float>(nPosX), static_cast<float>(nPosY) };

			Vec2 vDelta = vPoint - vRadar;
			const float flDeltaLength = vDelta.Length();

			if (static_cast<int>(flDeltaLength) > m_nCachedRadarSize / 2)
			{
				vDelta *= static_cast<float>(m_nCachedRadarSize / 2) / flDeltaLength;
				vPoint = vRadar + vDelta;

				x = static_cast<int>(vPoint.x);
				y = static_cast<int>(vPoint.y);
			}

			else
			{
				x = nPosX;
				y = nPosY;
			}

			break;
		}

		default: return false;
	}

	return true;
}

void CRadar::Run()
{
	if (!CFG::Radar_Active || ((I::EngineVGui->IsGameUIVisible() || SDKUtils::BInEndOfMatch()) && !F::Menu->IsOpen()))
		return;

	if (CFG::Misc_Clean_Screenshot && I::EngineClient->IsTakingScreenshot())
	{
		return;
	}

	const int nRadarSize = CFG::Radar_Size;

	const auto crossColor = F::VisualUtils->GetAlphaColor(CFG::Menu_Accent_Secondary, CFG::Radar_Cross_Alpha);
	const auto outlineColor = F::VisualUtils->GetAlphaColor(CFG::Menu_Accent_Secondary, CFG::Radar_Outline_Alpha);
	const auto bgColor = F::VisualUtils->GetAlphaColor(CFG::Menu_Background, CFG::Radar_Background_Alpha);

	if (F::Menu->IsOpen())
		Drag();

	switch (CFG::Radar_Style)
	{
		// Rectangle
		case 0:
		{
			const int nRadarX = CFG::Radar_Pos_X;
			const int nRadarY = CFG::Radar_Pos_Y;

			H::Draw->Rect(nRadarX, nRadarY, nRadarSize, nRadarSize, bgColor);
			H::Draw->OutlinedRect(nRadarX, nRadarY, nRadarSize, nRadarSize, outlineColor);

			H::Draw->Line(
				nRadarX + (nRadarSize / 8),
				nRadarY + (nRadarSize / 2),
				nRadarX + (nRadarSize - ((nRadarSize / 8))),
				nRadarY + (nRadarSize / 2),
				crossColor
			);

			H::Draw->Line(
				nRadarX + (nRadarSize / 2),
				nRadarY + (nRadarSize / 8),
				nRadarX + (nRadarSize / 2),
				nRadarY + (nRadarSize - ((nRadarSize / 8))),
				crossColor
			);

			break;
		}

		// Circle
		case 1:
		{
			int nRadarX = CFG::Radar_Pos_X + nRadarSize / 2;
			int nRadarY = CFG::Radar_Pos_Y + nRadarSize / 2;

			H::Draw->FilledCircle(nRadarX, nRadarY, nRadarSize / 2, 100, bgColor);
			H::Draw->OutlinedCircle(nRadarX, nRadarY, nRadarSize / 2, 100, outlineColor);

			nRadarX = CFG::Radar_Pos_X;
			nRadarY = CFG::Radar_Pos_Y;

			H::Draw->Line(
				nRadarX + (nRadarSize / 8),
				nRadarY + (nRadarSize / 2),
				nRadarX + (nRadarSize - ((nRadarSize / 8))),
				nRadarY + (nRadarSize / 2),
				crossColor
			);

			H::Draw->Line(
				nRadarX + (nRadarSize / 2),
				nRadarY + (nRadarSize / 8),
				nRadarX + (nRadarSize / 2),
				nRadarY + (nRadarSize - ((nRadarSize / 8))),
				crossColor
			);

			break;
		}

		default: return;
	}

	const auto pLocal = H::Entities->GetLocal();

	if (!pLocal)
		return;

	UpdateRadarCache(pLocal);

	const int nIconSize = CFG::Radar_Icon_Size;

	// Draw world objects
	if (CFG::Radar_World_Active)
	{
		if (!CFG::Radar_World_Ignore_HealthPacks)
		{
			for (const auto pEntity : H::Entities->GetGroup(EEntGroup::HEALTHPACKS))
			{
				if (!pEntity)
					continue;

				int x = 0, y = 0;

				if (!GetDrawPosition(x, y, pEntity->GetCenter()))
					continue;

				H::Draw->Texture(x, y, nIconSize, nIconSize, F::VisualUtils->GetHealthIconTextureId(), POS_CENTERXY);
			}
		}

		if (!CFG::Radar_World_Ignore_AmmoPacks)
		{
			for (const auto pEntity : H::Entities->GetGroup(EEntGroup::AMMOPACKS))
			{
				if (!pEntity)
					continue;

				int x = 0, y = 0;

				if (!GetDrawPosition(x, y, pEntity->GetCenter()))
					continue;

				H::Draw->Texture(x, y, nIconSize, nIconSize, F::VisualUtils->GetAmmoIconTextureId(), POS_CENTERXY);
			}
		}

		if (!CFG::Radar_World_Ignore_Halloween_Gift)
		{
			const float s = Math::RemapValClamped
			(
				static_cast<float>(nIconSize),
				18.0f,
				36.0f,
				0.5f,
				1.0f
			);

			for (const auto pEntity : H::Entities->GetGroup(EEntGroup::HALLOWEEN_GIFT))
			{
				if (!pEntity || !pEntity->ShouldDraw())
					continue;

				int x = 0, y = 0;

				if (!GetDrawPosition(x, y, pEntity->GetCenter()))
					continue;

				H::Draw->Texture(x, y, static_cast<int>(36.0f * s), static_cast<int>(40.0f * s), F::VisualUtils->GetHalloweenGiftTextureId(), POS_CENTERXY);
			}
		}
	}

	// Draw buildings
	if (CFG::Radar_Buildings_Active)
	{
		for (const auto pBuilding : F::VisualUtils->GetBuildingCandidates(pLocal))
		{
			if (!F::VisualUtils->ShouldRenderBuilding(
				pLocal,
				pBuilding,
				CFG::Radar_Buildings_Ignore_Local,
				CFG::Radar_Buildings_Ignore_Teammates,
				CFG::Radar_Buildings_Show_Teammate_Dispensers,
				CFG::Radar_Buildings_Ignore_Enemies
			))
				continue;

			const auto nTexture = F::VisualUtils->GetBuildingTextureId(pBuilding);

			if (!nTexture)
				continue;

			int x = 0, y = 0;

			if (!GetDrawPosition(x, y, pBuilding->GetCenter()))
				continue;

			Color_t entColor = F::VisualUtils->GetEntityColor(pLocal, pBuilding);
			entColor.a = 100;

			H::Draw->FilledCircle(x, y, (nIconSize + 8) / 2, 20, entColor);
			H::Draw->Texture(x, y, nIconSize, nIconSize, nTexture, POS_CENTERXY);
			//H::Draw->OutlinedCircle(x, y, (nIconSize + 8) / 2, 20, CFG::Color_ESP_Outline);
		}
	}

	// Draw players
	if (CFG::Radar_Players_Active)
	{
		for (const auto pPlayer : F::VisualUtils->GetPlayerCandidates(pLocal))
		{
			if (!F::VisualUtils->ShouldRenderPlayer(
				pLocal,
				pPlayer,
				CFG::Radar_Players_Ignore_Local,
				CFG::Radar_Players_Ignore_Friends,
				CFG::Radar_Players_Ignore_Teammates,
				CFG::Radar_Players_Show_Teammate_Medics,
				CFG::Radar_Players_Ignore_Enemies,
				CFG::Radar_Players_Ignore_Invisible
			))
				continue;

			int x = 0, y = 0;

			if (!GetDrawPosition(x, y, pPlayer->GetCenter()))
				continue;

			Color_t entColor = F::VisualUtils->GetEntityColor(pLocal, pPlayer);
			entColor.a = 100;

			H::Draw->FilledCircle(x, y, (nIconSize + 4) / 2, 20, entColor);
			H::Draw->Texture(x, y, nIconSize, nIconSize, F::VisualUtils->GetClassIcon(pPlayer->m_iClass()), POS_CENTERXY);
			//H::Draw->OutlinedCircle(x, y, (nIconSize + 4) / 2, 20, CFG::Color_ESP_Outline);
		}
	}

	// Draw MvM money
	if (CFG::Radar_World_Active && !CFG::Radar_World_Ignore_MVM_Money)
	{
		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::MVM_MONEY))
		{
			if (!pEntity || !pEntity->ShouldDraw())
				continue;

			int x = 0, y = 0;

			if (!GetDrawPosition(x, y, pEntity->GetCenter()))
				continue;

			H::Draw->String(H::Fonts->Get(EFonts::ESP_SMALL), x, y, CFG::Color_MVM_Money, POS_CENTERXY, "$");
		}
	}
}
