//#include <stdafx.h>
#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#    include <SDKDDKVer.h>
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#    include <winnt.h>
#else
#    include <cxxabi.h>
#    include <dlfcn.h>
#    include <elf.h>
#    include <sys/mman.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif
#include <algorithm>

/*
Import:
https://github.com/shoumikhin/ELF-Hook/blob/master/elf_hook.c
http://habrahabr.ru/post/106107/
*/
#ifdef WIN32
#    pragma pack(push, 1)
#else
#    pragma push()
#    pragma pack(1)
#endif
struct FuncHook2_s {
    unsigned char _jmp;  // e9
    int addr;
};
#ifdef WIN32
#    pragma pack(pop)
#else
#    pragma pop()
#endif

#ifndef PAGESIZE
#    define PAGESIZE 4096
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#    define MAX_PATH FILENAME_MAX
#    include <tier1/strtools.h>
#else
#    ifndef max
#        define max(a, b) (((a) > (b)) ? (a) : (b))
#    endif
#endif
#include "CDynPatcher.h"
#include "CSectionData.h"

#ifdef _WIN32
#    include "LinuxTypesSupport.h"
#    define ParseGenericDllData ParseGenericDllData_PE
#else
#    define PAGE_NOACCESS          (PROT_NONE)
#    define PAGE_READONLY          (PROT_READ)
#    define PAGE_READWRITE         (PROT_READ | PROT_WRITE)
#    define PAGE_WRITECOPY         PAGE_READWRITE  //???
#    define PAGE_EXECUTE           (PROT_EXEC)
#    define PAGE_EXECUTE_READ      (PROT_EXEC | PROT_READ)
#    define PAGE_EXECUTE_READWRITE (PROT_EXEC | PROT_READ | PROT_WRITE)
#    define PAGE_EXECUTE_WRITECOPY PAGE_EXECUTE_READWRITE
#    define ParseGenericDllData    ParseGenericDllData_ELF
#endif

CDynPatcher::CDynPatcher()
    : DllBase(0)
    , bSelfLoaded(bFalse)
    , szLibName(0)
    , CBaseNotification("CDynPatcher")
#ifdef POSIX
    , DLHandler(0)
#endif
{
    MemChange.clear();
    szLibName = 0;
#ifdef POSIX
    DLHandler = NULL;
#endif
}

CDynPatcher::~CDynPatcher()
{
    CloseLib();
}

iBool CDynPatcher::Init(void *FuncAddr)
{
    if (!FuncAddr) {
        return bFalse;
    }

    char szTmpName[4096];
    szTmpName[0] = 0;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION mem;
    VirtualQuery(FuncAddr, &mem, sizeof(mem));
    GetModuleFileNameA(reinterpret_cast<HMODULE>(mem.AllocationBase),
                       szTmpName,
                       sizeof(szTmpName) - 1);
#else
    Dl_info info;
    if (dladdr(FuncAddr, &info) && info.dli_fbase && info.dli_fname) {
        if (info.dli_fname) {
            strcpy(szTmpName, info.dli_fname);
        }
    }
#endif
    if (szTmpName[0] != 0) {
        DynMsg("Library \"%s\" was found by addr %p.", szTmpName, FuncAddr);
        return Init(szTmpName, bFalse);
    }
    else {
        DynErr("Failed  to find library at %p", FuncAddr);
        return bFalse;
    }
    return bFalse;
}

iBool CDynPatcher::Init(const char *LibName, iBool ForceLoad)
{
    if (!LibName) {
        szLibName = "<<===NO LIBRARY NAME===>>";
        return bFalse;
    }
    printf("Loading lib:%s\n", LibName);
    if (DllBase) {
        CloseLib();
    }
    if (LoadLib(LibName, ForceLoad) == bFalse) {
        DynErr("Unable to load \"%s\"", LibName);
        return bFalse;
    }

    char szTmpName[4096];
    szTmpName[0] = 0;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION mem;
    VirtualQuery(DllBase, &mem, sizeof(mem));
    GetModuleFileNameA(reinterpret_cast<HMODULE>(mem.AllocationBase),
                       szTmpName,
                       sizeof(szTmpName) - 1);
#else
    Dl_info info;
    if (dladdr(DllBase, &info) && info.dli_fbase && info.dli_fname) {
        if (info.dli_fname) {
            strcpy(szTmpName, info.dli_fname);
        }
    }
#endif
    FILE *fl = fopen(szTmpName, "rb");
    int LibSize;
    void *LibBuf;
    if (fl == NULL) {
        DynErr("Failed to open '%s' for read\n", szLibName);
        return bFalse;
    }

    fseek(fl, 0, SEEK_END);
    LibSize = ftell(fl);
    fseek(fl, 0, SEEK_SET);

    // printf("szLibName=%s ,LibSize=%x\n",szLibName,LibSize);
    if (LibSize < 0)
        LibSize = 0;
    LibBuf = malloc(LibSize + 4);
    fread(LibBuf, 1, LibSize, fl);
    fclose(fl);

    if (!ParseGenericDllData(LibBuf, LibSize)) {
        DynErr("Failed to parse \"%s\"", szLibName);
        free(LibBuf);
        return bFalse;
    }

    free(LibBuf);
    DynMsg("\"%s\" parsed", szLibName);
    return bTrue;
}

void *CDynPatcher::LocateLib(const char *LibName, uint32_t *pLibSize)
{
    // DynWarn("Locating lib:%s\n",LibName);
    void *ADDR = NULL;
    if (pLibSize) {
        *pLibSize = NULL;
    }
#ifdef WIN32
    ADDR = GetModuleHandleA(LibName);
    if (!ADDR) {
        return NULL;
    }
    auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(ADDR);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        DynErr("Invalid IMAGE_DOS_SIGNATURE in \"%s\"", LibName);
        return NULL;
    }

    auto NTHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>((size_t)dosHeader +
                                                         dosHeader->e_lfanew);
    if (NTHeaders->Signature != IMAGE_NT_SIGNATURE) {
        DynErr("Invalid IMAGE_NT_SIGNATURE in \"%s\"", LibName);
        return NULL;
    }
    auto opt = &NTHeaders->OptionalHeader;
    if (opt->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) {
        DynErr("Invalid IMAGE_NT_OPTIONAL_HDR_MAGIC in \"%s\"", LibName);
        return NULL;
    }
    if (pLibSize) {
        *pLibSize = opt->SizeOfImage;
    }

