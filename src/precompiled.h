#pragma once

#ifdef _WIN32 // WINDOWS
	#pragma warning(disable : 4005)
#else
	#ifdef __FUNCTION__
		#undef __FUNCTION__
	#endif
	#define __FUNCTION__ __func__
#endif // _WIN32

#define OBS_NONE	0
#define MAX_PATH	260

#include <vector>
#include <deque>
#include <thread>
#include <chrono>
#include <extdll.h>
#include <cbase.h>

#include "osdep.h"			// win32 vsnprintf, etc
#include "sdk_util.h"

#include <eiface.h>
#include <meta_api.h>

// regamedll API
#include <regamedll_api.h>
#include "mod_regamedll_api.h"
#include <mapinfo.h>
#include <studio.h>
#include <r_studioint.h>

// rehlds API
#include <rehlds_api.h>
#include "engine_rehlds_api.h"
#include "main.h"


#include "struct.h"
#undef DLLEXPORT
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#define NOINLINE __declspec(noinline)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#define NOINLINE __attribute__((noinline))
#define WINAPI		/* */
#endif

extern int UTIL_ReadFlags(const char* c);
extern void SV_StudioSetupBones(model_t* pModel, float frame, int sequence, const vec_t* angles, const vec_t* origin, const byte* pcontroller, const byte* pblending, int iBone, const edict_t* pEdict);
extern void (PlayerPreThinkPre)(edict_t* pEntity);
extern void (PlayerPostThinkPost)(edict_t* pEntity);
extern void (UpdateClientDataPost) (const struct edict_s* ent, int sendweapons, struct clientdata_s* cd);
extern int	(AddToFullPackPost)(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, int player, unsigned char* pSet);
extern void (PutInServer)	(edict_t* pEntity);
extern sv_blending_interface_s orig_interface;
extern cvar_t* phf_hitbox_fix;
void UTIL_ServerPrint(const char* fmt, ...);