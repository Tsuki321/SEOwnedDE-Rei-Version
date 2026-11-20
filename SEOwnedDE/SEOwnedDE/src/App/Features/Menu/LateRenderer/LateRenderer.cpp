#include "LateRenderer.h"
#include <optional>

void CLateRenderer::String(const CFont &font, int x, int y, Color_t col, short align, const char *str, const ClipRegion_t &clip)
{
	if (!str)
		return;
	m_vecStrings.emplace_back(String_t{ font, x, y, col, align, str, clip });
}

void CLateRenderer::Rect(int x, int y, int w, int h, Color_t col)
{
	m_vecRects.emplace_back(Rect_t{ x, y, w, h, col });
}

void CLateRenderer::OutlinedRect(int x, int y, int w, int h, Color_t col)
{
	m_vecOutlinedRects.emplace_back(Rect_t{ x, y, w, h, col });
}

void CLateRenderer::Texture(int id, int x, int y, int w, int h)
{
	m_vecTextures.emplace_back(Texture_t{ id, x, y, w, h });
}

void CLateRenderer::Clear()
{
	m_ClipBounds = {};
	m_vecStrings.clear();
	m_vecOutlinedRects.clear();
	m_vecRects.clear();
	m_vecTextures.clear();
}

void CLateRenderer::DrawAll()
{
	std::optional<MenuLayout::ScopedClip<IMatSystemSurface>> windowClip;
	if (m_ClipBounds.w > 0 && m_ClipBounds.h > 0)
		windowClip.emplace(I::MatSystemSurface, MenuLayout::Rect{ m_ClipBounds.x, m_ClipBounds.y, m_ClipBounds.w, m_ClipBounds.h });
	for (const auto &Texture : m_vecTextures) {
		I::MatSystemSurface->DrawSetColor(255, 255, 255, 255);
		I::MatSystemSurface->DrawSetTexture(Texture.m_nID);
		I::MatSystemSurface->DrawTexturedRect(Texture.m_nX, Texture.m_nY, Texture.m_nX + Texture.m_nW, Texture.m_nY + Texture.m_nH);
	}

	for (const auto &Rect : m_vecRects)
		H::Draw->Rect(Rect.m_nX, Rect.m_nY, Rect.m_nW, Rect.m_nH, Rect.m_Color);

	for (const auto &OutlinedRect : m_vecOutlinedRects)
		H::Draw->OutlinedRect(OutlinedRect.m_nX, OutlinedRect.m_nY, OutlinedRect.m_nW, OutlinedRect.m_nH, OutlinedRect.m_Color);

	for (const auto &String : m_vecStrings)
	{
		std::optional<MenuLayout::ScopedClip<IMatSystemSurface>> textClip;
		if (String.m_Clip.w > 0 && String.m_Clip.h > 0) {
			textClip.emplace(I::MatSystemSurface, MenuLayout::Rect{ String.m_Clip.x, String.m_Clip.y, String.m_Clip.w, String.m_Clip.h });
		}

		H::Draw->String(String.m_dwFont, String.m_nX, String.m_nY, String.m_Color, String.m_nAlign, "%s", String.m_String.c_str());

	}
}
