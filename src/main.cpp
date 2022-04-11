#include "precompiled.h"
#include "handles.h"
#include "patternscan.h"

#if defined(__linux__) || defined(__APPLE__)
#define __fastcall
#endif
#include <subhook.h>
cvar_t* sv_unlag;
cvar_t* sv_maxunlag;
cvar_t* sv_unlagpush;
extern server_studio_api_t IEngineStudio;
extern studiohdr_t* g_pstudiohdr;
extern float(*g_pRotationMatrix)[3][4];
extern float(*g_pBoneTransform)[128][3][4];
void StudioProcessGait(int index);

// Resource counts
#define MAX_MODEL_INDEX_BITS		9	// sent as a short
#define MAX_MODELS			(1<<MAX_MODEL_INDEX_BITS)

qboolean nofind;
typedef int(*SV_BLENDING_INTERFACE_FUNC)(int, struct sv_blending_interface_s**, struct server_studio_api_s*, float*, float*);

// How many data slots to use when in multiplayer (must be power of 2)
#define MULTIPLAYER_BACKUP			64
constexpr int SV_UPDATE_BACKUP = MULTIPLAYER_BACKUP;
constexpr int SV_UPDATE_MASK = (SV_UPDATE_BACKUP - 1);

sv_adjusted_positions_t truepositions[MAX_CLIENTS];
struct player_anim_params_hist_s
{
	player_anim_params_s hist[MULTIPLAYER_BACKUP][MAX_CLIENTS];
};
player_anim_params_hist_s player_params_history[MAX_CLIENTS]{};
player_anim_params_s player_params[MAX_CLIENTS]{};

subhook_t Server_GetBlendingInterfaceHook;
subhook_t SV_SendClientDatagramHook;
void (PutInServer)(edict_t* pEntity)
{
	if (!pEntity)
	{
		RETURN_META(MRES_IGNORED);
	}
	auto host_id = ENTINDEX(pEntity) - 1;
	memset(&player_params_history[host_id], 0, sizeof(player_params_history[host_id]));
	RETURN_META(MRES_IGNORED);
}

void UTIL_ServerPrint(const char* fmt, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	SERVER_PRINT(string);
}

void VectorMA(const vec_t* veca, float scale, const vec_t* vecm, vec_t* out)
{
	out[0] = scale * vecm[0] + veca[0];
	out[1] = scale * vecm[1] + veca[1];
	out[2] = scale * vecm[2] + veca[2];
}


