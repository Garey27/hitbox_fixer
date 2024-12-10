#include "precompiled.h"

#if defined(__linux__) || defined(__APPLE__)
#define __fastcall
#endif
#include <subhook.h>
#include <lz4.h>

cvar_t* sv_unlag;
cvar_t* sv_maxunlag;
cvar_t* sv_unlagpush;
cvar_t hf_hitbox_fix = { "hbf_enabled", "1", FCVAR_SERVER | FCVAR_PROTECTED, 1.0f, NULL };
cvar_t hf_debug = { "hbf_debug", "0", FCVAR_SERVER | FCVAR_PROTECTED, 0.0f, NULL };
cvar_t* phf_debug;
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
void UpdateClientAnimParams(int id, int host_id, player_anim_params_s& params, float frametime);
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


void InterpolateAngles(vec3_t start, vec3_t end, vec3_t& output, float frac);
float BitAngle(float fAngle, int numbits)
{
  if (numbits >= 32) {
    return fAngle;
  }

  unsigned int shift = (1 << numbits);
  unsigned int mask = shift - 1;

  int d = (int)(shift * fmod((double)fAngle, 360.0)) / 360;
  d &= mask;

  float angle = (float)(d * (360.0 / (1 << numbits)));


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

uint32_t MSG_ReadBits(int numbits, uint32_t& bitstream, int& bitPos) {
  uint32_t mask = (1 << numbits) - 1;
  uint32_t result = (bitstream >> bitPos) & mask;
  bitPos += numbits;
  return result;
}

void MSG_WriteBits(int numbits, uint32_t& bitstream, int& bitPos, uint32_t value) {
  uint32_t mask = (1 << numbits) - 1;
  bitstream |= (value & mask) << bitPos;
  bitPos += numbits;
}

float MSG_ReadAngleFromDelta(float baseAngle, int numbits, uint32_t& bitstream, int& bitPos) {
  int deltaBits = MSG_ReadBits(numbits, bitstream, bitPos);
  float deltaAngle = deltaBits * (360.0f / (1 << numbits));

  // Восстановить итоговый угол
  float resultAngle = baseAngle + deltaAngle;

  if (resultAngle >= 360.0f)
    resultAngle -= 360.0f;
  else if (resultAngle < 0.0f)
    resultAngle += 360.0f;

  return resultAngle;
}

void MSG_WriteAngleAsDelta(float baseAngle, float newAngle, int numbits, uint32_t& bitstream, int& bitPos) {
  float deltaAngle = newAngle - baseAngle;

  if (deltaAngle > 180.0f)
    deltaAngle -= 360.0f;
  else if (deltaAngle < -180.0f)
    deltaAngle += 360.0f;

  uint32_t deltaBits = static_cast<uint32_t>((deltaAngle * (1 << numbits)) / 360.0f) & ((1 << numbits) - 1);

  MSG_WriteBits(numbits, bitstream, bitPos, deltaBits);
}

class AnimProcessor
{
  static inline constexpr size_t k_MaxHistory = 64;
  std::array<std::array<std::pair<uint32_t, player_ent_hist_params_s>, k_MaxHistory>, 32> history;
public:
  bool can_debug = false;
  player_anim_params_s processed_params[32];
  float lastTimeReceived{};
  uint32_t last_proccesed_seq = 0;
  void add_history(int id, uint32_t out_seq, entity_state_t* state)
  {
    player_ent_hist_params_s params;
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

    history[id][out_seq % k_MaxHistory] = { out_seq, params };
  }
  bool process_anims(int id, uint32_t last_out, uint32_t recv_seq, double lerp, double frametime, player_anim_params_s& params)
  {
    if (recv_seq == last_proccesed_seq)
    {
      //params.m_clOldTime = params.m_clTime;
      //params.m_clTime += frametime;
    }
    auto processed = false;
    player_ent_hist_params_s* to_lerp = nullptr;
    size_t end = (last_out) % k_MaxHistory;
    for (size_t i = (last_out + 1) % k_MaxHistory; i != end; i = (i + 1) % k_MaxHistory)
    {
      auto& [seq, hist] = history[id][i];

      if (seq >= recv_seq)
      {
        to_lerp = &hist;
        break;
      }
    }
    float target_time = -1;
    if (to_lerp)
    {
      target_time = to_lerp->animtime - lerp;
    }
    player_ent_hist_params_s* from_lerp = nullptr;
    player_ent_hist_params_s* max_lerp = nullptr;

    for (size_t i = (last_out + 1) % k_MaxHistory; i != end; i = (i + 1) % k_MaxHistory)
    {
      auto& [seq, hist] = history[id][i];

      if ((hist.animtime) <= target_time)
      {
        if (!from_lerp || hist.animtime > from_lerp->animtime)
          from_lerp = &hist;
      }

      if (seq < recv_seq)
      {
        continue;
      }
      processed = true;
      params.sequence = hist.sequence ;
      params.gaitsequence = hist.gaitsequence;
      params.frame = uint32_t(hist.frame);

      params.origin.x = hist.origin.x;
      params.origin.y = hist.origin.y;
      params.origin.z = hist.origin.z;

      params.angles.x = hist.angles.x;
      params.angles.y = hist.angles.y;
      params.angles.z = hist.angles.z;

      params.m_clOldTime = params.m_clTime;
      params.m_clTime = hist.animtime + frametime;

      params.animtime = hist.animtime;
      params.framerate = hist.framerate;

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
        if (hist.controller[i] != params.controller[i])
          params.prevcontroller[i] = params.controller[i];
      }

      // copy blends
      for (int i = 0; i < 2; i++)
        params.prevblending[i] = params.blending[i];

      params.controller[0] = hist.controller[0];
      params.controller[1] = hist.controller[1];
      params.controller[2] = hist.controller[2];
      params.controller[3] = hist.controller[3];
      params.blending[0] = hist.blending[0];
      params.blending[1] = hist.blending[1];
      break;
    }
    if (from_lerp != nullptr && to_lerp != nullptr)
    {
      float from_time = from_lerp->animtime;
      float to_time = to_lerp->animtime;
      float frac = 0.0;
      vec3_t delta;
      VectorSubtract(from_lerp->origin, to_lerp->origin, delta);

#define bound( min, num, max )	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
      if (from_time != to_time)
        frac = bound(0.0, (to_time - target_time) / (to_time - from_time), 1.2);


      VectorMA(to_lerp->origin, frac, delta, params.origin);
      InterpolateAngles(to_lerp->angles, from_lerp->angles, params.angles, frac);
    }

    return processed;
  }


};
AnimProcessor PlayerAnimProcessor[32];
subhook_t Server_GetBlendingInterfaceHook{};

