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
cvar_t hf_hitbox_fix = { "hbf_enabled", "1", FCVAR_SERVER | FCVAR_PROTECTED, 0.0f, NULL };
cvar_t* phf_hitbox_fix;
char g_ExecConfigCmd[MAX_PATH];

GameType_e g_eGameType;
const char CFG_FILE[] = "hbf.cfg";
extern server_studio_api_t IEngineStudio;
extern studiohdr_t* g_pstudiohdr;
extern float(*g_pRotationMatrix)[3][4];
extern float(*g_pBoneTransform)[128][3][4];
void StudioProcessGait(int index);
sv_blending_interface_s** orig_ppinterface;
sv_blending_interface_s orig_interface;
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

subhook_t Server_GetBlendingInterfaceHook{};

subhook_t GetProcAddressHook{};
subhook_t dlsymHook{};
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
	auto host_id = ENTINDEX(pEntity) - 1;
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
		RETURN_META(MRES_IGNORED);

	if (sv_unlag->value == 0.0f || !_host_client->lw || !_host_client->lc)
		RETURN_META(MRES_IGNORED);

	if (g_RehldsSvs->GetMaxClients() <= 1 || !_host_client->active)
		RETURN_META(MRES_IGNORED);

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
		RETURN_META(MRES_IGNORED);
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
		RETURN_META(MRES_IGNORED);
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

			HL_StudioSetupBones(
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
	RETURN_META(MRES_IGNORED);
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

		if (abs(flYawDiff) < 0.1)
			flYawDiff = 0;

		player_params[index].gaityaw += flYawDiff;
		player_params[index].gaityaw = player_params[index].gaityaw - (int)(player_params[index].gaityaw / 360) * 360;
		player_params[index].m_flGaitMovement = 0;
	}
	else
	{
		player_params[index].gaityaw = (atan2(est_velocity.y, est_velocity.x) * (180 / M_PI));

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
	StudioEstimateGait(index);

	// calc side to side turning
	float flYaw = fmod(player_params[index].angles[1] - player_params[index].gaityaw, 360.0f);

	if (flYaw < -180)
		flYaw += 360;

	else if (flYaw > 180)
		flYaw -= 360;

	if (player_params[index].m_flGaitMovement)
	{
		float maxyaw = 120.0;
		if (flYaw > maxyaw)
		{
			player_params[index].gaityaw -= 180;
			player_params[index].m_flGaitMovement = -player_params[index].m_flGaitMovement;
			flYaw -= 180;
		}
		else if (flYaw < -maxyaw)
		{
			player_params[index].gaityaw += 180;
			player_params[index].m_flGaitMovement = -player_params[index].m_flGaitMovement;
			flYaw += 180;
		}
	}

	float blend_yaw = (flYaw / 90.0) * 128.0 + 127.0;
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

/*
====================
StudioPlayerBlend
====================
*/
void HL_StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch)
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3.0f);

	if (*pBlend < pseqdesc->blendstart[0])
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0f;
		*pBlend = 0;
	}
	else if (*pBlend > pseqdesc->blendend[0])
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0f;
		*pBlend = 255;
	}
	else
	{
		if (pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1f) // catch qc error
			*pBlend = 127;
		else *pBlend = 255 * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0.0f;
	}
}

/*
====================
StudioEstimateGait
====================
*/
void HL_StudioEstimateGait(int index)
{
	vec3_t	est_velocity;
	float dt = player_params[index].m_clTime - player_params[index].m_clOldTime;

	if (dt < 0.0)
		dt = 0;

	else if (dt > 1.0)
		dt = 1;


	VectorSubtract(player_params[index].origin, player_params[index].m_prevgaitorigin, est_velocity);
	VectorCopy(player_params[index].origin, player_params[index].m_prevgaitorigin);
	player_params[index].m_flGaitMovement = (est_velocity).Length();

	if (dt <= 0.0f || player_params[index].m_flGaitMovement / dt < 5.0f)
	{
		player_params[index].m_flGaitMovement = 0.0f;
		est_velocity[0] = 0.0f;
		est_velocity[1] = 0.0f;
	}

	if (est_velocity[1] == 0.0f && est_velocity[0] == 0.0f)
	{
		float	flYawDiff = player_params[index].angles[1] - player_params[index].gaityaw;

		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180.0f) flYawDiff -= 360.0f;
		if (flYawDiff < -180.0f) flYawDiff += 360.0f;

		if (dt < 0.25f)
			flYawDiff *= dt * 4.0f;
		else flYawDiff *= dt;

		player_params[index].gaityaw += flYawDiff;
		player_params[index].gaityaw = player_params[index].gaityaw - (int)(player_params[index].gaityaw / 360) * 360;

		player_params[index].m_flGaitMovement = 0.0f;
	}
	else
	{
		player_params[index].gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if (player_params[index].gaityaw > 180.0f) player_params[index].gaityaw = 180.0f;
		if (player_params[index].gaityaw < -180.0f) player_params[index].gaityaw = -180.0f;
	}

}