void (PlayerPreThinkPre)(edict_t* pEntity)
{
	auto host_id = ENTINDEX(pEntity)-1;
	auto _host_client = g_RehldsSvs->GetClient_t(host_id);
	client_t* cl;
	float cl_interptime;
	client_frame_t* nextFrame;
	entity_state_t* state;
	sv_adjusted_positions_t* pos;
	float frac;
	int i;
	client_frame_t* frame;
	player_anim_params_s(*histframe)[32];
	player_anim_params_s(*nexthistframe)[32];
	vec3_t origin;
	vec3_t delta;
	float realtime = g_RehldsFuncs->GetRealTime();
	double targettime; // FP precision fix
	vec3_t angles;
	vec3_t mins;
	vec3_t maxs;

	memset(truepositions, 0, sizeof(truepositions));
	nofind = 1;
	if (!MDLL_AllowLagCompensation())
		return;

	if (sv_unlag->value == 0.0f || !_host_client->lw || !_host_client->lc)
		return;

	if (g_RehldsSvs->GetMaxClients() <= 1 || !_host_client->active)
		return;

	nofind = 0;
	for (int i = 0; i < g_RehldsSvs->GetMaxClients(); i++)
	{
		cl = g_RehldsSvs->GetClient_t(i);
		if (cl == _host_client || !cl->active)
			continue;
			
		truepositions[i].active = 1;
		truepositions[i].extra = player_params[i];
	}

	float clientLatency = _host_client->latency;
	if (clientLatency > 1.5)
		clientLatency = 1.5f;

	if (sv_maxunlag->value != 0.0f)
	{
		if (sv_maxunlag->value < 0.0)
			sv_maxunlag->value = 0.f;

		if (clientLatency >= sv_maxunlag->value)
			clientLatency = sv_maxunlag->value;
	}

	cl_interptime = _host_client->lastcmd.lerp_msec / 1000.0f;

	if (cl_interptime > 0.1)
		cl_interptime = 0.1f;

	if (_host_client->next_messageinterval > cl_interptime)
		cl_interptime = (float)_host_client->next_messageinterval;

	// FP Precision fix (targettime is double there, not float)
	targettime = realtime - clientLatency - cl_interptime + sv_unlagpush->value;

	if (targettime > realtime)
		targettime = realtime;

	if (SV_UPDATE_BACKUP <= 0)
	{
		memset(truepositions, 0, sizeof(truepositions));
		nofind = 1;
		return;
	}

	frame = nextFrame = NULL;
	histframe = nexthistframe = NULL;
	for (i = 0; i < SV_UPDATE_BACKUP; i++, frame = nextFrame, histframe = nexthistframe)
	{
		nextFrame = &_host_client->frames[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence + ~i)];
		nexthistframe = &player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence + ~i)];
		if (targettime >= nextFrame->senttime)
			break;
	}

	if (i >= SV_UPDATE_BACKUP || targettime - nextFrame->senttime > 1.0)
	{
		memset(truepositions, 0, sizeof(truepositions));
		nofind = 1;
		return;

	}

	if (!frame)
	{
		frame = nextFrame;
		histframe = nexthistframe;
		frac = 0.0;
	}
	else
	{
	}

	for (i = 0; i < nextFrame->entities.num_entities; i++)
	{
		state = &nextFrame->entities.entities[i];

		if (state->number <= 0)
			continue;

		if (state->number > g_RehldsSvs->GetMaxClients())
			break; // players are always in the beginning of the list, no need to look more


		cl = g_RehldsSvs->GetClient_t(state->number - 1);
		if (cl == _host_client || !cl->active)
			continue;

		pos = &truepositions[state->number - 1];
		if (pos->deadflag)
			continue;

		if (!pos->active)
		{
			continue;
		}


		player_params[state->number - 1] = (*nexthistframe)[state->number - 1];
		pos->needrelink = 1;
#if 0
		static float(g_pRotationMatrixCopy)[3][4];
		static float(g_pBoneTransformCopy)[128][3][4];
		static bool Init = false;
		if (!Init)
		{
			Init = true;
			UTIL_ServerPrint("\n\n\nDEBUG AT %p | %p \n\n\n", &g_pRotationMatrixCopy, &g_pBoneTransformCopy);
		}
		if (i == 1 && cl->edict->v.modelindex)
		{			
			auto model = g_RehldsApi->GetServerData()->GetModel(cl->edict->v.modelindex);
			
			SV_StudioSetupBones(
				model,
				cl->edict->v.frame,
				cl->edict->v.sequence,
				cl->edict->v.angles,
				cl->edict->v.origin,				
				cl->edict->v.controller,
				cl->edict->v.blending,
				-1,
				cl->edict);
			memcpy(&g_pRotationMatrixCopy, g_pRotationMatrix, sizeof(g_pRotationMatrixCopy));
			memcpy(&g_pBoneTransformCopy, g_pBoneTransform, sizeof(g_pBoneTransformCopy));
		}
#endif
	}
}


