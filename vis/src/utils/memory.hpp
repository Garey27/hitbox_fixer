#pragma once

#include <cstddef>
#include <cstring>

struct MAccess {
    MAccess(void *address, size_t size);
    ~MAccess();

    void *address_;
    size_t size_;
    unsigned long oldProtect_;
};

inline void ReadMem(void *dest, void *source, size_t sz)
{
    memcpy(dest, source, sz);
}

inline void WriteMem(void *dest, void *source, size_t sz)
{
    memcpy(dest, source, sz);
}

template<typename T, typename N>
inline T Read(N addr)
{
    T ret;
    ReadMem(&ret, (void *)addr, sizeof(T));
    return ret;
}

template<typename T, typename N>
inline void ReadArr(N addr, T *arr, size_t count)
{
    ReadMem((void *)arr, (void *)addr, sizeof(T) * count);
}

template<typename T, typename N>
inline void Write(N addr, T value)
{
    WriteMem((void *)addr, &value, sizeof(T));
}

template<typename T, typename N>
inline void WriteArr(N addr, T *arr, size_t count)
{
    WriteMem((void *)addr, (void *)arr, sizeof(T) * count);
}

template<typename T = int32_t>
inline uintptr_t GetAbsoluteAddress(uintptr_t addr,
                                    intptr_t offset,
                                    intptr_t instructionSize)
{
    return addr + Read<T>(addr + offset) + instructionSize;
}

template<typename T, size_t idx, typename N>
inline T GetVFunc(N *inst)
{
    return Read<T>(Read<T *>(inst) + idx);
}