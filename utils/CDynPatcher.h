#ifndef CDynPatcher_h__
#define CDynPatcher_h__
#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include "CSectionData.h"
#undef min
#undef max
#include <vector>
#include <string>
#include <cassert>
#ifndef WIN32
#include <elf.h>
#include <dlfcn.h> 
#endif

// class FindRef_Mov;

#ifdef WIN32
#    include "LinuxTypesSupport.h"
#    define FindPushMov FindRef_Push
#else
#    define FindPushMov FindRef_Mov
#endif

typedef void **VTable_t;

class CDynPatcher : private CBaseNotification {
public:
    struct Pattern {
      std::string pattern;
      std::string mask;
    };
    CDynPatcher();
    ~CDynPatcher();

    iBool Init(void *FuncAddr);
    iBool Init(const char *LibName, iBool ForceLoad = bFalse);

    std::vector<uint32_t> FindString(uint32_t StartAddr,
                                     const char *str,
                                     bool FullMatch = true);
    std::vector<uint32_t> FindDataRef(uint32_t StartAddr, uint32_t RefAddr);
    std::vector<uint32_t> FindRef_Mov(uint32_t StartAddr, uint32_t RefAddress);
    std::vector<uint32_t> FindRef_Push(uint32_t StartAddr, uint32_t RefAddress);
    std::vector<uint32_t> FindRef_Call(uint32_t StartAddr, uint32_t RefAddress);
    std::vector<uint32_t> FindRef_Jmp(uint32_t StartAddr, uint32_t RefAddress);
    std::vector<uint32_t> FindDataRef(uint32_t StartAddr,
                                      const std::vector<uint32_t> &RefVec);
    std::vector<uint32_t> FindRef_Mov(uint32_t StartAddr,
                                      const std::vector<uint32_t> &RefVec);
    std::vector<uint32_t> FindRef_Push(uint32_t StartAddr,
                                       const std::vector<uint32_t> &RefVec);
    std::vector<uint32_t> FindRef_Call(uint32_t StartAddr,
                                       const std::vector<uint32_t> &RefVec);
    std::vector<uint32_t> FindRef_Jmp(uint32_t StartAddr,
                                      const std::vector<uint32_t> &RefVec);

    uintptr_t FindPatternIDA(const std::string& pattern, uintptr_t start = 0) const;



    uint32_t GetCallFromAddress(uint32_t StartAddr);
    uint32_t FindNearestCall(uint32_t StartAddr, uint32_t Size);
    uint32_t FindFuntionStart(uint32_t StartAddr, uint32_t Size);
    void *HookFunctionCall(void *OrigAddr, void *NewAddr);
    uint32_t UnsetFunctionHook(void *OrigAddr);
    uint32_t ChangePtrValue(void *Addr, void *NewValue);
    uint32_t RestorePtrValue(void *Addr);
    iBool AddMemChangeInfo(void *Addr,
                           uint32_t OriginalValue,
                           void *OriginalFuncAddr = NULL);
    uint32_t HookImportFunc(const char *FuncName,
                            void *NewAddr,
                            const char *Library = NULL);
    uint32_t GetOriginalPtrValue(void *addr);
    uint32_t GetImportFuncAddr(const char *FuncName,
                               const char *Library = NULL);
    // uint32_t GetVFuncOffset(void **Vtable, void *FuncAddr);
    uint32_t GetVTableSize(void **VTable);
    uint32_t DumpVtable(void **VTable);

    iBool CloseLib();
    void UnsetHooks();

    const char *GetLibraryName()
    {
        return CBaseNotification::GetFileName(szLibName);
    }
    iBool IsValidCodeAddr(void *Addr);
    iBool IsRangeInLib(uint32_t Addr, uint32_t Size);
    iBool IsRangeInCode(uint32_t Addr, uint32_t Size)
    {
        iBool ret = Code.IsRangeInSections(Addr, Size);
        return ret;
    }
    iBool IsRangeInData(uint32_t Addr, uint32_t Size)
    {
        iBool ret = Data.IsRangeInSections(Addr, Size);
        return ret;
    }
    iBool ContainsAddress(uint32_t Addr)
    {
        auto ret = (Addr >= reinterpret_cast<uint32_t>(this->DllBase)) &&
                   (Addr < (reinterpret_cast<uint32_t>(this->DllBase) +
                            this->DllSize));
        return static_cast<iBool>(ret);
    }
    iBool ContainsAddress(void *Addr)
    {
        iBool ret = ContainsAddress(reinterpret_cast<uint32_t>(Addr));
        return ret;
    }
    void **FindVTable(void *address, iBool MayBeInCode = bFalse);
    CSectionData *GetData() { return &Data; }
    CSectionData *GetCode() { return &Code; }
    void *GetBase() { return DllBase; }
    iBool IsInitialised() { return DllBase ? bTrue : bFalse; }
    void *GetDllHandler()
    {
#ifdef WIN32
        return DllBase;
#else
        return DLHandler;
#endif
    }
    // CSectionData *GetWData(){ return &WData; }

private:
    iBool ParseGenericDllData_PE(void *FileData, uint32_t FileSize);

    iBool ParseGenericDllData_ELF(void *FileData, uint32_t FileSize);
    void *LocateLib(const char *LibName, uint32_t *pLibSize = 0);
    uint32_t GetMemoryFlags(void *addr);
    uint32_t MProtect(void *addr, uint32_t nBytes, uint32_t NewFlags);

