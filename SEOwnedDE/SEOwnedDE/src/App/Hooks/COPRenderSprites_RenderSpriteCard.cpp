#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/VisualUtils/VisualUtils.h"

MAKE_SIGNATURE(COPRenderSprites_RenderSpriteCard, "client.dll", "48 8B C4 48 89 58 ? 57 41 54", 0x0);

union fltx4
{
	float m128_f32[4];
	uint32_t m128_u32[4];
};

struct SpriteRenderInfo_t
{
	size_t m_nXYZStride{};
	fltx4 *m_pXYZ{};
	size_t m_nRotStride{};
	fltx4 *m_pRot{};
	size_t m_nYawStride{};
	fltx4 *m_pYaw{};
	size_t m_nRGBStride{};
	fltx4 *m_pRGB{};
	size_t m_nCreationTimeStride{};
	fltx4 *m_pCreationTimeStamp{};
	size_t m_nSequenceStride{};
	fltx4 *m_pSequenceNumber{};
	size_t m_nSequence1Stride{};
	fltx4 *m_pSequence1Number{};
	float m_flAgeScale{};
	float m_flAgeScale2{};
	void *m_pSheet{};
	int m_nVertexOffset{};
	void *m_pParticles{};
};

MAKE_HOOK(COPRenderSprites_RenderSpriteCard, Signatures::COPRenderSprites_RenderSpriteCard.Get(), void, __fastcall,
	void* ecx, void* meshBuilder, void* pCtx, SpriteRenderInfo_t& info, int hParticle, void* pSortList, void* pCamera)
{
	// The original path is the common case. Check the particle mode before any
	// screenshot or color work, then use the frame-stamped screenshot cache so
	// this per-particle hook does not call into the engine repeatedly.
	const auto mode = CFG::Visuals_Particles_Mode;
	if (!mode || (CFG::Misc_Clean_Screenshot && F::VisualUtils->IsTakingScreenshotCached()))
	{
		CALL_ORIGINAL(ecx, meshBuilder, pCtx, info, hParticle, pSortList, pCamera);
		return;
	}

	{
		Color_t color = {};

		switch (mode)
		{
			// Custom color
			case 1:
			{
				color = CFG::Color_Particles;
				break;
			}

			// Rainbow
			case 2:
			{
				// Rainbow is shared by every particle in a frame. Cache it locally;
				// the source color is deliberately refreshed when the configured rate
				// changes so menu edits take effect immediately.
				static int nRainbowFrame = -1;
				static float flRainbowRate = -1.0f;
				static Color_t cachedRainbow = {};
				const int nFrame = I::GlobalVars ? I::GlobalVars->framecount : 0;
				const float flRate = CFG::Visuals_Particles_Rainbow_Rate;

				if (nRainbowFrame != nFrame || flRainbowRate != flRate)
				{
					nRainbowFrame = nFrame;
					flRainbowRate = flRate;
					cachedRainbow = ColorUtils::Rainbow(I::GlobalVars->realtime, flRate);
				}

				color = cachedRainbow;
				break;
			}
		}

		info.m_pRGB[((hParticle / 4) * info.m_nRGBStride) + 0].m128_f32[hParticle & 0x3] = ColorUtils::ToFloat(color.r);
		info.m_pRGB[((hParticle / 4) * info.m_nRGBStride) + 1].m128_f32[hParticle & 0x3] = ColorUtils::ToFloat(color.g);
		info.m_pRGB[((hParticle / 4) * info.m_nRGBStride) + 2].m128_f32[hParticle & 0x3] = ColorUtils::ToFloat(color.b);
	}

	CALL_ORIGINAL(ecx, meshBuilder, pCtx, info, hParticle, pSortList, pCamera);
}