/*
====================
StudioProcessGait
====================
*/
void HL_StudioProcessGait(int index)
{
	mstudioseqdesc_t* pseqdesc;
	int		iBlend{};
	float		flYaw; // view direction relative to movement

	auto ent = INDEXENT(index + 1);
	if (!ent)
	{
		return;
	}


	g_pstudiohdr = (studiohdr_t*)GET_MODEL_PTR(ent);

	if (!g_pstudiohdr)
		return;

	pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + player_params[index].sequence;

	if (player_params[index].sequence >= g_pstudiohdr->numseq)
		player_params[index].sequence = 0;

	float dt = player_params[index].m_clTime - player_params[index].m_clOldTime;

	if (dt < 0.0)
		dt = 0;

	else if (dt > 1.0)
		dt = 1;

	pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + player_params[index].sequence;


	HL_StudioPlayerBlend(pseqdesc, &iBlend, &player_params[index].angles[0]);

	player_params[index].prevangles[0] = player_params[index].angles[0];
	player_params[index].blending[0] = iBlend;
	player_params[index].prevblending[0] = player_params[index].blending[0];
	player_params[index].prevseqblending[0] = player_params[index].blending[0];
	HL_StudioEstimateGait(index);

	// calc side to side turning
	flYaw = player_params[index].angles[1] - player_params[index].gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180.0f) flYaw = flYaw + 360.0f;
	if (flYaw > 180.0f) flYaw = flYaw - 360.0f;

	if (flYaw > 120.0f)
	{
		player_params[index].gaityaw = player_params[index].gaityaw - 180.0f;
		player_params[index].m_flGaitMovement = -player_params[index].m_flGaitMovement;
		flYaw = flYaw - 180.0f;
	}
	else if (flYaw < -120.0f)
	{
		player_params[index].gaityaw = player_params[index].gaityaw + 180.0f;
		player_params[index].m_flGaitMovement = -player_params[index].m_flGaitMovement;
		flYaw = flYaw + 180.0f;
	}

	// adjust torso
	player_params[index].controller[0] = ((flYaw / 4.0f) + 30.0f) / (60.0f / 255.0f);
	player_params[index].controller[1] = ((flYaw / 4.0f) + 30.0f) / (60.0f / 255.0f);
	player_params[index].controller[2] = ((flYaw / 4.0f) + 30.0f) / (60.0f / 255.0f);
	player_params[index].controller[3] = ((flYaw / 4.0f) + 30.0f) / (60.0f / 255.0f);
	player_params[index].prevcontroller[0] = player_params[index].controller[0];
	player_params[index].prevcontroller[1] = player_params[index].controller[1];
	player_params[index].prevcontroller[2] = player_params[index].controller[2];
	player_params[index].prevcontroller[3] = player_params[index].controller[3];

	player_params[index].angles[1] = player_params[index].gaityaw;
	if (player_params[index].angles[1] < -0) player_params[index].angles[1] += 360.0f;
	player_params[index].prevangles[1] = player_params[index].angles[1];

	if (player_params[index].gaitsequence >= g_pstudiohdr->numseq)
		player_params[index].gaitsequence = 0;

	pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + player_params[index].gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement[0] > 0)
		player_params[index].gaitframe += (player_params[index].m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	else player_params[index].gaitframe += pseqdesc->fps * dt;

	// do modulo
	player_params[index].gaitframe = player_params[index].gaitframe - (int)(player_params[index].gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if (player_params[index].gaitframe < 0) player_params[index].gaitframe += pseqdesc->numframes;
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

void ProcessAnimParams(int id, float frametime, player_anim_params_s& params, player_anim_params_s* prev_params, entity_state_s* state, edict_t* ent)
{
	int i;
	if (state && prev_params)
	{
		player_params[id] = *prev_params;
		player_params[id].sequence = state->sequence;
		player_params[id].gaitsequence = state->gaitsequence;
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
		player_params[id].m_clTime = state->animtime + frametime;

		player_params[id].m_clOldTime = prev_params->m_clTime;

		player_params[id].m_prevgaitorigin = prev_params->origin;

		player_params[id].prevangles = prev_params->angles;
		player_params[id].prevframe = prev_params->frame;
		player_params[id].prevsequence = prev_params->sequence;

		if (player_params[id].sequence < 0)
			player_params[id].sequence = 0;

		// sequence has changed, hold the previous sequence info
		if (player_params[id].sequence != player_params[id].prevsequence)
		{
			player_params[id].sequencetime = player_params[id].animtime + 0.01f;

			// save current blends to right lerping from last sequence
			for (int i = 0; i < 2; i++)
				player_params[id].prevseqblending[i] = prev_params->blending[i];
			player_params[id].prevsequence = prev_params->sequence; // save old sequence	
		}


		// copy controllers
		for (i = 0; i < 4; i++)
		{
			if (player_params[id].controller[i] != prev_params->controller[i])
				player_params[id].prevcontroller[i] = prev_params->controller[i];
		}

		// copy blends
		for (i = 0; i < 2; i++)
			player_params[id].prevblending[i] = prev_params->blending[i];
	}
	else
	{

		player_params[id].sequence = ent->v.sequence;
		player_params[id].gaitsequence = ent->v.gaitsequence;
		player_params[id].prevangles = player_params[id].angles;
		player_params[id].prevframe = player_params[id].frame;
		player_params[id].frame = ent->v.frame;
		player_params[id].angles = ent->v.angles;
		player_params[id].origin = ent->v.origin;
		player_params[id].animtime = ent->v.animtime;
		player_params[id].m_clOldTime = player_params[id].m_clTime;
		player_params[id].m_clTime = ent->v.animtime;
		player_params[id].framerate = ent->v.framerate;
		player_params[id].controller[0] = ent->v.controller[0];
		player_params[id].controller[1] = ent->v.controller[1];
		player_params[id].controller[2] = ent->v.controller[2];
		player_params[id].controller[3] = ent->v.controller[3];
		player_params[id].blending[0] = ent->v.blending[0];
		player_params[id].blending[1] = ent->v.blending[1];
		if (player_params[id].sequence < 0)
			player_params[id].sequence = 0;

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

	}

	if (g_eGameType == GT_CStrike || g_eGameType == GT_CZero)
	{
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
	}
	else
	{
		if (player_params[id].gaitsequence)
		{
			HL_StudioProcessGait(id);
			//player_params[id].angles[0] = -player_params[id].angles[0]; // stupid quake bug
			//player_params[id].angles[0] = 0.0f;
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
		}
	}
}

void PlayerPostThinkPost(edict_t* pEntity)
{
	int i;
	auto id = ENTINDEX(pEntity) - 1;
	auto _host_client = g_RehldsSvs->GetClient_t(id);

	ProcessAnimParams(id, _host_client->lastcmd.msec * 0.001f, player_params[id], nullptr, nullptr, pEntity);
	sv_adjusted_positions_t* pos;
	client_t* cli;

	if (nofind)
	{
		nofind = 0;
		RETURN_META(MRES_IGNORED);
	}

	if (!MDLL_AllowLagCompensation())
		RETURN_META(MRES_IGNORED);

	if (g_RehldsSvs->GetMaxClients() <= 1 || sv_unlag->value == 0.0)
		RETURN_META(MRES_IGNORED);

	if (!_host_client->lw || !_host_client->lc || !_host_client->active)
		RETURN_META(MRES_IGNORED);

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
	RETURN_META(MRES_IGNORED);
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
	player_params[id] = player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence + 1)][id];
	ProcessAnimParams(id, _host_client->lastcmd.msec * 0.001f, player_params[id],
		&player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence)][id],
		state, host);

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
	orig_ppinterface = ppinterface;
	if(Server_GetBlendingInterfaceHook)
		subhook_remove(Server_GetBlendingInterfaceHook);
	if(Server_GetBlendingInterfaceOrig)
		Server_GetBlendingInterfaceOrig(version, ppinterface, pstudio, rotationmatrix, bonetransform);
	orig_interface = **ppinterface;
	if(Server_GetBlendingInterfaceHook)
		subhook_install(Server_GetBlendingInterfaceHook);
	if (version != SV_BLENDING_INTERFACE_VERSION)
		return 0;

	if (g_eGameType == GT_CStrike || g_eGameType == GT_CZero)
		(*ppinterface)->SV_StudioSetupBones = decltype((*ppinterface)->SV_StudioSetupBones)(CS_StudioSetupBones);
	else
		(*ppinterface)->SV_StudioSetupBones = decltype((*ppinterface)->SV_StudioSetupBones)(HL_StudioSetupBones);


	IEngineStudio.Mem_Calloc = pstudio->Mem_Calloc;
	IEngineStudio.Cache_Check = pstudio->Cache_Check;
	IEngineStudio.LoadCacheFile = pstudio->LoadCacheFile;
	IEngineStudio.Mod_Extradata = ((struct server_studio_api_s*)pstudio)->Mod_Extradata;

	g_pRotationMatrix = (float(*)[3][4])rotationmatrix;
	g_pBoneTransform = (float(*)[128][3][4])bonetransform;

	return 1;
}

