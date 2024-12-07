#include <Windows.h>
#include <psapi.h>

#include <cassert>
#include <map>
#include "auto_offset.hpp"
#include "utils/memory.hpp"
#include "utils/win32exception.hpp"

static const std::vector<std::string> moduleList_{"hw.dll", "client.dll"};
struct pOperation {
    short op;
    intptr_t offset, v1;

    pOperation(short o = 0, intptr_t off = 0, intptr_t v = 0)
    {
        op = o;
        offset = off;
        v1 = v;
    }

    uintptr_t RunOp(uintptr_t addr)
    {
        switch (op) {
            case 1:
                return addr + offset;
            case 11:
                return Read<uint8_t>(addr + offset);
            case 12:
                return Read<uint16_t>(addr + offset);
            case 14:
                return Read<uint32_t>(addr + offset);
            case 18:
                return Read<uint64_t>(addr + offset);
            case 21:
                return GetAbsoluteAddress<int8_t>(addr, offset, v1);
            case 22:
                return GetAbsoluteAddress<int16_t>(addr, offset, v1);
            case 24:
            case 28:
                return GetAbsoluteAddress<int32_t>(addr, offset, v1);
            default:
                printf("INVALID OPPPP!\n");
        }
        return addr;
    }
};

uintptr_t ScanPattern(uintptr_t start,
                      uintptr_t end,
                      uintptr_t length,
                      uintptr_t *data,
                      uintptr_t *mask);

static std::map<char, size_t> readSizes = {
    {'$', 1}, {'%', 2}, {'^', 4}, {'&', 8}, {'*', sizeof(uintptr_t)}};

static void ParsePattern(const char *pattern,
                         short *&patternBytes,
                         size_t &length,
                         std::vector<pOperation> &operations)
{
    char *p = (char *)(uintptr_t)pattern - 1;
    bool inRelDeref = false;
    bool derefDone = false;
    int relIdx = 0;
    int relStartIdx = 0;
    int idx = 0;
    int initDerefIdx = 0;

    length = strlen(pattern);
    patternBytes = new short[length];

    while ((++p) - pattern <= (long)length && *p) {
        while (*p == ' ')
            p++;

        if (*p == '?') {
            if (*(p + 1) == '?')
                p++;
            patternBytes[idx++] = -1;
        }
        else if (*p == '@') {
            assert(!inRelDeref && !derefDone && operations.size() == 0);
            if (idx)
                operations.emplace_back(pOperation(1, idx));
            derefDone = true;
        }
        else if (*p == '[') {
            assert(!inRelDeref && !derefDone);
            inRelDeref = true;
            relStartIdx = idx;
            if (idx) {
                relIdx++;
                operations.emplace_back(pOperation(1, idx));
            }
            operations.emplace_back(pOperation());
        }
        else if (*p == ']') {
            assert(inRelDeref);
            inRelDeref = false;
            derefDone = true;

            pOperation &op = operations.at(relIdx);

            op.offset = initDerefIdx - relStartIdx;
            op.v1 = idx - relStartIdx;
        }
        else if (readSizes[(int)*p] != 0) {
            assert(!derefDone);
            derefDone = true;

            initDerefIdx = idx;

            if (!inRelDeref)
                operations.emplace_back(
                    pOperation(10 + readSizes[(int)*p], idx));
            else
                operations.at(relIdx).op = 20 + readSizes[(int)*p];

            p++;

            while (*p == '+' || *p == '-' || readSizes[(int)*p] || *p == ':') {
                if (readSizes[(int)*p])
                    operations.emplace_back(
                        pOperation(10 + readSizes[(int)*p++]));
                else if (*p == ':') {
                    pOperation op = pOperation();
                    p++;
                    op.offset = strtol(p, &p, 10);
                    p++;
                    op.v1 = strtol(p, &p, 10);
                    op.op = 20 + sizeof(uintptr_t);
                    operations.emplace_back(op);
                }
                else {
                    pOperation op = pOperation();
                    if (*p == '+' || *p == '-')
                        op.offset = strtol(p, &p, 10);
                    // Compress the offset operation into a dereference
                    op.op = readSizes[(int)*p] ? 10 + readSizes[(int)*p] : 1;
                    if (readSizes[(int)*p])
                        p++;
                    operations.emplace_back(op);
                }
            }

            if (*p != ' ')
                p--;
        }
        else {
            patternBytes[idx++] = (uint8_t)strtoul(p, &p, 16);
            if (*p != ' ')
                p--;
        }
    }

    length = idx;
}