    iBool LoadLib(const char *LibName, iBool ForceLoad = bFalse);

public:
    template<typename T>
    iBool FindSymbol(const char *sName, T *pSym)
    {
        if (!DllBase) {
            return bFalse;
        }
#ifdef WIN32
        auto GetProcAdr = [this](const char *Sym) -> FARPROC {
            return GetProcAddress(reinterpret_cast<HMODULE>(DllBase), Sym);
        };
        auto csym = GetProcAdr(sName);
#endif
#ifdef WIN32

#else
        auto csym = (::dlsym)(this->DLHandler, sName);
#endif
        if (!csym) {
#ifndef WIN32
            DynWarn("Can't resolve '%s'\n", sName);
#endif
            return bFalse;
        }
        if (pSym) {
            *pSym = reinterpret_cast<T>(csym);
        }
        return bTrue;
    }

    template<typename T>
    int32_t GetVFuncOffset(T Func)
    {
        void *vAddr = reinterpret_cast<void *>((size_t &)Func);
        int32_t Offset = 0;

#ifdef POSIX
        Offset = (reinterpret_cast<uint32_t>(vAddr) - 1) / 4;
#else
        do {
            if (*reinterpret_cast<unsigned char *>(vAddr) == 0xE9) {
                // E9 XX XX XX XX     jmp     RelativeAddrToJump
                vAddr = reinterpret_cast<void *>(
                    reinterpret_cast<uint32_t>(vAddr) +
                    *reinterpret_cast<int32_t *>(
                        reinterpret_cast<uint32_t>(vAddr) + 1) +
                    5);
            }
            else if (*reinterpret_cast<uint32_t *>(vAddr) == 0x60ff018b) {
                // 8B 01               mov     eax, [ecx]
                // FF 60 XX            jmp     dword ptr[eax + XX]
                Offset =
                    static_cast<int32_t>(*reinterpret_cast<unsigned char *>(
                        reinterpret_cast<uint32_t>(vAddr) + 4));
            }
            else if (*reinterpret_cast<uint32_t *>(vAddr) == 0xa0ff018b) {
                // 8B 01               mov     eax, [ecx]
                // FF A0 XX XX XX XX   jmp     dword ptr[eax + XXXXXXXX]
                Offset = *reinterpret_cast<int32_t *>(
                    reinterpret_cast<uint32_t>(vAddr) + 4);
            }
            else if (*reinterpret_cast<uint32_t *>(vAddr) == 0x20ff018b) {
                // 8B 01                                         mov     eax,
                // [ecx] FF 20                                         jmp dword
                // ptr[eax]
                return 0;
            }
            else {
                return -1;
            }
        } while (!Offset);
        Offset /= sizeof(size_t);
#endif
        return Offset;
    }
    template<typename F, typename T>
    F CastFunc(T Func)
    {
        void *Addr = reinterpret_cast<void *>((size_t &)Func);
        return (F &)(Addr);
    }
    template<typename F, typename T>
    F HookVFunctionCall(void **VTable, F OrigFunc, T NewAddr)
    {
        int32_t Offset = GetVFuncOffset(OrigFunc);
        if (Offset < 0) {
            DynErr("HookVFunctionCall: Unable to get vfunc offset.");
            return 0;
        }
        uint32_t MaxOffset = GetVTableSize(VTable);
        if (!MaxOffset) {
            DynErr("Looks like there is no virtual table at %p", VTable);
            return 0;
        }
        DynMsg("VtSz=%i", MaxOffset);
        if (static_cast<uint32_t>(Offset) >= MaxOffset) {
            DynErr("Invalid offset %i in virtual table %p. Max offset=%i",
                   Offset,
                   VTable,
                   MaxOffset - 1);
            return 0;
        }
        void *OrigAddr = reinterpret_cast<void *>(ChangePtrValue(
            &VTable[Offset], reinterpret_cast<void *>((size_t &)NewAddr)));
        if (!OrigAddr) {
            DynErr("Unable to hook function");
            return 0;
        }
        return (F &)(OrigAddr);
    }

private:
    iBool bSelfLoaded;
    void *DllBase;
#ifndef WIN32
    void *DLHandler;  // for dlopen storage
#endif

    uint32_t DllSize;
    CSectionData Code;
    CSectionData Data;
    enum ImpDynType_e : uint8_t { IDT_NO, IDT_DIRECT, IDT_RELATIVE };
    typedef struct ImportDescription_s {
        ImportDescription_s()
            : LibraryName(0)
            , FunctionName(0)
            , FunctionOrdinal(0)
            , ImpFuncAddr(0)
            , pImpFuncAddr(0)
            , hasDynRel(bFalse)
            , hasDynDirect(bFalse)
            , hasPlt(bFalse){};
        const char *LibraryName;
        const char *FunctionName;
        uint32_t FunctionOrdinal;
        uint32_t ImpFuncAddr;                // for R_386_PC32 .rel.dyn import;
        uint32_t *pImpFuncAddr;              // for Win32 && .rel.plt import;
        std::vector<uint32_t *> DirectPtrs;  // for R_386_32 .rel.dyn import
        union {
            struct {
                iBool hasDynRel : 1;
                iBool hasDynDirect : 1;
                iBool hasPlt : 1;
            };
            uint8_t ImpTypeFlags;
        };
    } ImportDescription_t;

    std::vector<ImportDescription_t *> ImportDescription;

    typedef struct MemEditInfo_s {
        void *pAddr;
        uint32_t OrigValue;
        void *OldFuncAddr;
    } MemEditInfo_t;
    std::vector<MemEditInfo_t *> MemChange;
    char *szLibName;
};

template<typename T>
struct ParseDescription_s {
    T ParseFunc;
    const char *Name;
};
#define AddDynParseFunc(func) {func, #func},

#endif  // CDynPatcher_h__
