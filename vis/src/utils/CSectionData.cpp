#ifdef _WIN32
#    define _CRT_SECURE_NO_WARNINGS
#    include <SDKDDKVer.h>
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#    include <intrin.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#ifndef _WIN32
#    include <unistd.h>
#    define MAX_PATH FILENAME_MAX
#endif

#include <list>
#include "CDynPatcher.h"
#include "CSectionData.h"

pfnPrintFunc CBaseNotification::PrintFunc = NULL;
iBool CBaseNotification::bConsoleActive = bFalse;

CBaseNotification::CBaseNotification(const char *ClassName)
{
    szClassName = NULL;
    if (ClassName) {
        szClassName = new char[strlen(ClassName) + 1];
        strcpy(const_cast<char *>(szClassName), ClassName);
    }
}

CBaseNotification::~CBaseNotification()
{
    if (szClassName) {
        delete[] szClassName;
    }
}

CBaseNotification::MsgDescription_t
    CBaseNotification::MsgDescription[_MsgTypeMax] = {
        {"FATAL ERROR"}, {"ERROR"}, {"WARNING"}, {"TRACE"}};

const char *CBaseNotification::GetBaseDir(const char *ModuleName)
{
    static char BaseDir[MAX_PATH] = {0};
    static const char *ExeName = NULL;
    if (BaseDir[0]) {
        if (ModuleName) {
            strcpy(const_cast<char *>(ModuleName), ExeName);
        }
        return BaseDir;
    }

    const char *LastPathSeparator = NULL;
#ifdef _WIN32
    GetModuleFileNameA(NULL, BaseDir, MAX_PATH);
    if (BaseDir[0]) {
        LastPathSeparator =
            strrchr(reinterpret_cast<const char *>(&BaseDir), '\\');
    }
#else
    size_t ReadLinkLen =
        readlink("/proc/self/exe", BaseDir, sizeof(BaseDir) - 1);
    if (ReadLinkLen > 0) {
        BaseDir[ReadLinkLen] = 0;
    }
    else {
        BaseDir[0] = 0;
    }
    if (BaseDir[0]) {
        LastPathSeparator =
            strrchr(reinterpret_cast<const char *>(&BaseDir), '/');
    }
#endif
    ExeName = BaseDir;
    if (LastPathSeparator) {
        ExeName = LastPathSeparator + 1;
        *const_cast<char *>(LastPathSeparator) = 0;
    }
    if (ModuleName) {
        strcpy(const_cast<char *>(ModuleName), ExeName);
    }
    return BaseDir;
}

const char *CBaseNotification::GetFileName(const char *fpath)
{
    const char *cp = fpath + strlen(fpath);
    const char *BaseDir = GetBaseDir();
    char *IsLocal = strstr(const_cast<char *>(fpath), BaseDir);
    if (IsLocal == fpath) {
        return fpath + strlen(BaseDir) + 1;
    }

    while (size_t(cp) > size_t(fpath)) {
        if (*cp == '\\' || *cp == '/') {
            return cp + 1;
        }
        cp--;
    }
    return cp;
}

void CBaseNotification::SetPrintFunc(pfnPrintFunc NewFunc)
{
    PrintFunc = NewFunc;
}

void CBaseNotification::SetConsoleActive(iBool bActive)
{
    bConsoleActive = bActive;
}

