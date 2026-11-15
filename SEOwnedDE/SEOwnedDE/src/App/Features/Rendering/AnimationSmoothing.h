#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace AnimationSmoothing
{
	// Interpolate rotation separately from scale. Blending matrix coefficients
	// directly collapses limbs when the two poses face opposite directions.
	template <typename Matrix>
	bool Decompose(const Matrix& matrix, float (&q)[4], float (&scale)[3])
	{
		float r[3][3]{};
		for (int col = 0; col < 3; ++col)
		{
			scale[col] = std::sqrt(matrix[0][col] * matrix[0][col]
				+ matrix[1][col] * matrix[1][col] + matrix[2][col] * matrix[2][col]);
			if (!std::isfinite(scale[col]) || scale[col] < 0.0001f)
				return false;
			for (int row = 0; row < 3; ++row)
				r[row][col] = matrix[row][col] / scale[col];
		}
		const float trace = r[0][0] + r[1][1] + r[2][2];
		const float determinant = r[0][0] * (r[1][1]*r[2][2] - r[1][2]*r[2][1])
			- r[0][1] * (r[1][0]*r[2][2] - r[1][2]*r[2][0])
			+ r[0][2] * (r[1][0]*r[2][1] - r[1][1]*r[2][0]);
		if (determinant < 0.9f) return false; // Preserve mirrored/sheared engine transforms.
		if (trace > 0.0f)
		{
			const float s = std::sqrt(trace + 1.0f) * 2.0f;
			q[3] = s * 0.25f;
			q[0] = (r[2][1] - r[1][2]) / s;
			q[1] = (r[0][2] - r[2][0]) / s;
			q[2] = (r[1][0] - r[0][1]) / s;
		}
		else
		{
			int i = r[1][1] > r[0][0] ? 1 : 0;
			if (r[2][2] > r[i][i]) i = 2;
			const int j = (i + 1) % 3, k = (i + 2) % 3;
			const float s = std::sqrt(1.0f + r[i][i] - r[j][j] - r[k][k]) * 2.0f;
			if (!std::isfinite(s) || s < 0.0001f) return false;
			q[i] = s * 0.25f;
			q[j] = (r[j][i] + r[i][j]) / s;
			q[k] = (r[k][i] + r[i][k]) / s;
			q[3] = (r[k][j] - r[j][k]) / s;
		}
		return true;
	}

	template <typename Matrix>
	void BlendBone(const Matrix& from, const Matrix& to, float amount, Matrix& out)
	{
		std::memcpy(&out, &to, sizeof(Matrix));
		if (amount >= 1.0f) return;
		float a[4]{}, b[4]{}, sa[3]{}, sb[3]{};
		if (!Decompose(from, a, sa) || !Decompose(to, b, sb)) return;
		float dot = 0.0f;
		for (int i = 0; i < 4; ++i) dot += a[i] * b[i];
		float q[4]{}, length = 0.0f;
		for (int i = 0; i < 4; ++i)
		{
			q[i] = a[i] * (1.0f - amount) + b[i] * (dot < 0.0f ? -amount : amount);
			length += q[i] * q[i];
		}
		if (!std::isfinite(length) || length < 0.0001f) return;
		const float invLength = 1.0f / std::sqrt(length);
		for (float& value : q) value *= invLength;
		const float x = q[0], y = q[1], z = q[2], w = q[3];
		out[0][0] = 1 - 2 * (y*y + z*z); out[0][1] = 2 * (x*y - z*w); out[0][2] = 2 * (x*z + y*w);
		out[1][0] = 2 * (x*y + z*w); out[1][1] = 1 - 2 * (x*x + z*z); out[1][2] = 2 * (y*z - x*w);
		out[2][0] = 2 * (x*z - y*w); out[2][1] = 2 * (y*z + x*w); out[2][2] = 1 - 2 * (x*x + y*y);
		for (int row = 0; row < 3; ++row)
		{
			out[row][3] = from[row][3] + (to[row][3] - from[row][3]) * amount;
			for (int col = 0; col < 3; ++col)
				out[row][col] *= sa[col] + (sb[col] - sa[col]) * amount;
		}
	}

	template <typename Matrix, std::size_t MaxBones>
	class Pose
	{
		std::array<Matrix, MaxBones> m_From{}, m_Output{};
		std::array<bool, MaxBones> m_FromValid{}, m_OutputValid{};
		std::uint64_t m_Identity = 0;
		int m_Model = -1, m_Count = 0, m_Frame = -1;
		float m_SimulationTime = 0.0f, m_Start = 0.0f, m_Duration = 0.0f, m_LastTime = 0.0f;
		std::array<float, 3> m_Origin{};

	public:
		// The input belongs to the engine. Only these private render copies change;
		// hitboxes, lag records and historical chams retain the exact captured pose.
		Matrix* Update(const Matrix* bones, int count, std::uint64_t identity, int model,
			int frame, float now, float simulationTime, float tickInterval, const float* origin,
			const bool* validBones = nullptr)
		{
			if (!bones || !origin || count <= 0 || count > static_cast<int>(MaxBones)
				|| !std::isfinite(now) || !std::isfinite(simulationTime)
				|| !std::isfinite(tickInterval) || tickInterval <= 0.0f)
				return nullptr;
			float distanceSqr = 0.0f;
			for (int i = 0; i < 3; ++i)
			{
				if (!std::isfinite(origin[i])) return nullptr;
				const float delta = origin[i] - m_Origin[i];
				distanceSqr += delta * delta;
			}
			const float delta = simulationTime - m_SimulationTime;
			const bool reset = m_Count != count || m_Identity != identity || m_Model != model
				|| frame < m_Frame || now < m_LastTime || now - m_LastTime > 0.25f
				|| delta < 0.0f || delta > 0.25f || distanceSqr > 128.0f * 128.0f;
			if (reset)
			{
				std::memcpy(m_Output.data(), bones, sizeof(Matrix) * count);
				std::memcpy(m_From.data(), bones, sizeof(Matrix) * count);
				m_Duration = 0.0f;
				m_Start = now;
				m_FromValid.fill(false);
			}
			else if (delta > 0.0f)
			{
				std::memcpy(m_From.data(), m_Output.data(), sizeof(Matrix) * count);
				m_FromValid = m_OutputValid;
				m_Start = now;
				m_Duration = std::clamp(delta, std::min(tickInterval, 0.1f), 0.1f);
			}
			const float amount = m_Duration > 0.0f ? std::clamp((now - m_Start) / m_Duration, 0.0f, 1.0f) : 1.0f;
			for (int i = 0; i < count; ++i)
			{
				const bool valid = !validBones || validBones[i];
				if (valid && m_FromValid[i])
					BlendBone(m_From[i], bones[i], amount, m_Output[i]);
				else
					std::memcpy(&m_Output[i], &bones[i], sizeof(Matrix));
				m_OutputValid[i] = valid;
			}
			m_Count = count;
			m_Identity = identity;
			m_Model = model;
			m_Frame = frame;
			m_LastTime = now;
			m_SimulationTime = simulationTime;
			std::copy_n(origin, 3, m_Origin.begin());
			return m_Output.data();
		}
	};
}
