#include "precompiled.h"

IRehldsApi* g_RehldsApi;
IRehldsHookchains* g_RehldsHookchains; 
IRehldsServerStatic *g_RehldsSvs;
const RehldsFuncs_t *g_RehldsFuncs;
bool RehldsApi_TryInit(CSysModule* engineModule, char* failureReason) {
	if (!engineModule)return false;
	CreateInterfaceFn ifaceFactory = Sys_GetFactory(engineModule);
	if (!ifaceFactory)return false;
	int retCode = 0;
	g_RehldsApi = (IRehldsApi*)ifaceFactory(VREHLDS_HLDS_API_VERSION, &retCode);
	if (!g_RehldsApi)return false;
	int majorVersion = g_RehldsApi->GetMajorVersion();
	int minorVersion = g_RehldsApi->GetMinorVersion();
	if (majorVersion != REHLDS_API_VERSION_MAJOR)return false;
	if (minorVersion < REHLDS_API_VERSION_MINOR)return false;
	g_RehldsHookchains = g_RehldsApi->GetHookchains();
	g_RehldsSvs = g_RehldsApi->GetServerStatic();
	g_RehldsFuncs = g_RehldsApi->GetFuncs();
	return true;
}

bool RehldsApi_Init()
{
	char failReason[2048];

#ifdef WIN32
	// Find the most appropriate module handle from a list of DLL candidates
	// Notes:
	// - "swds.dll" is the library Dedicated Server
	//
	//    Let's also attempt to locate the ReHLDS API in the client's library
	// - "sw.dll" is the client library for Software render, with a built-in listenserver
	// - "hw.dll" is the client library for Hardware render, with a built-in listenserver
	const char *dllNames[] = { "swds.dll", "sw.dll", "hw.dll" }; // List of DLL candidates to lookup for the ReHLDS API
	CSysModule *engineModule = NULL; // The module handle of the selected DLL
	for (const char *dllName : dllNames)
	{
		if (engineModule = Sys_GetModuleHandle(dllName))
			break; // gotcha
	}
#else
	CSysModule* engineModule = Sys_GetModuleHandle("engine_i486.so");
#endif

	return RehldsApi_TryInit(engineModule, failReason);
}