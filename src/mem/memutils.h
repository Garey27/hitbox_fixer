#ifndef MEMUTILS_H
#define MEMUTILS_H

#include "patternscan.h"
#include "handles.h"
#include <string.h>

//External programs might want to use custom RPM/WPM functions
#ifndef MEMUTILS_CUSTOM_RW
inline void ReadMem(void* dest, void* source, size_t sz)
{
	memcpy(dest, source, sz);
}

inline void WriteMem(void* dest, void* source, size_t sz)
{
	memcpy(dest, source, sz);
}
#else
void ReadMem(void* dest, void* source, size_t sz);
void WriteMem(void* dest, void* source, size_t sz);
#endif

template<typename T, typename N>
inline T Read(N addr)
{
	T ret;
	ReadMem(&ret, (void*)addr, sizeof(T));
	return ret;
}

template<typename T, typename N>
inline void ReadArr(N addr, T* arr, size_t count)
{
	ReadMem((void*)arr, (void*)addr, sizeof(T) * count);
}

template<typename T, typename N>
inline void Write(N addr, T value)
{
	WriteMem((void*)addr, &value, sizeof(T));
}

template<typename T, typename N>
inline void WriteArr(N addr, T* arr, size_t count)
{
	WriteMem((void*)addr, (void*)arr, sizeof(T) * count);
}

template<typename T = int32_t>
inline uintptr_t GetAbsoluteAddress(uintptr_t addr, intptr_t offset, intptr_t instructionSize)
{
	return addr + Read<T>(addr + offset) + instructionSize;
}

template<typename T, size_t idx, typename N>
inline T GetVFunc(N* inst)
{
	return Read<T>(Read<T*>(inst) + idx);
}

#endif