#else

    pid_t pid = getpid();
    char file[255];
    char buffer[2048];
    Dl_info DlInfo;
    void *start = NULL;
    void *end = NULL;
    const char *DlLibPath;
    int value;
    uint32_t AddrStart;
    uint32_t AddrEnd;
    snprintf(file, sizeof(file) - 1, "/proc/%d/maps", pid);
    FILE *fp = fopen(file, "rt");
    if (fp) {
        while (!feof(fp)) {
            if (fgets(buffer, sizeof(buffer) - 1, fp) == NULL) {
                DynWarn("%s:%i:Exiting\n", __FUNCTION__, __LINE__);
                return NULL;
            }

            sscanf(buffer,
                   "%lx-%lx",
                   reinterpret_cast<uint32_t *>(&start),
                   reinterpret_cast<uint32_t *>(&end));
            if (dladdr(start, &DlInfo) == 0) {
                continue;
            }
            // DynWarn("%s:%i:DlInfo.dli_fname=%s\n",__FUNCTION__,__LINE__,DlInfo.dli_fname);
            DlLibPath = CBaseNotification::GetFileName(DlInfo.dli_fname);
            if (strcmp(LibName, DlLibPath) == 0) {
                DynWarn("%s:%i:strcmp OK\n", __FUNCTION__, __LINE__);
                ADDR = DlInfo.dli_fbase;
                if (pLibSize) {
                    //*pLibSize = (uint32_t)end - (uint32_t)start;
                    AddrStart = reinterpret_cast<uint32_t>(start);
                    while (!feof(fp)) {
                        if (fgets(buffer, sizeof(buffer) - 1, fp) == NULL) {
                            DynWarn("%s:%i:Exiting\n", __FUNCTION__, __LINE__);
                            return 0;
                        }
                        sscanf(buffer,
                               "%lx-%lx %*s %*s %*s %d",
                               reinterpret_cast<long unsigned int *>(&start),
                               reinterpret_cast<long unsigned int *>(&end),
                               &value);

                        if (!value) {
                            *pLibSize = AddrEnd - AddrStart;
                            if (ADDR) {
                                delete[] szLibName;
                                szLibName =
                                    new char[strlen(DlInfo.dli_fname) + 1];
                                strcpy(szLibName, DlInfo.dli_fname);
                            }
                            break;
                        }
                        else {
                            AddrEnd = reinterpret_cast<uint32_t>(end);
                            //*pLibSize += (unsigned long)end - (unsigned
                            //long)start;
                        }
                    }
                }

                break;
            }
        }
        fclose(fp);
    }
#endif
    return ADDR;
}

uint32_t CDynPatcher::GetMemoryFlags(void *addr)
{
    uint32_t Flags = PAGE_NOACCESS;
#ifdef WIN32
    MEMORY_BASIC_INFORMATION MemInfo;

    if (VirtualQuery(addr, &MemInfo, sizeof(MemInfo)) == sizeof(MemInfo)) {
        Flags = MemInfo.Protect;
    }

#else
    pid_t pid = getpid();
    char file[255];
    char buffer[2048];
    uint32_t Start;
    uint32_t End;
    char sFlags[30];

    snprintf(file, sizeof(file) - 1, "/proc/%d/maps", pid);
    FILE *fp = fopen(file, "rt");
    if (fp) {
        while (!feof(fp)) {
            if (fgets(buffer, sizeof(buffer) - 1, fp) == NULL) {
                return PAGE_NOACCESS;
            }

            sscanf(buffer, "%lx-%lx %s", &Start, &End, &sFlags);
            if (reinterpret_cast<uint32_t>(addr) >= Start &&
                reinterpret_cast<uint32_t>(addr) <= End) {
                if (sFlags[0] == 'r') {
                    Flags |= PROT_READ;
                }
                if (sFlags[1] == 'w') {
                    Flags |= PROT_WRITE;
                }
                if (sFlags[2] == 'x') {
                    Flags |= PROT_EXEC;
                }
                // DynMsg("Region=[%x->%p->%x](%s) [%x]", Start, addr, End,
                // sFlags, Flags);
                break;
            }
        }
        fclose(fp);
    }
#endif
    return Flags;
}

uint32_t CDynPatcher::MProtect(void *addr, uint32_t nBytes, uint32_t NewFlags)
{
    uint32_t OldProtect = 0;
#ifdef WIN32

    if (VirtualProtect(
            addr, nBytes, NewFlags, reinterpret_cast<PDWORD>(&OldProtect))) {
        return OldProtect;
    }
#else
    uint32_t nPages = nBytes / PAGESIZE + 1;
    OldProtect = GetMemoryFlags(addr);
    void *paddr = reinterpret_cast<void *>((reinterpret_cast<size_t>(addr)) &
                                           ~(PAGESIZE - 1));
    // DynMsg("pid=%i,addr=%p,
    // paddr=%p,nPages=%i,OldProtect=%i",getpid(),addr,paddr,nPages,OldProtect);
    if (OldProtect && !mprotect(paddr, PAGESIZE * nPages, NewFlags)) {
        return OldProtect;
    }
#endif
    return PAGE_NOACCESS;
}

iBool CDynPatcher::LoadLib(const char *LibName, iBool ForceLoad)
{
    if (!LibName) {
        return bFalse;
    }

    DynMsg("Loading library:%s%s", LibName, ForceLoad ? " (Force)" : "");
    if (DllBase) {
        DynErr("Library \"%s\" already loaded", szLibName);
        return bFalse;
    }
    szLibName = new char[strlen(LibName) + 1];
    strcpy(szLibName, LibName);
    const char *LibFileName = GetFileName(szLibName);
    DynMsg("LibName=\"%s\"; LibFileName=\"%s\"", szLibName, LibFileName);
    DllBase = LocateLib(LibFileName, &DllSize);
    if (DllBase) {
        bSelfLoaded = bFalse;
#ifdef POSIX
        DLHandler = dlopen(szLibName, RTLD_NOW);
        DynMsg("dlopen(\"%s\")=%p\n", szLibName, DLHandler);
        dlclose(DLHandler);
#endif
        return bTrue;
    }
    else if (ForceLoad != bFalse) {
#ifdef WIN32
        DllBase = LoadLibraryA(LibName);
#else  // POSIX
        DLHandler = dlopen(LibName, RTLD_NOW);
#endif
        DllBase = LocateLib(LibFileName, &DllSize);
        if (DllBase) {
            bSelfLoaded = bTrue;
            return bTrue;
        }
    }

    return bFalse;
}

iBool CDynPatcher::CloseLib()
{
    if (!DllBase)
        return bFalse;

    DynMsg("Closing \"%s\"", szLibName);
    UnsetHooks();

    for (auto ImpDescr : ImportDescription) {
        delete ImpDescr;
    }
    ImportDescription.clear();

    if (szLibName) {
        delete[] szLibName;
    }

    if (bSelfLoaded) {
#ifndef _WIN32
        if (DLHandler) {
            dlclose(DLHandler);
            DLHandler = NULL;
        }
#else
        if (DllBase) {
            FreeLibrary(reinterpret_cast<HMODULE>(DllBase));
        }
#endif
    }
    DllBase = NULL;
    return bTrue;
}

