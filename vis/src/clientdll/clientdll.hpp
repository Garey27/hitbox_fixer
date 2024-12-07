#pragma once

// clang-format off
#include <rehlds/public/basetypes.h>
#include <rehlds/common/vmodes.h>
#include <rehlds/common/quakedef.h>
#include <rehlds/engine/cdll_int.h>
#include <rehlds/public/rehlds/bspfile.h>
#include <rehlds/pm_shared/pm_defs.h>
#include <rehlds/pm_shared/pm_movevars.h>
#include "rehlds/common/r_studioint.h"
// clang-format on

extern cldll_func_t *gClientDll;
extern cldll_func_t oClientDll;

extern engine_studio_api_t *g_pStudio;
extern engine_studio_api_t g_Studio;
void CL_CreateMove_Hook(float frametime, struct usercmd_s *cmd, int active);