subhook_t GetProcAddressHook{};
subhook_t dlsymHook{};
std::unordered_map<uint32_t, AnimProcessor*> cvar_requests;

void CvarValue2_PreHook(const edict_t* pEnt, int requestID, const char* cvarName, const char* cvarValue)
{
  auto found = cvar_requests.find(requestID);
  if (found == cvar_requests.end())
  {
    RETURN_META(MRES_IGNORED);
  }
  bool* can_debug = &found->second->can_debug;

  *can_debug = false;
  cvar_requests.erase(requestID);

  if (!strcmp(cvarValue, "Bad CVAR request"))
  {
    RETURN_META(MRES_IGNORED);
  }

  if (strcmp(cvarValue, "1.0"))
  {
    RETURN_META(MRES_IGNORED);
  }
  *can_debug = true;;

  RETURN_META(MRES_IGNORED);
}


void (PutInServer)(edict_t* pEntity)
{
  if (!pEntity)
  {
    RETURN_META(MRES_IGNORED);
  }
  auto host_id = ENTINDEX(pEntity);

  auto m_RequestId = MAKE_REQUESTID(PLID);
  PlayerAnimProcessor[host_id - 1] = {};
  cvar_requests[m_RequestId] = &PlayerAnimProcessor[host_id - 1];
  g_engfuncs.pfnQueryClientCvarValue2(pEntity, "hbf_debug_vis", m_RequestId);
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
  auto _host_client = api->GetClient(host_id);
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
  auto cmd = _host_client->lastcmd;
  auto dt = _host_client->netchan.last_received - PlayerAnimProcessor[host_id].lastTimeReceived;
  if(!dt)
    dt = cmd.msec * 0.001f;
  float clientLatency = frame->ping_time;

  if (sv_maxunlag->value != 0.0f)
  {
    if (clientLatency >= sv_maxunlag->value)
      clientLatency = sv_maxunlag->value;
  }

  cl_interptime = _host_client->lastcmd.lerp_msec / 1000.0f;

  if (cl_interptime > 0.1)
    cl_interptime = 0.1f;

  if (_host_client->next_messageinterval > cl_interptime)
    cl_interptime = (float)_host_client->next_messageinterval;


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


    PlayerAnimProcessor[host_id].process_anims(client_id, _host_client->netchan.outgoing_sequence, _host_client->netchan.incoming_acknowledged, cl_interptime, dt, PlayerAnimProcessor[host_id].processed_params[client_id]);
    UpdateClientAnimParams(client_id, host_id, PlayerAnimProcessor[host_id].processed_params[client_id], dt);
    player_params[client_id] = PlayerAnimProcessor[host_id].processed_params[client_id];


  }
  PlayerAnimProcessor[host_id].lastTimeReceived = _host_client->netchan.last_received;
  PlayerAnimProcessor[host_id].last_proccesed_seq = _host_client->netchan.incoming_acknowledged;
  RETURN_META(MRES_IGNORED);
}


