#pragma once

#include <system_error>

std::string GetSystemErrorMessage(int error);
[[noreturn]] void ThrowLastError();

class Win32Exception : public std::system_error {
public:
    Win32Exception(int error)
        : std::system_error(
              error, std::system_category(), GetSystemErrorMessage(error))
    {
    }
};