void StudioEstimateGait(int index)
{
	float dt;
	Vector est_velocity;

	dt = player_params[index].m_clTime - player_params[index].m_clOldTime;

	if (dt < 0)
		dt = 0;

	else if (dt > 1.0)
		dt = 1;

	if (dt == 0)
	{
		player_params[index].m_flGaitMovement = 0;
		return;
	}

	est_velocity = player_params[index].origin - player_params[index].m_prevgaitorigin;
	player_params[index].m_prevgaitorigin = player_params[index].origin;

	player_params[index].m_flGaitMovement = est_velocity.Length();

	if (dt <= 0 || player_params[index].m_flGaitMovement / dt < 5)
	{
		player_params[index].m_flGaitMovement = 0;

		est_velocity.x = 0;
		est_velocity.y = 0;
	}

	float flYaw = player_params[index].angles.y - player_params[index].gaityaw;
	if (player_params[index].sequence > 100) {
		player_params[index].gaityaw += flYaw;
		return;
	}
	if (!est_velocity.x && !est_velocity.y)
	{
		float flYawDiff = flYaw - (int)(flYaw / 360) * 360;

		if (flYawDiff > 180)
			flYawDiff -= 360;

		if (flYawDiff < -180)
			flYawDiff += 360;

		flYaw = fmod(flYaw, 360.0);

		if (flYaw < -180)
			flYaw += 360;

		else if (flYaw > 180)
			flYaw -= 360;

		if (flYaw > -5.0 && flYaw < 5.0)
			player_params[index].m_flYawModifier = 0.05;

		if (flYaw < -90.0 || flYaw > 90.0)
			player_params[index].m_flYawModifier = 3.5;

		if (dt < 0.25)
			flYawDiff *= dt * player_params[index].m_flYawModifier;
		else
			flYawDiff *= dt;

#if 1
		if (float(abs(flYawDiff)) < 0.1f)
#else
		if (float(abs(int64(flYawDiff))) < 0.1f)
#endif
			flYawDiff = 0;

		player_params[index].gaityaw += flYawDiff;
		player_params[index].gaityaw -= int(player_params[index].gaityaw / 360) * 360;
		player_params[index].m_flGaitMovement = 0;
	}
	else
	{
		player_params[index].gaityaw = (atan2(float(est_velocity.y), float(est_velocity.x)) * 180 / M_PI);

		if (player_params[index].gaityaw > 180)
			player_params[index].gaityaw = 180;

		if (player_params[index].gaityaw < -180)
			player_params[index].gaityaw = -180;
	}
}

void StudioPlayerBlend(int* pBlend, float* pPitch)
{
	// calc up/down pointing
	float range = float(int64(*pPitch * 3.0f));

	*pBlend = range;

	if (range <= -45.0f)
	{
		*pBlend = 255;
		*pPitch = 0;
	}
	else if (range >= 45.0f)
	{
		*pBlend = 0;
		*pPitch = 0;
	}
	else
	{
		*pBlend = int64((45.0f - range) * (255.0f / 90.0f));
		*pPitch = 0;
	}
}

void CalculatePitchBlend(int index)
{
	int iBlend;

	StudioPlayerBlend(&iBlend, &player_params[index].angles.x);

	
	player_params[index].prevangles.x = player_params[index].angles.x;
	player_params[index].blending[1] = iBlend;
	player_params[index].prevblending[1] = player_params[index].blending[1];
	player_params[index].prevseqblending[1] = player_params[index].blending[1];

}

void CalculateYawBlend(int index)
{
	float dt;
	float maxyaw = 255.0f;

	float flYaw;		// view direction relative to movement
	float blend_yaw;

	dt = player_params[index].m_clTime - player_params[index].m_clOldTime;

	if (dt < 0.0f)
		dt = 0;

	else if (dt > 1.0f)
		dt = 1;

	StudioEstimateGait(index);

	// calc side to side turning
	flYaw = fmod(float(player_params[index].angles[1] - player_params[index].gaityaw), 360);

	if (flYaw < -180)
		flYaw += 360;

	else if (flYaw > 180)
		flYaw -= 360;

	if (player_params[index].m_flGaitMovement != 0.0)
	{
		if (flYaw > 120)
		{
			player_params[index].gaityaw -= 180;
			player_params[index].m_flGaitMovement = -player_params[index].m_flGaitMovement;
			flYaw -= 180;
		}
		else if (flYaw < -120)
		{
			player_params[index].gaityaw += 180;
			player_params[index].m_flGaitMovement = -player_params[index].m_flGaitMovement;
			flYaw += 180;
		}
	}

	blend_yaw = (flYaw / 90.0) * 128.0 + 127.0;
	blend_yaw = clamp<float>(blend_yaw, 0.0f, 255.0f);
	blend_yaw = 255.0 - blend_yaw;

	player_params[index].blending[0] = (int)(blend_yaw);
	player_params[index].prevblending[0] = player_params[index].blending[0];
	player_params[index].prevseqblending[0] = player_params[index].blending[0];
	player_params[index].angles[1] = player_params[index].gaityaw;

	if (player_params[index].angles[1] < 0)
		player_params[index].angles[1] += 360;

	player_params[index].prevangles[1] = player_params[index].angles[1];
}