// Optimize the parsed pattern into larger long sized values to be compared.
// This way we will utilize the full potential of the CPUs native register size
// when reading the memory. Going wider (into SIMD) is not worth it, because if
// the pattern does not match, it will usually be within the first 4-8
// instructions.
static void ProduceScanData(short *parsedData,
                            uintptr_t *&data,
                            uintptr_t *&mask,
                            size_t &size)
{
    constexpr size_t iSize = sizeof(long);
    size_t size2 = (size - 1) / iSize + 1;

    data = new uintptr_t[size2];
    mask = new uintptr_t[size2];

    for (size_t i = 0; i < size2; i++) {
        data[i] = 0;
        mask[i] = 0;

        for (size_t o = 0; o < iSize; o++) {
            if (i * iSize + o >= size || parsedData[i * iSize + o] < 0)
                mask[i] |= (0xffll << (8ll * o));
            if (i * iSize + o < size)
                data[i] |= (((uintptr_t)((parsedData[i * iSize + o]) & 0xffll))
                            << (8ll * o));
        }
        data[i] |= mask[i];
    }

    size = size2;
}

AO::AO(bool fatal)
    : fatal_(fatal)
{
    for (const auto &module : moduleList_) {
        AddModule(module);
    }
}


void AO::Reload()
{
    modules_.clear();
    for (const auto &module : moduleList_) {
        AddModule(module);
    }
}

AO &AO::Get(bool fatal)
{
    static AO ao(fatal);
    return ao;
}


uintptr_t AO::FindPatternIDA(const std::string &module,
                             const std::string &pattern, uintptr_t start) const const
{
    auto &m = modules_.at(module);
    short *patternBytes = nullptr;
    size_t length = 0;
    std::vector<pOperation> operations;

    uintptr_t addr = 0;

    ParsePattern(pattern.c_str(), patternBytes, length, operations);

    uintptr_t *data;
    uintptr_t *mask;
    ProduceScanData(patternBytes, data, mask, length);
    delete[] patternBytes;

    if (!start)
        start = (uintptr_t)m.begin;

    addr =
        ScanPattern((uintptr_t)start, (uintptr_t)m.end, length, data, mask);

    if (addr)
        for (auto &i : operations)
            addr = i.RunOp(addr);

    delete[] data;
    delete[] mask;
    return addr;
}

uintptr_t ScanPattern(uintptr_t start,
                      uintptr_t end,
                      uintptr_t length,
                      uintptr_t *data,
                      uintptr_t *mask)
{
    uintptr_t llength = sizeof(long) * length;
    for (uintptr_t i = start; i < end - llength; i++) {
        bool miss = false;
        for (uintptr_t o = 0; o < length && !miss; o++)
            miss = data[o] ^
                   (Read<uintptr_t>(i + o * sizeof(uintptr_t)) | mask[o]);

        if (!miss)
            return i;
    }

    return 0;
}

const char *AO::FindPattern(const std::string &module,
                            const Pattern &pattern) const
{
    auto &m = modules_.at(module);
    return FindPattern(m.begin, m.end, pattern);
}

const char *AO::FindReference(const std::string &module,
                              const Bytes &prefix,
                              const char *address) const
{
    std::string pattern(prefix.size() + sizeof(const char *), 0);
    std::copy(prefix.begin(), prefix.end(), pattern.begin());
    *reinterpret_cast<const char **>(
        &pattern[pattern.size() - sizeof(const char *)]) = address;
    return FindPattern(module, {pattern});
}

const char *AO::FindPattern(const char *begin,
                            const char *end,
                            const Pattern &pattern) const
{
    assert(pattern.pattern.size() != pattern.mask.size() ||
           pattern.mask.empty());

    end -= pattern.mask.size();

    auto patternBegin = pattern.pattern.data();
    auto patternEnd = pattern.pattern.data() + pattern.pattern.size();

    const char *maskBegin = nullptr;
    const char *maskEnd = nullptr;

    if (pattern.mask.empty()) {
        for (auto b = begin; b != end; b++) {
            if (*b != *patternBegin) {
                continue;
            }
            if (!memcmp(b + 1, patternBegin + 1, pattern.pattern.size() - 1)) {
                return b;
            }
        }
    }
    else {
        maskBegin = pattern.mask.data();
        maskEnd = pattern.mask.data() + pattern.mask.size();
        assert(*maskBegin == 0xFF);
        for (auto b = begin; b != end; b++) {
            if (*b != *patternBegin) {
                continue;
            }
            bool match = true;
            for (auto x = b + 1, p = patternBegin + 1, m = maskBegin + 1;
                 x != end && p != patternEnd;
                 x++, p++, m++) {
                if ((*x & *m) != *p) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return b;
            }
        }
    }

    if (fatal_) {
        throw std::runtime_error("failed to find offset");
    }

    return nullptr;
}

void AO::AddModule(const std::string &name)
{
    auto [it, inserted] = modules_.insert({name, {}});
    assert(inserted);
    auto &module = it->second;
    module.name = name;
    auto handle = GetModuleHandleA(name.c_str());
    if (!handle) {
        return;//
        //ThrowLastError();
    }
    MODULEINFO mi;
    BOOL ok =
        GetModuleInformation(GetCurrentProcess(), handle, &mi, sizeof(mi));
    if (!ok) {
        return;  // ThrowLastError();
    }
    module.begin = static_cast<const char *>(mi.lpBaseOfDll);
    module.end = module.begin + mi.SizeOfImage;
}
