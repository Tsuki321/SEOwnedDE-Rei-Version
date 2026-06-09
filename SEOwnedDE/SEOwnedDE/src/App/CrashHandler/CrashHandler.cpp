#include "CrashHandler.h"

#include <DbgHelp.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "dbghelp.lib")

namespace
{
	// Reentry guard. If the handler itself raises an exception (e.g. the
	// MessageBoxA call site is corrupted), SetUnhandledExceptionFilter can
	// recurse into the same handler. The atomic CAS converts that into a
	// one-shot bail-out.
	volatile LONG s_nHandlerActive = 0;

	// dbghelp.dll is loaded on first crash. The library may not be in the
	// process address space yet when DllMain runs, and DllMain restrictions
	// prohibit LoadLibrary during DLL_PROCESS_ATTACH anyway.
	typedef BOOL(WINAPI* MiniDumpWriteDumpFn)(
		HANDLE hProcess,
		DWORD ProcessId,
		HANDLE hFile,
		MINIDUMP_TYPE DumpType,
		PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
		PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
		PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

	MiniDumpWriteDumpFn ResolveMiniDumpWriteDump()
	{
		static MiniDumpWriteDumpFn s_fnResolved = reinterpret_cast<MiniDumpWriteDumpFn>(
			GetProcAddress(LoadLibraryA("dbghelp.dll"), "MiniDumpWriteDump"));

		return s_fnResolved;
	}

	void Append(char*& pDst, size_t& nRemaining, const char* szFormat, ...)
	{
		if (nRemaining == 0)
			return;

		va_list args;
		va_start(args, szFormat);
		const int nWritten = _vsnprintf_s(pDst, nRemaining, _TRUNCATE, szFormat, args);
		va_end(args);

		if (nWritten <= 0)
			return;

		if (static_cast<size_t>(nWritten) >= nRemaining)
		{
			pDst += nRemaining - 1;
			nRemaining = 1;
		}
		else
		{
			pDst += nWritten;
			nRemaining -= nWritten;
		}
	}
}

void CCrashHandler::Install()
{
	SetUnhandledExceptionFilter(Handler);
}

const char* CCrashHandler::GetExceptionName(DWORD nCode)
{
	switch (nCode)
	{
		case EXCEPTION_ACCESS_VIOLATION:       return "Access Violation";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:     return "Integer Divide By Zero";
		case EXCEPTION_STACK_OVERFLOW:         return "Stack Overflow";
		case EXCEPTION_BREAKPOINT:             return "Breakpoint";
		case EXCEPTION_ILLEGAL_INSTRUCTION:    return "Illegal Instruction";
		case EXCEPTION_PRIV_INSTRUCTION:       return "Privileged Instruction";
		case EXCEPTION_IN_PAGE_ERROR:          return "In Page Error";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:  return "Array Bounds Exceeded";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:     return "Float Divide By Zero";
		case EXCEPTION_FLT_STACK_CHECK:        return "Float Stack Check";
		case 0xC0000409:                       return "Stack Buffer Overrun (FASTFAIL)";
		case 0xC0000374:                       return "Heap Corruption";
		case 0x80000003:                       return "Breakpoint";
		default:                               return "Unknown Exception";
	}
}

void CCrashHandler::WriteMiniDump(EXCEPTION_POINTERS* pExceptionPointers, const char* szDumpPath)
{
	const auto fnMiniDumpWriteDump = ResolveMiniDumpWriteDump();
	if (!fnMiniDumpWriteDump)
		return;

	HANDLE hFile = CreateFileA(szDumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo = {};
	exceptionInfo.ThreadId = GetCurrentThreadId();
	exceptionInfo.ExceptionPointers = pExceptionPointers;
	exceptionInfo.ClientPointers = FALSE;

	// MiniDumpWithThreadInfo is enough to walk back the call site; the larger
	// flags (full memory, indirectly-referenced memory) balloon the file to
	// hundreds of MB and are not needed for a single faulting thread.
	fnMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
		MiniDumpWithThreadInfo, &exceptionInfo, nullptr, nullptr);

	CloseHandle(hFile);
}

void CCrashHandler::BuildMessage(EXCEPTION_POINTERS* pExceptionPointers, const char* szDumpPath,
	char* szOut, size_t nOutSize)
{
	if (nOutSize == 0)
		return;

	szOut[0] = '\0';
	char* pCur = szOut;
	size_t nRemaining = nOutSize;

	const DWORD nCode = pExceptionPointers->ExceptionRecord->ExceptionCode;
	const PVOID pAddress = pExceptionPointers->ExceptionRecord->ExceptionAddress;

	HMODULE hFaultModule = nullptr;
	char szModule[MAX_PATH] = "<unknown>";
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(pAddress), &hFaultModule) && hFaultModule)
	{
		if (GetModuleFileNameA(hFaultModule, szModule, sizeof(szModule)) > 0)
		{
			// Strip path, keep filename only.
			char* pLastSlash = strrchr(szModule, '\\');
			if (pLastSlash)
				memmove(szModule, pLastSlash + 1, strlen(pLastSlash + 1) + 1);
		}
	}
	const auto nModuleOffset = reinterpret_cast<DWORD64>(pAddress) - reinterpret_cast<DWORD64>(hFaultModule);

	Append(pCur, nRemaining,
		"SEOwnedDE has crashed!\n\n"
		"Exception: 0x%08lX (%s)\n"
		"Address:   0x%016llX\n"
		"Module:    %s+0x%llX\n\n"
		"A crash dump was written to:\n%s\n\n"
		"Please share this dump with the developer.",
		nCode, GetExceptionName(nCode),
		reinterpret_cast<unsigned long long>(pAddress),
		szModule, static_cast<unsigned long long>(nModuleOffset),
		szDumpPath);
}

LONG WINAPI CCrashHandler::Handler(EXCEPTION_POINTERS* pExceptionPointers)
{
	// Reentry guard. If the handler itself faults (or any SEH-overwritten code
	// path recurses), the second invocation bails out immediately.
	if (InterlockedCompareExchange(&s_nHandlerActive, 1, 0) != 0)
		return EXCEPTION_EXECUTE_HANDLER;

	if (!pExceptionPointers || !pExceptionPointers->ExceptionRecord)
		return EXCEPTION_EXECUTE_HANDLER;

	// Build a unique dump path. %TEMP% is always writable; the user gets the
	// path back in the MessageBox so they can attach the file to a bug report.
	char szDumpPath[MAX_PATH] = {};
	{
		char szTempDir[MAX_PATH] = {};
		const DWORD nTempLen = GetTempPathA(MAX_PATH, szTempDir);
		if (nTempLen > 0 && nTempLen < MAX_PATH)
		{
			SYSTEMTIME st = {};
			GetLocalTime(&st);
			_snprintf_s(szDumpPath, MAX_PATH, _TRUNCATE,
				"%sSEOwnedDE-crash-%04u%02u%02u-%02u%02u%02u.dmp",
				szTempDir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		}
	}

	WriteMiniDump(pExceptionPointers, szDumpPath);

	char szMessage[2048] = {};
	BuildMessage(pExceptionPointers, szDumpPath, szMessage, sizeof(szMessage));

	// MB_TOPMOST so the dialog surfaces above fullscreen TF2. MB_TASKMODAL
	// ties the owner to the calling thread's window station, which is what
	// the user expects.
	MessageBoxA(nullptr, szMessage, "SEOwnedDE Crash",
		MB_OK | MB_ICONERROR | MB_TOPMOST | MB_TASKMODAL);

	return EXCEPTION_EXECUTE_HANDLER;
}
