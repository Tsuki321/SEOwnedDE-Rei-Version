#pragma once

#include <atomic>
#include "../SDK/SDK.h"

class CApp
{
	std::atomic<bool> bUnload = false;

public:
	void Start();
	void Loop();
	void Shutdown();
};

MAKE_SINGLETON(CApp, App);