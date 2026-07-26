#pragma once

#include "../../../../Utils/Vector/Vector.h"
#include "../../../../Utils/Math/Math.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <vector>
#include <limits>

#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#define SEOWNED_HAS_SSE 1
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
#include <immintrin.h>
#define SEOWNED_HAS_AVX2 1
#endif
#endif

namespace ProjectilePredictionMath
{
	inline float ClampNonNegative(float value)
	{
		return value > 0.0f ? value : 0.0f;
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
}

namespace Simd
{
#if SEOWNED_HAS_SSE
	inline float FastRSqrt(float x) noexcept
	{
		__m128 v = _mm_set_ss(x);
		__m128 r = _mm_rsqrt_ss(v);
		float y = _mm_cvtss_f32(r);
		const float threeHalfs = 1.5f;
		const float x2 = x * 0.5f;
		y = y * (threeHalfs - x2 * y * y);
		return y;
	}

	inline float FastSqrt(float x) noexcept
	{
		return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));
	}

	inline float FastReciprocal(float x) noexcept
	{
		__m128 v = _mm_set_ss(x);
		__m128 r = _mm_rcp_ss(v);
		float y = _mm_cvtss_f32(r);
		y = y * (2.0f - x * y);
		return y;
	}

	inline float FastDot3(float ax, float ay, float az, float bx, float by, float bz) noexcept
	{
		__m128 a = _mm_set_ps(0.0f, az, ay, ax);
		__m128 b = _mm_set_ps(0.0f, bz, by, bx);
		__m128 mul = _mm_mul_ps(a, b);
		__m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(0, 1, 2, 3));
		__m128 sums = _mm_add_ps(mul, shuf);
		shuf = _mm_movehl_ps(sums, sums);
		sums = _mm_add_ss(sums, shuf);
		return _mm_cvtss_f32(sums);
	}
#else
	inline float FastRSqrt(float x) noexcept { return 1.0f / std::sqrt(x); }
	inline float FastSqrt(float x) noexcept { return std::sqrt(x); }
	inline float FastReciprocal(float x) noexcept { return 1.0f / x; }
	inline float FastDot3(float ax, float ay, float az, float bx, float by, float bz) noexcept
	{
		return ax * bx + ay * by + az * bz;
	}
#endif

	inline Vec3 NormalizedFast(const Vec3& v) noexcept
	{
		const float lenSq = v.LengthSqr();
		if (lenSq < 1e-12f)
			return Vec3(0.0f, 0.0f, 0.0f);
		const float invLen = FastRSqrt(lenSq);
		return Vec3(v.x * invLen, v.y * invLen, v.z * invLen);
	}
}

namespace BallisticSolver
{
	struct SolveResult
	{
		Vec3  Direction = {};
		float Time      = 0.0f;
		bool  Valid     = false;
	};

	struct DragBasis
	{
		float Linear   = 0.0f;
		float Angular  = 0.0f;
		float AirDensity = 2.0f;
	};

	enum class WeaponClass : int
	{
		GrenadeLauncher = 0,
		LochnLoad,
		Stickybomb,
		Cannonball,
		None
	};

	inline DragBasis GetDragBasis(WeaponClass cls) noexcept
	{
		switch (cls)
		{
			case WeaponClass::GrenadeLauncher:
				return { (0.003902f + 0.009962f + 0.009962f) / 3.0f,
				         (0.003618f + 0.001514f + 0.001514f) / 3.0f, 2.0f };
			case WeaponClass::Stickybomb:
				return { (0.007491f + 0.007491f + 0.007306f) / 3.0f,
				         (0.002777f + 0.002842f + 0.002812f) / 3.0f, 2.0f };
			case WeaponClass::Cannonball:
				return { (0.020971f + 0.019420f + 0.020971f) / 3.0f,
				         (0.012997f + 0.013496f + 0.013714f) / 3.0f, 2.0f };
			case WeaponClass::LochnLoad:
				return { (0.003902f + 0.009962f + 0.009962f) / 3.0f,
				         (0.003618f + 0.001514f + 0.001514f) / 3.0f, 2.0f };
			default:
				return {};
		}
	}

