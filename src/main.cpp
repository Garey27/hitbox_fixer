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
//#define DEBUG
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
#ifdef DEBUG
void TestFunc(uint32_t host_id, float t, float frac, uintptr_t sequence)
{
	auto _host_client = api->GetClient(host_id);
	if (TestingHitboxes)
	{

		char msg[1024];
		snprintf(msg, 1024, "%.2f %.2f | %.2f %.2f\n", t, frac, player_params_history[host_id].hist[SV_UPDATE_MASK & sequence][1].t, player_params_history[host_id].hist[SV_UPDATE_MASK & sequence][1].frac);
		g_engfuncs.pfnServerPrint(msg);
	}
	for (int i = 0; i < api->GetMaxClients(); i++)
	{
		auto cl = api->GetClient(i);
		player_params[i] = player_params_history[host_id].hist[SV_UPDATE_MASK & sequence][i];
		if (cl == _host_client || !cl->active)
			continue;

		auto model = api->GetModel(cl->edict->v.modelindex);
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
}
#endif
void (PlayerPreThinkPre)(edict_t* pEntity)
{
	auto host_id = ENTINDEX(pEntity) - 1;
	auto _host_client = api->GetClient(host_id);
	client_t* cl;
	float cl_interptime = 0.f;
	entity_state_t* state;
	sv_adjusted_positions_t* pos;
	int i;
	client_frame_t* frame;
	vec3_t origin;
	vec3_t delta;
	float realtime = g_engfuncs.pfnTime();
	double targettime; // FP precision fix
	vec3_t angles;
	vec3_t mins;
	vec3_t maxs;

	memset(truepositions, 0, sizeof(truepositions));
	nofind = 1;
	if (_host_client->fakeclient)
		RETURN_META(MRES_IGNORED);

	if (!MDLL_AllowLagCompensation())
		RETURN_META(MRES_IGNORED);

	if (sv_unlag->value == 0.0f || !_host_client->lw || !_host_client->lc)
		RETURN_META(MRES_IGNORED);

	if (api->GetMaxClients() <= 1 || !_host_client->active)
		RETURN_META(MRES_IGNORED);


	nofind = 0;
	for (int i = 0; i < api->GetMaxClients(); i++)
	{
		cl = api->GetClient(i);
		if (cl == _host_client || !cl->active)
			continue;

		truepositions[i].active = 1;
		truepositions[i].extra = player_params[i];
	}
	

	if (SV_UPDATE_BACKUP <= 0)
	{
		memset(truepositions, 0, sizeof(truepositions));
		nofind = 1;
		RETURN_META(MRES_IGNORED);
	}

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

		pos = &truepositions[state->number - 1];
		if (pos->deadflag)
			continue;

		if (!pos->active)
		{
			continue;
		}

		static bool Init = false;
		player_params[state->number - 1] = player_params_history[host_id].hist[frame_index][state->number - 1];
		
		pos->needrelink = 1;
#ifdef DEBUG
		if (!Init)
		{
			Init = true;
			//UTIL_ServerPrint("\n\n\nDEBUG AT %p | %p \n\n\n", &g_pRotationMatrixCopy, &g_pBoneTransformCopy);
			g_engfuncs.pfnClientCommand(pEntity, "hbdeb_test_rot_matrix %x \n", &g_pRotationMatrixCopy);
			g_engfuncs.pfnClientCommand(pEntity, "hbdeb_test_transform %x \n", &g_pBoneTransformCopy);
			g_engfuncs.pfnClientCommand(pEntity, "hbdeb_test_hitboxes %x \n", &TestingHitboxes);
			g_engfuncs.pfnClientCommand(pEntity, "hbdeb_test_sequence %x \n", (uintptr_t)TestFunc);
			g_engfuncs.pfnClientCommand(pEntity, "r_drawentities 6 \n", &TestingHitboxes);
			g_engfuncs.pfnAddServerCommand("test_hb", [] {
				TestingHitboxes = !TestingHitboxes;
				});
		}
#if 0
		if (i == 1 && cl->edict->v.modelindex)
		{
			auto model = api->GetModel(cl->edict->v.modelindex);
		
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

	est_velocity = params.final_origin - params.m_prevgaitorigin;
	params.m_prevgaitorigin = params.final_origin;

	params.m_flGaitMovement = est_velocity.Length();
	
	if (dt <= 0 || params.m_flGaitMovement / dt < 5)
	{
		params.m_flGaitMovement = 0;

		est_velocity.x = 0;
		est_velocity.y = 0;
	}

	float flYaw = params.final_angles.y - params.gaityaw;


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

	StudioPlayerBlend(&iBlend, &params.final_angles.x);


	params.prevangles.x = params.final_angles.x;
	params.blending[1] = iBlend;
	params.prevblending[1] = params.blending[1];
	params.prevseqblending[1] = params.blending[1];

}

void CalculateYawBlend(player_anim_params_s& params)
{
	StudioEstimateGait(params);

	// calc side to side turning
	float flYaw = fmod(params.final_angles[1] - params.gaityaw, 360.0f);

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
	params.final_angles[1] = params.gaityaw;

	if (params.final_angles[1] < 0)
		params.final_angles[1] += 360;

	params.prevangles[1] = params.final_angles[1];
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


	VectorSubtract(params.final_origin, params.m_prevgaitorigin, est_velocity);
	VectorCopy(params.final_origin, params.m_prevgaitorigin);
	params.m_flGaitMovement = (est_velocity).Length();

	if (dt <= 0.0f || params.m_flGaitMovement / dt < 5.0f)
	{
		params.m_flGaitMovement = 0.0f;
		est_velocity[0] = 0.0f;
		est_velocity[1] = 0.0f;
	}

	if (est_velocity[1] == 0.0f && est_velocity[0] == 0.0f)
	{
		float	flYawDiff = params.final_angles[1] - params.gaityaw;

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


	HL_StudioPlayerBlend(pseqdesc, &iBlend, &params.final_angles[0]);

	params.prevangles[0] = params.final_angles[0];
	params.blending[0] = iBlend;
	params.prevblending[0] = params.blending[0];
	params.prevseqblending[0] = params.blending[0];
	HL_StudioEstimateGait(params);

	// calc side to side turning
	flYaw = params.final_angles[1] - params.gaityaw;
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

	params.final_angles[1] = params.gaityaw;
	if (params.final_angles[1] < -0) params.final_angles[1] += 360.0f;
	params.prevangles[1] = params.final_angles[1];

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

	if (dt < 0.0)
		dt = 0;

	else if (dt > 1.0)
		dt = 1;

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

qboolean CL_FindInterpolationUpdates(int host, int target, int sequence, float targettime, player_anim_params_s** ph0, player_anim_params_s** ph1)
{
	qboolean	extrapolate = true;
	int	i, i0, i1, imod;
	float	at;

	i0 = (sequence) & SV_UPDATE_MASK;
	i1 = (sequence - 1) & SV_UPDATE_MASK;
	imod = i0;

	for (i = 1; i < SV_UPDATE_BACKUP - 1; i++)
	{
		at = player_params_history[host].hist[(imod - i) & SV_UPDATE_MASK][target].animtime;

		if (at == 0.0)
			break;

		if (at < targettime)
		{
			i0 = ((imod - i) + 1) & SV_UPDATE_MASK;
			i1 = (imod - i) & SV_UPDATE_MASK;
			extrapolate = false;
			break;
		}
	}

	if (ph0 != NULL) *ph0 = &player_params_history[host].hist[i0][target];
	if (ph1 != NULL) *ph1 = &player_params_history[host].hist[i1][target];
	
	return extrapolate;
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
float CL_PureOrigin(int host, int target, float t, vec3_t& outorigin, vec3_t& outangles)
{
	player_anim_params_s* ph0, * ph1;
	float				t1, t0;
	float				frac = 0.f;
	vec3_t				delta;
	vec3_t				pos, angles;

	auto _host_client = api->GetClient(host);
	CL_FindInterpolationUpdates(host, target, (_host_client->netchan.outgoing_sequence + 1), t, & ph0, & ph1);

	if (!ph0 || !ph1)
		return 0.0f;

	t0 = ph0->animtime;
	t1 = ph1->animtime;


	if (t0 != 0)
	{
		VectorSubtract(ph0->origin, ph1->origin, delta);

#define bound( min, num, max )	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
		if (t0 != t1)
			frac = bound(0, (t - t1) / (t0 - t1), 1.2);
		else
			frac = 1.0f;
		//frac = (t - t1) / (t0 - t1);
		VectorMA(ph1->origin, frac, delta, pos);
		InterpolateAngles(ph0->angles, ph1->angles, angles, frac);

		VectorCopy(pos, outorigin);
		VectorCopy(angles, outangles);
	}
	else
	{
		VectorCopy(ph1->origin, outorigin);
		VectorCopy(ph1->angles, outangles);
	}
	return frac;
}


float BitAngle(float fAngle, int numbits)
{
	if (numbits >= 32) {
		return fAngle;
	}

	unsigned int shift = (1 << numbits);
	unsigned int mask = shift - 1;

	int d = (int)(shift * fmod((double)fAngle, 360.0)) / 360;
	d &= mask;

	float angle =  (float)(d * (360.0 / (1 << numbits)));


	if (angle > 180.0)
	{
		angle -= 360.0;
	}
	else if (angle < -180.0)
	{
		angle += 360.0;
	}

	return angle;
}

float BitTime8(float f2)
{
	int32 twVal = (int)(f2 * 100.0);
	return (float)(twVal / 100.0);
}

void ProcessAnimParams(int id, int host_id, player_anim_params_s& params, player_anim_params_s& prev_params, entity_state_s* state, edict_t* ent)
{
	auto _host_client = api->GetClient(host_id);
	int i;
	if (state)
	{
		params = prev_params;
		params.playerId = state->number;
		params.sequence = state->sequence;
		params.gaitsequence = state->gaitsequence;
		params.frame = uint32_t(state->frame);
		params.angles.x = BitAngle(state->angles.x, 16);
		params.angles.y = BitAngle(state->angles.y, 16);
		params.angles.z = BitAngle(state->angles.z, 16);

		params.origin.x = int(state->origin.x * 32) / 32.f;
		params.origin.y = int(state->origin.y * 32) / 32.f;
		params.origin.z = int(state->origin.z * 32) / 32.f;


		params.animtime = BitTime8(state->animtime);
		params.framerate = state->framerate;
		params.controller[0] = state->controller[0];
		params.controller[1] = state->controller[1];
		params.controller[2] = state->controller[2];
		params.controller[3] = state->controller[3];
		params.blending[0] = state->blending[0];
		params.blending[1] = state->blending[1];
		params.m_clTime = gpGlobals->time;

		params.m_clOldTime = prev_params.m_clTime;

		float t = gpGlobals->time;
		t -= (_host_client->lastcmd.lerp_msec) * 0.001f;		
		CL_PureOrigin(host_id, id, t , params.final_origin, params.final_angles);
		
		params.m_prevgaitorigin = prev_params.final_origin;

		params.prevangles = prev_params.final_angles;
		params.prevframe = prev_params.f;
		params.prevsequence = prev_params.sequence;

		if (params.sequence < 0)
			params.sequence = 0;

		// sequence has changed, hold the previous sequence info
		if (params.sequence != params.prevsequence)
		{
			params.sequencetime = params.animtime + 0.01f;

			// save current blends to right lerping from last sequence
			for (int i = 0; i < 2; i++)
				params.prevseqblending[i] = prev_params.blending[i];
			params.prevsequence = prev_params.sequence; // save old sequence	
		}


		// copy controllers
		for (i = 0; i < 4; i++)
		{
			if (params.controller[i] != prev_params.controller[i])
				params.prevcontroller[i] = prev_params.controller[i];
		}

		// copy blends
		for (i = 0; i < 2; i++)
			params.prevblending[i] = prev_params.blending[i];


	}

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
	int i;
	auto id = ENTINDEX(pEntity) - 1;
	auto _host_client = api->GetClient(id);

	sv_adjusted_positions_t* pos;
	client_t* cli;

	if (_host_client->fakeclient)
		RETURN_META(MRES_IGNORED);

	if (nofind)
	{
		nofind = 0;
		RETURN_META(MRES_IGNORED);
	}

	if (!MDLL_AllowLagCompensation())
		RETURN_META(MRES_IGNORED);

	if (api->GetMaxClients() <= 1 || sv_unlag->value == 0.0)
		RETURN_META(MRES_IGNORED);

	if (!_host_client->lw || !_host_client->lc || !_host_client->active)
		RETURN_META(MRES_IGNORED);

	for (int i = 0; i < api->GetMaxClients(); i++)
	{
		cli = api->GetClient(i);
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
	auto _host_client = api->GetClient(host_id);
	if (!player || ent == host)
	{
		RETURN_META_VALUE(MRES_IGNORED, 0);
	}
	auto id = ENTINDEX(ent) - 1;
	ProcessAnimParams(id, host_id, 
		player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence)][id],
		player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence - 1)][id],
		state, host);

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void (UpdateClientDataPost)(const struct edict_s* ent, int sendweapons, struct clientdata_s* cd)
{

	auto host_id = ENTINDEX(ent) - 1;
	auto _host_client = api->GetClient(host_id);
	for (int i = 0; i < api->GetMaxClients(); i++)
	{

		//player_params_history[host_id].hist[SV_UPDATE_MASK & (_host_client->netchan.outgoing_sequence + 1)][i] = player_params_history[host_id].hist[SV_UPDATE_MASK & _host_client->netchan.outgoing_sequence][i];
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
	api = std::make_unique<rehlds_api>();
	if (!api->Init())
	{		
		api = std::make_unique<hlds_api>();
		if (!api->Init())
		{
			UTIL_ServerPrint("Hitbox fixer is not compatible with your HLDS/ReHLDS version. Create issue on github [https://github.com/Garey27/hitbox_fixer].");
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
