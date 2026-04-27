#pragma once

#include <algorithm>
#include <cmath>

namespace ProjectilePredictionMath
{
	inline float ClampNonNegative(float value)
	{
		return value > 0.0f ? value : 0.0f;
	}

	inline float ResolveInterpolationAmount(float lerpAmount, float clientInterpAmount)
	{
		return std::max(ClampNonNegative(lerpAmount), ClampNonNegative(clientInterpAmount));
	}

	inline float ApplyStickyArmTime(float travelTime, float stickyArmTime)
	{
		const float safeTravelTime = ClampNonNegative(travelTime);
		const float safeArmTime = ClampNonNegative(stickyArmTime);
		return safeTravelTime < safeArmTime ? safeArmTime : safeTravelTime;
	}

	inline float ComputeTimingBias(float outgoingLatency, float interpolationAmount)
	{
		return ClampNonNegative(outgoingLatency) + ClampNonNegative(interpolationAmount);
	}

	inline float ComputeTemporalResidual(float simulatedTime, float travelTime, float timingBias)
	{
		return std::fabs((ClampNonNegative(travelTime) + ClampNonNegative(timingBias)) - ClampNonNegative(simulatedTime));
	}

	inline bool IsWithinTemporalTolerance(float residual, float tolerance)
	{
		return ClampNonNegative(residual) <= ClampNonNegative(tolerance);
	}

	inline float ComputeTemporalScore(float residual, float tolerance)
	{
		const float safeTolerance = std::max(ClampNonNegative(tolerance), 1e-6f);
		return ClampNonNegative(residual) / safeTolerance;
	}

	inline float CombineTemporalAndSpatialScore(float temporalScore, float spatialResidual, float spatialWeight)
	{
		const float safeTemporalScore = ClampNonNegative(temporalScore);
		const float safeSpatialResidual = ClampNonNegative(spatialResidual);
		const float safeSpatialWeight = ClampNonNegative(spatialWeight);
		return safeTemporalScore + (safeSpatialWeight * std::log1p(safeSpatialResidual));
	}

	inline float ComputeHybridBlendFactor(int tickCount, float decayRate, float confidence, float tickInterval = 0.015f)
	{
		float safeConfidence = std::clamp(confidence, 0.0f, 1.0f);
		float safeDecayRate = std::max(0.1f, decayRate);
		float safeTickInterval = std::max(0.0001f, tickInterval);

		float tickTime = static_cast<float>(tickCount) * safeTickInterval;
		float t = 1.0f - std::exp(-tickTime * safeDecayRate);

		float startAlpha = 0.85f * safeConfidence;
		float endAlpha = 0.15f;

		return startAlpha + t * (endAlpha - startAlpha);
	}

	inline float ComputeVelocityConfidence(float varianceSq, float maxSpeedSq)
	{
		float safeMaxSpeedSq = std::max(maxSpeedSq, 1.0f);
		float ratio = std::clamp(varianceSq / safeMaxSpeedSq, 0.0f, 0.8f);
		return 1.0f - ratio;
	}
}