void StudioProcessGait(int index)
{
	mstudioseqdesc_t* pseqdesc;
	float dt = player_params[index].m_clTime - player_params[index].m_clOldTime;

	if (dt < 0.0)
		dt = 0;

	else if (dt > 1.0)
		dt = 1;

	CalculateYawBlend(index);
	CalculatePitchBlend(index);
	auto ent = INDEXENT(index + 1);
	if (!ent)
	{
		return;
	}
	studiohdr_t* pstudiohdr = (studiohdr_t*)GET_MODEL_PTR(ent);

	if (!pstudiohdr)
		return;

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + player_params[index].gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement.x > 0.0f)
		player_params[index].gaitframe += (player_params[index].m_flGaitMovement / pseqdesc->linearmovement.x) * pseqdesc->numframes;
	else
		player_params[index].gaitframe += player_params[index].framerate * pseqdesc->fps * dt;

	// do modulo
	player_params[index].gaitframe -= int(player_params[index].gaitframe / pseqdesc->numframes) * pseqdesc->numframes;

	if (player_params[index].gaitframe < 0)
		player_params[index].gaitframe += pseqdesc->numframes;
}

void PlayerPostThinkPost(edict_t* pEntity)
{
	int i;
	auto id = ENTINDEX(pEntity) - 1;
	auto _host_client = g_RehldsSvs->GetClient_t(id);

	player_params[id].sequence = _host_client->edict->v.sequence;
	player_params[id].gaitsequence = _host_client->edict->v.gaitsequence;
	player_params[id].prevangles = player_params[id].angles;
	player_params[id].prevframe = player_params[id].frame;
	player_params[id].frame = _host_client->edict->v.frame;
	player_params[id].angles = _host_client->edict->v.angles;
	player_params[id].origin = _host_client->edict->v.origin;
	player_params[id].animtime = _host_client->edict->v.animtime;
	player_params[id].m_clOldTime = player_params[id].m_clTime;
	player_params[id].m_clTime = _host_client->edict->v.animtime;
	player_params[id].framerate = _host_client->edict->v.framerate;
	player_params[id].controller[0] = _host_client->edict->v.controller[0];
	player_params[id].controller[1] = _host_client->edict->v.controller[1];
	player_params[id].controller[2] = _host_client->edict->v.controller[2];
	player_params[id].controller[3] = _host_client->edict->v.controller[3];
	player_params[id].blending[0] = _host_client->edict->v.blending[0];
	player_params[id].blending[1] = _host_client->edict->v.blending[1];
	// sequence has changed, hold the previous sequence info
	if (player_params[id].sequence != player_params[id].prevsequence)
	{
		player_params[id].sequencetime = player_params[id].animtime + 0.01f;

		// save current blends to right lerping from last sequence
		for (int i = 0; i < 2; i++)
			player_params[id].prevseqblending[i] = player_params[id].blending[i];
		player_params[id].prevsequence = player_params[id].sequence; // save old sequence	
	}


	// copy controllers
	for (i = 0; i < 4; i++)
	{
		if (player_params[id].controller[i] != player_params[id].controller[i])
			player_params[id].prevcontroller[i] = player_params[id].controller[i];
	}

	// copy blends
	for (i = 0; i < 2; i++)
		player_params[id].prevblending[i] = player_params[id].blending[i];


	if (player_params[id].gaitsequence)
	{
		StudioProcessGait(id);
	}
	else
	{
		player_params[id].controller[0] = 127;
		player_params[id].controller[1] = 127;
		player_params[id].controller[2] = 127;
		player_params[id].controller[3] = 127;
		player_params[id].prevcontroller[0] = player_params[id].controller[0];
		player_params[id].prevcontroller[1] = player_params[id].controller[1];
		player_params[id].prevcontroller[2] = player_params[id].controller[2];
		player_params[id].prevcontroller[3] = player_params[id].controller[3];

		CalculatePitchBlend(id);
		CalculateYawBlend(id);
	}


	sv_adjusted_positions_t* pos;
	client_t* cli;

	if (nofind)
	{
		nofind = 0;
		return;
	}

	if (!MDLL_AllowLagCompensation())
		return;

	if (g_RehldsSvs->GetMaxClients() <= 1 || sv_unlag->value == 0.0)
		return;

	if (!_host_client->lw || !_host_client->lc || !_host_client->active)
		return;

	for (int i = 0; i < g_RehldsSvs->GetMaxClients(); i++)
	{
		cli = g_RehldsSvs->GetClient_t(i);
		pos = &truepositions[i];

		if (cli == _host_client || !cli->active)
			continue;

		if (!pos->needrelink)
			continue;

		if (!pos->active)
		{
			continue;
		}

		player_params[i] = truepositions[i].extra;
	}
}