#ifdef WIN32
iBool CDynPatcher::ParseGenericDllData_PE(void *FileData, uint32_t FileSize)
{
    if (!this->DllBase) {
        DynErr("DllBase not set");
        return bFalse;
    }

    // DynMsg("Base addr=%p\n", this->DllBase);
    if (FileSize < sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS) +
                       sizeof(IMAGE_OPTIONAL_HEADER) +
                       sizeof(IMAGE_SECTION_HEADER)) {
        DynErr("File too small.");
        return bFalse;
    }

    int i = 0;
    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(FileData);

    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        DynErr("Invalid dos header signature");
        return bFalse;
    }

    PIMAGE_NT_HEADERS NTHeaders =
        (PIMAGE_NT_HEADERS)((size_t)FileData + dosHeader->e_lfanew);
    if (NTHeaders->Signature != IMAGE_NT_SIGNATURE) {
        DynErr("Invalid NT Headers signature");
        return bFalse;
    }
    PIMAGE_OPTIONAL_HEADER OptionalHeader = &NTHeaders->OptionalHeader;
    if (OptionalHeader->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) {
        DynErr("Invalid IMAGE_NT_OPTIONAL_HDR_MAGIC");
        return bFalse;
    }
    PIMAGE_SECTION_HEADER cSection =
        (PIMAGE_SECTION_HEADER)((size_t)(&NTHeaders->OptionalHeader) +
                                NTHeaders->FileHeader.SizeOfOptionalHeader);

    PIMAGE_SECTION_HEADER CodeSection = NULL;

    char SectionName[IMAGE_SIZEOF_SHORT_NAME + 1];

    for (i = 0; i < NTHeaders->FileHeader.NumberOfSections; i++, cSection++) {
        memcpy(&SectionName, cSection->Name, IMAGE_SIZEOF_SHORT_NAME);
        SectionName[IMAGE_SIZEOF_SHORT_NAME] = 0;
        if (cSection->Characteristics & IMAGE_SCN_CNT_CODE) {
            if (cSection->Characteristics & IMAGE_SCN_MEM_EXECUTE &&
                cSection->Characteristics & IMAGE_SCN_MEM_READ) {
                Code.Add((uint32_t)cSection->VirtualAddress,
                         cSection->Misc.VirtualSize,
                         bTrue,
                         cSection->Characteristics & IMAGE_SCN_MEM_WRITE
                             ? bTrue
                             : bFalse,
                         bTrue,
                         this,
                         SectionName);
            }
            else {
                DynMsg("Non-executable or not-readable section '%s' with code!",
                       SectionName);
            }
        }
        else if (cSection->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA ||
                 cSection->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) {
            Data.Add((uint32_t)cSection->VirtualAddress,
                     cSection->Misc.VirtualSize,
                     bTrue,
                     cSection->Characteristics & IMAGE_SCN_MEM_WRITE ? bTrue
                                                                     : bFalse,
                     bFalse,
                     this,
                     SectionName);
        }
        else {
            // Data.Add((uint32_t)this->DllHandler + cSection->VirtualAddress,
            // cSection->Misc.VirtualSize, this, SectionName);
        }
    }
    if (!Code) {
        DynErr("Code section not found");
        return bFalse;
    }

    if (!Data) {
        DynErr("Data sections not found", __FUNCTION__);
        return bFalse;
    }

    Code.Sort();
    Data.Sort();

    // DynMsg("ENTRY_IMPORT.size=%i",
    // NTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size);
    // DynMsg("ENTRY_IAT.size=%i",
    // NTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size);
    // DynMsg("ENTRY_BOUND_IMPORT.size=%i",
    // NTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size);
    // DynMsg("ENTRY_DELAY_IMPORT.size=%i",
    // NTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].Size);
    // DynMsg("ENTRY_BASERELOC.size=%i",
    // NTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

    PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor;
    PIMAGE_THUNK_DATA pThunkDataName;
    PIMAGE_THUNK_DATA pThunkDataFunc;
    char *DllName, *iFuncName;
    void *ImpFuncPtr = NULL;
    uint32_t ImpFuncOrd = NULL;
    ImportDescription_t *tImpDescr;
    CDynPatcher tdp;
    if (NTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
            .Size !=
        0) /*if size of the table is 0 - Import Table does not exist */
    {
        // ImpDump = fopen(FName, "wt");
        pImportDescriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
            reinterpret_cast<DWORD>(DllBase) +
            OptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                .VirtualAddress);

        while (pImportDescriptor->Name) {
            DllName = reinterpret_cast<char *>(
                reinterpret_cast<DWORD>(DllBase) + pImportDescriptor->Name);

            if (DllName) {
                pThunkDataName = reinterpret_cast<PIMAGE_THUNK_DATA>(
                    reinterpret_cast<DWORD>(DllBase) +
                    pImportDescriptor->OriginalFirstThunk);
                pThunkDataFunc = reinterpret_cast<PIMAGE_THUNK_DATA>(
                    reinterpret_cast<DWORD>(DllBase) +
                    pImportDescriptor->FirstThunk);
                while (pThunkDataName->u1.AddressOfData) {
                    iFuncName = NULL;
                    if (!IMAGE_SNAP_BY_ORDINAL(pThunkDataName->u1.Ordinal)) {
                        iFuncName = reinterpret_cast<char *>(
                            reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
                                reinterpret_cast<DWORD>(DllBase) +
                                pThunkDataName->u1.ForwarderString)
                                ->Name);
                        ImpFuncOrd = NULL;
                    }
                    else {
                        iFuncName = NULL;
                        ImpFuncOrd = IMAGE_ORDINAL(pThunkDataName->u1.Ordinal);
                    }

                    tImpDescr = new ImportDescription_t;
                    tImpDescr->LibraryName = DllName;
                    tImpDescr->FunctionName = iFuncName;
                    tImpDescr->hasPlt = bTrue;
                    tImpDescr->FunctionOrdinal = ImpFuncOrd;
                    tImpDescr->pImpFuncAddr = reinterpret_cast<uint32_t *>(
                        &pThunkDataFunc->u1.Function);
                    tImpDescr->ImpFuncAddr = *tImpDescr->pImpFuncAddr;
                    ImportDescription.push_back(tImpDescr);
                    pThunkDataName++;
                    pThunkDataFunc++;
                }
                char szTmpName[4096];
                szTmpName[0] = 0;
                MEMORY_BASIC_INFORMATION mem;
                VirtualQuery(reinterpret_cast<void *>(tImpDescr->ImpFuncAddr),
                             &mem,
                             sizeof(mem));
                GetModuleFileNameA(
                    reinterpret_cast<HMODULE>(mem.AllocationBase),
                    szTmpName,
                    sizeof(szTmpName) - 1);
                // DynWarn("Imp:%s", GetFileName(szTmpName));
            }
            pImportDescriptor++;
        }
    }
    return bTrue;
}

