#pragma once

namespace RenderPassState
{
	extern bool g_bDrawingMainWorld;

	class CMainWorldScope
	{
		bool m_bPrevious = false;

	public:
		explicit CMainWorldScope(bool bDrawingMainWorld)
			: m_bPrevious(g_bDrawingMainWorld)
		{
			g_bDrawingMainWorld = bDrawingMainWorld;
		}

		~CMainWorldScope()
		{
			g_bDrawingMainWorld = m_bPrevious;
		}

		CMainWorldScope(const CMainWorldScope&) = delete;
		CMainWorldScope& operator=(const CMainWorldScope&) = delete;
	};
}
