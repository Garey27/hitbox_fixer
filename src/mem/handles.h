#ifndef HANDLES_H
#define HANDLES_H
#include <cstdint>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <stdint.h>
typedef void* MHandle;
#else
#ifdef _WIN32
#ifndef WINCLUDES_H
#define WINCLUDES_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>

#endif
#endif
typedef HMODULE MHandle;
#endif

typedef struct
{
	MHandle handle;
	uintptr_t address;
	size_t size;
} ModuleInfo;

namespace Handles
{
	MHandle GetModuleHandle(const char* module);
	ModuleInfo GetModuleInfo(const char* module);
	MHandle GetPtrModuleHandle(void* ptr);
}

#endif