#endif
iBool CDynPatcher::ParseGenericDllData_ELF(void *FileData, uint32_t FileSize)
{
    if (!this->DllBase) {
        DynErr("DllBase not set");
        return bFalse;
    }

    if (FileSize < sizeof(Elf32_Ehdr)) {
        DynErr("bad library file (header)");
        return bFalse;
    }

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)FileData;
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        DynErr("ELF Signature mismatch (got %.2X %.2X %.2X %.2X)\n",
               ehdr->e_ident[0],
               ehdr->e_ident[1],
               ehdr->e_ident[2],
               ehdr->e_ident[3]);
        return bFalse;
    }

    int i;

    if (sizeof(Elf32_Phdr) > ehdr->e_phentsize)
        return bFalse;

    if (sizeof(Elf32_Shdr) > ehdr->e_shentsize)
        return bFalse;

    if (FileSize < (ehdr->e_phoff + ehdr->e_phentsize * ehdr->e_phnum)) {
        DynErr("bad library file (program headers)");
        return bFalse;
    }

    if (FileSize < (ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shnum)) {
        DynErr("bad library file (section headers)");
        return bFalse;
    }

    uint32_t StringSectionHdrOff =
        ehdr->e_shoff + ehdr->e_shstrndx * ehdr->e_shentsize;
    if (FileSize < (StringSectionHdrOff + ehdr->e_shentsize)) {
        DynErr("bad library file (string section not found)");
        return bFalse;
    }
    Elf32_Shdr *shstrHdr =
        (Elf32_Shdr *)((size_t)FileData + StringSectionHdrOff);
    char *StringTable = (char *)((size_t)FileData + shstrHdr->sh_offset);

    auto GetSectionHdr = [ehdr, FileData](uint32_t SectID) -> Elf32_Shdr * {
        if (SectID >= ehdr->e_shnum) {
            return NULL;
        }
        return reinterpret_cast<Elf32_Shdr *>(
            reinterpret_cast<size_t>(FileData) + ehdr->e_shoff +
            ehdr->e_shentsize * SectID);
    };

    Elf32_Shdr *csHdr;
    Elf32_Sym *dyn_sym = NULL;  // symbol table entry for symbol named "name"
    Elf32_Rel *rel_plt_table = NULL;  // array with ".rel.plt" entries
    Elf32_Rel *rel_dyn_table = NULL;  // array with ".rel.dyn" entries
    size_t rel_plt_amount;            // amount of ".rel.plt" entries
    size_t rel_dyn_amount;            // amount of ".rel.dyn" entries
    const char *dyn_strings = NULL;
    size_t dyn_strings_size;

    size_t dyn_sym_amount;
    char *sname;
    //   char fname[MAX_PATH];
    //   FILE *Dump;
    for (i = 0; i < ehdr->e_shnum; i++) {
        csHdr = GetSectionHdr(i);
        if (!csHdr) {
            break;
        }
        sname = StringTable + csHdr->sh_name;
        // DynMsg("Section
        // %s:%p-%p\n",sname,(uint32_t)csHdr->sh_addr,(uint32_t)csHdr->sh_addr+csHdr->sh_size);
        if (csHdr->sh_type & SHT_PROGBITS) {
            if (csHdr->sh_flags & SHF_EXECINSTR) {
                Code.Add((uint32_t)csHdr->sh_addr,
                         csHdr->sh_size,
                         bTrue,
                         csHdr->sh_flags & SHF_WRITE ? bTrue : bFalse,
                         bTrue,
                         this,
                         sname);
            }
            else  // if(!strcmp(sname,".data")||!strcmp(sname,".rodata"))
            {
                Data.Add((uint32_t)csHdr->sh_addr,
                         csHdr->sh_size,
                         bTrue,
                         csHdr->sh_flags & SHF_WRITE ? bTrue : bFalse,
                         bFalse,
                         this,
                         sname);
            }
        }

        if (!strcmp(sname, ".rel.plt")) {
            rel_plt_table = reinterpret_cast<Elf32_Rel *>(
                reinterpret_cast<size_t>(DllBase) + csHdr->sh_addr);
            rel_plt_amount = csHdr->sh_size / sizeof(Elf32_Rel);
            DynWarn(
                "rel_plt_table =%p, size=%x", rel_plt_table, rel_plt_amount);
        }
        if (!strcmp(sname, ".rel.dyn")) {
            rel_dyn_table = reinterpret_cast<Elf32_Rel *>(
                reinterpret_cast<size_t>(DllBase) + csHdr->sh_addr);
            rel_dyn_amount = csHdr->sh_size / sizeof(Elf32_Rel);
            DynWarn(
                "rel_dyn_table =%p, size=%x", rel_dyn_table, rel_dyn_amount);
        }
        if (csHdr->sh_type == SHT_DYNSYM) {
            dyn_sym = reinterpret_cast<Elf32_Sym *>(
                reinterpret_cast<size_t>(DllBase) + csHdr->sh_addr);
            dyn_sym_amount = csHdr->sh_size / sizeof(Elf32_Sym);
            if (csHdr->sh_link && csHdr->sh_link < ehdr->e_shnum) {
                dyn_strings = reinterpret_cast<const char *>(
                    reinterpret_cast<uint32_t>(DllBase) +
                    GetSectionHdr(csHdr->sh_link)->sh_addr);
                dyn_strings_size = GetSectionHdr(csHdr->sh_link)->sh_size;
            }
        }

#ifdef DUMP_SECTIONS
        if (sname) {
            DynMsg("section %s, type=%i,sz=%i",
                   sname,
                   csHdr->sh_type,
                   csHdr->sh_size);
            sprintf(fname, "/home/chuvi/%s_sect_%s.bin", szLibName, sname + 1);
            Dump = fopen(fname, "wb");
            if (Dump) {
                fwrite((void *)((uint32_t)(this->DllBase) +
                                (uint32_t)csHdr->sh_addr),
                       csHdr->sh_size,
                       1,
                       Dump);
                fclose(Dump);
            }
            sprintf(fname, "/home/chuvi/%s_sect_%s.hdr", szLibName, sname + 1);
            Dump = fopen(fname, "wb");
            if (Dump) {
                fwrite((void *)(csHdr), sizeof(Elf32_Shdr), 1, Dump);
                fclose(Dump);
            }
        }
#endif
    }

    if (!Code) {
        DynErr("bad  library file (code sections not found)");
        return bFalse;
    }

    if (!Data) {
        DynErr("bad  library file (data sections not found)");
        return bFalse;
    }

    Code.Sort();
    Data.Sort();

    size_t DynSymID;
    size_t DynSymRtType;
    Elf32_Sym *CurSym;
    //   uint32_t *SymAddr;
    uint32_t FuncAddr;
    uint32_t *CallAddr;
    const char *CurFuncName;
    const char *CurLibName;
    //   ImpDynType_e ImpType;
    auto CheckFuncPtr = [this](uint32_t FuncAddr,
                               const char *FuncName,
                               const char **LibName) -> iBool {
        Dl_info DlInfo;
        void *DlLib;
        uint32_t DlFunc = 0;
        if (dladdr(reinterpret_cast<void *>(FuncAddr), &DlInfo)) {
            if (!DlInfo.dli_saddr) {
                if (!DlInfo.dli_fname) {
                    DynWarn(
                        "Unable to get lib for \"%s\" %p", FuncName, FuncAddr);
                }
                else {
                    DlLib = dlopen(DlInfo.dli_fname, RTLD_NOW);
                    if (!DlLib) {
                        DynWarn("Unable to open \"%s\".", DlInfo.dli_fname);
                    }
                    else {
                        DlFunc = reinterpret_cast<uint32_t>(
                            dlsym(DlLib, const_cast<char *>(FuncName)));
                        if (!DlFunc) {
                            DynWarn("Unable to find \"%s\" in \"%s\"",
                                    FuncName,
                                    DlInfo.dli_fname);
                        }
                        dlclose(DlLib);
                    }
                }
            }
            else {
                DlFunc = reinterpret_cast<uint32_t>(DlInfo.dli_saddr);
            }
            /*
                    if(DlInfo.dli_sname)
                    {
                       if(strcmp(DlInfo.dli_sname,FuncName))
                       {
                          DynWarn("%s!=%s",DlInfo.dli_sname,FuncName);
                       }
                    }
           */
            if (DlFunc != FuncAddr) {
                DynWarn(
                    "\"%s\" addr error (%p!=%p)", FuncName, DlFunc, FuncAddr);
            }
            else {
                // DynWarn("\"%s\" found in \"%s\"",FuncName,DlInfo.dli_fname);
                if (LibName) {
                    *LibName =
                        DlInfo.dli_fname ? GetFileName(DlInfo.dli_fname) : 0;
                }
                return bTrue;
            }
        }
        else {
            DynWarn("dla_fail(\"%s\",%p)", FuncName, FuncAddr);
        }
        return bFalse;
    };

    auto AddImportFunc = [this](const char *FunctionName,
                                const char *Library,
                                uint32_t Addr,
                                ImpDynType_e ImpType) {
        uint32_t ImpFuncAddr = 0;
        uint32_t CurFuncAddr = 0;
        switch (ImpType) {
            case IDT_NO:
            case IDT_DIRECT: {
                ImpFuncAddr = *reinterpret_cast<uint32_t *>(Addr);
                break;
            }
            case IDT_RELATIVE: {
                ImpFuncAddr = Addr;
                break;
            }
        }

        for (auto Im : ImportDescription) {
            if (!strcmp(Im->FunctionName, FunctionName) &&
                !strcmp(Im->LibraryName, Library)) {
                if (ImpType == IDT_DIRECT) {
                    // DynWarn("%s direct
                    // sz=%i",FunctionName,Im->DirectPtrs.size())
                    for (auto dp : Im->DirectPtrs) {
                        if (dp == reinterpret_cast<uint32_t *>(Addr)) {
                            return;
                        }
                    }
                    Im->DirectPtrs.push_back(
                        reinterpret_cast<uint32_t *>(Addr));
                    Im->hasDynDirect = bTrue;
                    return;
                }
                else {
                    return;
                }
            }
        }
        ImportDescription_t *ImpDescr = new ImportDescription_t;
        ImpDescr->LibraryName = Library;
        ImpDescr->FunctionOrdinal = NULL;
        ImpDescr->FunctionName = FunctionName;
        ImpDescr->ImpFuncAddr = ImpFuncAddr;
        switch (ImpType) {
            case IDT_NO: {
                ImpDescr->hasPlt = bTrue;
                ImpDescr->pImpFuncAddr = reinterpret_cast<uint32_t *>(Addr);
                break;
            }
            case IDT_RELATIVE: {
                ImpDescr->hasDynRel = bTrue;
                break;
            }
            case IDT_DIRECT: {
                ImpDescr->hasDynDirect = bTrue;
                ImpDescr->DirectPtrs.push_back(
                    reinterpret_cast<uint32_t *>(Addr));
                // DynWarn("ImpDirect: %s %p",FunctionName,ImpDescr->ImpAddr);
                break;
            }
        }
        // ImpDescr->ImpAddr=reinterpret_cast<uint32_t*>(Addr);

        ImportDescription.push_back(ImpDescr);
    };
    ImportDescription.clear();
    if (dyn_strings) {
        for (size_t j = 0; j < rel_plt_amount; ++j) {
            DynSymID = ELF32_R_SYM(rel_plt_table[j].r_info);
            DynSymRtType = ELF32_R_TYPE(rel_plt_table[j].r_info);
            if (DynSymID > dyn_sym_amount) {
                DynWarn("WTF?!");
                continue;
            }
            CurSym = &dyn_sym[DynSymID];
            if (CurSym->st_name > dyn_strings_size) {
                DynWarn("CurSym->st_name>dyn_strings_size");
                continue;
            }
            if (CurSym->st_value || !CurSym->st_name ||
                ELF32_ST_TYPE(CurSym->st_info) != STT_FUNC) {
                continue;
            }
            CurFuncName = &dyn_strings[CurSym->st_name];
            if (!CurFuncName) {
                continue;
            }
            FuncAddr =
                reinterpret_cast<uint32_t>(DllBase) + rel_plt_table[j].r_offset;
            if (CheckFuncPtr(*reinterpret_cast<uint32_t *>(FuncAddr),
                             CurFuncName,
                             &CurLibName)) {
                AddImportFunc(CurFuncName, CurLibName, FuncAddr, IDT_NO);
                // DynWarn("rel_plt(%i):[%05i:%s]n=%08x,v=%08x,sz=%04x,bi=%i,ti=%i,o=%i,ndx=%i,fa=%p,rt=%i",j,DynSymID,CurFuncName,CurSym->st_name,CurSym->st_value,CurSym->st_size,
                // ELF32_ST_BIND(CurSym->st_info),ELF32_ST_TYPE(CurSym->st_info),CurSym->st_other,CurSym->st_shndx,FuncAddr,DynSymRtType);
            }
        }

        for (size_t j = 0; j < rel_dyn_amount; ++j) {
            DynSymID = ELF32_R_SYM(rel_dyn_table[j].r_info);
            DynSymRtType = ELF32_R_TYPE(rel_dyn_table[j].r_info);
            if (DynSymID > dyn_sym_amount) {
                DynWarn("WTF?!");
                continue;
            }
            CurSym = &dyn_sym[DynSymID];
            if (CurSym->st_value || !CurSym->st_name ||
                ELF32_ST_TYPE(CurSym->st_info) != STT_FUNC) {
                continue;
            }
            if (CurSym->st_name > dyn_strings_size) {
                DynWarn("CurSym->st_name>dyn_strings_size");
                continue;
            }
            CurFuncName = &dyn_strings[CurSym->st_name];
            switch (DynSymRtType) {
                default: {
                    FuncAddr = 0;
                    break;
                }
                case R_386_PC32: {
                    CallAddr = reinterpret_cast<uint32_t *>(
                        reinterpret_cast<uint32_t>(DllBase) +
                        rel_dyn_table[j].r_offset);
                    FuncAddr =
                        (*CallAddr + reinterpret_cast<uint32_t>(CallAddr) +
                         sizeof(uint32_t));
                    if (CheckFuncPtr(FuncAddr, CurFuncName, &CurLibName)) {
                        AddImportFunc(
                            CurFuncName, CurLibName, FuncAddr, IDT_RELATIVE);
                    }
                    break;
                }
                case R_386_32: {
                    FuncAddr = (reinterpret_cast<uint32_t>(DllBase) +
                                rel_dyn_table[j].r_offset);
                    // if(CheckFuncPtr(*reinterpret_cast<uint32_t*>(FuncAddr),CurFuncName))
                    if (CheckFuncPtr(*reinterpret_cast<uint32_t *>(FuncAddr),
                                     CurFuncName,
                                     &CurLibName)) {
                        AddImportFunc(
                            CurFuncName, CurLibName, FuncAddr, IDT_DIRECT);
                    }

                    break;
                }
            }
            uint32_t *p = reinterpret_cast<uint32_t *>(
                reinterpret_cast<uint32_t>(DllBase) +
                rel_dyn_table[j].r_offset);

            if (!FuncAddr) {
                continue;
            }

            /*
                     if(!CheckFuncPtr(FuncAddr,CurFuncName))
                     //if(DynSymRtType==R_386_32)
                     {
                        DynWarn("rel_dyn(%i):[%05i:%s]n=%08x,v=%08x,sz=%04x,bi=%i,ti=%i,o=%i,ndx=%i,rt=%i,fa=%p",i,DynSymID,CurFuncName,CurSym->st_name,CurSym->st_value,CurSym->st_size,\
                     ELF32_ST_BIND(CurSym->st_info),ELF32_ST_TYPE(CurSym->st_info),CurSym->st_other,CurSym->st_shndx,DynSymRtType,FuncAddr);

                     }
            */
        }
    }

    //   uint32_t Addr1;
    //   uint32_t Addr2;
    DynWarn("ImportDescription size=%i", ImportDescription.size());
    return bTrue;
}

