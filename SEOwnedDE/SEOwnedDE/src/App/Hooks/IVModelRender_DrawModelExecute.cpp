#include "../../SDK/SDK.h"

#include "../Features/Materials/Materials.h"
#include "../Features/Outlines/Outlines.h"

#include "../Features/CFG.h"
#include "../Features/Rendering/RenderPassState.h"
#include "../Features/SpyCamera/SpyCamera.h"
#include "../Features/VisualUtils/VisualUtils.h"
#include "../Features/LagRecords/LagRecords.h"
#include "../Features/Rendering/AnimationSmoothing.h"

#include <unordered_map>

MAKE_SIGNATURE(CBaseAnimating_DrawModel, "client.dll", "4C 8B DC 49 89 5B ? 89 54 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 83 EC ? 48 8B 05 ? ? ? ? 48 8D 3D", 0x0);
MAKE_SIGNATURE(ViewmodelAttachment_DrawModel, "client.dll", "41 8B D5 FF 50 ? 8B 97", 0x6);

static matrix3x4_t* GetDisplayBones(const DrawModelState_t& state,
	const ModelRenderInfo_t& info, matrix3x4_t* bones)
{
	static std::unordered_map<int, AnimationSmoothing::Pose<matrix3x4_t, MAX_BONE_COUNT>> poses;
	if (!CFG::Misc_Accuracy_Improvements || !I::EngineClient || !I::EngineClient->IsInGame())
	{
		poses.clear();
		return bones;
	}
	if (!bones || !state.m_pStudioHdr || !I::GlobalVars || !I::ClientEntityList
		|| info.entity_index <= 0 || info.entity_index >= NUM_ENT_ENTRIES)
		return bones;

	const auto entity = I::ClientEntityList->GetClientEntity(info.entity_index);
	if (!entity) return bones;
	const auto base = entity->As<C_BaseEntity>();
	// Bone-merged wearables and weapons follow the same player clock. Do not
	// include projectiles merely because their owner is a player.
	auto parent = base;
	for (int depth = 0; parent && depth < 4 && parent->GetClassId() != ETFClassIds::CTFPlayer; ++depth)
		parent = parent->GetMoveParent();
	if (!parent || parent->GetClassId() != ETFClassIds::CTFPlayer)
		return bones;
	const auto player = parent->As<C_TFPlayer>();
	if (player == H::Entities->GetLocal() || player->deadflag() || player->IsDormant())
	{
		poses.erase(info.entity_index);
		return bones;
	}
	// Manual-backtrack chams must remain at the exact stored hitbox pose.
	if (F::LagRecordMatrixHelper->IsActiveFor(player))
		return bones;

	const int count = state.m_pStudioHdr->numbones;
	if (count <= 0 || count > MAX_BONE_COUNT) return bones;
	bool validBones[MAX_BONE_COUNT]{};
	const int mask = BONE_USED_BY_ANYTHING_AT_LOD(std::clamp(state.m_lod, 0, 7));
	for (int i = 0; i < count; ++i)
		validBones[i] = (state.m_pStudioHdr->pBone(i)->flags & mask) != 0;
	const auto identity = static_cast<std::uint64_t>(static_cast<std::uint32_t>(base->GetRefEHandle().ToInt()))
		| (static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->GetRefEHandle().ToInt())) << 32);
	const auto& origin = player->GetAbsOrigin();
	auto display = poses[info.entity_index].Update(bones, count, identity, base->m_nModelIndex(),
		I::GlobalVars->framecount, I::GlobalVars->curtime, player->m_flSimulationTime(),
		I::GlobalVars->interval_per_tick, &origin.x, validBones);
	return display ? display : bones;
}

