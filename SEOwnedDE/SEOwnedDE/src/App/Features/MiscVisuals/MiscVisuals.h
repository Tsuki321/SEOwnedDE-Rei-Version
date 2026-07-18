#pragma once

#include "../../../SDK/SDK.h"
#include <vector>

class CMiscVisuals
{
	struct ProjectileArcSegment_t
	{
		Vec3 Start = {};
		Vec3 End = {};
		Vec3 HitAngles = {};
		bool Hit = false;
	};

	std::vector<ProjectileArcSegment_t> m_vecProjectileArc = {};
	int m_nProjectileArcTick = -1;
	C_TFPlayer* m_pProjectileArcLocal = nullptr;
	C_TFWeaponBase* m_pProjectileArcWeapon = nullptr;
	Vec3 m_vProjectileArcOrigin = {};
	Vec3 m_vProjectileArcAngles = {};
	Vec3 m_vProjectileArcViewOffset = {};
	int m_nProjectileArcFlags = 0;
	int m_nProjectileArcTickBase = 0;
	int m_nProjectileArcItemDefinition = 0;
	float m_flProjectileArcChargeBeginTime = 0.0f;

public:
	void AimbotFOVCircle();
	void ViewModelSway();
	void DetailProps();
	void ShiftBar();

	void SniperLines();
	void ProjectileArc();

	void CustomFOV(CViewSetup* pSetup);
	void Thirdperson(CViewSetup* pSetup);
};

MAKE_SINGLETON_SCOPED(CMiscVisuals, MiscVisuals, F);