std::vector<uint32_t> CDynPatcher::FindString(uint32_t StartAddr,
                                              const char *str,
                                              bool FullMatch /*= true*/)
{
    auto ret = Data.FindString(str, StartAddr, FullMatch);
    return ret;
}

std::vector<uint32_t> CDynPatcher::FindDataRef(uint32_t StartAddr,
                                               uint32_t RefAddr)
{
    auto ret = Code.FindDataRef(RefAddr, StartAddr);
    return ret;
}

std::vector<uint32_t> CDynPatcher::FindRef_Mov(uint32_t gStartAddr,
                                               uint32_t RefAddress)
{
    auto ret = Code.FindRef_Mov(gStartAddr, RefAddress);
    return ret;
}

std::vector<uint32_t> CDynPatcher::FindRef_Push(uint32_t StartAddr,
                                                uint32_t RefAddress)
{
    auto ret = Code.FindRef_Push(StartAddr, RefAddress);
    return ret;
}

std::vector<uint32_t> CDynPatcher::FindRef_Call(uint32_t StartAddr,
                                                uint32_t RefAddress)
{
    auto ret = Code.FindRef_Call(StartAddr, RefAddress);
    return ret;
}

std::vector<uint32_t> CDynPatcher::FindRef_Jmp(uint32_t StartAddr,
                                               uint32_t RefAddress)
{
    auto ret = Code.FindRef_Jmp(StartAddr, RefAddress);
    return ret;
}

