#pragma once

#include "../../../SDK/SDK.h"
#include <unordered_map>
#include <vector>

class CVisualUtils
{
private:
	struct FrameCacheEntry
	{
		int Frame = -1;
		const C_TFPlayer* Local = nullptr;
		bool OwnedByLocalValid = false;
		bool OwnedByLocal = false;
		bool ColorValid = false;
		Color_t Color = { 255, 255, 255, 255 };
		bool OnScreenValid = false;
		bool OnScreen = false;
	};

	int m_nCachedFrame = -1;
	const C_TFPlayer* m_pCachedLocal = nullptr;
	int m_nCachedScreenW = 0;
	int m_nCachedScreenH = 0;
	Vec3 m_vCachedLocalOrigin = {};
	// Per-frame cache for I::EngineClient->IsTakingScreenshot(). The engine
	// vfunc is called hundreds of times per frame from the per-draw hooks
	// (IVModelRender_DrawModelExecute, CBaseAnimating_DrawModel). The result
	// only changes between frames, so we frame-stamp and avoid the indirect
	// call after the first access in a new frame.
	int m_nScreenshotFrame = -1;
	bool m_bTakingScreenshot = false;
	std::unordered_map<const C_BaseEntity*, FrameCacheEntry> m_mapFrameCache = {};
	bool m_bEntityCandidatesPrepared = false;
	std::vector<C_TFPlayer*> m_vecPlayerCandidates = {};
	std::vector<C_BaseObject*> m_vecBuildingCandidates = {};
	std::vector<C_BaseEntity*> m_vecProjectileCandidates = {};
	bool m_bModelCandidatesPrepared = false;
	std::vector<C_TFPlayer*> m_vecModelPlayerCandidates = {};
	std::vector<C_BaseObject*> m_vecModelBuildingCandidates = {};
	std::vector<C_BaseEntity*> m_vecModelProjectileCandidates = {};

	void ResetFrameCacheIfNeeded(const C_TFPlayer* pLocal);
	FrameCacheEntry& GetFrameCacheEntry(const C_BaseEntity* pEntity, const C_TFPlayer* pLocal);
	bool IsOwnedByLocalCached(const C_TFPlayer* pLocal, const C_BaseEntity* pEntity);
	void BuildEntityCandidatesIfNeeded(C_TFPlayer* pLocal);
	void BuildModelCandidatesIfNeeded(C_TFPlayer* pLocal);

public:
	bool IsEntityOwnedBy(C_BaseEntity* pEntity, C_BaseEntity* pWho);
	bool ShouldRenderPlayer(
		C_TFPlayer* pLocal,
		C_TFPlayer* pPlayer,
		bool bIgnoreLocal,
		bool bIgnoreFriends,
		bool bIgnoreTeammates,
		bool bShowTeammateMedics,
		bool bIgnoreEnemies,
		bool bIgnoreInvisible = false
	);
	bool ShouldRenderBuilding(
		C_TFPlayer* pLocal,
		C_BaseObject* pBuilding,
		bool bIgnoreLocal,
		bool bIgnoreTeammates,
		bool bShowTeammateDispensers,
		bool bIgnoreEnemies
	);
	bool ShouldRenderProjectile(
		C_TFPlayer* pLocal,
		C_BaseEntity* pProjectile,
		bool bIgnoreLocal,
		bool bIgnoreEnemies,
		bool bIgnoreTeammates
	);

	const std::vector<C_TFPlayer*>& GetPlayerCandidates(C_TFPlayer* pLocal);
	const std::vector<C_BaseObject*>& GetBuildingCandidates(C_TFPlayer* pLocal);
	const std::vector<C_BaseEntity*>& GetProjectileCandidates(C_TFPlayer* pLocal);

	const std::vector<C_TFPlayer*>& GetModelPlayerCandidates(C_TFPlayer* pLocal);
	const std::vector<C_BaseObject*>& GetModelBuildingCandidates(C_TFPlayer* pLocal);
	const std::vector<C_BaseEntity*>& GetModelProjectileCandidates(C_TFPlayer* pLocal);

	Color_t GetAlphaColor(Color_t base, float alpha);
	Color_t GetEntityColor(C_TFPlayer* pLocal, C_BaseEntity* pEntity);
	Color_t GetHealthColor(int nHealth, int nMaxHealth);
	Color_t GetHealthColorAlt(int nHealth, int nMaxHealth);

	int CreateTextureFromArray(const unsigned char* rgba, int w, int h);
	int CreateTextureFromVTF(const char* name);

	int GetClassIcon(int nClassNum);
	int GetBuildingTextureId(C_BaseObject* pObject);
	int GetHealthIconTextureId();
	int GetAmmoIconTextureId();
	int GetHalloweenGiftTextureId();

	bool IsOnScreen(const C_TFPlayer* pLocal, const C_BaseEntity* pEntity);
	bool IsOnScreenNoEntity(const C_TFPlayer* pLocal, const Vec3& vAbsOrigin);
	// Frame-cached wrapper around I::EngineClient->IsTakingScreenshot().
	// Direct vfunc in the per-draw hooks is wasteful (called per model per
	// frame) - this refreshes once per frame and returns the cached value.
	bool IsTakingScreenshotCached();

	int GetCat(int nFrame);
	int GetCat2(int nFrame);
	int GetCatSleep(int nFrame);
	int GetCatRun(int nFrame);

	Color_t Rainbow();
	Color_t RainbowTickOffset(int nTick);
};

MAKE_SINGLETON_SCOPED(CVisualUtils, VisualUtils, F)
