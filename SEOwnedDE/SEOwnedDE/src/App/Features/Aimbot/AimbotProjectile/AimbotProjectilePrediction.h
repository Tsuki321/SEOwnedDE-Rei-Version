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

	inline float ComputeSignedTemporalResidual(float simulatedTime, float travelTime, float timingBias)
	{
		return (ClampNonNegative(travelTime) + ClampNonNegative(timingBias)) - ClampNonNegative(simulatedTime);
	}

	inline float ResolveTemporalTolerance(float tickInterval, float requestedTolerance = 0.0f)
	{
		const float halfTick = 0.5f * ClampNonNegative(tickInterval);
		if (requestedTolerance <= 0.0f)
			return halfTick;

		return std::min(requestedTolerance, halfTick);
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
		float EndpointError = std::numeric_limits<float>::infinity();
		int   PositiveRootCount = 0;
		bool  DragConverged = true;
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

	struct QuarticRoots
	{
		std::array<double, 4> Values = {};
		int Count = 0;
	};

	using Polynomial = std::array<double, 5>;

	inline double EvaluatePolynomial(const Polynomial& coefficients, int degree, double x) noexcept
	{
		double value = coefficients[degree];
		for (int i = degree - 1; i >= 0; --i)
			value = value * x + coefficients[i];
		return value;
	}

	inline double PolynomialMagnitude(const Polynomial& coefficients, int degree, double x) noexcept
	{
		const double absX = std::fabs(x);
		double magnitude = std::fabs(coefficients[degree]);
		for (int i = degree - 1; i >= 0; --i)
			magnitude = magnitude * absX + std::fabs(coefficients[i]);
		return std::max(1.0, magnitude);
	}

	inline bool IsPolynomialZero(const Polynomial& coefficients, int degree, double x, double value) noexcept
	{
		return std::fabs(value) <= 1e-11 * PolynomialMagnitude(coefficients, degree, x);
	}

	inline void AddUniqueRoot(QuarticRoots& roots, double root) noexcept
	{
		if (!std::isfinite(root))
			return;

		for (int i = 0; i < roots.Count; ++i)
		{
			const double existing = roots.Values[i];
			if (std::fabs(existing - root) <= 1e-8 * std::max({ 1.0, std::fabs(existing), std::fabs(root) }))
				return;
		}

		if (roots.Count < static_cast<int>(roots.Values.size()))
			roots.Values[roots.Count++] = root;
	}

	inline double BisectPolynomialRoot(const Polynomial& coefficients, int degree,
	                                  double lo, double hi) noexcept
	{
		double fLo = EvaluatePolynomial(coefficients, degree, lo);
		const double fHi = EvaluatePolynomial(coefficients, degree, hi);

		if (IsPolynomialZero(coefficients, degree, lo, fLo))
			return lo;
		if (IsPolynomialZero(coefficients, degree, hi, fHi))
			return hi;

		for (int iteration = 0; iteration < 80; ++iteration)
		{
			const double mid = lo + (hi - lo) * 0.5;
			const double fMid = EvaluatePolynomial(coefficients, degree, mid);

			if (IsPolynomialZero(coefficients, degree, mid, fMid) ||
			    (hi - lo) <= 1e-10 * std::max(1.0, std::fabs(mid)))
			{
				return mid;
			}

			if (std::signbit(fLo) != std::signbit(fMid))
			{
				hi = mid;
			}
			else
			{
				lo = mid;
				fLo = fMid;
			}
		}

		return lo + (hi - lo) * 0.5;
	}

	// Recursively isolate roots between derivative roots. Each resulting interval is
	// monotonic, so bisection cannot jump from the low arc to the high arc.
	inline QuarticRoots FindPolynomialRootsInInterval(
		const Polynomial& coefficients, int degree, double minX, double maxX) noexcept
	{
		QuarticRoots roots;
		if (degree <= 0 || !std::isfinite(minX) || !std::isfinite(maxX) || maxX < minX)
			return roots;

		while (degree > 0 && std::fabs(coefficients[degree]) <= 1e-18)
			--degree;

		if (degree <= 0)
			return roots;

		if (degree == 1)
		{
			const double root = -coefficients[0] / coefficients[1];
			if (root >= minX && root <= maxX)
				AddUniqueRoot(roots, root);
			return roots;
		}

		Polynomial derivative = {};
		for (int i = 1; i <= degree; ++i)
			derivative[i - 1] = coefficients[i] * static_cast<double>(i);

		QuarticRoots criticalPoints = FindPolynomialRootsInInterval(
			derivative, degree - 1, minX, maxX);
		std::sort(criticalPoints.Values.begin(), criticalPoints.Values.begin() + criticalPoints.Count);

		std::array<double, 5> boundaries = {};
		int boundaryCount = 0;
		boundaries[boundaryCount++] = minX;
		for (int i = 0; i < criticalPoints.Count; ++i)
		{
			const double point = criticalPoints.Values[i];
			if (point > minX && point < maxX)
				boundaries[boundaryCount++] = point;
		}
		boundaries[boundaryCount++] = maxX;
		std::sort(boundaries.begin(), boundaries.begin() + boundaryCount);

		for (int i = 0; i < boundaryCount; ++i)
		{
			const double point = boundaries[i];
			const double value = EvaluatePolynomial(coefficients, degree, point);
			if (IsPolynomialZero(coefficients, degree, point, value))
				AddUniqueRoot(roots, point);
		}

		for (int i = 0; i + 1 < boundaryCount; ++i)
		{
			const double lo = boundaries[i];
			const double hi = boundaries[i + 1];
			const double fLo = EvaluatePolynomial(coefficients, degree, lo);
			const double fHi = EvaluatePolynomial(coefficients, degree, hi);

			if (std::signbit(fLo) != std::signbit(fHi))
				AddUniqueRoot(roots, BisectPolynomialRoot(coefficients, degree, lo, hi));
		}

		std::sort(roots.Values.begin(), roots.Values.begin() + roots.Count);
		return roots;
	}

	inline double ComputePolynomialRootBound(const Polynomial& coefficients, int degree) noexcept
	{
		const double leading = std::fabs(coefficients[degree]);
		if (leading <= 1e-18)
			return 0.0;

		double bound = 0.0;
		for (int i = 0; i < degree; ++i)
		{
			const double ratio = std::fabs(coefficients[i]) / leading;
			if (ratio > 0.0)
				bound = std::max(bound, std::pow(ratio, 1.0 / static_cast<double>(degree - i)));
		}

		// Fujiwara's bound is twice the largest scaled coefficient root.
		return std::max(0.001, 2.0 * bound + 0.001);
	}

	inline QuarticRoots FindPositiveQuarticRoots(double a4, double a3, double a2,
	                                            double a1, double a0,
	                                            double maxTime = 0.0) noexcept
	{
		QuarticRoots result;
		const Polynomial coefficients = { a0, a1, a2, a3, a4 };
		double searchLimit = ComputePolynomialRootBound(coefficients, 4);
		if (maxTime > 0.0)
			searchLimit = std::min(searchLimit, maxTime);

		if (!(searchLimit > 1e-6) || !std::isfinite(searchLimit))
			return result;

		const QuarticRoots roots = FindPolynomialRootsInInterval(
			coefficients, 4, 1e-6, searchLimit);

		for (int i = 0; i < roots.Count; ++i)
		{
			const double root = roots.Values[i];
			if (root <= 1e-6 || !std::isfinite(root))
				continue;
			if (result.Count >= static_cast<int>(result.Values.size()))
				break;
			result.Values[result.Count++] = root;
		}

		return result;
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
		float MaxTime       = 0.0f; // zero leaves the polynomial's positive-root bound unconstrained
	};

	inline SolveResult SolveBallistic(const SolverParams& p) noexcept
	{
		auto solveWithUp = [&](const Vec3& upVec) noexcept -> SolveResult
		{
			SolveResult result = {};

			const Vec3 D = p.TargetPos - p.ShootPos;
			const double s = static_cast<double>(p.Speed);
			if (s <= 0.0f)
				return result;

			const double g = static_cast<double>(p.Gravity);
			const Vec3 vEff = p.TargetVel - upVec * p.MuzzleUpZ;
			const double Vtx = static_cast<double>(vEff.x);
			const double Vty = static_cast<double>(vEff.y);
			const double Vtz = static_cast<double>(vEff.z);
			const double Dx = static_cast<double>(D.x);
			const double Dy = static_cast<double>(D.y);
			const double Dz = static_cast<double>(D.z);

			const double velocityLengthSq = Vtx * Vtx + Vty * Vty + Vtz * Vtz;
			const double distanceLengthSq = Dx * Dx + Dy * Dy + Dz * Dz;
			const double distanceDotVelocity = Dx * Vtx + Dy * Vty + Dz * Vtz;

			auto solveQuadraticRoots = [](double a2, double a1, double a0,
			                              std::array<double, 2>& roots) noexcept -> int
			{
				if (std::fabs(a2) <= 1e-14)
				{
					if (std::fabs(a1) <= 1e-14)
						return 0;
					const double root = -a0 / a1;
					if (root > 1e-6 && std::isfinite(root))
					{
						roots[0] = root;
						return 1;
					}
					return 0;
				}

				const double discriminant = a1 * a1 - 4.0 * a2 * a0;
				if (discriminant < 0.0)
					return 0;

				const double sqrtDiscriminant = std::sqrt(discriminant);
				const double q = -0.5 * (a1 + std::copysign(sqrtDiscriminant, a1));
				double candidates[2] = {};
				int candidateCount = 0;
				if (std::fabs(q) > 1e-14)
				{
					candidates[candidateCount++] = q / a2;
					candidates[candidateCount++] = a0 / q;
				}
				else
				{
					candidates[candidateCount++] = -a1 / (2.0 * a2);
				}

				int count = 0;
				for (int i = 0; i < candidateCount; ++i)
				{
					const double root = candidates[i];
					if (root <= 1e-6 || !std::isfinite(root))
						continue;
					if (count > 0 && std::fabs(roots[0] - root) <= 1e-8 * std::max(1.0, std::fabs(root)))
						continue;
					roots[count++] = root;
				}

				if (count == 2 && roots[1] < roots[0])
					std::swap(roots[0], roots[1]);
				return count;
			};

			auto solveTime = [&](double effectiveSpeed, double& time, int& rootCount) noexcept -> bool
			{
				const double speedSq = effectiveSpeed * effectiveSpeed;
				if (g > 1e-6)
				{
					const double a4 = 0.25 * g * g;
					const double a3 = g * Vtz;
					const double a2 = velocityLengthSq + g * Dz - speedSq;
					const double a1 = 2.0 * distanceDotVelocity;
					const double a0 = distanceLengthSq;
					const QuarticRoots roots = FindPositiveQuarticRoots(a4, a3, a2, a1, a0);
					rootCount = roots.Count;
					if (roots.Count == 0)
						return false;

					time = p.UseHighArc ? roots.Values[roots.Count - 1] : roots.Values[0];
					if (p.MaxTime > 0.0f && time > static_cast<double>(p.MaxTime))
						return false;
					return true;
				}

				std::array<double, 2> roots = {};
				rootCount = solveQuadraticRoots(
					velocityLengthSq - speedSq, 2.0 * distanceDotVelocity,
					distanceLengthSq, roots);
				if (rootCount == 0)
					return false;

				time = p.UseHighArc ? roots[rootCount - 1] : roots[0];
				if (p.MaxTime > 0.0f && time > static_cast<double>(p.MaxTime))
					return false;
				return true;
			};

			double effectiveSpeed = s;
			const int dragRefinements = p.DragCoeff > 0.0f ? std::clamp(p.DragIters, 0, 8) : 0;
			result.DragConverged = p.DragCoeff <= 0.0f;

			for (int dragIteration = 0; dragIteration <= dragRefinements; ++dragIteration)
			{
				double time = 0.0;
				int rootCount = 0;
				if (!solveTime(effectiveSpeed, time, rootCount) || !(time > 0.0) || !std::isfinite(time))
					return {};

				const double requiredX = Dx + Vtx * time;
				const double requiredY = Dy + Vty * time;
				const double requiredZ = Dz + Vtz * time + 0.5 * g * time * time;
				const double launchDistance = effectiveSpeed * time;
				if (!(launchDistance > 0.0) || !std::isfinite(launchDistance))
					return {};

				const double requiredLength = std::sqrt(
					requiredX * requiredX + requiredY * requiredY + requiredZ * requiredZ);
				if (!(requiredLength > 0.0) || !std::isfinite(requiredLength))
					return {};

				const Vec3 direction(
					static_cast<float>(requiredX / requiredLength),
					static_cast<float>(requiredY / requiredLength),
					static_cast<float>(requiredZ / requiredLength));

				const double errorX = static_cast<double>(direction.x) * launchDistance - requiredX;
				const double errorY = static_cast<double>(direction.y) * launchDistance - requiredY;
				const double errorZ = static_cast<double>(direction.z) * launchDistance - requiredZ;
				const double endpointError = std::sqrt(errorX * errorX + errorY * errorY + errorZ * errorZ);
				const double endpointTolerance = std::max(0.01, launchDistance * 2e-6);
				if (!std::isfinite(endpointError) || endpointError > endpointTolerance)
					return {};

				result.Direction = direction;
				result.Time = static_cast<float>(time);
				result.EndpointError = static_cast<float>(endpointError);
				result.PositiveRootCount = rootCount;
				result.Valid = p.DragCoeff <= 0.0f;

				if (p.DragCoeff <= 0.0f)
					break;

				const double kt = static_cast<double>(p.DragCoeff) * time;
				double nextEffectiveSpeed = s;
				if (kt > 0.0)
				{
					if (kt < 1e-4)
						nextEffectiveSpeed = s * (1.0 - kt * 0.5 + kt * kt / 6.0);
					else
						nextEffectiveSpeed = s * (-std::expm1(-kt) / kt);
				}

				// Convergence is measured as endpoint displacement. One thirty-second
				// of a world unit is below Source's useful positional resolution while
				// still rejecting materially unconverged fixed-point seeds.
				const double dragDisplacementError = std::fabs(nextEffectiveSpeed - effectiveSpeed) * time;
				const double dragConvergenceTolerance = std::max(1.0 / 32.0, launchDistance * 2e-6);
				if (dragDisplacementError <= dragConvergenceTolerance)
				{
					result.DragConverged = true;
					result.Valid = true;
					break;
				}

				if (dragIteration == dragRefinements)
					break;

				effectiveSpeed = nextEffectiveSpeed;
			}

			return result;
		};

		SolveResult result = solveWithUp(Vec3(0.0f, 0.0f, 1.0f));

		if (!p.UseViewUpMuzzle || std::fabs(p.MuzzleUpZ) <= 1e-6f || !result.Valid)
			return result;

		// The view-up impulse depends on the solved view angle. Iterate the small
		// angle/up-vector fixed point so the returned direction reconstructs using
		// its own view-up vector rather than the previous iteration's vector.
		for (int iteration = 0; iteration < 6; ++iteration)
		{
			const Vec3 previousDirection = result.Direction;
			const float horizontalLength = Simd::FastSqrt(
				previousDirection.x * previousDirection.x + previousDirection.y * previousDirection.y);
			if (horizontalLength < 1e-4f)
				return result;

			const Vec3 viewUp(
				-previousDirection.z * previousDirection.x / horizontalLength,
				-previousDirection.z * previousDirection.y / horizontalLength,
				horizontalLength);
			SolveResult refined = solveWithUp(viewUp);
			if (!refined.Valid)
				return {};

			result = refined;
			if ((result.Direction - previousDirection).LengthSqr() <= 1e-10f)
				break;
		}

		return result;
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

		const int maxTicks = std::max(0, static_cast<int>(ProjectilePredictionMath::ClampNonNegative(maxSimTime) / tickInterval));
		const int hi = std::min(static_cast<int>(path.size()) - 1, maxTicks);
		const float temporalTolerance = ProjectilePredictionMath::ResolveTemporalTolerance(tickInterval, tolerance);

		float bestResidual = std::numeric_limits<float>::max();
		int bestTick = -1;
		auto evaluateTick = [&](int tick, float* signedResidual = nullptr) noexcept -> bool
		{
			SolverParams params;
			params.ShootPos = shootPos;
			params.TargetPos = path[tick];
			params.Speed = speed;
			params.Gravity = gravity;
			params.MuzzleUpZ = muzzleUpZ;
			params.UseViewUpMuzzle = true;
			params.UseHighArc = useHighArc;
			params.DragCoeff = dragCoeff;
			params.DragIters = dragCoeff > 0.0f ? 3 : 0;

			const SolveResult solve = SolveBallistic(params);
			if (!solve.Valid)
				return false;

			const float residual = ProjectilePredictionMath::ComputeSignedTemporalResidual(
				tick * tickInterval, solve.Time, timingBias);
			const float absResidual = std::fabs(residual);
			if (absResidual < bestResidual)
			{
				bestResidual = absResidual;
				bestTick = tick;
			}
			if (signedResidual)
				*signedResidual = residual;
			return true;
		};

		int lo2 = 0;
		int hi2 = hi;

		while (hi2 - lo2 > 1)
		{
			const int mid = (lo2 + hi2) / 2;
			float residual = 0.0f;
			if (!evaluateTick(mid, &residual))
			{
				hi2 = mid;
				continue;
			}

			if (residual > 0.0f)
				lo2 = mid;
			else
				hi2 = mid;
		}

		evaluateTick(lo2);
		if (hi2 != lo2)
			evaluateTick(hi2);

		if (bestTick >= 0 && bestTick <= maxTicks && bestResidual <= temporalTolerance)
			return bestTick;

		return -1;
	}

	// Full-path scan for the meeting tick. Robust to non-monotonic residuals
	// (e.g. jumping/falling targets) where BinarySearchMeetingTick can miss the bracket.
	inline int ScanMeetingTick(const std::vector<Vec3>& path, float tickInterval,
	                           const Vec3& shootPos, float speed, float gravity,
	                           float muzzleUpZ, float dragCoeff,
	                           float maxSimTime, bool useHighArc,
	                           float timingBias = 0.0f,
	                           float tolerance = 0.0f) noexcept
	{
		if (path.empty() || tickInterval <= 0.0f)
			return -1;

		const int maxTicks = std::min(
			static_cast<int>(path.size()) - 1,
			std::max(0, static_cast<int>(ProjectilePredictionMath::ClampNonNegative(maxSimTime) / tickInterval)));
		const float temporalTolerance = ProjectilePredictionMath::ResolveTemporalTolerance(tickInterval, tolerance);

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
			params.DragIters = dragCoeff > 0.0f ? 3 : 0;

			SolveResult res = SolveBallistic(params);
			if (!res.Valid)
				continue;

			const float residual = ProjectilePredictionMath::ComputeTemporalResidual(
				tick * tickInterval, res.Time, timingBias);

			if (residual < bestResidual) // strict less-than: ties prefer the smaller tick
			{
				bestResidual = residual;
				bestTick = tick;
			}
		}

		return bestResidual <= temporalTolerance ? bestTick : -1;
	}

	struct NewtonRefineResult
	{
		Vec3  AimPoint = {};
		float Time     = 0.0f;
		float SimulatedTime = 0.0f;
		float RequiredTime = 0.0f;
		float PathHorizon = 0.0f;
		float TemporalResidual = std::numeric_limits<float>::infinity();
		float SignedTemporalResidual = std::numeric_limits<float>::infinity();
		int   Tick     = -1;
		bool  AtPathBoundary = false;
		bool  HorizonLimited = false;
		bool  Valid    = false;
	};

	inline NewtonRefineResult NewtonRefineOverPath(
		const std::vector<Vec3>& path, float tickInterval,
		int startTick, const Vec3& shootPos, float speed,
		float gravity, float muzzleUpZ, float dragCoeff,
		bool useHighArc, int maxIters = 3, float timingBias = 0.0f,
		float tolerance = 0.0f) noexcept
	{
		NewtonRefineResult result = {};

		if (path.empty() || startTick < 0 || startTick >= static_cast<int>(path.size()))
			return result;

		const float pathHorizon = static_cast<float>(path.size() - 1) * tickInterval;
		const float temporalTolerance = ProjectilePredictionMath::ResolveTemporalTolerance(tickInterval, tolerance);
		const float clampedTimingBias = ProjectilePredictionMath::ClampNonNegative(timingBias);
		result.PathHorizon = pathHorizon;

		struct PathSample
		{
			Vec3 AimPoint = {};
			float SimulatedTime = 0.0f;
			float TravelTime = 0.0f;
			float SignedResidual = std::numeric_limits<float>::infinity();
			bool Valid = false;
		};

		auto evaluateTime = [&](float requestedTime) noexcept -> PathSample
		{
			PathSample sample;
			sample.SimulatedTime = std::clamp(requestedTime, 0.0f, pathHorizon);

			const float pathIndex = sample.SimulatedTime / tickInterval;
			const int lowerTick = std::clamp(
				static_cast<int>(std::floor(pathIndex)), 0, static_cast<int>(path.size()) - 1);
			const int upperTick = std::min(lowerTick + 1, static_cast<int>(path.size()) - 1);
			const float fraction = std::clamp(pathIndex - static_cast<float>(lowerTick), 0.0f, 1.0f);
			sample.AimPoint = path[lowerTick] + (path[upperTick] - path[lowerTick]) * fraction;

			SolverParams params;
			params.ShootPos = shootPos;
			params.TargetPos = sample.AimPoint;
			params.Speed = speed;
			params.Gravity = gravity;
			params.MuzzleUpZ = muzzleUpZ;
			params.UseViewUpMuzzle = true;
			params.UseHighArc = useHighArc;
			params.DragCoeff = dragCoeff;
			params.DragIters = 3;

			const SolveResult solve = SolveBallistic(params);
			if (!solve.Valid)
				return sample;

			sample.TravelTime = solve.Time;
			sample.SignedResidual = ProjectilePredictionMath::ComputeSignedTemporalResidual(
				sample.SimulatedTime, solve.Time, clampedTimingBias);
			sample.Valid = true;
			return sample;
		};

		PathSample bestSample;
		bool found = false;
		auto considerSample = [&](const PathSample& sample) noexcept
		{
			if (!sample.Valid)
				return;

			if (!found || std::fabs(sample.SignedResidual) < std::fabs(bestSample.SignedResidual))
			{
				bestSample = sample;
				found = true;
			}
		};

		auto evaluateTick = [&](int tick) noexcept -> PathSample
		{
			const PathSample sample = evaluateTime(static_cast<float>(tick) * tickInterval);
			considerSample(sample);
			return sample;
		};

		PathSample bracketLeft;
		PathSample bracketRight;
		bool hasBracket = false;
		float bestBracketScore = std::numeric_limits<float>::infinity();
		auto considerBracket = [&](const PathSample& left, const PathSample& right) noexcept
		{
			if (!left.Valid || !right.Valid)
				return;
			if (std::signbit(left.SignedResidual) == std::signbit(right.SignedResidual) &&
			    left.SignedResidual != 0.0f && right.SignedResidual != 0.0f)
			{
				return;
			}

			const float score = std::min(std::fabs(left.SignedResidual), std::fabs(right.SignedResidual));
			if (score < bestBracketScore)
			{
				bestBracketScore = score;
				bracketLeft = left;
				bracketRight = right;
				hasBracket = true;
			}
		};

		int tick = startTick;
		for (int iteration = 0; iteration < std::clamp(maxIters, 1, 16); ++iteration)
		{
			const PathSample sample = evaluateTick(tick);
			if (!sample.Valid || std::fabs(sample.SignedResidual) <= temporalTolerance)
				break;

			int tickDelta = static_cast<int>(std::round(sample.SignedResidual / tickInterval));
			if (tickDelta == 0)
				tickDelta = sample.SignedResidual > 0.0f ? 1 : -1;

			const int nextTick = std::clamp(
				tick + tickDelta, 0, static_cast<int>(path.size()) - 1);
			if (nextTick == tick)
				break;
			tick = nextTick;
		}

		if (found)
		{
			const int nearestTick = std::clamp(
				static_cast<int>(std::round(bestSample.SimulatedTime / tickInterval)),
				0, static_cast<int>(path.size()) - 1);
			const int firstTick = std::max(0, nearestTick - 1);
			const int lastTick = std::min(static_cast<int>(path.size()) - 1, nearestTick + 1);
			PathSample previous;
			bool havePrevious = false;
			for (int localTick = firstTick; localTick <= lastTick; ++localTick)
			{
				const PathSample current = evaluateTick(localTick);
				if (havePrevious)
					considerBracket(previous, current);
				previous = current;
				havePrevious = true;
			}
		}

		if ((!found || std::fabs(bestSample.SignedResidual) > temporalTolerance) && !hasBracket)
		{
			PathSample previous = evaluateTick(0);
			for (int pathTick = 1; pathTick < static_cast<int>(path.size()); ++pathTick)
			{
				const PathSample current = evaluateTick(pathTick);
				considerBracket(previous, current);
				previous = current;
			}
		}

		if (hasBracket)
		{
			PathSample left = bracketLeft;
			PathSample right = bracketRight;
			for (int iteration = 0; iteration < 32; ++iteration)
			{
				const PathSample middle = evaluateTime(
					left.SimulatedTime + (right.SimulatedTime - left.SimulatedTime) * 0.5f);
				if (!middle.Valid)
					break;

				considerSample(middle);
				if (std::fabs(middle.SignedResidual) <= std::min(1e-5f, temporalTolerance * 0.01f))
					break;

				if (std::signbit(left.SignedResidual) != std::signbit(middle.SignedResidual))
					right = middle;
				else
					left = middle;
			}
		}

		if (found)
		{
			result.AimPoint = bestSample.AimPoint;
			result.Time = bestSample.TravelTime;
			result.SimulatedTime = bestSample.SimulatedTime;
			result.RequiredTime = bestSample.TravelTime + clampedTimingBias;
			result.TemporalResidual = std::fabs(bestSample.SignedResidual);
			result.SignedTemporalResidual = bestSample.SignedResidual;
			result.Tick = std::clamp(
				static_cast<int>(std::round(bestSample.SimulatedTime / tickInterval)),
				0, static_cast<int>(path.size()) - 1);
			const float boundaryEpsilon = std::max(1e-5f, tickInterval * 0.001f);
			result.AtPathBoundary = bestSample.SimulatedTime <= boundaryEpsilon ||
				bestSample.SimulatedTime >= pathHorizon - boundaryEpsilon;
			result.HorizonLimited = bestSample.SimulatedTime >= pathHorizon - boundaryEpsilon &&
				bestSample.SignedResidual > boundaryEpsilon;
			result.Valid = result.TemporalResidual <= temporalTolerance && !result.HorizonLimited;
		}

		return result;
	}
}