void NormalizePath(char* path)
{
	for (char* cp = path; *cp; cp++) {
		if (isupper(*cp))
			*cp = tolower(*cp);

		if (*cp == '\\')
			*cp = '/';
	}
}

void HF_Exec_Config()
{
	if (!g_ExecConfigCmd[0]) {
		return;
	}

	g_engfuncs.pfnServerCommand(g_ExecConfigCmd);
	g_engfuncs.pfnServerExecute();
}

bool HF_Init_Config()
{
	const char* pszGameDir = GET_GAME_INFO(PLID, GINFO_GAMEDIR);
	const char* pszPluginDir = GET_PLUGIN_PATH(PLID);

	char szRelativePath[MAX_PATH];
	strncpy(szRelativePath, &pszPluginDir[strlen(pszGameDir) + 1], sizeof(szRelativePath) - 1);
	szRelativePath[sizeof(szRelativePath) - 1] = '\0';
	NormalizePath(szRelativePath);

	char* pos = strrchr(szRelativePath, '/');
	if (pos) {
		*(pos + 1) = '\0';
	}

	snprintf(g_ExecConfigCmd, sizeof(g_ExecConfigCmd), "exec \"%s%s\"\n", szRelativePath, CFG_FILE);
	return true;
}

#if defined(__linux__) || defined(__APPLE__)
void* dlsym_hook(void* __restrict __handle,
	const char* __restrict __name)
{
	subhook_remove(dlsymHook);
	auto ret = dlsym(__handle, __name);
	if (__name && !strcmp(__name, "Server_GetBlendingInterface"))
	{
		ret = (void*)Server_GetBlendingInterface;
	}
	else
	{
		subhook_install(dlsymHook);
	}

	return ret;
}

