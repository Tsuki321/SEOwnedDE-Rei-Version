#pragma once

#include <atomic>

namespace RenderPassState
{
	inline thread_local bool g_bDrawingMainWorld = false;
	inline thread_local bool g_bCurrentRenderViewHasMainWorld = false;
	inline std::atomic<int> g_nMainWorldModelPassFrame{ -1 };
	inline std::atomic<int> g_nCompletedMainWorldModelPassFrame{ -1 };
	inline std::atomic<int> g_nCompositePassFrame{ -1 };

	inline bool TryClaimFrame(std::atomic<int>& claimedFrame, int frame)
	{
		if (frame < 0)
			return false;

		int observedFrame = claimedFrame.load(std::memory_order_acquire);
		while (observedFrame != frame)
		{
			if (claimedFrame.compare_exchange_weak(
				observedFrame,
				frame,
				std::memory_order_acq_rel,
				std::memory_order_acquire))
			{
				return true;
			}
		}

		return false;
	}

	inline bool TryBeginMainWorldModelPass(int frame)
	{
		return TryClaimFrame(g_nMainWorldModelPassFrame, frame);
	}

	inline void CompleteMainWorldModelPass(int frame)
	{
		g_nCompletedMainWorldModelPassFrame.store(frame, std::memory_order_release);
	}

	inline void ReleaseMainWorldModelPass(int frame)
	{
		int expectedFrame = frame;
		g_nMainWorldModelPassFrame.compare_exchange_strong(
			expectedFrame,
			-1,
			std::memory_order_acq_rel,
			std::memory_order_acquire
		);
	}

	inline bool IsMainWorldModelPassComplete(int frame)
	{
		return frame >= 0
			&& g_nCompletedMainWorldModelPassFrame.load(std::memory_order_acquire) == frame;
	}

	inline bool TryBeginCompositePass(int frame)
	{
		return TryClaimFrame(g_nCompositePassFrame, frame);
	}

	inline void MarkCurrentRenderViewMainWorld()
	{
		g_bCurrentRenderViewHasMainWorld = true;
	}

	inline bool IsCurrentRenderViewMainWorld()
	{
		return g_bCurrentRenderViewHasMainWorld;
	}

	inline void ResetFrameGates()
	{
		g_nMainWorldModelPassFrame.store(-1, std::memory_order_release);
		g_nCompletedMainWorldModelPassFrame.store(-1, std::memory_order_release);
		g_nCompositePassFrame.store(-1, std::memory_order_release);
	}

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

	class CRenderViewScope
	{
		bool m_bPrevious = false;

	public:
		CRenderViewScope()
			: m_bPrevious(g_bCurrentRenderViewHasMainWorld)
		{
			g_bCurrentRenderViewHasMainWorld = false;
		}

		~CRenderViewScope()
		{
			g_bCurrentRenderViewHasMainWorld = m_bPrevious;
		}

		CRenderViewScope(const CRenderViewScope&) = delete;
		CRenderViewScope& operator=(const CRenderViewScope&) = delete;
	};
}
