#ifndef _INCLUDE_REHLDS_API_H_
#define _INCLUDE_REHLDS_API_H_

#pragma once

extern IRehldsApi* g_RehldsApi;
extern IRehldsHookchains* g_RehldsHookchains;
extern IRehldsServerStatic *g_RehldsSvs;
extern const RehldsFuncs_t *g_RehldsFuncs;
extern bool RehldsApi_Init();
#endif //_INCLUDE_REHLDS_API_H_