#else

FARPROC WINAPI GetProcAddressHooked(
	_In_ HMODULE hModule,
	_In_ LPCSTR lpProcName)
{
	subhook_remove(GetProcAddressHook);
	auto ret = GetProcAddress(hModule, lpProcName);
	if (lpProcName && !strcmp(lpProcName, "Server_GetBlendingInterface"))
	{
		ret = (FARPROC)Server_GetBlendingInterface;
	}
	else
	{
		subhook_install(GetProcAddressHook);
	}

	return ret;
}
#endif
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
	CVAR_REGISTER(&hf_hitbox_fix);
	phf_hitbox_fix = CVAR_GET_POINTER(hf_hitbox_fix.name);
	HF_Init_Config();
	HF_Exec_Config();

	char gameDir[512];
	GET_GAME_DIR(gameDir);
	char* a = gameDir;
	int i = 0;

	while (gameDir[i])
		if (gameDir[i++] == '/')
			a = &gameDir[i];

	std::string linux_game_library = "cs.so";
	std::string game_library = "mp.dll";

	if (g_eGameType == GT_Unitialized)
	{
		if (!strcmp(a, "valve"))
		{
			game_library = "hl.dll";
			linux_game_library = "hl.so";
			g_eGameType = GT_HL1;
		}
		else if (!strcmp(a, "cstrike") || !strcmp(a, "cstrike_beta"))
		{
			g_eGameType = GT_CStrike;
		}
		else if (!strcmp(a, "czero"))
		{
			g_eGameType = GT_CZero;
		}
		else if (!strcmp(a, "czeror"))
		{
			g_eGameType = GT_CZeroRitual;
		}
		else if (!strcmp(a, "terror"))
		{
			g_eGameType = GT_TerrorStrike;
		}
		else if (!strcmp(a, "tfc"))
		{
			g_eGameType = GT_TFC;
		}
	}
	if (g_eGameType != GT_CZero && g_eGameType != GT_CStrike && g_eGameType != GT_HL1)
	{
		UTIL_ServerPrint("Hitbox fixer is not ready for your mod. (%s). Create issue on github [https://github.com/Garey27/hitbox_fixer] if you wanna see support for this game.", game_library.c_str());
		return false;
	}

