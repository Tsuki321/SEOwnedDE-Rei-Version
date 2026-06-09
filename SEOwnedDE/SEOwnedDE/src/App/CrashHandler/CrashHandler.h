#pragma once

#include <windows.h>

#include "../../../Utils/Singleton/Singleton.h"

// Top-level structured-exception handler. Registered via
// SetUnhandledExceptionFilter from DllMain on DLL_PROCESS_ATTACH so it covers
// every code path the mod executes, including MainThread init and the per-frame
// hooks. On crash:
//   1. Writes a minidump under %TEMP% (so the developer can debug post-hoc)
//   2. Pops a MessageBox with the exception code, faulting address, and the
//      dump path - the user sees something useful even if they never report it
//   3. Returns EXCEPTION_EXECUTE_HANDLER so the process unwinds cleanly
//
// Safety constraints (the handler runs with a corrupted stack, so the CRT may
// be in a bad state):
//   - No heap allocations, no std::string, no fopen - stack buffers + Win32 API
//   - dbghelp.dll is loaded lazily in the handler (DllMain cannot LoadLibrary)
//   - Atomic reentry guard so a crash inside the handler does not recurse
class CCrashHandler
{
	static LONG WINAPI Handler(EXCEPTION_POINTERS* pExceptionPointers);
	static void WriteMiniDump(EXCEPTION_POINTERS* pExceptionPointers, const char* szDumpPath);
	static void BuildMessage(EXCEPTION_POINTERS* pExceptionPointers, const char* szDumpPath,
		char* szOut, size_t nOutSize);
	static const char* GetExceptionName(DWORD nCode);

public:
	void Install();
};

MAKE_SINGLETON_SCOPED(CCrashHandler, CrashHandler, F);