std::vector<uint32_t> CDynPatcher::FindDataRef(
    uint32_t StartAddr, const std::vector<uint32_t> &RefVec)
{
    std::vector<uint32_t> Refs, res;
    for (auto ref : RefVec) {
        res = FindDataRef(StartAddr, ref);
        if (!res.empty()) {
            Refs.insert(Refs.end(), res.begin(), res.end());
        }
    }
    return Refs;
}
std::vector<uint32_t> CDynPatcher::FindRef_Mov(
    uint32_t StartAddr, const std::vector<uint32_t> &RefVec)
{
    std::vector<uint32_t> Refs, res;
    for (auto ref : RefVec) {
        res = FindRef_Mov(StartAddr, ref);
        if (!res.empty()) {
            Refs.insert(Refs.end(), res.begin(), res.end());
        }
    }
    return Refs;
}
std::vector<uint32_t> CDynPatcher::FindRef_Push(
    uint32_t StartAddr, const std::vector<uint32_t> &RefVec)
{
    std::vector<uint32_t> Refs, res;
    for (auto ref : RefVec) {
        res = FindRef_Push(StartAddr, ref);
        if (!res.empty()) {
            Refs.insert(Refs.end(), res.begin(), res.end());
        }
    }
    return Refs;
}
std::vector<uint32_t> CDynPatcher::FindRef_Call(
    uint32_t StartAddr, const std::vector<uint32_t> &RefVec)
{
    std::vector<uint32_t> Refs, res;
    for (auto ref : RefVec) {
        res = FindRef_Call(StartAddr, ref);
        if (!res.empty()) {
            Refs.insert(Refs.end(), res.begin(), res.end());
        }
    }
    return Refs;
}
std::vector<uint32_t> CDynPatcher::FindRef_Jmp(
    uint32_t StartAddr, const std::vector<uint32_t> &RefVec)
{
    std::vector<uint32_t> Refs, res;
    for (auto ref : RefVec) {
        res = FindRef_Jmp(StartAddr, ref);
        if (!res.empty()) {
            Refs.insert(Refs.end(), res.begin(), res.end());
        }
    }
    return Refs;
}

iBool CDynPatcher::AddMemChangeInfo(void *Addr,
                                    uint32_t OriginalValue,
                                    void *OriginalFuncAddr /*= NULL*/)
{
    iBool AlreadyExists = bFalse;
    for (auto MemData : MemChange) {
        if (OriginalFuncAddr) {
            //          if (reinterpret_cast<uint32_t>(OriginalFuncAddr) ==
            //          reinterpret_cast<uint32_t>(MemData.OldFuncAddr))
            //          {
            //             AlreadyExists = bTrue;
            //             break;
            //          }
            if (reinterpret_cast<uint32_t>(Addr) ==
                reinterpret_cast<uint32_t>(MemData->pAddr)) {
                AlreadyExists = bTrue;
                break;
            }
        }
    }
    MemEditInfo_t *MemEditInfo;
    if (!AlreadyExists) {
        MemEditInfo = new MemEditInfo_t;
        MemEditInfo->pAddr = Addr;
        MemEditInfo->OrigValue = OriginalValue;
        MemEditInfo->OldFuncAddr = OriginalFuncAddr;
        MemChange.push_back(MemEditInfo);
    }
    return AlreadyExists;
}

void *CDynPatcher::HookFunctionCall(void *OrigAddr, void *NewAddr)
{
#ifndef _WIN32
    Dl_info AddrInfo;
    Dl_info RefInfo;
    if (!dladdr(OrigAddr, &AddrInfo)) {
        memset(&AddrInfo, 0, sizeof(AddrInfo));
    }
#endif

    auto CallRefs = FindRef_Call(0, reinterpret_cast<uint32_t>(OrigAddr));

    uint32_t NumCalls = 0;
    uint32_t NumHooks = 0;
    for (uint32_t ref : CallRefs) {
#ifndef _WIN32
        Dl_info RefInfo;
        if (!dladdr(ref, &RefInfo)) {
            memset(&RefInfo, 0, sizeof(RefInfo));
        }
        DynMsg("Call %i to 0x%p(%s) at %x(%s)\n",
               NumCalls++,
               OrigAddr,
               AddrInfo.dli_sname ? AddrInfo.dli_sname : "_",
               ref,
               AddrInfo.dli_sname ? RefInfo.dli_sname : "_");
#else
        DynMsg("Call %i to %p at %p\n", NumCalls++, OrigAddr, ref);
#endif

        FuncHook2_s *hook;
        uint32_t OldProt =
            MProtect(reinterpret_cast<void *>(ref), 8, PAGE_EXECUTE_READWRITE);
        if (OldProt != PAGE_NOACCESS) {
            AddMemChangeInfo(reinterpret_cast<void *>(ref + 1),
                             *reinterpret_cast<uint32_t *>(ref + 1),
                             OrigAddr);
            hook = reinterpret_cast<FuncHook2_s *>(ref);
            hook->_jmp = 0xe8;
            hook->addr =
                reinterpret_cast<int>(NewAddr) - static_cast<int>(ref) - 5;
            NumHooks++;
            MProtect(reinterpret_cast<void *>(ref), 8, OldProt);
        }
        else {
            DynErr("Unable to change protection for %p", ref);
        }
    }

    auto JmpRefs = FindRef_Jmp(0, reinterpret_cast<uint32_t>(OrigAddr));
    NumCalls = 0;
    for (uint32_t ref : JmpRefs) {
#ifndef _WIN32
        Dl_info RefInfo;
        if (!dladdr(ref, &RefInfo)) {
            memset(&RefInfo, 0, sizeof(RefInfo));
        }
        DynMsg("Jmp %i to 0x%p(%s) at %x(%s)\n",
               NumCalls++,
               OrigAddr,
               AddrInfo.dli_sname ? AddrInfo.dli_sname : "_",
               ref,
               AddrInfo.dli_sname ? RefInfo.dli_sname : "_");
#else
        DynMsg("Jmp %i to %p at %p\n", NumCalls++, OrigAddr, ref);
#endif

        FuncHook2_s *hook;
        uint32_t OldProt =
            MProtect(reinterpret_cast<void *>(ref), 8, PAGE_EXECUTE_READWRITE);
        if (OldProt != PAGE_NOACCESS) {
            AddMemChangeInfo(reinterpret_cast<void *>(ref + 1),
                             *reinterpret_cast<uint32_t *>(ref + 1),
                             OrigAddr);
            hook = reinterpret_cast<FuncHook2_s *>(ref);
            hook->_jmp = 0xe9;
            hook->addr =
                reinterpret_cast<int>(NewAddr) - static_cast<int>(ref) - 5;
            NumHooks++;
            MProtect(reinterpret_cast<void *>(ref), 8, OldProt);
        }
        else {
            DynErr("Unable to change protection for %p", ref);
        }
    }

    return NumHooks ? OrigAddr : NULL;
}

