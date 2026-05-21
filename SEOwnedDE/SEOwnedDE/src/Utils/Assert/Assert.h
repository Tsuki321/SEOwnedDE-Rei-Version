#pragma once

#include <cstdlib>
#include <stdexcept>

#define FUNCSIG __FUNCSIG__
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

#ifdef SEOWNEDDE_UNIT_TESTS
#define ASSERT_FAIL(message) do { throw std::runtime_error(message); } while(0)
#else
#define ASSERT_FAIL(message) do { MessageBoxA(0, message, "Error", MB_OK | MB_ICONERROR); exit(EXIT_FAILURE); } while(0)
#endif

#define Assert(cond) do { if (!(cond)) { MessageBoxA(0, #cond "\n\n" FUNCSIG " Line " LINE_STRING, "Error", MB_OK | MB_ICONERROR); } } while(0)
#define AssertFatal(cond) do { if (!(cond)) { ASSERT_FAIL(#cond "\n\n" FUNCSIG " Line " LINE_STRING); } } while(0)
#define AssertFatalCustom(cond, message) do { if (!(cond)) { ASSERT_FAIL(message); } } while(0)
#define AssertCustom(cond, message) do { if (!(cond)) { MessageBoxA(0, message, "Error", MB_OK | MB_ICONERROR); } } while(0)
