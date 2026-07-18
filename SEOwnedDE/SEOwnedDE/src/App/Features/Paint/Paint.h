#pragma once

#include "../../../SDK/SDK.h"
#include <array>
#include <memory>

class CPaint
{
	IMaterial *m_pMatGlowColor = nullptr, *m_pMatHaloAddToScreen = nullptr;
	ITexture *m_pRtFullFrame = nullptr, *m_pRenderBuffer0 = nullptr, *m_pRenderBuffer1 = nullptr;
	IMaterial *m_pMatBlurX = nullptr, *m_pMatBlurY = nullptr;
	IMaterialVar* m_pBloomAmount = nullptr;

	void Initialize();

	struct PaintRecord_t
	{
		Vec3 Position = {};
		float TimeAdded = 0.0f;
		int StartTick = 0;
	};

	static constexpr size_t MAX_PAINT_POINTS = 4096;
	static constexpr size_t MAX_PAINT_STROKES = 256;

	struct PaintStorage_t
	{
		std::array<PaintRecord_t, MAX_PAINT_POINTS> Points = {};
		std::array<Color_t, MAX_PAINT_POINTS> RainbowColors = {};
	};

	std::unique_ptr<PaintStorage_t> m_pStorage = nullptr;
	size_t m_nOldestPaintPoint = 0;
	size_t m_nPaintPointCount = 0;
	size_t m_nPaintStrokeCount = 0;
	size_t m_nRainbowColorCount = 0;
	int m_nRainbowFrame = -1;

	void ClearPoints(bool bReleaseStorage = false);
	void PopOldestPoint();
	void AddPoint(const Vec3& vPosition, float flTimeAdded, int nStartTick);
	const PaintRecord_t& GetPoint(size_t nOffset) const;
	void PrunePoints();
	void PrepareRainbowColors(size_t nColorCount);

public:
	void Run();
	void CleanUp();
};

MAKE_SINGLETON_SCOPED(CPaint, Paint, F);