uint32_t CDynPatcher::UnsetFunctionHook(void *OrigAddr)
{
    uint32_t HooksRemoved = 0;
    auto Cur = MemChange.begin();
    while (Cur != MemChange.end()) {
        if (reinterpret_cast<uint32_t>((*Cur)->OldFuncAddr) ==
            reinterpret_cast<uint32_t>(OrigAddr)) {
            delete *Cur;
            MemChange.erase(
                std::remove(MemChange.begin(), MemChange.end(), *Cur),
                MemChange.end());
            HooksRemoved++;
        }
        else {
            ++Cur;
        }
    }
    return HooksRemoved;
}

uint32_t CDynPatcher::ChangePtrValue(void *Addr, void *NewValue)
{
    iBool IsNewValueIsOriginalValue = bFalse;
    auto Cur = MemChange.begin();
    while (Cur != MemChange.end()) {
        if (reinterpret_cast<uint32_t>((*Cur)->pAddr) ==
            reinterpret_cast<uint32_t>(Addr)) {
            if ((*Cur)->OrigValue == reinterpret_cast<uint32_t>(NewValue)) {
                delete *Cur;
                MemChange.erase(
                    std::remove(MemChange.begin(), MemChange.end(), *(Cur)),
                    MemChange.end());
                IsNewValueIsOriginalValue = bTrue;
            }
        }
        else {
            ++Cur;
        }
    }
    uint32_t OrigValue = *reinterpret_cast<uint32_t *>(Addr);
    uint32_t OldProtect = MProtect(Addr, 8, PAGE_READWRITE);
    if (OldProtect == PAGE_NOACCESS) {
        DynErr("Unable to change protection for %p", Addr);
        return NULL;
    }
    if (!IsNewValueIsOriginalValue) {
        AddMemChangeInfo(Addr, *reinterpret_cast<uint32_t *>(Addr));
    }
    *reinterpret_cast<uint32_t *>(Addr) = reinterpret_cast<uint32_t>(NewValue);
    MProtect(Addr, 8, OldProtect);
    return OrigValue;
}

uint32_t CDynPatcher::RestorePtrValue(void *Addr)
{
    auto Cur = MemChange.begin();
    uint32_t OldProtect;
    uint32_t OriginalValue = NULL;
    while (Cur != MemChange.end()) {
        if (reinterpret_cast<uint32_t>((*Cur)->pAddr) ==
            reinterpret_cast<uint32_t>(Addr)) {
            OldProtect = MProtect(Addr, 8, PAGE_READWRITE);
            if (OldProtect != PAGE_NOACCESS) {
                *reinterpret_cast<uint32_t *>(Addr) = ((*Cur)->OrigValue);
                OriginalValue = ((*Cur)->OrigValue);
                delete *Cur;
                MemChange.erase(
                    std::remove(MemChange.begin(), MemChange.end(), *(Cur)),
                    MemChange.end());
                MProtect(Addr, 8, OldProtect);
            }
        }
        else {
            ++Cur;
        }
    }
    return OriginalValue;
}

uint32_t CDynPatcher::GetVTableSize(void **VTable)
{
    int i = 0;
    int Sz = 0;
    // VFuncAddr = ;
    //
    for (i = 0; IsValidCodeAddr(VTable[i]); i++) {
        Sz++;
    }
    return Sz;
}

uint32_t CDynPatcher::DumpVtable(void **VTable)
{
    void *VFuncAddr;
#ifdef POSIX
    Dl_info info;
    char *DName;
    int status;
#endif
    int i = 0;
    int ret = -1;
    VFuncAddr = VTable[i];
    while (IsValidCodeAddr(VTable[i])) {
#ifdef POSIX
        if (dladdr(VFuncAddr, &info)) {
            DName = abi::__cxa_demangle(info.dli_sname, 0, 0, &status);
            // LOG_MESSAGE(PLID,"\\*%02i:*\\ \"%s\"  (%p,%i)", i,
            // status?info.dli_sname:DName,VFuncAddr,(uint32_t)VFuncAddr-(uint32_t)info.dli_saddr);
            printf("\\*%02i:*\\ \"%s\"  (%p,%i)\n",
                   i,
                   status ? info.dli_sname : DName,
                   VFuncAddr,
                   (uint32_t)VFuncAddr - (uint32_t)info.dli_saddr);
            if (!status) {
                free(DName);
            }
        }
        else
#endif
        {
            printf("Vfunc[%i]=%p\n", i, VFuncAddr);
        }
        i++;
        VFuncAddr = VTable[i];
    }
    return ret;
}

uint32_t CDynPatcher::HookImportFunc(const char *FuncName,
                                     void *NewAddr,
                                     const char *Library /*= NULL*/)
{
    uint32_t OriginalFuncAddr = NULL;
    ImportDescription_t *ImpData = NULL;
    uint32_t Candidates = 0;
    if (ImportDescription.empty()) {
        return NULL;
    }
    for (auto ImpDescr : ImportDescription) {
        if (!Library || !strcmp(Library, ImpDescr->LibraryName)) {
            if (!ImpDescr->FunctionName) {
                continue;
            }
            if (!strcmp(ImpDescr->FunctionName, FuncName)) {
                Candidates++;
                ImpData = ImpDescr;
            }
        }
    }

    if (Candidates > 1) {
        DynWarn(
            "%i candidates for import func %s found!", Candidates, FuncName);
    }

    if (!Candidates) {
        DynWarn("Import func %s NOT found!", FuncName);
        return NULL;
    }
    if (ImpData) {
        if (NewAddr) {
            if (ImpData->hasPlt) {
                ChangePtrValue(ImpData->pImpFuncAddr, NewAddr);
            }

            if (ImpData->hasDynRel) {
                HookFunctionCall(reinterpret_cast<void *>(ImpData->ImpFuncAddr),
                                 NewAddr);
            }

            if (ImpData->hasDynDirect) {
                for (auto dp : ImpData->DirectPtrs) {
                    ChangePtrValue(dp, NewAddr);
                }
            }
            return ImpData->ImpFuncAddr;
        }
        else {
            if (ImpData->hasPlt) {
                RestorePtrValue(ImpData->pImpFuncAddr);
            }

            if (ImpData->hasDynRel) {
                UnsetFunctionHook(
                    reinterpret_cast<void *>(ImpData->ImpFuncAddr));
            }

            if (ImpData->hasDynDirect) {
                for (auto dp : ImpData->DirectPtrs) {
                    RestorePtrValue(dp);
                }
            }
            return 1;
        }
    }

    return NULL;
}

