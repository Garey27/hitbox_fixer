#include <Windows.h>
#include <cstdint>

enum M3D_DATA_SUBTYPE {
    M3D_NONE = 0,
    M3D_CUSTOM,
    M3D_DEC,
    M3D_HEX,
    M3D_FLOAT,
    M3D_PERCENT,
    M3D_BOOL,
    M3D_STRING
};

enum M3D_ENTRY_TYPE { M3D_FUNCTION = 0, M3D_ATTRIBUTE = 1, M3D_PREFERENCE = 2 };
typedef uint32_t M3D_PROVIDER;
M3D_PROVIDER hProvider = NULL;
typedef int M3D_RESULT;
struct M3D_INTERFACE_ENTRY {
    M3D_ENTRY_TYPE type;
    const char* entry_name;
    uint32_t token;
    M3D_DATA_SUBTYPE subtype;
};

typedef M3D_RESULT(__stdcall* RIB_register_interfaceFunc)(
    M3D_PROVIDER provider,
    const char* interface_name,
    int entry_count,
    M3D_INTERFACE_ENTRY const* rlist);
RIB_register_interfaceFunc RIB_register_interfaceFuncPtr = NULL;

typedef M3D_PROVIDER(__stdcall* RIB_provider_library_handleFunc)(void);
RIB_provider_library_handleFunc RIB_provider_library_handleFuncPtr = NULL;

typedef M3D_PROVIDER(__stdcall* RIB_alloc_provider_handleFunc)(uint32_t module);
RIB_alloc_provider_handleFunc RIB_alloc_provider_handleFuncPtr = NULL;
M3D_PROVIDER __stdcall RIB_alloc_provider_handle(uint32_t module)
{
    return RIB_alloc_provider_handleFuncPtr(module);
}

void(__stdcall* RIB_free_provider_handle)(M3D_PROVIDER provider);
void(__stdcall* RIB_free_provider_library)(M3D_PROVIDER provider);
M3D_PROVIDER __stdcall RIB_provider_library_handle(void)
{
    if (RIB_provider_library_handleFuncPtr)
        return RIB_provider_library_handleFuncPtr();
    return RIB_alloc_provider_handle(0);
}
typedef M3D_RESULT(__stdcall* RIB_unregister_interfaceFunc)(
    M3D_PROVIDER provider,
    const char* interface_name,
    int entry_count,
    const M3D_INTERFACE_ENTRY* rlist);
RIB_unregister_interfaceFunc RIB_unregister_interfaceFuncPtr = NULL;

M3D_RESULT __stdcall RIB_register_interface(M3D_PROVIDER provider,
    const char* interface_name,
    int entry_count,
    const M3D_INTERFACE_ENTRY* rlist)
{
    return RIB_register_interfaceFuncPtr(
        provider, interface_name, entry_count, rlist);
}

M3D_RESULT __stdcall RIB_unregister_interface(M3D_PROVIDER provider,
    const char* interface_name,
    int entry_count,
    M3D_INTERFACE_ENTRY const* rlist)
{
    return RIB_unregister_interfaceFuncPtr(
        provider, interface_name, entry_count, rlist);
}

extern BOOL Attach(HINSTANCE dll);

extern "C" __declspec(dllexport) BOOL WINAPI RIB_Main(
    M3D_PROVIDER hModule, LPVOID lp2, LPVOID lp3, LPVOID lp4, LPVOID lp5)
{
#if 0
    if (hModule && lp2) {
        hProvider = hModule;
    }

    auto hMod = (HMODULE)GetModuleHandleA("Mss32.dll");
    if (hMod) {
        RIB_alloc_provider_handleFuncPtr =
            (RIB_alloc_provider_handleFunc)(GetProcAddress(
                hMod, "_RIB_alloc_provider_handle@4"));
        RIB_provider_library_handleFuncPtr =
            (RIB_provider_library_handleFunc)GetProcAddress(
                hMod, "_RIB_provider_library_handle@0");
        RIB_register_interfaceFuncPtr =
            (RIB_register_interfaceFunc)GetProcAddress(
                hMod, "_RIB_register_interface@16");
        RIB_unregister_interfaceFuncPtr =
            (RIB_unregister_interfaceFunc)GetProcAddress(
                hMod, "_RIB_unregister_interface@16");
        RIB_free_provider_handle = decltype(RIB_free_provider_handle)(
            GetProcAddress(hMod, "_RIB_free_provider_handle@4"));
        RIB_free_provider_library = decltype(RIB_free_provider_library)(
            GetProcAddress(hMod, "_RIB_free_provider_library@4"));
        M3D_INTERFACE_ENTRY SampleServices[1];
        RIB_register_interface(
            hProvider, "Mss sample services2", 0, SampleServices);
    }
#endif
    return TRUE;
}