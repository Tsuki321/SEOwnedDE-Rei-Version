#pragma once

#include <algorithm>

namespace MenuLayout
{
	struct Rect { int x = 0, y = 0, w = 0, h = 0; };

	inline Rect Intersect(const Rect& first, const Rect& second)
	{
		const int x = std::max(first.x, second.x);
		const int y = std::max(first.y, second.y);
		return { x, y, std::max(0, std::min(first.x + first.w, second.x + second.w) - x),
			std::max(0, std::min(first.y + first.h, second.y + second.h) - y) };
	}

	inline bool Contains(const Rect& rect, int x, int y)
	{
		return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
	}

	inline int WindowHeight(int minimumHeight, int contentHeight, int screenHeight, int margin)
	{
		const int available = std::max(1, screenHeight - std::max(0, margin) * 2);
		return std::clamp(std::max(minimumHeight, contentHeight), 1, available);
	}

	inline int ScrollLimit(int contentHeight, int viewportHeight)
	{
		return std::max(0, contentHeight - std::max(0, viewportHeight));
	}

	inline int ClampScroll(int offset, int contentHeight, int viewportHeight)
	{
		return std::clamp(offset, 0, ScrollLimit(contentHeight, viewportHeight));
	}

	inline int PopupTop(int preferredTop, int popupHeight, const Rect& bounds)
	{
		return std::clamp(preferredTop, bounds.y, std::max(bounds.y, bounds.y + bounds.h - popupHeight));
	}

	// Surface clipping is shared with the game and other overlays. Every nested
	// region intersects the caller's region and restores it exactly on exit.
	template <typename Surface>
	class ScopedClip
	{
		Surface* m_Surface;
		int m_Left, m_Top, m_Right, m_Bottom;
		bool m_Disabled;

	public:
		ScopedClip(Surface* surface, const Rect& rect) : m_Surface(surface)
		{
			m_Surface->GetClippingRect(m_Left, m_Top, m_Right, m_Bottom, m_Disabled);
			const Rect bounds = m_Disabled ? rect : Intersect(rect,
				{ m_Left, m_Top, m_Right - m_Left, m_Bottom - m_Top });
			m_Surface->DisableClipping(false);
			m_Surface->SetClippingRect(bounds.x, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h);
		}
		ScopedClip(const ScopedClip&) = delete;
		ScopedClip& operator=(const ScopedClip&) = delete;
		~ScopedClip()
		{
			m_Surface->SetClippingRect(m_Left, m_Top, m_Right, m_Bottom);
			m_Surface->DisableClipping(m_Disabled);
		}
	};
}
