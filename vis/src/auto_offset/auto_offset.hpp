#pragma once

#include <string>
#include <unordered_map>
#include <vector>

using Bytes = std::vector<char>;

template<typename T>
inline T *ReadPtr(const char *ptr)
{
    return *reinterpret_cast<T **>(const_cast<char *>(ptr));
}

class AO {
private:
    AO(bool fatal);

public:
    struct Pattern {
        std::string pattern;
        std::string mask;
    };

    void Reload();

    static AO &Get(bool fatal = false);

    uintptr_t FindPatternIDA(
        const std::string &module, const std::string &pattern,
        uintptr_t start = 0) const;

    const char *FindPattern(const std::string &module,
                            const Pattern &pattern) const;

    const char *FindReference(const std::string &module,
                              const Bytes &prefix,
                              const char *address) const;

    const char *FindPattern(const char *begin,
                            const char *end,
                            const Pattern &pattern) const;
    void AddModule(const std::string &name);

private:
    struct Module {
        std::string name;
        const char *begin, *end;
    };

    std::unordered_map<std::string, Module> modules_;
    bool fatal_ = false;

};
