#include "precompiled.h"

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
std::unique_ptr<players_api> api;
GameType_e g_eGameType;
const char CFG_FILE[] = "hbf.cfg";
extern server_studio_api_t IEngineStudio;
extern studiohdr_t* g_pstudiohdr;
extern float(*g_pRotationMatrix)[3][4];
extern float(*g_pBoneTransform)[128][3][4];
extern void CS_StudioProcessParams(int player, player_anim_params_s& params);
void ProcessAnimParams(int id, int host_id, player_anim_params_s& params, player_anim_params_s& prev_params, entity_state_s* state);
void UpdateClientAnimParams(int id, int host_id, player_anim_params_s& params, float frametime, float interp);
void VectorMA(const vec_t* veca, float scale, const vec_t* vecm, vec_t* out);
sv_blending_interface_s** orig_ppinterface;
sv_blending_interface_s orig_interface;
// Resource counts
#define MAX_MODEL_INDEX_BITS		9	// sent as a short
#define MAX_MODELS			(1<<MAX_MODEL_INDEX_BITS)
qboolean nofind = false;
typedef int(*SV_BLENDING_INTERFACE_FUNC)(int, struct sv_blending_interface_s**, struct server_studio_api_s*, float*, float*);

// How many data slots to use when in multiplayer (must be power of 2)
#define MULTIPLAYER_BACKUP			64
constexpr int SV_UPDATE_BACKUP = MULTIPLAYER_BACKUP;
constexpr int SV_UPDATE_MASK = (SV_UPDATE_BACKUP - 1);

uint32_t ServerFrameId;
player_anim_params_s player_params[MAX_CLIENTS]{};

class AnimProcessor
{
	std::deque<std::pair<uint32_t, player_ent_hist_params_s>> history[33];
public:
	float interpTime[33];
	player_anim_params_s processed_params[33];
	void add_history(int id, uint32_t out_seq, entity_state_t* state)
	{
		player_ent_hist_params_s params;
		params.sendTime = gpGlobals->time;
		params.sequence = state->sequence;
		params.gaitsequence = state->gaitsequence;
		params.frame = uint32_t(state->frame);
		params.angles.x = state->angles.x;
		params.angles.y = state->angles.y;
		params.angles.z = state->angles.z;

		params.origin.x = state->origin.x;
		params.origin.y = state->origin.y;
		params.origin.z = state->origin.z;


		params.animtime = state->animtime;
		params.framerate = state->framerate;
		params.controller[0] = state->controller[0];
		params.controller[1] = state->controller[1];
		params.controller[2] = state->controller[2];
		params.controller[3] = state->controller[3];
		params.blending[0] = state->blending[0];
		params.blending[1] = state->blending[1];

		history[id].push_back({ out_seq, params });
	}
	void addFrametime(int id, float time)
	{
		interpTime[id] += time;
	}
	bool process_anims(int id, uint32_t seq, float lerp_time, player_anim_params_s& params)
	{
		auto size = history[id].size();
		if (!size)
			return false;

		auto processed = false;
		for (auto& hist: history[id])
		{
			if (hist.first > seq)
			{
				vec3_t delta = (hist.second.origin - params.origin);
				float frac = (interpTime[id] + lerp_time) / (hist.second.sendTime - params.serverSendTime);
				if (frac > 1.f)
					frac = 1.f;
				VectorMA(params.origin, frac, delta, params.origin);
				break;
			}
			interpTime[id] = 0.f;
			processed = true;
			params.serverSendTime = hist.second.sendTime;
			params.sequence = hist.second.sequence;
			params.gaitsequence = hist.second.gaitsequence;
			params.frame = uint32_t(hist.second.frame);
			params.angles.x = hist.second.angles.x;
			params.angles.y = hist.second.angles.y;
			params.angles.z = hist.second.angles.z;

			params.origin.x = hist.second.origin.x;
			params.origin.y = hist.second.origin.y;
			params.origin.z = hist.second.origin.z;


			params.animtime = params.m_clTime;
			params.framerate = hist.second.framerate;

			if (params.sequence < 0)
				params.sequence = 0;

			// sequence has changed, hold the previous sequence info
			if (params.sequence != params.prevsequence)
			{
				params.sequencetime = params.animtime + 0.01f;

				// save current blends to right lerping from last sequence
				for (int i = 0; i < 2; i++)
					params.prevseqblending[i] = params.blending[i];
				params.prevsequence = params.sequence; // save old sequence	
			}


			// copy controllers
			for (int i = 0; i < 4; i++)
			{
				if (hist.second.controller[i] != params.controller[i])
					params.prevcontroller[i] = params.controller[i];
			}

			// copy blends
			for (int i = 0; i < 2; i++)
				params.prevblending[i] = params.blending[i];

			params.controller[0] = hist.second.controller[0];
			params.controller[1] = hist.second.controller[1];
			params.controller[2] = hist.second.controller[2];
			params.controller[3] = hist.second.controller[3];
			params.blending[0] = hist.second.blending[0];
			params.blending[1] = hist.second.blending[1];
		}
		while (!history[id].empty() && history[id].front().first < seq)
		{
			history[id].pop_front();
		}
		return processed;
	}


};
AnimProcessor PlayerAnimProcessor[33];
subhook_t Server_GetBlendingInterfaceHook{};

