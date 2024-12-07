#include "memory.hpp"

#include "utils/win32exception.hpp"

#include <Windows.h>

MAccess::MAccess(void *address, size_t size)
    : address_(address)
    , size_(size)
{
    BOOL ok =
        VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect_);
    if (!ok) {
        ThrowLastError();
    }
}

MAccess::~MAccess()
{
    BOOL ok = VirtualProtect(address_, size_, oldProtect_, &oldProtect_);
    if (!ok) {
        ThrowLastError();
    }
}