void StudioEstimateGait(player_anim_params_s& params)
{
  float dt;
  Vector est_velocity;

  dt = params.m_clTime - params.m_clOldTime;

  //dt = 0.01;
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

  double flYaw = params.angles[1] - params.gaityaw;


  if (params.sequence > 100) {
    params.gaityaw += flYaw;
    return;
  }
  if (!est_velocity.x && !est_velocity.y)
  {  
    float flYawDiff = flYaw - (double)(int)(360 * (int64_t)(0.0027777778 * flYaw));

    if (flYawDiff > 180.0)
      flYawDiff -= 360.0;

    if (flYawDiff < -180.0)
      flYawDiff += 360.0;

    flYaw = fmod(float(flYaw), 360.0);

    if (flYaw < -180)
      flYaw += 360;

    else if (flYaw > 180)
      flYaw -= 360;

    if (flYaw > -5.0 && flYaw < 5.0)
      params.m_flYawModifier = 0.050000001;

    if (flYaw < -90.0 || flYaw > 90.0)
      params.m_flYawModifier = 3.5;

    if (dt < 0.25)
      flYawDiff *= params.m_flYawModifier;

     flYawDiff *= dt;

     if ((double)(int)abs((int64_t)flYawDiff) < 0.1)
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
#if 0
  static int test = 0;
  if (params.sequence > 100)
  {
    test = 0;
  }

  if (test < 50)
  {
    UTIL_ServerPrint("PRE %f %f %f %f %f\n", params.angles[1], params.gaityaw, params.origin[0], params.origin[1], params.origin[2]);
  }
#endif
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

#if 0
  if (++test < 50)
  {
    UTIL_ServerPrint("POS %f %f\n", params.angles[1], params.gaityaw);
  }
#endif
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

void InterpolateAngles(vec3_t start, vec3_t end, vec3_t& output, float frac)
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

void UpdateClientAnimParams(int id, int host_id, player_anim_params_s& params, float frametime)
{
  params.playerId = id + 1;

  auto _host_client = api->GetClient(host_id);

  //params.m_prevgaitorigin = params.origin;
  //params.prevangles = params.angles;

  //}
  if (g_eGameType == GT_CStrike || g_eGameType == GT_CZero)
  {
    if (params.gaitsequence)
    {
      vec3_t save = params.angles;
      StudioProcessGait(params);
      params.final_angles = params.angles;
      params.final_angles[0] = -params.final_angles[0];
      params.angles = save;
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
      params.final_angles = params.angles;
    }
    CS_StudioProcessParams(id, params);
  }
  else
  {

    if (params.gaitsequence)
    {
      vec3_t save = params.angles;
      HL_StudioProcessGait(params);
      params.angles[0] = -params.angles[0];
      params.final_angles = params.angles;
      params.angles = save;
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
      params.final_angles = params.angles;
    }
  }

}

void PlayerPostThinkPost(edict_t* pEntity)
{
  nofind = 1;

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
  PlayerAnimProcessor[host_id].add_history(id, _host_client->netchan.outgoing_sequence, state);

  RETURN_META_VALUE(MRES_IGNORED, 0);
}

static bool Init = false;


int (*Server_GetBlendingInterfaceOrig)(int version, struct sv_blending_interface_s** ppinterface, struct engine_studio_api_s* pstudio, float* rotationmatrix, float* bonetransform);
int Server_GetBlendingInterface(int version, struct sv_blending_interface_s** ppinterface, struct engine_studio_api_s* pstudio, float* rotationmatrix, float* bonetransform)
{
  orig_ppinterface = ppinterface;
  if (Server_GetBlendingInterfaceHook)
    subhook_remove(Server_GetBlendingInterfaceHook);
  if (Server_GetBlendingInterfaceOrig)
    Server_GetBlendingInterfaceOrig(version, ppinterface, pstudio, rotationmatrix, bonetransform);
  orig_interface = **ppinterface;
  if (Server_GetBlendingInterfaceHook)
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

void COM_Munge2(unsigned char* data, int len, int seq)
{
  static const unsigned char mungify_table2[] =
  {
    0x05, 0x61, 0x7A, 0xED,
    0x1B, 0xCA, 0x0D, 0x9B,
    0x4A, 0xF1, 0x64, 0xC7,
    0xB5, 0x8E, 0xDF, 0xA0
  };
  int i;
  int mungelen;
  int c;
  int* pc;
  unsigned char* p;
  int j;

  mungelen = len & ~3;
  mungelen /= 4;

  for (i = 0; i < mungelen; i++)
  {
    pc = (int*)&data[i * 4];
    c = *pc;
    c ^= ~seq;
    c = bswap(c);

    p = (unsigned char*)&c;
    for (j = 0; j < 4; j++)
    {
      *p++ ^= (0xa5 | (j << j) | j | mungify_table2[(i + j) & 0x0f]);
    }

    c ^= seq;
    *pc = c;
  }
}
void (SendDebugInfo)(size_t player_index);
void StartFramePost()
{
  if (!phf_debug->value)
    RETURN_META(MRES_IGNORED);

  for (int i = 0; i < api->GetMaxClients(); i++)
  {
    SendDebugInfo(i);
  }

}
void (SendDebugInfo)(size_t player_index)
{
  auto host_plr = api->GetClient(player_index);

  if (!host_plr->fully_connected)
    return;

  if (host_plr->fakeclient)
    return;

  if (gpGlobals->frametime + api->GetRealTime() < host_plr->next_messagetime)
    return;

  if (!PlayerAnimProcessor[player_index].can_debug)
    return;

  bool full_info = phf_debug->value == 2;

  CUtlBuffer buffer;
  // 1st version
  buffer.PutChar(0x1);
  for (int i = 0; i < api->GetMaxClients(); i++)
  {
    if (i == player_index)
      continue;

    auto dst_plr = api->GetClient(i);
    if (!dst_plr || !dst_plr->active)
    {
      continue;

    }
    auto model = api->GetModel(dst_plr->edict->v.modelindex);

    player_params[player_index] = PlayerAnimProcessor[player_index].processed_params[i];
    size_t numbones = 0;
    if (full_info)
    {
      if (!MDLL_AllowLagCompensation() || sv_unlag->value == 0.0f || !host_plr->lw || !host_plr->lc)
      {
        nofind = 1;
      }
      else
      {
        nofind = 0;
      }

      (*orig_ppinterface)->SV_StudioSetupBones(
        model,
        dst_plr->edict->v.frame,
        dst_plr->edict->v.sequence,
        dst_plr->edict->v.angles,
        dst_plr->edict->v.origin,
        dst_plr->edict->v.controller,
        dst_plr->edict->v.blending,
        -1,
        dst_plr->edict);

      nofind = 1;
      auto pstudiohdr = (studiohdr_t*)IEngineStudio.Mod_Extradata(model);
      numbones = pstudiohdr->numbones;
    }
    player_params[player_index].pack_params(i, buffer, g_pBoneTransform, numbones);
  }
  // End of players loop
  buffer.PutUnsignedChar(0xff);

  size_t worst_size = LZ4_compressBound(buffer.TellPut());
  std::vector<char> output(worst_size);
  size_t output_size = LZ4_compress_fast(static_cast<const char*>(buffer.Base()), output.data(), buffer.TellPut(), worst_size, 1);

  buffer.SeekPut(CUtlBuffer::SEEK_HEAD, 0);
  uint32_t w1 = host_plr->netchan.outgoing_sequence;
  uint32_t w2 = host_plr->netchan.incoming_sequence - 1;
  buffer.PutInt(w1);
  buffer.PutInt(w2);
  buffer.PutChar(55); // Time scale used only in HLTV	
  buffer.PutChar(0xff);
  buffer.PutUnsignedInt(output_size);

  buffer.Put(output.data(), output_size);
  uint32_t seq = *(uint32_t*)(buffer.Base());
  COM_Munge2(reinterpret_cast<unsigned char*>(buffer.Base()) + 8, buffer.TellPut() - 8, seq & 0xff);
  //buffer.PutChar(0);
  api->SendPacket(buffer.TellPut(), buffer.Base(), host_plr->netchan);
  host_plr->netchan.outgoing_sequence++;
}
void C_ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
  RETURN_META(MRES_IGNORED);
}

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
  CVAR_REGISTER(&hf_debug);

  phf_hitbox_fix = CVAR_GET_POINTER(hf_hitbox_fix.name);
  phf_debug = CVAR_GET_POINTER(hf_debug.name);

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

  CDynPatcher game_p;

#if defined(__linux__) || defined(__APPLE__)
  game_p.Init(linux_game_library.c_str());
#else 
  game_p.Init(game_library.c_str());
#endif
  if (game_p.FindSymbol("Server_GetBlendingInterface", &Server_GetBlendingInterfaceOrig))
  {
    Server_GetBlendingInterfaceHook = subhook_new(
      (void*)Server_GetBlendingInterfaceOrig, (void*)Server_GetBlendingInterface, (subhook_flags_t)0);
    subhook_install(Server_GetBlendingInterfaceHook);
  }
  else
  {
#if defined(__linux__) || defined(__APPLE__)
    dlsymHook = subhook_new(
      (void*)dlsym, (void*)dlsym_hook, (subhook_flags_t)0);
    subhook_install(dlsymHook);
#else
    GetProcAddressHook = subhook_new(
      (void*)GetProcAddress, (void*)GetProcAddressHooked, (subhook_flags_t)0);
    subhook_install(GetProcAddressHook);
#endif
  }
  Init = true;
  return true;
}


void OnMetaDetach()
{
  subhook_remove(Server_GetBlendingInterfaceHook);
  (*orig_ppinterface)->SV_StudioSetupBones = orig_interface.SV_StudioSetupBones;

}