int	(AddToFullPackPost)(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, int player, unsigned char* pSet)
{
	int i;
	auto host_id = ENTINDEX(host) - 1;
	auto _host_client = g_RehldsSvs->GetClient_t(host_id);
	if (!player || ent == host)
	{
		RETURN_META_VALUE(MRES_IGNORED, 0);
	}
	auto id = ENTINDEX(ent) - 1;
	auto save = player_params[id];
	player_params[id].sequence = state->sequence;
	player_params[id].gaitsequence = state->gaitsequence;
	player_params[id].prevangles = state->angles;
	player_params[id].prevframe = state->frame;
	player_params[id].frame = state->frame;
	player_params[id].angles = state->angles;
	player_params[id].origin = state->origin;
	player_params[id].animtime = state->animtime;
	player_params[id].framerate = state->framerate;
	player_params[id].controller[0] = state->controller[0];
	player_params[id].controller[1] = state->controller[1];
	player_params[id].controller[2] = state->controller[2];
	player_params[id].controller[3] = state->controller[3];
	player_params[id].blending[0] = state->blending[0];
	player_params[id].blending[1] = state->blending[1];
	player_params[id].m_clTime = state->animtime;
	// sequence has changed, hold the previous sequence info
	if (player_params[id].sequence != player_params[id].prevsequence)
	{
		player_params[id].sequencetime = player_params[id].animtime + 0.01f;

		// save current blends to right lerping from last sequence
		for (int i = 0; i < 2; i++)
			player_params[id].prevseqblending[i] = player_params[id].blending[i];
		player_params[id].prevsequence = player_params[id].sequence; // save old sequence	
	}


	// copy controllers
	for (i = 0; i < 4; i++)
	{
		if (player_params[id].controller[i] != player_params[id].controller[i])
			player_params[id].prevcontroller[i] = player_params[id].controller[i];
	}

	// copy blends
	for (i = 0; i < 2; i++)
		player_params[id].prevblending[i] = player_params[id].blending[i];

	player_params[id].m_clOldTime = player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence)][id].m_clTime;
	player_params[id].m_prevgaitorigin = player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence)][id].origin;
#if 0
	if (host_id == 0)
	{
		auto len = (player_params[id].m_prevgaitorigin - player_params[id].origin).Length();
		if (len)
		{
			UTIL_ServerPrint("%f | %f %f %f\n", len, player_params[id].m_prevgaitorigin[0], player_params[id].m_prevgaitorigin[1], player_params[id].m_prevgaitorigin[2]);
		}
	}