subhook_t GetProcAddressHook{};
subhook_t dlsymHook{};
#define DEBUG
#ifdef DEBUG

cvar_t hbf_timescale = { "hbf_timescale", "1", FCVAR_SERVER | FCVAR_PROTECTED, 0.0f, NULL };
cvar_t* phbf_timescale;
bool TestingHitboxes = false;
uintptr_t TestingSequence;
float(g_pRotationMatrixCopy)[3][4];
float(g_pBoneTransformCopy)[128][3][4];
#endif
void (PutInServer)(edict_t* pEntity)
{
	if (!pEntity)
	{
		RETURN_META(MRES_IGNORED);
	}
	auto host_id = ENTINDEX(pEntity);
	PlayerAnimProcessor[host_id] = {};
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
	auto host_id = ENTINDEX(pEntity);
	auto _host_client = api->GetClient(host_id-1);
	client_t* cl;
	float cl_interptime = 0.f;
	entity_state_t* state;
	int i;
	client_frame_t* frame;
	vec3_t origin;
	vec3_t delta;
	vec3_t angles;
	vec3_t mins;
	vec3_t maxs;

	nofind = 1;
	if (_host_client->fakeclient)
		RETURN_META(MRES_IGNORED);

	if (!MDLL_AllowLagCompensation() || sv_unlag->value == 0.0f || !_host_client->lw || !_host_client->lc)
	{
		RETURN_META(MRES_IGNORED);
	}

	if (api->GetMaxClients() <= 1 || !_host_client->active)
		RETURN_META(MRES_IGNORED);

	nofind = 0;

	size_t frame_index = SV_UPDATE_MASK & (_host_client->netchan.incoming_acknowledged);
	frame = &_host_client->frames[frame_index];

	for (i = 0; i < frame->entities.num_entities; i++)
	{
		state = &frame->entities.entities[i];

		if (state->number <= 0)
			continue;

		if (state->number > api->GetMaxClients())
			break; // players are always in the beginning of the list, no need to look more


		cl = api->GetClient(state->number - 1);
		if (cl == _host_client || !cl->active)
			continue;
		auto client_id = state->number - 1;
		auto cmd = _host_client->lastcmd;
		PlayerAnimProcessor[host_id].process_anims(client_id, _host_client->netchan.incoming_acknowledged, cmd.lerp_msec * 0.001f, PlayerAnimProcessor[host_id].processed_params[client_id]);
		PlayerAnimProcessor[host_id].addFrametime(client_id, cmd.msec * 0.001f);
		if (host_id == 1 && i == 1)
		{
			UpdateClientAnimParams(client_id, host_id, PlayerAnimProcessor[host_id].processed_params[client_id], cmd.msec * 0.001f, cmd.lerp_msec * 0.001f);
			//UTIL_ServerPrint("%f\n", player_params[client_id].gaityaw);
		}
		player_params[client_id] = PlayerAnimProcessor[host_id].processed_params[client_id];
#ifdef DEBUG
		static bool Init = false;
		
#if 10
		if (host_id == 1 && i == 1 && cl->edict->v.modelindex)
		{
			if (!Init)
			{
				Init = true;
				UTIL_ServerPrint("\n\n\nDEBUG AT %p | %p \n\n\n", &g_pRotationMatrixCopy, &g_pBoneTransformCopy);
				g_engfuncs.pfnClientCommand(pEntity, "hbdeb_test_rot_matrix %x \n", &g_pRotationMatrixCopy);
				g_engfuncs.pfnClientCommand(pEntity, "hbdeb_test_transform %x \n", &g_pBoneTransformCopy);
				g_engfuncs.pfnClientCommand(pEntity, "hbdeb_test_hitboxes %x \n", &TestingHitboxes);
				g_engfuncs.pfnClientCommand(pEntity, "r_drawentities 6 \n", &TestingHitboxes);
				g_engfuncs.pfnAddServerCommand("test_hb", [] {
					TestingHitboxes = !TestingHitboxes;
					});
			}
			auto model = api->GetModel(cl->edict->v.modelindex);
			//UTIL_ServerPrint("%f %f %f\n", player_params[state->number - 1].origin[0], player_params[state->number - 1].origin[1], player_params[state->number - 1].origin[2]);

			CS_StudioSetupBones(
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
#endif
	}
	RETURN_META(MRES_IGNORED);
}


void StudioEstimateGait(player_anim_params_s& params)
{
	float dt;
	Vector est_velocity;

	dt = params.m_clTime - params.m_clOldTime;

	if (dt < 0)
		dt = 0;

	else if (dt > 1.0)
		dt = 1;

	if (dt == 0)
	{
		params.m_flGaitMovement = 0;
		return;
	}

	est_velocity = params.origin - params.m_prevgaitorigin;
	params.m_prevgaitorigin = params.origin;

	params.m_flGaitMovement = est_velocity.Length();
	
	if (dt <= 0 || params.m_flGaitMovement / dt < 5)
	{
		params.m_flGaitMovement = 0;

		est_velocity.x = 0;
		est_velocity.y = 0;
	}

	float flYaw = params.angles.y - params.gaityaw;


	if (params.sequence > 100) {
		params.gaityaw += flYaw;
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
			params.m_flYawModifier = 0.05;

		if (flYaw < -90.0 || flYaw > 90.0)
			params.m_flYawModifier = 3.5;

		if (dt < 0.25)
			flYawDiff *= dt * params.m_flYawModifier;
		else
			flYawDiff *= dt;

		if (abs(flYawDiff) < 0.1)
			flYawDiff = 0;

		params.gaityaw += flYawDiff;
		params.gaityaw = params.gaityaw - (int)(params.gaityaw / 360) * 360;
		params.m_flGaitMovement = 0;
	}
	else
	{
		params.gaityaw = (atan2(est_velocity.y, est_velocity.x) * (180 / M_PI));

		if (params.gaityaw > 180)
			params.gaityaw = 180;

		if (params.gaityaw < -180)
			params.gaityaw = -180;
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

void CalculatePitchBlend(player_anim_params_s& params)
{
	int iBlend;

	StudioPlayerBlend(&iBlend, &params.angles.x);

	params.blending[1] = iBlend;
	params.prevblending[1] = params.blending[1];
	params.prevseqblending[1] = params.blending[1];

}

void CalculateYawBlend(player_anim_params_s& params)
{
	StudioEstimateGait(params);

	// calc side to side turning
	float flYaw = fmod(params.angles[1] - params.gaityaw, 360.0f);

	if (flYaw < -180)
		flYaw += 360;

	else if (flYaw > 180)
		flYaw -= 360;

	if (params.m_flGaitMovement)
	{
		float maxyaw = 120.0;
		if (flYaw > maxyaw)
		{
			params.gaityaw -= 180;
			params.m_flGaitMovement = -params.m_flGaitMovement;
			flYaw -= 180;
		}
		else if (flYaw < -maxyaw)
		{
			params.gaityaw += 180;
			params.m_flGaitMovement = -params.m_flGaitMovement;
			flYaw += 180;
		}
	}

	float blend_yaw = (flYaw / 90.0) * 128.0 + 127.0;
	blend_yaw = clamp<float>(blend_yaw, 0.0f, 255.0f);
	blend_yaw = 255.0 - blend_yaw;

	params.blending[0] = (int)(blend_yaw);
	params.prevblending[0] = params.blending[0];
	params.prevseqblending[0] = params.blending[0];
	params.angles[1] = params.gaityaw;

	if (params.angles[1] < 0)
		params.angles[1] += 360;
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
void HL_StudioEstimateGait(player_anim_params_s& params)
{
	vec3_t	est_velocity;
	float dt = params.m_clTime - params.m_clOldTime;

	if (dt < 0.0)
		dt = 0;

	else if (dt > 1.0)
		dt = 1;


	VectorSubtract(params.origin, params.m_prevgaitorigin, est_velocity);
	VectorCopy(params.origin, params.m_prevgaitorigin);
	params.m_flGaitMovement = (est_velocity).Length();

	if (dt <= 0.0f || params.m_flGaitMovement / dt < 5.0f)
	{
		params.m_flGaitMovement = 0.0f;
		est_velocity[0] = 0.0f;
		est_velocity[1] = 0.0f;
	}

	if (est_velocity[1] == 0.0f && est_velocity[0] == 0.0f)
	{
		float	flYawDiff = params.angles[1] - params.gaityaw;

		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180.0f) flYawDiff -= 360.0f;
		if (flYawDiff < -180.0f) flYawDiff += 360.0f;

		if (dt < 0.25f)
			flYawDiff *= dt * 4.0f;
		else flYawDiff *= dt;

		params.gaityaw += flYawDiff;
		params.gaityaw = params.gaityaw - (int)(params.gaityaw / 360) * 360;

		params.m_flGaitMovement = 0.0f;
	}
	else
	{
		params.gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if (params.gaityaw > 180.0f) params.gaityaw = 180.0f;
		if (params.gaityaw < -180.0f) params.gaityaw = -180.0f;
	}

}

/*
====================
StudioProcessGait
====================
*/
void HL_StudioProcessGait(player_anim_params_s& params)
{
	mstudioseqdesc_t* pseqdesc;
	int		iBlend{};
	float		flYaw; // view direction relative to movement

	auto ent = INDEXENT(params.playerId);
	if (!ent)
	{
		return;
	}


	g_pstudiohdr = (studiohdr_t*)GET_MODEL_PTR(ent);

	if (!g_pstudiohdr)
		return;

	pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + params.sequence;

	if (params.sequence >= g_pstudiohdr->numseq)
		params.sequence = 0;

	float dt = params.m_clTime - params.m_clOldTime;

	if (dt < 0.0)
		dt = 0;

	else if (dt > 1.0)
		dt = 1;

	pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + params.sequence;


	HL_StudioPlayerBlend(pseqdesc, &iBlend, &params.angles[0]);

	params.blending[0] = iBlend;
	params.prevblending[0] = params.blending[0];
	params.prevseqblending[0] = params.blending[0];
	HL_StudioEstimateGait(params);

	// calc side to side turning
	flYaw = params.angles[1] - params.gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180.0f) flYaw = flYaw + 360.0f;
	if (flYaw > 180.0f) flYaw = flYaw - 360.0f;

	if (flYaw > 120.0f)
	{
		params.gaityaw = params.gaityaw - 180.0f;
		params.m_flGaitMovement = -params.m_flGaitMovement;
		flYaw = flYaw - 180.0f;
	}
	else if (flYaw < -120.0f)
	{
		params.gaityaw = params.gaityaw + 180.0f;
		params.m_flGaitMovement = -params.m_flGaitMovement;
		flYaw = flYaw + 180.0f;
	}

	// adjust torso
	params.controller[0] = ((flYaw / 4.0f) + 30.0f) / (60.0f / 255.0f);
	params.controller[1] = ((flYaw / 4.0f) + 30.0f) / (60.0f / 255.0f);
	params.controller[2] = ((flYaw / 4.0f) + 30.0f) / (60.0f / 255.0f);
	params.controller[3] = ((flYaw / 4.0f) + 30.0f) / (60.0f / 255.0f);
	params.prevcontroller[0] = params.controller[0];
	params.prevcontroller[1] = params.controller[1];
	params.prevcontroller[2] = params.controller[2];
	params.prevcontroller[3] = params.controller[3];

	params.angles[1] = params.gaityaw;
	if (params.angles[1] < -0) params.angles[1] += 360.0f;

	if (params.gaitsequence >= g_pstudiohdr->numseq)
		params.gaitsequence = 0;

	pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + params.gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement[0] > 0)
		params.gaitframe += (params.m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	else params.gaitframe += pseqdesc->fps * dt;

	// do modulo
	params.gaitframe = params.gaitframe - (int)(params.gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if (params.gaitframe < 0) params.gaitframe += pseqdesc->numframes;
}


void StudioProcessGait(player_anim_params_s& params)
{
	mstudioseqdesc_t* pseqdesc;
	float dt = params.m_clTime - params.m_clOldTime;

	if (dt < 0.f)
		dt = 0;

	else if (dt > 1.f)
		dt = 1.f;

	
	CalculateYawBlend(params);
	CalculatePitchBlend(params);
	auto ent = INDEXENT(params.playerId);
	if (!ent)
	{
		return;
	}
	studiohdr_t* pstudiohdr = (studiohdr_t*)GET_MODEL_PTR(ent);

	if (!pstudiohdr)
		return;

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + params.gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement.x > 0.0f)
		params.gaitframe += (params.m_flGaitMovement / pseqdesc->linearmovement.x) * pseqdesc->numframes;
	else
		params.gaitframe += params.framerate * pseqdesc->fps * dt;

	// do modulo
	params.gaitframe -= int(params.gaitframe / pseqdesc->numframes) * pseqdesc->numframes;

	if (params.gaitframe < 0)
		params.gaitframe += pseqdesc->numframes;
}

void NormalizeAngles(vec3_t& angles)
{
	int i;
	// Normalize angles
	for (i = 0; i < 3; i++)
	{
		if (angles[i] > 180.0)
		{
			angles[i] -= 360.0;
		}
		else if (angles[i] < -180.0)
		{
			angles[i] += 360.0;
		}
	}
}

inline void InterpolateAngles(vec3_t start, vec3_t end, vec3_t& output, float frac)
{
	int i;
	float ang1, ang2;
	float d;

	NormalizeAngles(start);
	NormalizeAngles(end);

	for (i = 0; i < 3; i++)
	{
		ang1 = start[i];
		ang2 = end[i];

		d = ang2 - ang1;
		if (d > 180)
		{
			d -= 360;
		}
		else if (d < -180)
		{
			d += 360;
		}

		output[i] = ang1 + d * frac;
	}

	NormalizeAngles(output);
}

void UpdateClientAnimParams(int id, int host_id, player_anim_params_s& params, float frametime, float interp)
{
	params.playerId = id + 1;

	params.m_clOldTime = params.m_clTime;
	params.m_clTime += frametime;
	//params.animtime = params.m_clTime;
	auto _host_client = api->GetClient(host_id - 1);
	auto t = params.m_clTime + interp;	

	//params.m_prevgaitorigin = params.origin;
	//params.prevangles = params.angles;
	
	//}
	if (g_eGameType == GT_CStrike || g_eGameType == GT_CZero)
	{
		if (params.gaitsequence)
		{
			StudioProcessGait(params);
			CS_StudioProcessParams(id, params);
		}
		else
		{
			params.controller[0] = 127;
			params.controller[1] = 127;
			params.controller[2] = 127;
			params.controller[3] = 127;
			params.prevcontroller[0] = params.controller[0];
			params.prevcontroller[1] = params.controller[1];
			params.prevcontroller[2] = params.controller[2];
			params.prevcontroller[3] = params.controller[3];

			CalculatePitchBlend(params);
			CalculateYawBlend(params);
		}
	}
	else
	{
		if (params.gaitsequence)
		{
			HL_StudioProcessGait(params);
			//params.angles[0] = -params.angles[0]; // stupid quake bug
			//params.angles[0] = 0.0f;
		}
		else
		{
			params.controller[0] = 127;
			params.controller[1] = 127;
			params.controller[2] = 127;
			params.controller[3] = 127;
			params.prevcontroller[0] = params.controller[0];
			params.prevcontroller[1] = params.controller[1];
			params.prevcontroller[2] = params.controller[2];
			params.prevcontroller[3] = params.controller[3];
		}
	}
	
}

void PlayerPostThinkPost(edict_t* pEntity)
{
	nofind = 0;	

	RETURN_META(MRES_IGNORED);
}


int	(AddToFullPackPost)(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, int player, unsigned char* pSet)
{
	int i;
	auto host_id = ENTINDEX(host);
	auto _host_client = api->GetClient(host_id-1);
	if (!player || ent == host)
	{
		RETURN_META_VALUE(MRES_IGNORED, 0);
	}
	auto id = ENTINDEX(ent) - 1;
	PlayerAnimProcessor[host_id].add_history(id, _host_client->netchan.outgoing_sequence, state);

	RETURN_META_VALUE(MRES_IGNORED, 0);
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
	api = std::make_unique<rehlds_api>();
	if (!api->Init())
	{		
		if (g_RehldsApi)
		{
			UTIL_ServerPrint("Hitbox fixer is not compatible with your ReHLDS version. Hitbox Fixer needs at least 3.10 version.");
			return false;
		}
		api = std::make_unique<hlds_api>();
		if (!api->Init())
		{
			UTIL_ServerPrint("Hitbox fixer is not compatible with your HLDS version. Create issue on github [https://github.com/Garey27/hitbox_fixer].");
			return false;
		}
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
		else if (!strcmp(a, "ag"))
		{
			game_library = "ag.dll";
			linux_game_library = "ag.so";
			g_eGameType = GT_HL1;
		}
		else if (!strcmp(a, "firearms"))
		{
			game_library = "firearms.dll";
			linux_game_library = "fa_i386.so";
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

		GetProcAddressHook = subhook_new(
			(void*)GetProcAddress, (void*)GetProcAddressHooked, (subhook_flags_t)0);
		subhook_install(GetProcAddressHook);
	}
#endif
	Init = true;
	return true;
}


void OnMetaDetach()
{
	subhook_remove(Server_GetBlendingInterfaceHook);
	(*orig_ppinterface)->SV_StudioSetupBones = orig_interface.SV_StudioSetupBones;
	
}