	inline float ComputeDragCoefficient(WeaponClass cls) noexcept
	{
		if (cls == WeaponClass::None)
			return 0.0f;
		const DragBasis basis = GetDragBasis(cls);
		return basis.Linear * basis.AirDensity;
	}

	inline float ExponentialDragEffectiveSpeed(float speed, float k, float t) noexcept
	{
		if (k <= 0.0f || t <= 0.0f)
			return speed;
		const float kt = k * t;
		if (kt < 0.01f)
			return speed * (1.0f - kt * 0.5f + kt * kt / 6.0f);
		return speed * (1.0f - std::exp(-kt)) / kt;
	}

	inline float EvaluateQuartic(float a4, float a3, float a2, float a1, float a0, float t) noexcept
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		const float t4 = t3 * t;
		return a4 * t4 + a3 * t3 + a2 * t2 + a1 * t + a0;
	}

	inline float EvaluateQuarticDerivative(float a4, float a3, float a2, float a1, float t) noexcept
	{
		const float t2 = t * t;
		return 4.0f * a4 * t2 * t + 3.0f * a3 * t2 + 2.0f * a2 * t + a1;
	}

	inline float SolveQuarticNewton(float a4, float a3, float a2, float a1, float a0,
	                                 float tInit, int maxIter = 8, float epsilon = 1e-5f) noexcept
	{
		float t = std::max(tInit, 0.001f);

		for (int i = 0; i < maxIter; ++i)
		{
			const float f  = EvaluateQuartic(a4, a3, a2, a1, a0, t);
			const float fp = EvaluateQuarticDerivative(a4, a3, a2, a1, t);

			if (std::fabs(fp) < 1e-10f)
				break;

			float tNew = t - f / fp;

			if (tNew < 0.0f)
				tNew = t * 0.5f;

			if (std::fabs(tNew - t) < epsilon)
			{
				t = tNew;
				break;
			}

			t = tNew;
		}

		return t;
	}

	inline float SolveQuadratic(float a2, float a1, float a0) noexcept
	{
		if (std::fabs(a2) < 1e-10f)
		{
			if (std::fabs(a1) < 1e-10f)
				return -1.0f;
			return -a0 / a1;
		}

		const float disc = a1 * a1 - 4.0f * a2 * a0;
		if (disc < 0.0f)
			return -1.0f;

		const float sq = Simd::FastSqrt(disc);
		const float inv2a = 0.5f / a2;

		float t1 = (-a1 + sq) * inv2a;
		float t2 = (-a1 - sq) * inv2a;

		if (t1 > 0.0f && t2 > 0.0f)
			return std::min(t1, t2);
		if (t1 > 0.0f)
			return t1;
		if (t2 > 0.0f)
			return t2;
		return -1.0f;
	}

	struct SolverParams
	{
		Vec3  ShootPos      = {};
		Vec3  TargetPos    = {};
		Vec3  TargetVel    = {};
		float Speed         = 0.0f;
		float Gravity       = 0.0f;
		float MuzzleUpZ     = 0.0f;
		bool  UseViewUpMuzzle = false; // model MuzzleUpZ along the view up-vector (game-accurate) instead of world +Z
		bool  UseHighArc    = false;
		float DragCoeff     = 0.0f;
		int   DragIters     = 3;
	};

	inline SolveResult SolveBallistic(const SolverParams& p) noexcept
	{
		// Core solve for a given muzzle "up" vector. The muzzle-up kick is folded
		// into the relative frame by subtracting it from the target velocity, so
		// up = (0,0,1) reproduces the classic world +Z approximation exactly.
		auto solveWithUp = [&](const Vec3& upVec) noexcept -> SolveResult
		{
			SolveResult result = {};

			const Vec3 D = p.TargetPos - p.ShootPos;
			const float s = p.Speed;
			if (s <= 0.0f)
				return result;

			const float g = p.Gravity;
			const Vec3 vEff = p.TargetVel - upVec * p.MuzzleUpZ;
			const float Vpz = vEff.z;

			const float Dhx = D.x, Dhy = D.y;
			const float Vtx = vEff.x, Vty = vEff.y;
			const float Dz = D.z;

			const float DhLenSq = Dhx * Dhx + Dhy * Dhy;
			const float VthLenSq = Vtx * Vtx + Vty * Vty;
			const float DLenSq = DhLenSq + Dz * Dz;
			const float DhDotVth = Dhx * Vtx + Dhy * Vty;
			const float DzVpz = Dz * Vpz;

			float speed = s;

			for (int dragIter = 0; dragIter <= p.DragIters; ++dragIter)
			{
				const float s2 = speed * speed;

				if (g > 1e-6f)
				{
					const float a4 = 0.25f * g * g;
					const float a3 = g * Vpz;
					const float a2 = VthLenSq + Vpz * Vpz + g * Dz - s2;
					const float a1 = 2.0f * (DhDotVth + DzVpz);
					const float a0 = DLenSq;

					float t;
					if (a4 < 1e-10f && a3 < 1e-10f)
					{
						t = SolveQuadratic(a2, a1, a0);
						if (t <= 0.0f)
							return result;
					}
					else
					{
						float tInit = Simd::FastSqrt(DLenSq) / std::max(speed, 1.0f);
						t = SolveQuarticNewton(a4, a3, a2, a1, a0, tInit);

						if (EvaluateQuartic(a4, a3, a2, a1, a0, t) > 1.0f)
						{
							return result;
						}
						if (t <= 0.0f)
							return result;
					}

					if (p.UseHighArc && g > 1e-6f)
					{
						float tLo = t;
						for (int i = 0; i < 30; ++i)
						{
							if (tLo <= 0.001f)
								break;

							const float tNext = SolveQuarticNewton(a4, a3, a2, a1, a0, tLo * 1.5f);
							if (tNext == tLo)
								break;

							tLo = tNext;
						}
						if (tLo > t)
							t = tLo;
					}

					const float invST = 1.0f / (speed * t);
					Vec3 dir(
						(D.x + Vtx * t) * invST,
						(D.y + Vty * t) * invST,
						(Dz + Vpz * t + 0.5f * g * t * t) * invST
					);

					result.Direction = dir;
					result.Time = t;
					result.Valid = true;

					if (p.DragCoeff > 0.0f && dragIter < p.DragIters)
					{
						speed = ExponentialDragEffectiveSpeed(s, p.DragCoeff, t);
					}
					else
					{
						break;
					}
				}
				else
				{
					const float a2 = VthLenSq + Vpz * Vpz - s2;
					const float a1 = 2.0f * (DhDotVth + DzVpz);
					const float a0 = DLenSq;

					float t = SolveQuadratic(a2, a1, a0);
					if (t <= 0.0f)
						return result;

					const float invST = 1.0f / (speed * t);
					Vec3 dir(
						(D.x + Vtx * t) * invST,
						(D.y + Vty * t) * invST,
						(Dz + Vpz * t) * invST
					);

					result.Direction = dir;
					result.Time = t;
					result.Valid = true;

					if (p.DragCoeff > 0.0f && dragIter < p.DragIters)
					{
						speed = ExponentialDragEffectiveSpeed(s, p.DragCoeff, t);
					}
					else
					{
						break;
					}
				}
			}

			return result;
		};

		SolveResult result = solveWithUp(Vec3(0.0f, 0.0f, 1.0f));

		if (!p.UseViewUpMuzzle || std::fabs(p.MuzzleUpZ) <= 1e-6f || !result.Valid)
			return result;

		// Refine once with the true view up-vector of the solved direction:
		// up = (-sinP * cosY, -sinP * sinY, cosP) for unit forward d.
		const Vec3& d = result.Direction;
		const float flHorizLen = Simd::FastSqrt(d.x * d.x + d.y * d.y);
		if (flHorizLen < 1e-4f)
			return result; // near-vertical shot, world +Z approximation is fine

		const Vec3 vViewUp(-d.z * d.x / flHorizLen, -d.z * d.y / flHorizLen, flHorizLen);
		return solveWithUp(vViewUp);
	}

	inline Vec3 DirectionToAngles(const Vec3& dir) noexcept
	{
		Vec3 angles;
		Math::VectorAngles(dir, angles);
		return angles;
	}

	inline int BinarySearchMeetingTick(const std::vector<Vec3>& path, float tickInterval,
	                                   const Vec3& shootPos, float speed, float gravity,
	                                   float muzzleUpZ, float dragCoeff,
	                                   float maxSimTime, bool useHighArc,
	                                   float tolerance = 0.0f, float timingBias = 0.0f) noexcept
	{
		if (path.empty() || tickInterval <= 0.0f)
			return -1;

		int lo = 0;
		int hi = static_cast<int>(path.size()) - 1;

		if (hi == 0)
			return 0;

		const int maxTicks = static_cast<int>(maxSimTime / tickInterval);
		hi = std::min(hi, maxTicks);

		float bestResidual = std::numeric_limits<float>::max();
		int bestTick = -1;

		int lo2 = 0;
		int hi2 = hi;

		while (hi2 - lo2 > 1)
		{
			int mid = (lo2 + hi2) / 2;

			SolverParams params;
			params.ShootPos = shootPos;
			params.TargetPos = path[mid];
			params.Speed = speed;
			params.Gravity = gravity;
			params.MuzzleUpZ = muzzleUpZ;
			params.UseViewUpMuzzle = true; // pipes always kick along the view up-vector; no-op when muzzleUpZ == 0
			params.UseHighArc = useHighArc;
			params.DragCoeff = dragCoeff;
			params.DragIters = 0;

			SolveResult res = SolveBallistic(params);

			if (!res.Valid)
			{
				hi2 = mid;
				continue;
			}

			const float simTime = mid * tickInterval;
			const float residual = (res.Time + timingBias) - simTime;

			if (std::fabs(residual) < std::fabs(bestResidual))
			{
				bestResidual = residual;
				bestTick = mid;
			}

			if (residual > 0.0f)
				lo2 = mid;
			else
				hi2 = mid;
		}

		{
			int mid = lo2;
			SolverParams params;
			params.ShootPos = shootPos;
			params.TargetPos = path[mid];
			params.Speed = speed;
			params.Gravity = gravity;
			params.MuzzleUpZ = muzzleUpZ;
			params.UseViewUpMuzzle = true; // pipes always kick along the view up-vector; no-op when muzzleUpZ == 0
			params.UseHighArc = useHighArc;
			params.DragCoeff = dragCoeff;
			params.DragIters = 0;

			SolveResult res = SolveBallistic(params);
			if (res.Valid)
			{
				const float simTime = mid * tickInterval;
				const float residual = std::fabs((res.Time + timingBias) - simTime);
				if (residual < std::fabs(bestResidual))
				{
					bestTick = mid;
				}
			}
		}

		if (bestTick >= 0 && bestTick <= maxTicks)
			return bestTick;

		return -1;
	}

	// Full-path scan for the meeting tick. Robust to non-monotonic residuals
	// (e.g. jumping/falling targets) where BinarySearchMeetingTick can miss the bracket.
	inline int ScanMeetingTick(const std::vector<Vec3>& path, float tickInterval,
	                           const Vec3& shootPos, float speed, float gravity,
	                           float muzzleUpZ, float dragCoeff,
	                           float maxSimTime, bool useHighArc,
	                           float timingBias = 0.0f) noexcept
	{
		if (path.empty() || tickInterval <= 0.0f)
			return -1;

		const int maxTicks = std::min(static_cast<int>(path.size()) - 1, static_cast<int>(maxSimTime / tickInterval));

		float bestResidual = std::numeric_limits<float>::max();
		int bestTick = -1;

		for (int tick = 0; tick <= maxTicks; ++tick)
		{
			SolverParams params;
			params.ShootPos = shootPos;
			params.TargetPos = path[tick];
			params.Speed = speed;
			params.Gravity = gravity;
			params.MuzzleUpZ = muzzleUpZ;
			params.UseViewUpMuzzle = true; // pipes always kick along the view up-vector; no-op when muzzleUpZ == 0
			params.UseHighArc = useHighArc;
			params.DragCoeff = dragCoeff;
			params.DragIters = 0;

			SolveResult res = SolveBallistic(params);
			if (!res.Valid)
				continue;

			const float simTime = tick * tickInterval;
			const float residual = std::fabs((res.Time + timingBias) - simTime);

			if (residual < bestResidual) // strict less-than: ties prefer the smaller tick
			{
				bestResidual = residual;
				bestTick = tick;
			}
		}

		return bestTick;
	}

	struct NewtonRefineResult
	{
		Vec3  AimPoint = {};
		float Time     = 0.0f;
		int   Tick     = -1;
		bool  Valid    = false;
	};

	inline NewtonRefineResult NewtonRefineOverPath(
		const std::vector<Vec3>& path, float tickInterval,
		int startTick, const Vec3& shootPos, float speed,
		float gravity, float muzzleUpZ, float dragCoeff,
		bool useHighArc, int maxIters = 3, float timingBias = 0.0f) noexcept
	{
		NewtonRefineResult result = {};

		if (path.empty() || startTick < 0 || startTick >= static_cast<int>(path.size()))
			return result;

		int tick = startTick;
		float bestResidual = std::numeric_limits<float>::max();
		Vec3 bestAimPoint = {};
		int bestTick = -1;
		bool found = false;

		for (int iter = 0; iter < maxIters; ++iter)
		{
			if (tick < 0 || tick >= static_cast<int>(path.size()))
				break;

			SolverParams params;
			params.ShootPos = shootPos;
			params.TargetPos = path[tick];
			params.Speed = speed;
			params.Gravity = gravity;
			params.MuzzleUpZ = muzzleUpZ;
			params.UseViewUpMuzzle = true; // pipes always kick along the view up-vector; no-op when muzzleUpZ == 0
			params.UseHighArc = useHighArc;
			params.DragCoeff = dragCoeff;
			params.DragIters = 3;

			SolveResult res = SolveBallistic(params);
			if (!res.Valid)
				break;

			const float simTime = tick * tickInterval;
			const float residual = (res.Time + timingBias) - simTime; // bias only shifts WHICH tick is chosen, result.Time stays raw travel time
			const float absResidual = std::fabs(residual);

			if (absResidual < bestResidual)
			{
				bestResidual = absResidual;
				bestAimPoint = path[tick];
				bestTick = tick;
				found = true;
			}

			if (absResidual < 0.5f * tickInterval)
				break;

			int tickDelta = static_cast<int>(std::round(residual / tickInterval));
			if (tickDelta == 0)
				break;

			tick += tickDelta;
			tick = std::max(0, std::min(tick, static_cast<int>(path.size()) - 1));
		}

		if (found)
		{
			result.AimPoint = bestAimPoint;
			result.Tick = bestTick;
			result.Valid = true;

			SolverParams params;
			params.ShootPos = shootPos;
			params.TargetPos = bestAimPoint;
			params.Speed = speed;
			params.Gravity = gravity;
			params.MuzzleUpZ = muzzleUpZ;
			params.UseViewUpMuzzle = true; // pipes always kick along the view up-vector; no-op when muzzleUpZ == 0
			params.UseHighArc = useHighArc;
			params.DragCoeff = dragCoeff;
			params.DragIters = 3;

			SolveResult res = SolveBallistic(params);
			if (res.Valid)
				result.Time = res.Time;
		}

		return result;
	}
}
