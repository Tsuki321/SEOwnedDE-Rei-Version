#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace HitscanAssist
{
	struct Candidate
	{
		int Handle = -1;
		float FOV = 0.0f;
		bool OnCrosshair = false;
		bool Valid = true;
	};

	// The current entity wins while it remains visible and eligible. Acquisition
	// uses the actual crosshair contact before angular proximity, never distance.
	inline int SelectCandidate(const Candidate* candidates, std::size_t count,
		int lockedHandle, float acquireFOV)
	{
		int best = -1;
		for (std::size_t n = 0; n < count; ++n)
		{
			const auto& candidate = candidates[n];
			if (!candidate.Valid || candidate.Handle == -1
				|| !std::isfinite(candidate.FOV) || candidate.FOV < 0.0f)
				continue;

			if (candidate.Handle == lockedHandle)
				return static_cast<int>(n);

			if (!candidate.OnCrosshair && candidate.FOV > acquireFOV)
				continue;

			if (best < 0 || (candidate.OnCrosshair && !candidates[best].OnCrosshair)
				|| (candidate.OnCrosshair == candidates[best].OnCrosshair
					&& candidate.FOV < candidates[best].FOV))
				best = static_cast<int>(n);
		}
		return best;
	}

	class TargetLock
	{
		int m_LockedHandle = -1;
		int m_PendingHandle = -1;
		float m_PendingTime = 0.0f;
		int m_LastCommand = -1;
		int m_WeaponHandle = -1;

	public:
		int GetHandle() const { return m_LockedHandle; }
		void Reset() { *this = {}; }

		bool BeginCommand(int command, int weaponHandle)
		{
			const bool continuous = command > 0 && m_LastCommand > 0
				&& command == m_LastCommand + 1 && weaponHandle == m_WeaponHandle;
			if (!continuous)
				Reset();
			m_LastCommand = command;
			m_WeaponHandle = weaponHandle;
			return continuous;
		}

		int Select(const Candidate* candidates, std::size_t count, float acquireFOV, float dt)
		{
			const int selected = SelectCandidate(candidates, count, m_LockedHandle, acquireFOV);
			if (selected >= 0 && candidates[selected].Handle == m_LockedHandle)
				return selected;

			m_LockedHandle = -1;
			if (selected < 0 || !std::isfinite(dt) || dt <= 0.0f)
			{
				m_PendingHandle = -1;
				m_PendingTime = 0.0f;
				return -1;
			}

			if (m_PendingHandle != candidates[selected].Handle)
			{
				m_PendingHandle = candidates[selected].Handle;
				m_PendingTime = 0.0f;
			}
			m_PendingTime += std::min(dt, 0.03f);
			// A candidate must persist through crossing animations and single-frame
			// visibility changes before it inherits the lock.
			if (m_PendingTime < 0.10f)
				return -1;

			m_LockedHandle = m_PendingHandle;
			m_PendingHandle = -1;
			m_PendingTime = 0.0f;
			return selected;
		}
	};

	struct AngularStep
	{
		float Pitch = 0.0f;
		float Yaw = 0.0f;
	};

	class Motion
	{
		AngularStep m_Velocity = {};

		static void Limit(AngularStep& value, float maximum)
		{
			const float length = std::hypot(value.Pitch, value.Yaw);
			if (length > maximum && length > 0.0f)
			{
				value.Pitch *= maximum / length;
				value.Yaw *= maximum / length;
			}
		}

	public:
		void Reset() { m_Velocity = {}; }

		AngularStep Step(AngularStep error, float dt, float strength, bool stabilization)
		{
			if (!std::isfinite(error.Pitch) || !std::isfinite(error.Yaw)
				|| !std::isfinite(dt) || dt <= 0.0f || dt > 0.03f
				|| !std::isfinite(strength) || strength <= 0.0f)
			{
				Reset();
				return {};
			}
			error.Pitch = std::remainder(error.Pitch, 360.0f);
			error.Yaw = std::remainder(error.Yaw, 360.0f);
			const float errorLength = std::hypot(error.Pitch, error.Yaw);
			if (errorLength <= 0.025f)
			{
				Reset();
				return {};
			}

			strength = std::clamp(strength, 1.0f, 40.0f);
			const float maximumSpeed = std::clamp(150.0f / strength, 3.0f, 18.0f);
			const float gain = 66.0f / strength;
			AngularStep desired = { error.Pitch * gain, error.Yaw * gain };
			Limit(desired, maximumSpeed);

			// Velocity filtering is time-based; the acceleration bound also applies
			// with filtering disabled, so that option never licenses a view jump.
			const float response = stabilization ? -std::expm1(-dt / 0.10f) : 1.0f;
			AngularStep change = { (desired.Pitch - m_Velocity.Pitch) * response,
				(desired.Yaw - m_Velocity.Yaw) * response };
			Limit(change, maximumSpeed * 6.0f * dt);
			m_Velocity.Pitch += change.Pitch;
			m_Velocity.Yaw += change.Yaw;
			Limit(m_Velocity, maximumSpeed);

			// Drop momentum on a reversal and clamp each axis to its remaining
			// error. This prevents a filtered diagonal step overshooting one axis.
			if (m_Velocity.Pitch * error.Pitch <= 0.0f)
				m_Velocity.Pitch = 0.0f;
			if (m_Velocity.Yaw * error.Yaw <= 0.0f)
				m_Velocity.Yaw = 0.0f;
			AngularStep step = { m_Velocity.Pitch * dt, m_Velocity.Yaw * dt };
			step.Pitch = std::clamp(step.Pitch, -std::fabs(error.Pitch), std::fabs(error.Pitch));
			step.Yaw = std::clamp(step.Yaw, -std::fabs(error.Yaw), std::fabs(error.Yaw));
			m_Velocity = { step.Pitch / dt, step.Yaw / dt };
			return step;
		}
	};
}
