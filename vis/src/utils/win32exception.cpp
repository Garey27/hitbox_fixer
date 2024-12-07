#include "win32exception.hpp"

#include <Windows.h>

#include <vector>

std::string GetSystemErrorMessage(int error)
{
    std::vector<char> buffer(4096);
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                   nullptr,
                   error,
                   LANG_ENGLISH,
                   buffer.data(),
                   buffer.size(),
                   nullptr);
    return buffer.data();
}

void ThrowLastError()
{
    throw Win32Exception(GetLastError());
}
