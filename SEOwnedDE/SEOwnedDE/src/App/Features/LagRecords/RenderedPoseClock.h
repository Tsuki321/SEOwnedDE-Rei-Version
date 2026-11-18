#pragma once

#include <cmath>

// CreateMove/prediction can advance curtime after the body was interpolated.
// A shot must use the pose on screen, not the newest network simulation time.
class RenderedPoseClock
{
	float m_PoseTime = 0.0f;
	bool m_Valid = false;

public:
	void Clear() { m_PoseTime = 0.0f; m_Valid = false; }
	void Capture(float renderTime, float lerp)
	{
		m_Valid = std::isfinite(renderTime) && std::isfinite(lerp)
			&& lerp >= 0.0f && renderTime > lerp;
		m_PoseTime = m_Valid ? renderTime - lerp : 0.0f;
	}
	float PoseTime() const { return m_PoseTime; }
	int CommandTick(float lerp, float interval, int fallback) const
	{
		if (!m_Valid || !std::isfinite(lerp) || lerp < 0.0f
			|| !std::isfinite(interval) || interval <= 0.0f)
			return fallback;
		return static_cast<int>(0.5f + (m_PoseTime + lerp) / interval);
	}
};
