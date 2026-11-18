#pragma once

#include "../../TF2/IMatSystemSurface.h"

class CInput
{
private:
	enum EKeyState { NONE, PRESSED, HELD };
	EKeyState m_Keys[256] = {};
	int m_nMouseX = 0, m_nMouseY = 0;
	bool m_bGameFocused = false;

public:
	inline bool IsPressed(int key) { return key >= 0 && key < 256 && m_Keys[key] == PRESSED; }
	inline bool IsHeld(int key) { return key >= 0 && key < 256 && m_Keys[key] == HELD; }
	inline bool IsDown(int key) { return IsPressed(key) || IsHeld(key); }
	inline int GetMouseX() { return m_nMouseX; }
	inline int GetMouseY() { return m_nMouseY; }
	inline bool IsGameFocused() { return m_bGameFocused; }

public:
	bool IsPressedAndHeld(int key);

public:
	void Update();
};

MAKE_SINGLETON_SCOPED(CInput, Input, H);
