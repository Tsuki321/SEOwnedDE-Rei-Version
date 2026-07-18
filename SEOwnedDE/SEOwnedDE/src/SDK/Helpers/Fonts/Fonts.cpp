#include "Fonts.h"

void CFontManager::Reload()
{
	const auto configure = [](CFont &font, const char *name, int tall, int flags)
	{
		const auto handle = font.m_dwFont;
		font = { name, tall, flags, 0, handle };
	};

	configure(m_arrFonts[static_cast<size_t>(EFonts::Menu)], "Verdana", 12, FONTFLAG_ANTIALIAS);
	configure(m_arrFonts[static_cast<size_t>(EFonts::ESP)], "Verdana", 12, FONTFLAG_OUTLINE);
	configure(m_arrFonts[static_cast<size_t>(EFonts::ESP_CONDS)], "Small Fonts", 9, FONTFLAG_OUTLINE);
	configure(m_arrFonts[static_cast<size_t>(EFonts::ESP_SMALL)], "Small Fonts", 11, FONTFLAG_OUTLINE);

	for (auto &v : m_arrFonts)
	{
		if (!v.m_dwFont)
			v.m_dwFont = I::MatSystemSurface->CreateFont();

		I::MatSystemSurface->SetFontGlyphSet
		(
			v.m_dwFont,
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