#if 1
#if defined(__linux__) || defined(__APPLE__)


	ModuleInfo info = Handles::GetModuleInfo(linux_game_library.c_str());
	Server_GetBlendingInterfaceOrig = decltype(Server_GetBlendingInterfaceOrig)(dlsym(info.handle, "Server_GetBlendingInterface"));
	if (Server_GetBlendingInterfaceOrig)
	{
		Server_GetBlendingInterfaceHook = subhook_new(
			(void*)Server_GetBlendingInterfaceOrig, (void*)Server_GetBlendingInterface, (subhook_flags_t)0);
		subhook_install(Server_GetBlendingInterfaceHook);
	}
	else
	{
		dlsymHook = subhook_new(
			(void*)dlsym, (void*)dlsym_hook, (subhook_flags_t)0);
		subhook_install(dlsymHook);
	}

#else
	Server_GetBlendingInterfaceOrig = decltype(Server_GetBlendingInterfaceOrig)(GetProcAddress((HMODULE)GetModuleHandleA(game_library.c_str()), "Server_GetBlendingInterface"));
	if (Server_GetBlendingInterfaceOrig)
	{
		Server_GetBlendingInterfaceHook = subhook_new(
			(void*)Server_GetBlendingInterfaceOrig, (void*)Server_GetBlendingInterface, (subhook_flags_t)0);
		subhook_install(Server_GetBlendingInterfaceHook);
	}
	else
	{
#if 0
		auto func = g_engfuncs.pfnGetBonePosition;
		auto SV_SetupBonesfunc = 0;
		for (byte* pos = (byte*)func, *end = (byte*)((DWORD)func + 512); pos < end; pos++)
		{
			/* .text:049A18FE FF D0                                   call    eax */
			if (*pos == 0xff && *(pos + 1) == 0xd0)
			{
				orig_ppinterface = decltype(orig_ppinterface)(*(DWORD*)(pos - 7));

			}
		}
		if (orig_ppinterface && (*orig_ppinterface)->version == 0x1)
		{
		}
#endif
		GetProcAddressHook = subhook_new(
			(void*)GetProcAddress, (void*)GetProcAddressHooked, (subhook_flags_t)0);
		subhook_install(GetProcAddressHook);

	}
#endif
#endif
	Init = true;
	return true;
}


void OnMetaDetach()
{
	subhook_remove(Server_GetBlendingInterfaceHook);
	(*orig_ppinterface)->SV_StudioSetupBones = orig_interface.SV_StudioSetupBones;
}
