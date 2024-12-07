#pragma once

#include <winternl.h>

typedef struct _NT_PEB_LDR_DATA {
    ULONG Length;
    UCHAR Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID EntryInProgress;
} NT_PEB_LDR_DATA, *PNT_PEB_LDR_DATA;

typedef struct _NT_LDR_MODULE {
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID BaseAddress;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    SHORT LoadCount;
    SHORT TlsIndex;
    LIST_ENTRY HashTableEntry;
    ULONG TimeDateStamp;
} NT_LDR_MODULE, *PNT_LDR_MODULE;

typedef struct _NT_RTL_BALANCED_NODE {
    union {
        struct _RTL_BALANCED_NODE *Children[2];
        struct {
            struct _RTL_BALANCED_NODE *Left;
            struct _RTL_BALANCED_NODE *Right;
        };
    };
    union {
        struct {
            UCHAR Red : 1;
            UCHAR Balance : 2;
        };
        ULONGLONG ParentValue;
    };
} NT_RTL_BALANCED_NODE, *PNT_RTL_BALANCED_NODE;

typedef struct _NT_LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    union {
        LIST_ENTRY InInitializationOrderLinks;
        LIST_ENTRY InProgressLinks;
    };
    VOID *DllBase;
    VOID *EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    union {
        UCHAR FlagGroup[4];
        ULONG Flags;
        struct {
            ULONG PackagedBinary : 1;
            ULONG MarkedForRemoval : 1;
            ULONG ImageDll : 1;
            ULONG LoadNotificationsSent : 1;
            ULONG TelemetryEntryProcessed : 1;
            ULONG ProcessStaticImport : 1;
            ULONG InLegacyLists : 1;
            ULONG InIndexes : 1;
            ULONG ShimDll : 1;
            ULONG InExceptionTable : 1;
            ULONG ReservedFlags1 : 2;
            ULONG LoadInProgress : 1;
            ULONG ReservedFlags2 : 1;
            ULONG EntryProcessed : 1;
            ULONG ReservedFlags3 : 3;
            ULONG DontCallForThreads : 1;
            ULONG ProcessAttachCalled : 1;
            ULONG ProcessAttachFailed : 1;
            ULONG CorDeferredValidate : 1;
            ULONG CorImage : 1;
            ULONG DontRelocate : 1;
            ULONG CorILOnly : 1;
            ULONG ReservedFlags5 : 3;
            ULONG Redirected : 1;
            ULONG ReservedFlags6 : 2;
            ULONG CompatDatabaseProcessed : 1;
        };
    };
    USHORT ObsoleteLoadCount;
    USHORT TlsIndex;
    LIST_ENTRY HashLinks;
    ULONG TimeDateStamp;
    struct _ACTIVATION_CONTEXT *EntryPointActivationContext;
    VOID *PatchInformation;
    struct _LDR_DDAG_NODE *DdagNode;
    LIST_ENTRY NodeModuleLink;
    struct _LDRP_DLL_SNAP_CONTEXT *SnapContext;
    VOID *ParentDllBase;
    VOID *SwitchBackContext;
    NT_RTL_BALANCED_NODE BaseAddressIndexNode;
    NT_RTL_BALANCED_NODE MappingInfoIndexNode;
    ULONGLONG OriginalBase;
    union _LARGE_INTEGER LoadTime;
    ULONG BaseNameHashValue;
    enum _LDR_DLL_LOAD_REASON LoadReason;
} NT_LDR_DATA_TABLE_ENTRY, *PNT_LDR_DATA_TABLE_ENTRY;
