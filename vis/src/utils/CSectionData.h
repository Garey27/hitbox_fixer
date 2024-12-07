#ifndef CSectionData_h__
#define CSectionData_h__

#include <stdint.h>
#undef min
#undef max
#include <algorithm>
#include <list>
#include <vector>

enum iBool : uint32_t { bFalse, bTrue };

#ifdef USE_METAMOD
#    include <Metamod/meta_api.h>
#    include <Metamod/mutil.h>
#endif

#define DynErr(...) \
    this->SpewMsg(__FILE__, __FUNCTION__, __LINE__, MsgErr, __VA_ARGS__);
#define DynMsg(...) \
    this->SpewMsg(__FILE__, __FUNCTION__, __LINE__, MsgTrace, __VA_ARGS__);
#define DynFatal(...) \
    this->SpewMsg(__FILE__, __FUNCTION__, __LINE__, MsgFatal, __VA_ARGS__);
#define DynWarn(...) \
    this->SpewMsg(__FILE__, __FUNCTION__, __LINE__, MsgWarn, __VA_ARGS__);

typedef void (*pfnPrintFunc)(const char *szMsg);

class CBaseNotification {
public:
    CBaseNotification(const char *ClassName);
    ~CBaseNotification();
    static const char *GetBaseDir(const char *ModuleName = NULL);
    static const char *GetFileName(const char *fpath);
    static void SetPrintFunc(pfnPrintFunc NewFunc);
    static void SetConsoleActive(iBool bActive);
    enum _MsgType { MsgFatal, MsgErr, MsgWarn, MsgTrace, _MsgTypeMax };
    void SpewMsg(const char *File,
                 const char *Func,
                 int Line,
                 _MsgType Type,
                 char *Fmt,
                 ...);

    typedef struct MsgDescription_s {
        const char *Description;
    } MsgDescription_t;
    static MsgDescription_t MsgDescription[_MsgTypeMax];

private:
    const char *szClassName;
    static pfnPrintFunc PrintFunc;
    static iBool bConsoleActive;
};
class CDynPatcher;

class CSectionData : private CBaseNotification {
private:
    class CSectionInfo : private CBaseNotification {
    public:
        CSectionInfo(uint32_t rStart,
                     uint32_t pSize,
                     iBool R,
                     iBool W,
                     iBool E,
                     const char *SecName = NULL);
        ~CSectionInfo();
        iBool IsValid();
        uint32_t GetStart();
        uint32_t GetEnd();
        uint32_t GetSize();

        iBool IsRangeInSection(uint32_t Addr, uint32_t Size = 0);
        iBool IsRangeExecutable(uint32_t Addr, uint32_t Size = 0);
        iBool IsRangeWriteable(uint32_t Addr, uint32_t Size = 0);
        iBool CanMerge(CSectionInfo &pOther);
        iBool CanMerge(CSectionInfo *pOther);
        const char *GetName() { return SectionName; }
        uint32_t GetFlags() { return Flags; }
        bool operator<(const CSectionInfo &Other) const
        {
            return Start < Other.Start;
        }
        bool operator<=(const CSectionInfo &Other) const
        {
            return Start <= Other.Start;
        }
        bool operator==(const CSectionInfo &Other) const
        {
            return (Start == Other.Start) && (Size == Other.Size);
        }
        bool operator>(const CSectionInfo &Other) const
        {
            return Start > Other.Start;
        }
        bool operator>=(const CSectionInfo &Other) const
        {
            return Start >= Other.Start;
        }
        CSectionInfo &operator+=(CSectionInfo &Other)
        {
            if (!CanMerge(Other)) {
                DynErr("Unable to merge (%x-%x[%x]) and (%x-%x[%x])",
                       GetStart(),
                       GetEnd(),
                       Flags,
                       Other.GetStart(),
                       Other.GetEnd(),
                       Other.Flags);
            }
            if (Other.GetStart() < GetStart()) {
                Start = Other.GetStart();
                Size += (Start - Other.GetStart());
            }
            if (Other.GetEnd() > GetEnd()) {
                Size += (Other.GetEnd() - GetEnd());
            }
            if (!SectionName) {
                if (Other.SectionName) {
                    SectionName = new char[strlen(Other.SectionName) + 1];
                    strcpy(const_cast<char *>(SectionName), Other.SectionName);
                }
            }
            else {
                if (Other.SectionName) {
                    char *newSectionName =
                        new char[strlen(SectionName) +
                                 strlen(Other.SectionName) + 2];
                    strcpy(newSectionName, SectionName);
                    strcat(newSectionName, "+");
                    strcat(newSectionName, Other.SectionName);
                    delete[] SectionName;
                    SectionName = newSectionName;
                }
            }

            return *this;
        }

