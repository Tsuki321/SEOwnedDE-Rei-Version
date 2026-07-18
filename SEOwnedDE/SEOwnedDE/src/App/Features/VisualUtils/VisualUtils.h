#pragma once

#include "../../../SDK/SDK.h"
#include <array>
#include <vector>

class CVisualUtils
{
private:
	struct FrameCacheEntry
	{
		int Frame = -1;
		const C_TFPlayer* Local = nullptr;
		const C_BaseEntity* Entity = nullptr;
		bool OwnedByLocalValid = false;
		bool OwnedByLocal = false;
		bool ColorValid = false;
		Color_t Color = { 255, 255, 255, 255 };
		bool OnScreenValid = false;
		bool OnScreen = false;
		bool FriendValid = false;
		bool IsFriend = false;
	};

	struct NameCacheEntry
	{
		int Frame = -1;
		const C_TFPlayer* Local = nullptr;
		const C_TFPlayer* Player = nullptr;
		bool Valid = false;
		std::wstring Name;
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
	std::array<FrameCacheEntry, MAX_EDICTS> m_arrFrameCache = {};
	std::array<NameCacheEntry, MAX_PLAYERS> m_arrNameCache = {};
	FrameCacheEntry m_FallbackFrameCache = {};
	NameCacheEntry m_FallbackNameCache = {};
	bool m_bPlayerCandidatesPrepared = false;
	bool m_bBuildingCandidatesPrepared = false;
	bool m_bProjectileCandidatesPrepared = false;
	std::vector<C_TFPlayer*> m_vecPlayerCandidates = {};
	std::vector<C_BaseObject*> m_vecBuildingCandidates = {};
	std::vector<C_BaseEntity*> m_vecProjectileCandidates = {};
	bool m_bModelPlayerCandidatesPrepared = false;
	bool m_bModelBuildingCandidatesPrepared = false;
	bool m_bModelProjectileCandidatesPrepared = false;
	std::vector<C_TFPlayer*> m_vecModelPlayerCandidates = {};
	std::vector<C_BaseObject*> m_vecModelBuildingCandidates = {};
	std::vector<C_BaseEntity*> m_vecModelProjectileCandidates = {};

	void ResetFrameCacheIfNeeded(const C_TFPlayer* pLocal);
	FrameCacheEntry& GetFrameCacheEntry(const C_BaseEntity* pEntity, const C_TFPlayer* pLocal);
	NameCacheEntry& GetNameCacheEntry(const C_TFPlayer* pPlayer, const C_TFPlayer* pLocal);
	bool IsOwnedByLocalCached(const C_TFPlayer* pLocal, const C_BaseEntity* pEntity);
	bool GetCachedIsFriend(const C_TFPlayer* pLocal, C_TFPlayer* pPlayer);
	void BuildPlayerCandidatesIfNeeded(C_TFPlayer* pLocal);
	void BuildBuildingCandidatesIfNeeded(C_TFPlayer* pLocal);
	void BuildProjectileCandidatesIfNeeded(C_TFPlayer* pLocal);
	void BuildModelPlayerCandidatesIfNeeded(C_TFPlayer* pLocal);
	void BuildModelBuildingCandidatesIfNeeded(C_TFPlayer* pLocal);
	void BuildModelProjectileCandidatesIfNeeded(C_TFPlayer* pLocal);

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

	const std::wstring& GetCachedWideName(C_TFPlayer* pLocal, C_TFPlayer* pPlayer, const char* utf8Name = nullptr);

	int GetCat(int nFrame);
	int GetCat2(int nFrame);
	int GetCatSleep(int nFrame);
	int GetCatRun(int nFrame);

	Color_t Rainbow();
	Color_t RainbowTickOffset(int nTick);
};

MAKE_SINGLETON_SCOPED(CVisualUtils, VisualUtils, F)
