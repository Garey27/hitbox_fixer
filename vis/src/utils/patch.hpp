#pragma once
#include <vector>
#include <memory>
#include <src/utils/memory.hpp>


class IPatch {
public:
    virtual ~IPatch() = default;
};

template<typename T>
class MPatch : public IPatch {
    T* address_;
    T orig_;
    size_t size_;

public:
    MPatch(T &address, const T &src, size_t size)
        : address_(&address)
        , size_(size)
    {
        MAccess guard(address_, size_);
        orig_ = address;
        address = src;
    }
    MPatch(T *address, const T &src, size_t size)
        : address_(address)
        , size_(size)
    {
        MAccess guard(address_, size_);
        orig_ = *address;
        *address = src;
    }
    ~MPatch()
    {
        try {
            MAccess guard(address_, size_);
            *address_ = orig_;
        }
        catch(...) {
            // -_-
        }
        
    }
};