#endif
	if (player_params[id].gaitsequence)
	{
		StudioProcessGait(id);
	}
	else
	{
		player_params[id].controller[0] = 127;
		player_params[id].controller[1] = 127;
		player_params[id].controller[2] = 127;
		player_params[id].controller[3] = 127;
		player_params[id].prevcontroller[0] = player_params[id].controller[0];
		player_params[id].prevcontroller[1] = player_params[id].controller[1];
		player_params[id].prevcontroller[2] = player_params[id].controller[2];
		player_params[id].prevcontroller[3] = player_params[id].controller[3];

		CalculatePitchBlend(id);
		CalculateYawBlend(id);
	}

	player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence + 1)][id] = player_params[id];
	player_params[id] = save;

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void (UpdateClientDataPost)(const struct edict_s* ent, int sendweapons, struct clientdata_s* cd)
{

	auto host_id = ENTINDEX(ent) - 1;
	auto _host_client = g_RehldsSvs->GetClient_t(host_id);
	for (int i = 0; i < g_RehldsSvs->GetMaxClients(); i++)
	{

		player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence + 1)][i] = player_params_history[host_id].hist[SV_UPDATE_MASK & _host_client->netchan.outgoing_sequence][i];
	}
	RETURN_META(MRES_IGNORED);
}
static bool Init = false;


int (*Server_GetBlendingInterfaceOrig)(int version, struct sv_blending_interface_s** ppinterface, struct engine_studio_api_s* pstudio, float* rotationmatrix, float* bonetransform);
int Server_GetBlendingInterface(int version, struct sv_blending_interface_s** ppinterface, struct engine_studio_api_s* pstudio, float* rotationmatrix, float* bonetransform)
{
	subhook_remove(Server_GetBlendingInterfaceHook);
	Server_GetBlendingInterfaceOrig(version, ppinterface, pstudio, rotationmatrix, bonetransform);
	subhook_install(Server_GetBlendingInterfaceHook);
	if (version != SV_BLENDING_INTERFACE_VERSION)
		return 0;

	(*ppinterface)->SV_StudioSetupBones = decltype((*ppinterface)->SV_StudioSetupBones)(SV_StudioSetupBones);

	IEngineStudio.Mem_Calloc = pstudio->Mem_Calloc;
	IEngineStudio.Cache_Check = pstudio->Cache_Check;
	IEngineStudio.LoadCacheFile = pstudio->LoadCacheFile;
	IEngineStudio.Mod_Extradata = ((struct server_studio_api_s*)pstudio)->Mod_Extradata;

	g_pRotationMatrix = (float(*)[3][4])rotationmatrix;
	g_pBoneTransform = (float(*)[128][3][4])bonetransform;

	return 1;
}

bool OnMetaAttach()
{
	if (Init)
	{
		return Init;
	}
	if (!RehldsApi_Init())
	{
		return false;
	}
	sv_unlag = g_engfuncs.pfnCVarGetPointer("sv_unlag");
	sv_maxunlag = g_engfuncs.pfnCVarGetPointer("sv_maxunlag");
	sv_unlagpush = g_engfuncs.pfnCVarGetPointer("sv_unlagpush");
	//engine_patcher = std::make_unique< CDynPatcher>();
#if 1
#if defined(__linux__) || defined(__APPLE__)
	
	
	ModuleInfo info = Handles::GetModuleInfo("cs.so");
	Server_GetBlendingInterfaceOrig = decltype(Server_GetBlendingInterfaceOrig)(dlsym(info.handle, "Server_GetBlendingInterface"));

	printf("[Hitbox Fix] Found Server_GetBlendingInterface at 0x%p\n", Server_GetBlendingInterfaceOrig);
	Server_GetBlendingInterfaceHook = subhook_new(
		(void*)Server_GetBlendingInterfaceOrig, (void*)Server_GetBlendingInterface, (subhook_flags_t)0);
	subhook_install(Server_GetBlendingInterfaceHook);
		

#else
	
	Server_GetBlendingInterfaceOrig = decltype(Server_GetBlendingInterfaceOrig)(GetProcAddress((HMODULE)GetModuleHandleA("mp.dll"), "Server_GetBlendingInterface"));

	Server_GetBlendingInterfaceHook = subhook_new(
		(void*)Server_GetBlendingInterfaceOrig, (void*)Server_GetBlendingInterface, (subhook_flags_t)0);
	subhook_install(Server_GetBlendingInterfaceHook);
	
#endif
#endif
	Init = true;
	return true;
}


void OnMetaDetach()
{

}