void CBaseNotification::SpewMsg(const char *File,
                                const char *Func,
                                int Line,
                                CBaseNotification::_MsgType Type,
                                char *Fmt,
                                ...)
{
#ifndef _DEBUG
    if (Type == MsgTrace)
        return;
#endif

    if (Type != MsgFatal && !bConsoleActive && !PrintFunc
#ifdef WIN32
        && !IsDebuggerPresent()
#endif
    ) {
        return;
    }

    if (Type >= _MsgTypeMax) {
        Type = MsgWarn;
    }

    static char Buff[0x1000];
    int len = 0;

    len += _snprintf(&Buff[len],
                     sizeof(Buff) - len - 1,
                     "[%s:%s](%s[%s:%i]):",
                     szClassName ? szClassName : "---",
                     MsgDescription[Type].Description,
                     GetFileName(File),
                     Func,
                     Line);

    va_list marker;
    if (Fmt) {
        len += _snprintf(&Buff[len], sizeof(Buff) - len - 1, ":");
        va_start(marker, Fmt);
        len += _vsnprintf(&Buff[len], sizeof(Buff) - len - 1, Fmt, marker);
        va_end(marker);
    }
#ifdef USE_METAMOD
    iBool OutputOk = bFalse;
    if (gpMetaUtilFuncs) {
        switch (Type) {
            case CBaseNotification::MsgFatal:
            case CBaseNotification::MsgErr: {
                if (gpMetaUtilFuncs->pfnLogError) {
                    gpMetaUtilFuncs->pfnLogError(PLID, "%s", Buff);
                    OutputOk = bTrue;
                }
                break;
            }

            default: {
                if (gpMetaUtilFuncs->pfnLogMessage) {
                    gpMetaUtilFuncs->pfnLogMessage(PLID, "%s", Buff);
                    OutputOk = bTrue;
                }
                break;
            }
        }
    }
    if (!OutputOk) {
#endif
        len += _snprintf(&Buff[len], sizeof(Buff) - len - 1, "\r\n");
        if (PrintFunc) {
            PrintFunc(Buff);
        }
        else {
#ifndef POSIX
            if (bConsoleActive)
#endif
            {
                printf("%s", Buff);
            }
#ifdef WIN32
            if (IsDebuggerPresent()) {
                OutputDebugStringA(Buff);
            }
#endif
        }
#ifdef USE_METAMOD
    }
#endif
    if (Type == MsgFatal) {
#ifdef WIN32

        if (!IsDebuggerPresent()) {
            if (MessageBoxA(NULL,
                            Buff,
                            "Engine failure",
                            MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
                //__asm{int 3};
                __debugbreak();
            }
        }
        else {
            //__asm{int 3};
            __debugbreak();
        }
#endif
        exit(0);
    }
}

#define DebMsg(msg, ...) \
    Msg("[%s(%i)]" msg, __FUNCTION__, __LINE__, __VA_ARGS__)

CSectionData::CSectionInfo::CSectionInfo(uint32_t rStart,
                                         uint32_t pSize,
                                         iBool R,
                                         iBool W,
                                         iBool E,
                                         const char *SecName)
    : CBaseNotification("CSectionInfo")
{
    Start = rStart;
    Size = pSize;
    Flags = NULL;
    IsReadable = R;
    IsWriteable = W;
    IsExecuteable = E;
    if (SecName) {
        SectionName = new char[strlen(SecName) + 1];
        strcpy(const_cast<char *>(SectionName), SecName);
    }
}

CSectionData::CSectionInfo::~CSectionInfo()
{
    if (SectionName) {
        delete[] SectionName;
    }
}

iBool CSectionData::CSectionInfo::IsValid()
{
    if (Start == 0 || Size == 0) {
        return bFalse;
    }
    return bTrue;
}

uint32_t CSectionData::CSectionInfo::GetStart()
{
    return Start;
}

uint32_t CSectionData::CSectionInfo::GetEnd()
{
    return Start + Size;
}

uint32_t CSectionData::CSectionInfo::GetSize()
{
    return Size;
}

iBool CSectionData::CSectionInfo::IsRangeInSection(uint32_t Addr,
                                                   uint32_t rSize /*= NULL*/)
{
    uint32_t Addr_End = Addr + rSize - 1;
    if (Addr >= GetStart() && Addr_End <= (GetEnd()))
        return bTrue;
    return bFalse;
}

iBool CSectionData::CSectionInfo::IsRangeExecutable(uint32_t Addr,
                                                    uint32_t rSize /*= NULL*/)
{
    if (!IsRangeInSection(Addr, rSize)) {
        return bFalse;
    }
    return IsExecuteable ? bTrue : bFalse;
}

iBool CSectionData::CSectionInfo::IsRangeWriteable(uint32_t Addr,
                                                   uint32_t rSize /*= NULL*/)
{
    if (!IsRangeInSection(Addr, rSize)) {
        return bFalse;
    }
    return IsWriteable ? bTrue : bFalse;
}

iBool CSectionData::CSectionInfo::CanMerge(CSectionInfo &pOther)
{
    // if (((pOther.GetStart() <= GetStart()) && (pOther.GetEnd() >=
    // GetStart())) || ((pOther.GetStart() <= GetEnd()) &&
    // (pOther.GetEnd()>=GetEnd())))
    if (((pOther.GetEnd() >= GetStart()) && (pOther.GetEnd() <= GetEnd())) ||
        ((pOther.GetStart() >= GetStart()) &&
         (pOther.GetStart() <= GetEnd()))) {
        if (Flags == pOther.Flags) {
            // DynMsg("Section '%s'(%x-%x[%x]) and '%s'(%x-%x[%x]) can be
            // merged",SectionName, GetStart(), GetEnd(), Flags,
            // pOther.SectionName,pOther.GetStart(), pOther.GetEnd(),
            // pOther.Flags);
            return bTrue;
        }
        else {
            // DynErr("ERROR:Section '%s'(%x-%x[%x]) and '%s'(%x-%x[%x]) can't
            // be merged", SectionName, GetStart(), GetEnd(), Flags,
            // pOther.SectionName, pOther.GetStart(), pOther.GetEnd(),
            // pOther.Flags);
        }
    }
    return bFalse;
}

iBool CSectionData::CSectionInfo::CanMerge(CSectionInfo *pOther)
{
    return CanMerge(*pOther);
    /*
    if (pOther->GetEnd() >= GetStart() || pOther->GetStart() <= GetEnd())
    {
       if (Flags == pOther->Flags)
       {
          DynMsg("Section (%x-%x[%x]) and (%x-%x[%x]) can be merged",
    GetStart(), GetEnd(), Flags, pOther->GetStart(), pOther->GetEnd(),
    pOther->Flags); return bTrue;
       }
       else
       {
          DynErr("ERROR:Section (%x-%x[%x]) and (%x-%x[%x]) can't be merged",
    GetStart(), GetEnd(), Flags, pOther->GetStart(), pOther->GetEnd(),
    pOther->Flags);
       }
    }
    return bFalse;
    */
}

CSectionData::CSectionData()
    : CBaseNotification("CSectionData")
{
    this->Parent = 0;
    this->Sections.clear();
}

CSectionData::~CSectionData()
{
    for (auto a = Sections.begin(); a != Sections.end(); ++a) {
        delete (*a);
    }
}

iBool CSectionData::Add(uint32_t rStart,
                        uint32_t pSize,
                        iBool R,
                        iBool W,
                        iBool E,
                        CDynPatcher *pParent,
                        const char *SecName)
{
    if (!Parent) {
        this->Parent = pParent;
    }
    else if (this->Parent != pParent) {
        DynErr("Invalid parent for section '%s'. Was %p, now %p.",
               SecName,
               this->Parent,
               pParent);
        return bFalse;
    }
    CSectionInfo *Sec = new CSectionInfo(
        reinterpret_cast<uint32_t>(pParent->GetBase()) + rStart,
        pSize,
        R,
        W,
        E,
        SecName);
    if (!Sec->IsValid()) {
        delete Sec;
        DynErr("Invalid  section '%s'", SecName);
        return bFalse;
    }
    Sections.push_back(Sec);
    return bTrue;
}

iBool CSectionData::Sort()
{
    // std::sort(Sections.begin(), Sections.end());
    std::sort(
        Sections.begin(),
        Sections.end(),
        [](const CSectionInfo *a, const CSectionInfo *b) { return *a < *b; });
    auto Cur = Sections.begin();
    iBool CanMerge = bFalse;
    // DynMsg("Sort Start");
    while (Cur != Sections.end() - 1) {
        CanMerge = (*Cur)->CanMerge(*(Cur + 1));
        // DynMsg("%smerge [(%x-%x[%x])+(%x-%x[%x])] '%s'+'%s'",CanMerge?"Can
        // ":"Can't",(*Cur)->GetStart(), (*Cur)->GetEnd(),
        // (*Cur)->GetFlags(),(*(Cur + 1))->GetStart(), (*(Cur + 1))->GetEnd(),
        // (*(Cur + 1))->GetFlags(),(*Cur)->GetName(),(*(Cur + 1))->GetName());

        if (CanMerge != bFalse) {
            *(*Cur) += *(*(Cur + 1));
            Sections.erase(
                std::remove(Sections.begin(), Sections.end(), *(Cur + 1)),
                Sections.end());
        }
        else {
            ++Cur;
        }
    }
    // DynMsg("Sort End");
    /*
   DynMsg("SecDump Start");
   for(CSectionInfo *sec:Sections)
   {
      DynMsg("(%x-%x[%x])'%s'",sec->GetStart(), sec->GetEnd(), sec->GetFlags(),
   sec->GetName());
   }
   DynMsg("SecDump End");
    */
    return bTrue;
}

std::vector<uint32_t> CSectionData::FindRef_Mov(uint32_t StartAddr,
                                                uint32_t RefAddress)
{
    std::vector<uint32_t> FoundRefs;
    std::vector<uint32_t> res;
    res = FindRef(StartAddr,
                  RefAddress,
                  static_cast<uint8_t>(0xB8),
                  bFalse);  // mov     eax, offset RefAddress
    if (!res.empty()) {
        FoundRefs.insert(FoundRefs.end(), res.begin(), res.end());
    }
    res = FindRef(StartAddr,
                  RefAddress,
                  static_cast<uint8_t>(0xB9),
                  bFalse);  // mov     ecx, offset RefAddress
    if (!res.empty()) {
        FoundRefs.insert(FoundRefs.end(), res.begin(), res.end());
    }
    unsigned char ScanDataC7[] = "\xC7\x04\x24\x00\x00\x00\x00";
    unsigned char ScanMaskC7[] = "\xFF\xFF\xFF\xFF\xFF\xFF\xFF";
    *reinterpret_cast<uint32_t *>(&ScanDataC7[3]) = RefAddress;
    res = ScanForTemplate_Forward(ScanDataC7, ScanMaskC7, StartAddr);
    if (!res.empty()) {
        FoundRefs.insert(FoundRefs.end(), res.begin(), res.end());
    }
    return FoundRefs;
}

std::vector<uint32_t> CSectionData::FindRef_Push(uint32_t StartAddr,
                                                 uint32_t RefAddress)
{
    auto res =
        FindRef(StartAddr, RefAddress, static_cast<uint8_t>(0x68), bFalse);
    return res;
}

std::vector<uint32_t> CSectionData::FindRef_Call(uint32_t StartAddr,
                                                 uint32_t RefAddress)
{
    auto res =
        FindRef(StartAddr, RefAddress, static_cast<uint8_t>(0xE8), bTrue);
    return res;
}

std::vector<uint32_t> CSectionData::FindRef_Jmp(uint32_t StartAddr,
                                                uint32_t RefAddress)
{
    auto res =
        FindRef(StartAddr, RefAddress, static_cast<uint8_t>(0xE9), bTrue);
    return res;
}

iBool CSectionData::IsRangeInSections(uint32_t Addr, uint32_t Size)
{
    uint32_t Addr_End = Addr + Size - 1;
    for (auto i = Sections.begin(); i < Sections.end(); ++i) {
        if ((*i)->IsRangeInSection(Addr, Size)) {
            return bTrue;
        }
    }
    return bFalse;
}

std::vector<uint32_t> CSectionData::FindString(const char *str,
                                               uint32_t addr,
                                               bool FullMatch)
{
    size_t StrLen = strlen(str) + (FullMatch ? 1 : 0);
    std::vector<uint32_t> StrVec;
    char *cs = (char *)addr;
    SecIter_t SecStart;
    if (!addr) {
        SecStart = Sections.begin();
    }
    else {
        SecStart = FindContainingSection(addr);
        if (SecStart == Sections.end()) {
            return StrVec;
        }
    }

    std::for_each(
        SecStart,
        Sections.end(),
        [this, str, StrLen, &cs, &StrVec](CSectionInfo *CurSec) {
            char *cs_end;
            if (!CurSec->IsRangeInSection(reinterpret_cast<uint32_t>(cs))) {
                cs = reinterpret_cast<char *>(CurSec->GetStart());
            }
            cs_end = reinterpret_cast<char *>(CurSec->GetEnd() - StrLen - 1);

            while (cs <= cs_end) {
                if (!memcmp(str, cs, StrLen)) {
                    StrVec.push_back(reinterpret_cast<uint32_t>(cs));
                }
                cs++;
            }
        });

    return StrVec;
}

std::vector<uint32_t> CSectionData::FindDataRef(uint32_t RefAddr, uint32_t addr)
{
    uint32_t *cs = (uint32_t *)addr;
    std::vector<uint32_t> RefVec;
    SecIter_t SecStart;
    if (!addr) {
        SecStart = Sections.begin();
    }
    else {
        SecStart = FindContainingSection(addr);
        if (SecStart == Sections.end()) {
            return RefVec;
        }
    }
    std::for_each(
        SecStart,
        Sections.end(),
        [RefAddr, &cs, &RefVec](CSectionInfo *CurSec) {
            uint32_t *cs_end;
            if (!CurSec->IsRangeInSection(reinterpret_cast<uint32_t>(cs))) {
                cs = reinterpret_cast<uint32_t *>(CurSec->GetStart());
            }
            cs_end = reinterpret_cast<uint32_t *>(CurSec->GetEnd() - 4);
            while (cs <= cs_end) {
                if (*cs == RefAddr) {
                    RefVec.push_back(reinterpret_cast<uint32_t>(cs));
                }
                cs++;
            }
        });

    return RefVec;
}

std::vector<uint32_t> CSectionData::ScanForTemplate_Forward(
    const unsigned char *Templ,
    const unsigned char *Mask,
    int TemplSize,
    uint32_t Code_Start,
    uint32_t Code_Size)
{
    std::vector<uint32_t> FoundAddr;
    std::vector<uint32_t> res;
    for (auto s = Sections.begin(); s < Sections.end(); ++s) {
        res = (*s)->ScanForTemplate_Forward(
            Templ, Mask, TemplSize, Code_Start, Code_Size);
        if (!res.empty()) {
            FoundAddr.insert(FoundAddr.end(), res.begin(), res.end());
        }
    }
    return FoundAddr;
}

std::vector<uint32_t> CSectionData::ScanForTemplate_Forward(
    const unsigned char *Templ,
    const unsigned char *Mask,
    int TemplSize,
    const std::vector<uint32_t> &CodeStartVec,
    uint32_t Code_Size /*= NULL*/)
{
    std::vector<uint32_t> Refs, res;
    for (auto CodeStart : CodeStartVec) {
        res = ScanForTemplate_Forward(
            Templ, Mask, TemplSize, CodeStart, Code_Size);
        if (!res.empty()) {
            Refs.insert(Refs.end(), res.begin(), res.end());
        }
    }
    return Refs;
}

std::vector<uint32_t> CSectionData::ScanForTemplate_Backward(
    const unsigned char *Templ,
    const unsigned char *Mask,
    int TemplSize,
    uint32_t Code_Start,
    uint32_t Code_Size)
{
    std::vector<uint32_t> FoundAddr;
    std::vector<uint32_t> res;
    for (auto s = Sections.begin(); s < Sections.end(); ++s) {
        res = (*s)->ScanForTemplate_Backward(
            Templ, Mask, TemplSize, Code_Start, Code_Size);
        if (!res.empty()) {
            FoundAddr.insert(FoundAddr.end(), res.begin(), res.end());
        }
    }
    return FoundAddr;
}

std::vector<uint32_t> CSectionData::ScanForTemplate_Backward(
    const unsigned char *Templ,
    const unsigned char *Mask,
    int TemplSize,
    const std::vector<uint32_t> &CodeStartVec,
    uint32_t Code_Size /*= NULL*/)
{
    std::vector<uint32_t> Refs, res;
    for (auto CodeStart : CodeStartVec) {
        res = ScanForTemplate_Backward(
            Templ, Mask, TemplSize, CodeStart, Code_Size);
        if (!res.empty()) {
            Refs.insert(Refs.end(), res.begin(), res.end());
        }
    }
    return Refs;
}