        std::vector<uint32_t> ScanForTemplate_Forward(
            const unsigned char *Templ,
            const unsigned char *Mask,
            int TemplSize,
            uint32_t Code_Start,
            uint32_t Code_Size)
        {
            std::vector<uint32_t> FoundAddr;
            if (Code_Start) {
                if (!IsRangeInSection(Code_Start, TemplSize)) {
                    return FoundAddr;
                }
            }
            else {
                Code_Start = GetStart();
            }

            uint8_t *Code_Cur = (uint8_t *)(Code_Start);
            uint8_t *Code_End;

            if (Code_Size) {
                Code_End = (uint8_t *)(Code_Start + Code_Size);
                if ((uint32_t)Code_End > GetEnd()) {
                    Code_End = (uint8_t *)GetEnd();
                }
            }
            else {
                Code_End = (uint8_t *)GetEnd();
            }

            Code_End -= TemplSize;

            size_t Result = 0;
            int i;
            bool match;
            while (Code_Cur <= Code_End) {
                match = true;
                for (i = 0; i < TemplSize; i++) {
                    if (!Mask[i]) {
                        continue;
                    }
                    if ((Code_Cur[i] & Mask[i]) != (Templ[i] & Mask[i])) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    FoundAddr.push_back(reinterpret_cast<uint32_t>(Code_Cur));
                }
                Code_Cur++;
            }

            return FoundAddr;
        }

        std::vector<uint32_t> ScanForTemplate_Backward(
            const unsigned char *Templ,
            const unsigned char *Mask,
            int TemplSize,
            uint32_t Code_Start,
            uint32_t Code_Size)
        {
            std::vector<uint32_t> FoundAddr;
            if (Code_Start) {
                if (!IsRangeInSection(Code_Start - TemplSize, TemplSize)) {
                    return FoundAddr;
                }
            }
            else {
                Code_Start = GetEnd();
            }
            uint8_t *Code_End = (uint8_t *)(Code_Start - Code_Size);
            uint8_t *Code_Cur = (uint8_t *)(Code_Start - TemplSize);
            if (Code_Size) {
                Code_End = (uint8_t *)(Code_Start - Code_Size);
                if ((uint32_t)Code_End < GetStart()) {
                    Code_End = (uint8_t *)GetStart();
                }
            }
            else {
                Code_End = (uint8_t *)GetStart();
            }

            size_t Result = 0;
            int i;
            bool match;

            while (Code_Cur >= Code_End) {
                match = true;
                for (i = 0; i < TemplSize; i++) {
                    if (!Mask[i]) {
                        continue;
                    }
                    if ((Code_Cur[i] & Mask[i]) != (Templ[i] & Mask[i])) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    FoundAddr.push_back(reinterpret_cast<uint32_t>(Code_Cur));
                }
                Code_Cur--;
            }
            return FoundAddr;
        }

    private:
        uint32_t Start;
        uint32_t Size;
        union {
            struct {
                iBool IsWriteable : 2;
                iBool IsReadable : 2;
                iBool IsExecuteable : 2;
            };
            uint32_t Flags;
        };
        const char *SectionName;
    };
    typedef std::vector<CSectionInfo *> SectionInfo_t;
    SectionInfo_t Sections;

public:
    CSectionData();
    ~CSectionData();
    typedef SectionInfo_t::iterator SecIter_t;
    typedef SectionInfo_t::size_type SecSize_t;
    iBool Add(uint32_t rStart,
              uint32_t pSize,
              iBool R,
              iBool W,
              iBool E,
              CDynPatcher *Parent,
              const char *SecName = NULL);
    iBool Sort();

    auto CountSections()
    {
        return Sections.size();
    }

    auto FindContainingSection(uint32_t Addr)
    {
        auto CureIt = Sections.begin();

        while (CureIt != Sections.end()) {
            if ((*CureIt)->IsRangeInSection(Addr)) {
                break;
            }
        }
        return CureIt;
    }
    iBool IsRangeInSections(uint32_t Addr, uint32_t Size);
    std::vector<uint32_t> FindRef_Mov(uint32_t StartAddr, uint32_t RefAddress);
    std::vector<uint32_t> FindRef_Push(uint32_t StartAddr, uint32_t RefAddress);
    std::vector<uint32_t> FindRef_Call(uint32_t StartAddr, uint32_t RefAddress);
    std::vector<uint32_t> FindRef_Jmp(uint32_t StartAddr, uint32_t RefAddress);

