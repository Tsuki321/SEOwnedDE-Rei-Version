#include "Fonts.h"

void CFontManager::Reload()
{
	m_arrFonts[static_cast<size_t>(EFonts::Menu)] = { "Verdana", 12, FONTFLAG_ANTIALIAS, 0 };
	m_arrFonts[static_cast<size_t>(EFonts::ESP)] = { "Verdana", 12, FONTFLAG_OUTLINE, 0 };
	m_arrFonts[static_cast<size_t>(EFonts::ESP_CONDS)] = { "Small Fonts", 9, FONTFLAG_OUTLINE, 0 };
	m_arrFonts[static_cast<size_t>(EFonts::ESP_SMALL)] = { "Small Fonts", 11, FONTFLAG_OUTLINE, 0 };

	for (auto &v : m_arrFonts)
	{
		I::MatSystemSurface->SetFontGlyphSet
		(
			v.m_dwFont = I::MatSystemSurface->CreateFont(),
			v.m_szName,	//name
			v.m_nTall,	//tall
			v.m_nWeight,	//weight
			0,					//blur
			0,					//scanlines
			v.m_nFlags	//flags
		);
	}
}

const CFont &CFontManager::Get(EFonts eFont)
{
	return m_arrFonts[static_cast<size_t>(eFont)];
}