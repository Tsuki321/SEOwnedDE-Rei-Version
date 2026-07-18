#pragma once
#include "../../../../SDK/SDK.h"

struct AimTarget_t
{
	C_BaseEntity* Entity = nullptr;
    Vec3 Position = {};
    Vec3 AngleTo = {};
	float FOVTo = 0.0f;
	float DistanceTo = 0.0f;
};

class CAimbotCommon
{
private:
	static bool IsHigherPriority(const AimTarget_t& a, const AimTarget_t& b, int sortMode)
	{
		switch (sortMode)
		{
			case 0: return a.FOVTo < b.FOVTo;
			case 1: return a.DistanceTo < b.DistanceTo;
			default: return false;
		}
	}

public:
	template <typename T, typename = std::enable_if<std::is_base_of_v<AimTarget_t, T>>>
	void Sort(std::vector<T>& targets, int sortMode)
	{
		std::ranges::sort(targets, [sortMode](const AimTarget_t& a, const AimTarget_t& b)
		{
			return IsHigherPriority(a, b, sortMode);
		});
	}

	template <typename T, typename = std::enable_if<std::is_base_of_v<AimTarget_t, T>>>
	void SortFirst(std::vector<T>& targets, std::size_t count, int sortMode)
	{
		const auto middle = targets.begin() + std::min(count, targets.size());
		std::ranges::partial_sort(targets, middle, [sortMode](const AimTarget_t& a, const AimTarget_t& b)
		{
			return IsHigherPriority(a, b, sortMode);
		});
	}
};

MAKE_SINGLETON_SCOPED(CAimbotCommon, AimbotCommon, F);
