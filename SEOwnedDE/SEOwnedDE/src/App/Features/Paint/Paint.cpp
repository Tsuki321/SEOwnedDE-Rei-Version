#include "Paint.h"

#include "../CFG.h"
#include "../Rendering/RenderContextScope.h"
#include "../VisualUtils/VisualUtils.h"
#include <algorithm>
#include <cmath>

#pragma warning (disable : 4244) //possible loss of data (int to float)

void CPaint::Initialize()
{
	if (!m_pMatGlowColor)
	{
		m_pMatGlowColor = I::MaterialSystem->FindMaterial("dev/glow_color", TEXTURE_GROUP_OTHER);
	}

	if (!m_pRtFullFrame)
	{
		m_pRtFullFrame = I::MaterialSystem->FindTexture("_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET);
	}

	if (!m_pRenderBuffer0)
	{
		m_pRenderBuffer0 = I::MaterialSystem->CreateNamedRenderTargetTextureEx(
			"seo_paint_buffer0",
			m_pRtFullFrame->GetActualWidth(),
			m_pRtFullFrame->GetActualHeight(),
			RT_SIZE_LITERAL,
			IMAGE_FORMAT_RGB888,
			MATERIAL_RT_DEPTH_SHARED,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_EIGHTBITALPHA,
			CREATERENDERTARGETFLAGS_HDR
		);

		m_pRenderBuffer0->IncrementReferenceCount();
	}

	if (!m_pRenderBuffer1)
	{
		m_pRenderBuffer1 = I::MaterialSystem->CreateNamedRenderTargetTextureEx(
			"seo_paint_buffer1",
			m_pRtFullFrame->GetActualWidth(),
			m_pRtFullFrame->GetActualHeight(),
			RT_SIZE_LITERAL,
			IMAGE_FORMAT_RGB888,
			MATERIAL_RT_DEPTH_SHARED,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_EIGHTBITALPHA,
			CREATERENDERTARGETFLAGS_HDR
		);

		m_pRenderBuffer1->IncrementReferenceCount();
	}

	if (!m_pMatHaloAddToScreen)
	{
		const auto kv = new KeyValues("UnlitGeneric");
		kv->SetString("$basetexture", "seo_paint_buffer0");
		kv->SetString("$additive", "1");
		m_pMatHaloAddToScreen = I::MaterialSystem->CreateMaterial("seo_paint_material", kv);
	}

	if (!m_pMatBlurX)
	{
		const auto kv = new KeyValues("BlurFilterX");
		kv->SetString("$basetexture", "seo_paint_buffer0");
		m_pMatBlurX = I::MaterialSystem->CreateMaterial("seo_paint_material_blurx", kv);
	}

	if (!m_pMatBlurY)
	{
		const auto kv = new KeyValues("BlurFilterY");
		kv->SetString("$basetexture", "seo_paint_buffer1");
		m_pMatBlurY = I::MaterialSystem->CreateMaterial("seo_paint_material_blury", kv);
		m_pBloomAmount = m_pMatBlurY->FindVar("$bloomamount", nullptr);
	}
}

void CPaint::ClearPoints(bool bReleaseStorage)
{
	m_nOldestPaintPoint = 0;
	m_nPaintPointCount = 0;
	m_nPaintStrokeCount = 0;

	if (bReleaseStorage)
	{
		m_pStorage.reset();
		m_nRainbowColorCount = 0;
		m_nRainbowFrame = -1;
	}
}

const CPaint::PaintRecord_t& CPaint::GetPoint(size_t nOffset) const
{
	return m_pStorage->Points[(m_nOldestPaintPoint + nOffset) % MAX_PAINT_POINTS];
}

void CPaint::PopOldestPoint()
{
	if (!m_nPaintPointCount)
		return;

	const int nRemovedStroke = m_pStorage->Points[m_nOldestPaintPoint].StartTick;
	m_nOldestPaintPoint = (m_nOldestPaintPoint + 1) % MAX_PAINT_POINTS;
	--m_nPaintPointCount;

	if ((!m_nPaintPointCount || GetPoint(0).StartTick != nRemovedStroke) && m_nPaintStrokeCount)
		--m_nPaintStrokeCount;
}

void CPaint::AddPoint(const Vec3& vPosition, float flTimeAdded, int nStartTick)
{
	if (!m_pStorage)
		m_pStorage = std::make_unique<PaintStorage_t>();

	const bool bNewStroke = !m_nPaintPointCount || GetPoint(m_nPaintPointCount - 1).StartTick != nStartTick;
	if (bNewStroke)
	{
		while (m_nPaintPointCount && m_nPaintStrokeCount >= MAX_PAINT_STROKES)
		{
			const int nOldestStroke = GetPoint(0).StartTick;
			do
			{
				PopOldestPoint();
			}
			while (m_nPaintPointCount && GetPoint(0).StartTick == nOldestStroke);
		}

		++m_nPaintStrokeCount;
	}

	if (m_nPaintPointCount == MAX_PAINT_POINTS)
		PopOldestPoint();

	const size_t nInsertIndex = (m_nOldestPaintPoint + m_nPaintPointCount) % MAX_PAINT_POINTS;
	m_pStorage->Points[nInsertIndex] = { vPosition, flTimeAdded, nStartTick };
	++m_nPaintPointCount;
}

void CPaint::PrunePoints()
{
	const float flLifeTime = CFG::Visuals_Paint_LifeTime;
	if (flLifeTime > 0.0f)
	{
		const float flCutoff = I::GlobalVars->curtime - flLifeTime;

		while (m_nPaintPointCount && GetPoint(0).TimeAdded < flCutoff)
			PopOldestPoint();
	}
}

void CPaint::PrepareRainbowColors(size_t nColorCount)
{
	nColorCount = std::min(nColorCount, MAX_PAINT_POINTS);
	const int nFrame = I::GlobalVars ? I::GlobalVars->framecount : 0;
	if (m_nRainbowFrame == nFrame && m_nRainbowColorCount >= nColorCount)
		return;

	m_nRainbowFrame = nFrame;
	m_nRainbowColorCount = nColorCount;

	constexpr float flRate = 3.0f;
	const float flStep = TICKS_TO_TIME(1) * flRate;
	const float flStepCos = std::cosf(flStep);
	const float flStepSin = std::sinf(flStep);
	float flCos[3] = {};
	float flSin[3] = {};

	for (int nChannel = 0; nChannel < 3; ++nChannel)
	{
		const float flPhase = I::GlobalVars->realtime + static_cast<float>(nChannel * 2);
		flCos[nChannel] = std::cosf(flPhase);
		flSin[nChannel] = std::sinf(flPhase);
	}

	auto ToByte = [](float flValue)
	{
		return static_cast<byte>(std::clamp(std::lround(flValue * 127.5f + 127.5f), 0L, 255L));
	};

	for (size_t n = 0; n < nColorCount; ++n)
	{
		m_pStorage->RainbowColors[n] = {
			ToByte(flCos[0]),
			ToByte(flCos[1]),
			ToByte(flCos[2]),
			255
		};

		for (int nChannel = 0; nChannel < 3; ++nChannel)
		{
			const float flNextCos = flCos[nChannel] * flStepCos - flSin[nChannel] * flStepSin;
			flSin[nChannel] = flSin[nChannel] * flStepCos + flCos[nChannel] * flStepSin;
			flCos[nChannel] = flNextCos;
		}
	}
}

void CPaint::Run()
{
	if (!CFG::Visuals_Paint_Active)
	{
		ClearPoints(true);
		return;
	}

	auto pLocal = H::Entities->GetLocal();

	if (!pLocal)
	{
		ClearPoints(true);
		return;
	}

	const bool bCleanScreenshot = CFG::Misc_Clean_Screenshot && F::VisualUtils->IsTakingScreenshotCached();
	const bool bGameUIVisible = I::EngineVGui->IsGameUIVisible();

	if (!bCleanScreenshot && !bGameUIVisible && !pLocal->deadflag() && !I::MatSystemSurface->IsCursorVisible() && !SDKUtils::BInEndOfMatch())
	{
		static int nOldTick = I::GlobalVars->tickcount;

		if (I::GlobalVars->tickcount != nOldTick)
		{
			static int nTick = 0;

			if (H::Input->IsDown(CFG::Visuals_Paint_Key))
			{
				if (!nTick)
					nTick = TIME_TO_TICKS(I::EngineClient->Time());

				Vec3 vForward = {};
				Math::AngleVectors(I::EngineClient->GetViewAngles(), &vForward);

				Vec3 vStart = pLocal->GetShootPos();
				Vec3 vEnd = vStart + (vForward * 9001.0f);

				Ray_t ray = {};
				ray.Init(vStart, vEnd);
				trace_t trace = {};
				CTraceFilterWorldCustom filter = {};

				I::EngineTrace->TraceRay(ray, MASK_SOLID, &filter, &trace);

				AddPoint(trace.endpos, I::GlobalVars->curtime, nTick);
			}

			else
			{
				nTick = 0;
			}

			if (H::Input->IsPressed(CFG::Visuals_Paint_Erase_Key))
			{
				ClearPoints();
			}

			nOldTick = I::GlobalVars->tickcount;
		}
	}

	PrunePoints();

	if (bCleanScreenshot || bGameUIVisible)
		return;

	size_t nLongestStroke = 0;
	size_t nCurrentStrokeLength = 0;
	int nCurrentStroke = 0;
	bool bDrewSomething = false;
	for (size_t n = 0; n < m_nPaintPointCount; ++n)
	{
		const auto& point = GetPoint(n);
		if (!n || point.StartTick != nCurrentStroke)
		{
			nCurrentStroke = point.StartTick;
			nCurrentStrokeLength = 1;
		}
		else
		{
			++nCurrentStrokeLength;
			bDrewSomething = true;
		}

		nLongestStroke = std::max(nLongestStroke, nCurrentStrokeLength);
	}

	if (!bDrewSomething)
		return;

	const int w = H::Draw->GetScreenW();
	const int h = H::Draw->GetScreenH();
	if (w < 1 || h < 1 || w > 4096 || h > 2160)
		return;

	Initialize();
	PrepareRainbowColors(nLongestStroke);

	CRenderContextScope renderContext(I::MaterialSystem);
	if (!renderContext)
		return;

	auto* pRenderContext = renderContext.Get();
	m_pBloomAmount->SetIntValue(CFG::Visuals_Paint_Bloom_Amount);

	pRenderContext->PushRenderTargetAndViewport();
	{
		pRenderContext->SetRenderTarget(m_pRenderBuffer0);
		pRenderContext->Viewport(0, 0, w, h);
		pRenderContext->ClearColor4ub(0, 0, 0, 0);
		pRenderContext->ClearBuffers(true, false, false);

		I::ModelRender->ForcedMaterialOverride(m_pMatGlowColor);

		const PaintRecord_t* pPreviousPoint = nullptr;
		size_t nStrokePoint = 0;
		for (size_t n = 0; n < m_nPaintPointCount; ++n)
		{
			const auto& point = GetPoint(n);
			if (pPreviousPoint && point.StartTick == pPreviousPoint->StartTick)
			{
				++nStrokePoint;
				RenderUtils::RenderLine(point.Position, pPreviousPoint->Position, m_pStorage->RainbowColors[nStrokePoint], false);
			}
			else
			{
				nStrokePoint = 0;
			}

			pPreviousPoint = &point;
		}

		I::ModelRender->ForcedMaterialOverride(nullptr);
	}
	pRenderContext->PopRenderTargetAndViewport();

	if (bDrewSomething)
	{
		pRenderContext->PushRenderTargetAndViewport();
		{
			pRenderContext->Viewport(0, 0, w, h);
			pRenderContext->SetRenderTarget(m_pRenderBuffer1);
			pRenderContext->DrawScreenSpaceRectangle(m_pMatBlurX, 0, 0, w, h, 0.0f, 0.0f, w - 1, h - 1, w, h);
			pRenderContext->SetRenderTarget(m_pRenderBuffer0);
			pRenderContext->DrawScreenSpaceRectangle(m_pMatBlurY, 0, 0, w, h, 0.0f, 0.0f, w - 1, h - 1, w, h);
		}
		pRenderContext->PopRenderTargetAndViewport();

		ShaderStencilState_t sEffect = {};
		sEffect.m_bEnable = true;
		sEffect.m_nWriteMask = 0x0;
		sEffect.m_nTestMask = 0xFF;
		sEffect.m_nReferenceValue = 0;
		sEffect.m_CompareFunc = STENCILCOMPARISONFUNCTION_EQUAL;
		sEffect.m_PassOp = STENCILOPERATION_KEEP;
		sEffect.m_FailOp = STENCILOPERATION_KEEP;
		sEffect.m_ZFailOp = STENCILOPERATION_KEEP;
		sEffect.SetStencilState(pRenderContext);

		pRenderContext->DrawScreenSpaceRectangle(m_pMatHaloAddToScreen, 0, 0, w, h, 0.0f, 0.0f, w - 1, h - 1, w, h);
	}

	ShaderStencilState_t stencilStateDisable = {};
	stencilStateDisable.m_bEnable = false;
	stencilStateDisable.SetStencilState(pRenderContext);
}

void CPaint::CleanUp()
{
	ClearPoints(true);

	if (m_pMatHaloAddToScreen)
	{
		m_pMatHaloAddToScreen->DecrementReferenceCount();
		m_pMatHaloAddToScreen->DeleteIfUnreferenced();
		m_pMatHaloAddToScreen = nullptr;
	}

	if (m_pRenderBuffer0)
	{
		m_pRenderBuffer0->DecrementReferenceCount();
		m_pRenderBuffer0->DeleteIfUnreferenced();
		m_pRenderBuffer0 = nullptr;
	}

	if (m_pRenderBuffer1)
	{
		m_pRenderBuffer1->DecrementReferenceCount();
		m_pRenderBuffer1->DeleteIfUnreferenced();
		m_pRenderBuffer1 = nullptr;
	}

	if (m_pMatBlurX)
	{
		m_pMatBlurX->DecrementReferenceCount();
		m_pMatBlurX->DeleteIfUnreferenced();
		m_pMatBlurX = nullptr;
	}

	if (m_pMatBlurY)
	{
		m_pMatBlurY->DecrementReferenceCount();
		m_pMatBlurY->DeleteIfUnreferenced();
		m_pMatBlurY = nullptr;
	}
}