static IMaterial* GetViewmodelMaterialByIndex(int nIndex)
{
	if (nIndex >= 1 && nIndex <= 5)
		F::Materials->EnsureInitialized();

	switch (nIndex)
	{
	case 0: return nullptr;
	case 1: return F::Materials->m_pFlat;
	case 2: return F::Materials->m_pShaded;
	case 3: return F::Materials->m_pGlossy;
	case 4: return F::Materials->m_pGlow;
	case 5: return F::Materials->m_pPlastic;
	default: return nullptr;
	}
}

MAKE_HOOK(IVModelRender_DrawModelExecute, Memory::GetVFunc(I::ModelRender, 19), void, __fastcall,
	IVModelRender* ecx, const DrawModelState_t& state, ModelRenderInfo_t& pInfo, matrix3x4_t* pCustomBoneToWorld)
{
	// IsTakingScreenshotCached() frame-stamps the engine vfunc; the result
	// only changes once per frame at most, so most draws hit the cached bool.
	const bool bSpyCamRendering = F::SpyCamera->IsRendering();
	const bool bTakingScreenshot = F::VisualUtils->IsTakingScreenshotCached();
	const bool bCleanScreenshot = CFG::Misc_Clean_Screenshot && bTakingScreenshot;

	if (!bSpyCamRendering)
	{
		const auto pClientEntity = I::ClientEntityList->GetClientEntity(pInfo.entity_index);

		if (pClientEntity)
		{
			const auto nClassId = pClientEntity->GetClassId();

			if (CFG::Materials_ViewModel_Active && !bCleanScreenshot && nClassId == ETFClassIds::CTFViewModel)
			{
				if (const auto pLocal = H::Entities->GetLocal())
				{
					if (!pLocal->IsUbered() && !pLocal->deadflag())
					{
						const auto mat = GetViewmodelMaterialByIndex(CFG::Materials_ViewModel_Hands_Material);

						if (mat)
						{
							I::ModelRender->ForcedMaterialOverride(mat);
						}

						if (CFG::Materials_ViewModel_Hands_Alpha < 1.0f)
						{
							I::RenderView->SetBlend(CFG::Materials_ViewModel_Hands_Alpha);
						}

						if (mat)
						{
							const auto& base = CFG::Color_Hands;

							if (mat != F::Materials->m_pGlow)
							{
								I::RenderView->SetColorModulation(base);
							}

							else
							{
								if (F::Materials->m_pGlowEnvmapTint && F::Materials->m_pGlowSelfillumTint)
								{
									const auto& sheen = CFG::Color_Hands_Sheen;

									I::RenderView->SetColorModulation(1.0f, 1.0f, 1.0f);

									F::Materials->m_pGlowSelfillumTint
									            ->SetVecValue(ColorUtils::ToFloat(base.r), ColorUtils::ToFloat(base.g), ColorUtils::ToFloat(base.b));

									F::Materials->m_pGlowEnvmapTint
									            ->SetVecValue(ColorUtils::ToFloat(sheen.r), ColorUtils::ToFloat(sheen.g), ColorUtils::ToFloat(sheen.b));
								}
							}
						}

						CALL_ORIGINAL(ecx, state, pInfo, pCustomBoneToWorld);

						I::RenderView->SetColorModulation(1.0f, 1.0f, 1.0f);

						if (CFG::Materials_ViewModel_Hands_Alpha < 1.0f)
						{
							I::RenderView->SetBlend(1.0f);
						}

						if (mat)
						{
							I::ModelRender->ForcedMaterialOverride(nullptr);
						}

						return;
					}
				}
			}

			if (CFG::Visuals_Disable_Wearables && nClassId == ETFClassIds::CTFWearable)
				return;

			if (nClassId == ETFClassIds::CDynamicProp)
			{
				if (CFG::Visuals_World_Modulation_Mode == 0)
				{
					if (const auto flNightMode = CFG::Visuals_Night_Mode)
					{
						const auto col = static_cast<byte>(Math::RemapValClamped(flNightMode, 0.0f, 100.0f, 255.0f, 50.0f));

						I::RenderView->SetColorModulation({ col, col, col, static_cast<byte>(255) });
					}
				}

				else
				{
					I::RenderView->SetColorModulation(CFG::Color_Props);
				}

				CALL_ORIGINAL(ecx, state, pInfo, pCustomBoneToWorld);

				I::RenderView->SetColorModulation({ 255, 255, 255, 255 });

				return;
			}

			if (!bTakingScreenshot && RenderPassState::g_bDrawingMainWorld)
			{
				// Skip the two generation-array lookups entirely when neither feature drew
				// anything this frame (the common case with Materials/Outlines off).
				if (F::Materials->HasAnyDrawn() || F::Outlines->HasAnyDrawn())
				{
					const auto pEntity = pClientEntity->As<C_BaseEntity>();

					if (!F::Materials->IsRendering() && !F::Outlines->IsRendering() && (F::Outlines->HasDrawn(pEntity) || F::Materials->HasDrawn(pEntity)))
						return;
				}
			}
		}
	}

	if (CFG::Visuals_Simple_Models)
		*const_cast<int*>(&state.m_lod) = 5;

	pCustomBoneToWorld = GetDisplayBones(state, pInfo, pCustomBoneToWorld);
	CALL_ORIGINAL(ecx, state, pInfo, pCustomBoneToWorld);
}