    std::vector<uint32_t> FindString(const char *str,
                                     uint32_t addr,
                                     bool FullMatch = true);
    std::vector<uint32_t> FindDataRef(uint32_t RefAddr, uint32_t addr);
    std::vector<uint32_t> ScanForTemplate_Forward(const unsigned char *Templ,
                                                  const unsigned char *Mask,
                                                  int TemplSize,
                                                  uint32_t Code_Start = 0,
                                                  uint32_t Code_Size = 0);
    std::vector<uint32_t> ScanForTemplate_Backward(const unsigned char *Templ,
                                                   const unsigned char *Mask,
                                                   int TemplSize,
                                                   uint32_t Code_Start = 0,
                                                   uint32_t Code_Size = 0);
    std::vector<uint32_t> ScanForTemplate_Forward(
        const unsigned char *Templ,
        const unsigned char *Mask,
        int TemplSize,
        const std::vector<uint32_t> &CodeStartVec,
        uint32_t Code_Size = 0);
    std::vector<uint32_t> ScanForTemplate_Backward(
        const unsigned char *Templ,
        const unsigned char *Mask,
        int TemplSize,
        const std::vector<uint32_t> &CodeStartVec,
        uint32_t Code_Size = 0);

    operator bool() { return !!CountSections(); }
    template<typename PrefType>
    std::vector<uint32_t> FindRef(uint32_t StartAddr,
                                  uint32_t RefAddress,
                                  PrefType PrefixValue,
                                  bool Relative)
    {
#pragma pack(push, 1)
        struct prefixref_t {
            PrefType prefix;
            uint32_t Addr;
        };
#pragma pack(pop)
        std::vector<uint32_t> RefVec;
        SecIter_t SecStart;
        if (!StartAddr) {
            SecStart = Sections.begin();
        }
        else {
            SecStart = FindContainingSection(StartAddr);
            if (SecStart == Sections.end()) {
                return RefVec;
            }
        }
        //#pragma omp parallel
        {
            std::for_each(
                SecStart,
                Sections.end(),
                [&StartAddr, RefAddress, PrefixValue, &RefVec, Relative](
                    CSectionInfo *CurSec) {
                    if (!CurSec->IsRangeInSection(StartAddr)) {
                        StartAddr = CurSec->GetStart();
                    }
                    uint32_t EndAddr = CurSec->GetEnd() - sizeof(prefixref_t);
                    prefixref_t *CurInstr;
                    while (StartAddr < EndAddr) {
                        CurInstr = reinterpret_cast<prefixref_t *>(StartAddr);
                        if (CurInstr->prefix == PrefixValue) {
                            if (!Relative) {
                                if (CurInstr->Addr == RefAddress) {
                                    RefVec.push_back(StartAddr);
                                }
                            }
                            else {
                                if ((StartAddr + 5 + CurInstr->Addr) ==
                                    RefAddress) {
                                    RefVec.push_back(StartAddr);
                                }
                            }
                        }
                        StartAddr++;
                    }
                });
        }
        return RefVec;
    }

    template<size_t _TemplSize, size_t _MaskSize>
    std::vector<uint32_t> ScanForTemplate_Forward(
        const unsigned char (&Templ)[_TemplSize],
        const unsigned char (&Mask)[_MaskSize],
        uint32_t Code_Start = NULL,
        uint32_t Code_Size = NULL)
    {
        static_assert(_TemplSize == _MaskSize, "Template&Mask size mismatch");
        return ScanForTemplate_Forward(
            Templ, Mask, _MaskSize - 1, Code_Start, Code_Size);
    }
    template<size_t _TemplSize, size_t _MaskSize>
    std::vector<uint32_t> ScanForTemplate_Backward(
        const unsigned char (&Templ)[_TemplSize],
        const unsigned char (&Mask)[_MaskSize],
        uint32_t Code_Start = NULL,
        uint32_t Code_Size = NULL)
    {
        static_assert(_TemplSize == _MaskSize, "Template&Mask size mismatch");
        return ScanForTemplate_Backward(
            Templ, Mask, _MaskSize - 1, Code_Start, Code_Size);
    }

    template<size_t _TemplSize, size_t _MaskSize>
    std::vector<uint32_t> ScanForTemplate_Forward(
        const unsigned char (&Templ)[_TemplSize],
        const unsigned char (&Mask)[_MaskSize],
        const std::vector<uint32_t> &CodeStartVec,
        uint32_t Code_Size = NULL)
    {
        static_assert(_TemplSize == _MaskSize, "Template&Mask size mismatch");
        return ScanForTemplate_Forward(
            Templ, Mask, _MaskSize - 1, CodeStartVec, Code_Size);
    }
    template<size_t _TemplSize, size_t _MaskSize>
    std::vector<uint32_t> ScanForTemplate_Backward(
        const unsigned char (&Templ)[_TemplSize],
        const unsigned char (&Mask)[_MaskSize],
        const std::vector<uint32_t> &CodeStartVec,
        uint32_t Code_Size = NULL)
    {
        static_assert(_TemplSize == _MaskSize, "Template&Mask size mismatch");
        return ScanForTemplate_Backward(
            Templ, Mask, _MaskSize - 1, CodeStartVec, Code_Size);
    }

private:
    CDynPatcher *Parent;
};
#endif  // CSectionData_h__
