#pragma once

#include "../../../Utils/Math/Math.h"
#include <limits>
#include <vector>

namespace ProjectileTrajectory
{
	inline bool IsFinite(const Vec3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	struct Budget
	{
		int Trials = 160;
		int Steps = 12000;

		bool TakeTrial() { return Trials > 0 ? (--Trials, true) : false; }
		bool TakeStep() { return Steps > 0 ? (--Steps, true) : false; }
	};

	struct ChargeProfile
	{
		float Duration = 0.0f;
		float MinSpeed = 0.0f;
		float MaxSpeed = 0.0f;
		float MinGravity = 0.0f;
		float MaxGravity = 0.0f;

		void Evaluate(float charge, float& speed, float& gravity) const
		{
			const float fraction = Duration > 0.0f ? std::clamp(charge / Duration, 0.0f, 1.0f) : 0.0f;
			speed = MinSpeed + (MaxSpeed - MinSpeed) * fraction;
			gravity = MinGravity + (MaxGravity - MinGravity) * fraction;
		}
	};

	inline Vec3 SamplePath(const std::vector<Vec3>& path, float tickInterval, float time)
	{
		if (path.empty() || !(tickInterval > 0.0f) || !std::isfinite(time))
			return {};
		const float index = std::clamp(time / tickInterval, 0.0f, static_cast<float>(path.size() - 1));
		const auto lower = static_cast<std::size_t>(std::floor(index));
		const auto upper = std::min(lower + 1, path.size() - 1);
		return path[lower] + (path[upper] - path[lower]) * (index - static_cast<float>(lower));
	}

	// Continuous overlap for a projectile segment and translating target bounds.
	// Bounds transitions use the larger extent of the two poses, conservatively.
	inline bool SweepBounds(const Vec3& start, const Vec3& end,
		const Vec3& originStart, const Vec3& originEnd, const Vec3& minsStart, const Vec3& maxsStart,
		const Vec3& minsEnd, const Vec3& maxsEnd, const Vec3& projectileMins, const Vec3& projectileMaxs,
		float& fraction)
	{
		if (!IsFinite(start) || !IsFinite(end) || !IsFinite(originStart) || !IsFinite(originEnd))
			return false;
		const Vec3 relativeStart = start - originStart;
		const Vec3 delta = (end - originEnd) - relativeStart;
		float enter = 0.0f, leave = 1.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			const float lower = std::min(minsStart[axis], minsEnd[axis]) - projectileMaxs[axis];
			const float upper = std::max(maxsStart[axis], maxsEnd[axis]) - projectileMins[axis];
			if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper)
				return false;
			if (std::fabs(delta[axis]) < 1e-7f)
			{
				if (relativeStart[axis] < lower || relativeStart[axis] > upper)
					return false;
				continue;
			}
			float a = (lower - relativeStart[axis]) / delta[axis];
			float b = (upper - relativeStart[axis]) / delta[axis];
			if (a > b) std::swap(a, b);
			enter = std::max(enter, a);
			leave = std::min(leave, b);
			if (enter > leave) return false;
		}
		fraction = enter;
		return true;
	}

	enum class Failure { None, InvalidInput, Simulation, Unreachable, IterationLimit };

	struct Result
	{
		Vec3 Angle = {};
		Vec3 Endpoint = {};
		Vec3 Target = {};
		float FlightTime = 0.0f;
		float TargetTime = 0.0f;
		float Error = std::numeric_limits<float>::infinity();
		int Evaluations = 0;
		Failure Reason = Failure::InvalidInput;
		bool Valid = false;
	};

	inline bool SolveLinearSystem(double matrix[3][4], Vec3& solution)
	{
		for (int column = 0; column < 3; ++column)
		{
			int pivot = column;
			for (int row = column + 1; row < 3; ++row)
				if (std::fabs(matrix[row][column]) > std::fabs(matrix[pivot][column])) pivot = row;
			if (!std::isfinite(matrix[pivot][column]) || std::fabs(matrix[pivot][column]) < 1e-8)
				return false;
			for (int entry = column; entry < 4; ++entry) std::swap(matrix[pivot][entry], matrix[column][entry]);
			const double scale = matrix[column][column];
			for (int entry = column; entry < 4; ++entry) matrix[column][entry] /= scale;
			for (int row = 0; row < 3; ++row)
			{
				if (row == column) continue;
				const double factor = matrix[row][column];
				for (int entry = column; entry < 4; ++entry) matrix[row][entry] -= factor * matrix[column][entry];
			}
		}
		solution = { static_cast<float>(matrix[0][3]), static_cast<float>(matrix[1][3]), static_cast<float>(matrix[2][3]) };
		return IsFinite(solution);
	}

	// The callback is the trajectory backend, including angle-dependent muzzle setup.
	// Time is an unknown alongside pitch/yaw, so every trial meets the same target clock.
	template <typename Simulate>
	Result Refine(const std::vector<Vec3>& path, float tickInterval, float timingBias,
		float maxFlightTime, Vec3 seedAngle, float seedTime, Simulate&& simulate,
		int maxIterations = 6, float tolerance = 0.5f)
	{
		Result best;
		if (path.size() < 2 || !(tickInterval > 0.0f) || !std::isfinite(tickInterval)
			|| !std::isfinite(timingBias) || timingBias < 0.0f || !IsFinite(seedAngle)
			|| !std::isfinite(seedTime) || !std::isfinite(maxFlightTime) || !std::isfinite(tolerance) || !(tolerance > 0.0f))
			return best;
		const float horizon = static_cast<float>(path.size() - 1) * tickInterval;
		const float minTime = std::min(tickInterval * 0.25f, maxFlightTime);
		const float maxTime = std::min(maxFlightTime, horizon - timingBias);
		if (!(minTime > 0.0f) || !(maxTime > minTime)) return best;
		Vec3 parameters{ seedAngle.x, seedAngle.y, std::clamp(seedTime, minTime, maxTime) };
		auto clamp = [&](Vec3 value)
		{
			value.x = std::clamp(value.x, -89.0f, 89.0f);
			value.y = Math::NormalizeAngle(value.y);
			value.z = std::clamp(value.z, minTime, maxTime);
			return value;
		};
		parameters = clamp(parameters);
		auto evaluate = [&](const Vec3& value, Vec3& residual)
		{
			Vec3 endpoint;
			++best.Evaluations;
			if (!simulate(Vec3{ value.x, value.y, 0.0f }, value.z, endpoint) || !IsFinite(endpoint))
			{
				best.Reason = Failure::Simulation;
				return false;
			}
			const Vec3 target = SamplePath(path, tickInterval, timingBias + value.z);
			residual = endpoint - target;
			const float error = residual.Length();
			if (!std::isfinite(error)) return false;
			if (error < best.Error)
			{
				best.Angle = { value.x, value.y, 0.0f };
				best.Endpoint = endpoint;
				best.Target = target;
				best.FlightTime = value.z;
				best.TargetTime = timingBias + value.z;
				best.Error = error;
				best.Valid = error <= tolerance;
				best.Reason = best.Valid ? Failure::None : Failure::Unreachable;
			}
			return true;
		};
		Vec3 residual;
		if (!evaluate(parameters, residual)) return best;
		for (int iteration = 0; iteration < maxIterations && !best.Valid; ++iteration)
		{
			double system[3][4] = {};
			const Vec3 increments{ 0.25f, 0.25f, tickInterval * 0.5f };
			for (int column = 0; column < 3; ++column)
			{
				Vec3 probe = parameters;
				probe[column] += increments[column];
				probe = clamp(probe);
				if (std::fabs(probe[column] - parameters[column]) < 1e-6f)
				{
					probe = parameters;
					probe[column] -= increments[column];
					probe = clamp(probe);
				}
				const float delta = column == 1 ? Math::NormalizeAngle(probe.y - parameters.y) : probe[column] - parameters[column];
				if (std::fabs(delta) < 1e-6f) return best;
				Vec3 sampled;
				if (!evaluate(probe, sampled)) return best;
				if (best.Valid) return best;
				for (int row = 0; row < 3; ++row) system[row][column] = (sampled[row] - residual[row]) / delta;
			}
			if (best.Valid) return best;
			for (int row = 0; row < 3; ++row) system[row][3] = -residual[row];
			Vec3 correction;
			if (!SolveLinearSystem(system, correction)) return best;
			correction.x = std::clamp(correction.x, -12.0f, 12.0f);
			correction.y = std::clamp(correction.y, -12.0f, 12.0f);
			correction.z = std::clamp(correction.z, -0.35f, 0.35f);
			bool improved = false;
			for (float damping : { 1.0f, 0.5f, 0.25f })
			{
				const Vec3 trial = clamp(parameters + correction * damping);
				Vec3 sampled;
				if (!evaluate(trial, sampled)) return best;
				if (best.Valid) return best;
				if (sampled.LengthSqr() < residual.LengthSqr())
				{
					parameters = trial;
					residual = sampled;
					improved = true;
					break;
				}
			}
			if (!improved) return best;
		}
		if (!best.Valid) best.Reason = Failure::IterationLimit;
		return best;
	}

	// Validation is deliberately part of selection; failure of one candidate does
	// not suppress independently generated charge, arc, or splash alternatives.
	template <typename Candidate, typename Validate>
	bool SelectViable(std::vector<Candidate>& candidates, Candidate& selected, Validate&& validate)
	{
		std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.Score < b.Score; });
		bool found = false;
		for (auto& candidate : candidates)
		{
			if (!validate(candidate)) continue;
			if (!found || candidate.Score < selected.Score)
			{
				selected = candidate;
				found = true;
			}
		}
		return found;
	}
}
