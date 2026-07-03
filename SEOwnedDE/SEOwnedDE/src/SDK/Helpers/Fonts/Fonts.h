#pragma once

#include "../../TF2/IMatSystemSurface.h"

#include <array>

enum class EFonts
{
	Menu,
	ESP, ESP_CONDS, ESP_SMALL,
	COUNT
};

class CFont
{
public:
	const char *m_szName;
	int m_nTall, m_nFlags, m_nWeight;
	DWORD m_dwFont;
};

class CFontManager
{
private:
	std::array<CFont, static_cast<size_t>(EFonts::COUNT)> m_arrFonts = {};

public:
	void Reload();
	const CFont &Get(EFonts eFont);
};

MAKE_SINGLETON_SCOPED(CFontManager, Fonts, H);