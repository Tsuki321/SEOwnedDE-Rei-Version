#include "App/App.h"
#include "App/CrashHandler/CrashHandler.h"

DWORD WINAPI MainThread(LPVOID lpParam)
{
	App->Start();

	App->Loop();

	App->Shutdown();

	Sleep(500);

	FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), EXIT_SUCCESS);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		// Install the structured-exception handler first so it covers every
		// code path, including App->Start() and the per-frame hooks. The
		// handler lazy-loads dbghelp.dll so DLL_PROCESS_ATTACH stays within
		// the documented DllMain restrictions (no LoadLibrary here).
		F::CrashHandler->Install();

		if (const auto hMainThread = CreateThread(nullptr, 0, MainThread, hinstDLL, 0, nullptr))
		{
			CloseHandle(hMainThread);
		}
	}

	return TRUE;
}