MAKE_HOOK(CBaseAnimating_DrawModel, Signatures::CBaseAnimating_DrawModel.Get(), int, __fastcall,
	void *ecx, int flags)
{
	// Frame-cached screenshot state (see IVModelRender_DrawModelExecute above).
	const bool clean_ss = CFG::Misc_Clean_Screenshot && F::VisualUtils->IsTakingScreenshotCached();

	if (CFG::Materials_ViewModel_Active
		&& !clean_ss
		&& (flags & STUDIO_RENDER)
		&& reinterpret_cast<std::uintptr_t>(_ReturnAddress()) == Signatures::ViewmodelAttachment_DrawModel.Get())
	{
		if (const auto pLocal = H::Entities->GetLocal())
		{
			if (!pLocal->IsUbered() && !pLocal->deadflag())
			{
				const auto mat = GetViewmodelMaterialByIndex(CFG::Materials_ViewModel_Weapon_Material);

				if (mat)
				{
					I::ModelRender->ForcedMaterialOverride(mat);
				}

				if (CFG::Materials_ViewModel_Weapon_Alpha < 1.0f)
				{
					I::RenderView->SetBlend(CFG::Materials_ViewModel_Weapon_Alpha);
				}

				if (mat)
				{
					const auto& base = CFG::Color_Weapon;

					if (mat != F::Materials->m_pGlow)
					{
						I::RenderView->SetColorModulation(base);
					}

					else
					{
						if (F::Materials->m_pGlowEnvmapTint && F::Materials->m_pGlowSelfillumTint)
						{
							const auto& sheen = CFG::Color_Weapon_Sheen;

							I::RenderView->SetColorModulation(1.0f, 1.0f, 1.0f);

							F::Materials->m_pGlowSelfillumTint
							            ->SetVecValue(ColorUtils::ToFloat(base.r), ColorUtils::ToFloat(base.g), ColorUtils::ToFloat(base.b));

							F::Materials->m_pGlowEnvmapTint
							            ->SetVecValue(ColorUtils::ToFloat(sheen.r), ColorUtils::ToFloat(sheen.g), ColorUtils::ToFloat(sheen.b));
						}
					}
				}

				const auto result = CALL_ORIGINAL(ecx, flags);

				I::RenderView->SetColorModulation(1.0f, 1.0f, 1.0f);

				if (CFG::Materials_ViewModel_Weapon_Alpha < 1.0f)
				{
					I::RenderView->SetBlend(1.0f);
				}

				if (mat)
				{
					I::ModelRender->ForcedMaterialOverride(nullptr);
				}

				return result;
			}
		}
	}

	return CALL_ORIGINAL(ecx, flags);
}