uint32_t CDynPatcher::GetOriginalPtrValue(void *addr)
{
    for (auto MemInfo : MemChange) {
        if (MemInfo->pAddr == addr) {
            return MemInfo->OrigValue;
        }
    }
    return *reinterpret_cast<uint32_t *>(addr);
}

uint32_t CDynPatcher::GetImportFuncAddr(const char *FuncName,
                                        const char *Library /*= NULL*/)
{
    //   uint32_t OriginalFuncAddr;
    //   uint32_t NewPtrValue;
    ImportDescription_t *tImpDescr = NULL;
    uint32_t Candidates = NULL;
    if (ImportDescription.empty()) {
        return NULL;
    }
    for (auto ImpDescr : ImportDescription) {
        if (!Library || !strcmp(Library, ImpDescr->LibraryName)) {
            if (!ImpDescr->FunctionName) {
                continue;
            }
            if (!strcmp(ImpDescr->FunctionName, FuncName)) {
                tImpDescr = ImpDescr;
                Candidates++;
            }
        }
    }

    if (Candidates > 1) {
        DynWarn(
            "%i candidates for import func %s found!", Candidates, FuncName);
    }
    if (tImpDescr) {
        return tImpDescr->ImpFuncAddr;
    }
    return NULL;
}

void CDynPatcher::UnsetHooks()
{
    for (auto MemInfo : MemChange) {
        uint32_t OldProtect = MProtect(MemInfo->pAddr, 8, PAGE_READWRITE);
        if (OldProtect) {
            if (OldProtect != PAGE_NOACCESS) {
                *reinterpret_cast<uint32_t *>(MemInfo->pAddr) =
                    MemInfo->OrigValue;
                MProtect(MemInfo->pAddr, 8, OldProtect);
            }
        }
        else {
            DynErr("Unable to change protection for %p", MemInfo->pAddr);
        }
        delete MemInfo;
    }
    MemChange.clear();
}

void **CDynPatcher::FindVTable(void *address, iBool MayBeInCode /*=bFalse*/)
{
    void *vtAddr;
    void *FuncAddr;
    int sum;
    // DynMsg("Looking for virtual table at %p",address);
    for (int i = 0; i < 0xFF; ++i) {
        vtAddr =
            *reinterpret_cast<void **>(reinterpret_cast<uint32_t>(address) + i);
        if (IsRangeInData(reinterpret_cast<uint32_t>(vtAddr), 4) ||
            (MayBeInCode == bTrue &&
             IsRangeInCode(reinterpret_cast<uint32_t>(vtAddr), 4))) {
            DynMsg("[%i] %p is in rdata", i, vtAddr);
            sum = 0;
            for (int j = 0; j <= 10; ++j) {
                FuncAddr = (reinterpret_cast<void **>(vtAddr))[j];
                if (IsRangeInCode(reinterpret_cast<uint32_t>(FuncAddr), 4)) {
                    // DynMsg("[%i][%i] %p is in code", i, j, FuncAddr);
                    sum++;
                }
                else {
                    // DynMsg("[%i][%i] %p is not in code", i, j, FuncAddr);
                }
            }

            if (sum > 5) {
                // DynMsg("Virtual table offset=%i", i);
                return reinterpret_cast<void **>(
                    *reinterpret_cast<uint32_t *>(address) + i);
                // return i;
            }
        }
        else {
            DynMsg("[%i] %p is not in rdata", i, vtAddr);
        }
    }

    DynMsg("Virtual table was not found. This should not happen");

    return NULL;
}

uint32_t CDynPatcher::GetCallFromAddress(uint32_t StartAddr)
{
    if (!IsRangeInCode(StartAddr, 5)) {
        return NULL;
    }
    FuncHook2_s *Inst = reinterpret_cast<FuncHook2_s *>(StartAddr);
    if (Inst->_jmp != 0xE8) {
        return NULL;
    }
    uint32_t FuncAddr = StartAddr + Inst->addr + 5;
    if (!IsRangeInCode(FuncAddr, 5)) {
        return NULL;
    }
    if (Code.FindRef_Call(0, FuncAddr).empty()) {
        return NULL;
    }
    return FuncAddr;
}

uint32_t CDynPatcher::FindNearestCall(uint32_t StartAddr, uint32_t Size)
{
    uint32_t FuncAddr = 0;
    for (uint32_t i = 0; i < Size; i++) {
        if (!IsRangeInCode(StartAddr + i, 5)) {
            return 0;
        }
        FuncAddr = GetCallFromAddress(StartAddr + i);
        if (FuncAddr) {
            return FuncAddr;
        }
    }
    return 0;
}

uint32_t CDynPatcher::FindFuntionStart(uint32_t StartAddr, uint32_t Size)
{
    for (uint32_t i = 0; i < Size; i++) {
        if (!IsRangeInCode(StartAddr - i, 5)) {
            return 0;
        }
        if (!Code.FindRef_Call(0, (StartAddr - i)).empty()) {
            return StartAddr - i;
        }
    }
    return 0;
}

iBool CDynPatcher::IsValidCodeAddr(void *Addr)
{
    uint32_t Flags = GetMemoryFlags(Addr);
#ifdef POSIX
    if (Flags & PROT_EXEC)
#else
    if (Flags & PAGE_EXECUTE || Flags & PAGE_EXECUTE_READ ||
        Flags & PAGE_EXECUTE_READWRITE || Flags & PAGE_EXECUTE_WRITECOPY)
#endif
    {
        return bTrue;
    }
    return bFalse;
#if 0
   CDynPatcher *Dp=NULL;
   iBool bValid = bFalse;
   if (!IsRangeInCode(reinterpret_cast<uint32_t>(Addr), sizeof(size_t)))
   {
      if (IsRangeInLib(reinterpret_cast<uint32_t>(Addr), sizeof(size_t)))
      {
         bValid = bFalse;
      }
      else
      {
         Dp = new CDynPatcher;
         if (Dp->Init(Addr))
         {
            if (!Dp->IsRangeInLib(reinterpret_cast<uint32_t>(Addr), sizeof(size_t)))
            {
               bValid = bFalse;
            }
            if (Dp->IsRangeInCode(reinterpret_cast<uint32_t>(Addr), sizeof(size_t)))
            {
               bValid = bTrue;
            }
         }
         delete Dp;
      }
   }
   else
   {
      bValid = bTrue;
   }
   return bValid;
#endif
}

iBool CDynPatcher::IsRangeInLib(uint32_t Addr, uint32_t Size)
{
    iBool InCode = bFalse;
    iBool InData = bFalse;
    InCode = Code.IsRangeInSections(Addr, Size);
    InData = Data.IsRangeInSections(Addr, Size);
    if (InData || InCode) {
        DynMsg("Found %p in %s%s", Addr, InCode ? "C" : "", InData ? "D" : "");
        return bTrue;
    }
    return bFalse;
}
