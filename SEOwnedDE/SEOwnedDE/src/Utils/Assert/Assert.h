#pragma once

#define FUNCSIG __FUNCSIG__
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

#define Assert(cond) do { if (!(cond)) { MessageBoxA(0, #cond "\n\n" FUNCSIG " Line " LINE_STRING, "Error", MB_OK | MB_ICONERROR); } } while(0)
#define AssertFatal(cond) do { if (!(cond)) { MessageBoxA(0, #cond "\n\n" FUNCSIG " Line " LINE_STRING, "Error", MB_OK | MB_ICONERROR); exit(EXIT_FAILURE); } } while(0)
#define AssertCustom(cond, message) do { if (!(cond)) { MessageBoxA(0, message, "Error", MB_OK | MB_ICONERROR); } } while(0)