#pragma once

#include "../../../SDK/SDK.h"

// IMaterialSystem::GetRenderContext returns an owning reference. Keep the
// lifetime tied to the synchronous render pass so early returns cannot leak a
// context (and so callers do not need to duplicate Release() branches).
class CRenderContextScope
{
	IMatRenderContext* m_pContext = nullptr;

public:
	explicit CRenderContextScope(IMaterialSystem* pMaterialSystem)
		: m_pContext(pMaterialSystem ? pMaterialSystem->GetRenderContext() : nullptr)
	{
	}

	~CRenderContextScope()
	{
		if (m_pContext)
			m_pContext->Release();
	}

	CRenderContextScope(const CRenderContextScope&) = delete;
	CRenderContextScope& operator=(const CRenderContextScope&) = delete;

	IMatRenderContext* Get() const { return m_pContext; }
	IMatRenderContext* operator->() const { return m_pContext; }
	explicit operator bool() const { return m_pContext != nullptr; }
};
