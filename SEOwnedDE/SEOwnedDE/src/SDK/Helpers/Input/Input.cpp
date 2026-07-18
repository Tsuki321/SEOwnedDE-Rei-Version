#include "Input.h"

#include "../../SDK.h"

bool CInput::IsPressedAndHeld(short key)
{
	auto Now = std::chrono::steady_clock::now();

	static std::chrono::time_point<std::chrono::steady_clock> KeyTimes[256] = { Now };

	if (m_Keys[key] == PRESSED) {
		KeyTimes[key] = Now;
		return true;
	}

	if (m_Keys[key] == HELD && std::chrono::duration_cast<std::chrono::milliseconds>(Now - KeyTimes[key]).count() > 400)
		return true;

	return false;
}

void CInput::Update()
{
	m_bGameFocused = SDKUtils::IsGameWindowInFocus();

	// GetKeyState performs a user32 query for every virtual key.  The complete
	// keyboard snapshot gives us the same per-thread state with one call.
	BYTE keyboardState[256];
	const bool hasKeyboardState = m_bGameFocused && GetKeyboardState(keyboardState);

	for (int n = 0; n < 256; n++)
	{
		if (!hasKeyboardState) {
			m_Keys[n] = NONE;
			continue;
		}

		if (keyboardState[n] & 0x80)
		{
			if (m_Keys[n] == PRESSED)
				m_Keys[n] = HELD;

			else if (m_Keys[n] != HELD)
				m_Keys[n] = PRESSED;
		}

		else m_Keys[n] = NONE;
	}

	I::MatSystemSurface->SurfaceGetCursorPos(m_nMouseX, m_nMouseY);
}
