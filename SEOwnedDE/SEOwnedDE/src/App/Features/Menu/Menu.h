#pragma once

#include "../../../SDK/SDK.h"
#include "../SkinChanger/SkinChanger.h"
#include <initializer_list>

class CMenu
{
private:
	bool m_bOpen = false;
	bool m_bMenuWindowHovered = false;
	int m_nCursorX = 0, m_nCursorY = 0;

	int m_nLastGroupBoxY = 0, m_nLastGroupBoxW = 0;
	int m_nLastButtonW = 0;

	bool m_bClickConsumed = false;
	void* m_pActiveControl = nullptr;
	std::unordered_map<std::string *, std::string> m_mapTempStrings = {};
	std::unordered_map<int *, std::string> m_mapTempNumbers = {};
	SkinChangerSettings m_SkinEditorSettings = {};
	int m_nSkinEditorItemDefinition = -1;

	bool IsControlActive(const void* pControl) const { return m_pActiveControl == pControl; }
	bool CanActivateControl(const void* pControl) const { return !m_pActiveControl || IsControlActive(pControl); }
	void SetControlActive(void* pControl, bool bActive)
	{
		if (bActive)
			m_pActiveControl = pControl;
		else if (IsControlActive(pControl))
			m_pActiveControl = nullptr;
	}

	//std::string m_strConfigPath = {};

	std::unique_ptr<Color_t[]> m_pGradient = nullptr;
	unsigned int m_nColorPickerTextureId = 0;

private:
	void Drag(int &x, int &y, int w, int h, int offset_y);
	bool IsHovered(int x, int y, int w, int h, void *pVar, bool bStrict = false);
	bool IsHoveredSimple(int x, int y, int w, int h);

	bool CheckBox(const char *szLabel, bool &bVar);
	bool SliderFloat(const char *szLabel, float &flVar, float flMin, float flMax, float flStep, const char *szFormat);
	bool SliderInt(const char *szLabel, int &nVar, int nMin, int nMax, int nStep);
	bool InputInt(const char *szLabel, int &nVar, int nMin, int nMax);
	bool InputKey(const char *szLabel, int &nKeyOut);
	bool Button(const char *szLabel, bool bActive = false, int nCustomWidth = 0);
	bool playerListButton(const wchar_t *label, int nCustomWidth, Color_t clr, bool center_txt);
	bool InputText(const char *szLabel, const char *szLabel2, std::string &strOutput);
	bool SelectSingle(const char *szLabel, int &nVar, std::initializer_list<std::pair<const char *, int>> vecSelects);
	bool SelectMulti(const char *szLabel, std::vector<std::pair<const char *, bool &>> &vecSelects);
	bool ColorPicker(const char *szLabel, Color_t &colVar);
	void GroupBoxStart(const char *szLabel, int nWidth);
	void GroupBoxEnd();
	void Label(const char *szText);

public:
	inline bool IsOpen() { return m_bOpen; }
	inline bool IsMenuWindowHovered() { return m_bMenuWindowHovered; }

	bool m_bWantTextInput = false;
	bool m_bInKeybind = false;

private:
	void MainWindow();
	void Snow();
	void Indicators();

public:
	void Run();
	CMenu();
};

MAKE_SINGLETON_SCOPED(CMenu, Menu, F);
