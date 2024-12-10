#include <Windows.h>


#include "auto_offset/auto_offset.hpp"
#include "utils/memory.hpp"
#include "utils/win32exception.hpp"

#include "clientdll/clientdll.hpp"
#include "engine/engine.hpp"
#include <rehlds/common/triangleapi.h>
#include <CDynPatcher.h>
#include <utils/patch.hpp>
#include <gl/GL.h>
#include <array>
#include <CStudioModelRenderer.h>
#include <rehlds/engine/mathlib_e.h>
#include <utlbuffer.h>
#include <shared.h>
#include <lz4.h>


#include "dep/VGUI/VGUI_Frame.h"
#include "dep/VGUI/VGUI_Panel.h"
#include "dep/VGUI2/IPanel.h"
#include "dep/VGUI2/ISurface.h"
#include "dep/VGUI2/IEngineVGui.h"

std::unordered_map<std::string, std::unique_ptr<CDynPatcher>> patchers;
std::vector<std::unique_ptr<IPatch>> patches;



client_static_t* cls;

vgui::ISurface* g_pISurface = nullptr;

typedef void (*pfnEngineMessage)();
typedef void (*pfnSVC_Parse)(void);
typedef struct svc_func_s {
	unsigned char opcode;
	char* pszname;
	pfnSVC_Parse pfnParse;
} svc_func_t;
svc_func_t* cl_parsefuncs;

int(*VGUI2_GetConsoleFont)();
enum FontRenderFlag_t
{
	FONT_LEFT = 0,
	FONT_RIGHT = 1,
	FONT_CENTER = 2
};


static int (*MSG_ReadByte_)();
static int (*MSG_ReadLong_)();
static int (*MSG_ReadBuf_)(int iSize, void* pbuf);

int* MSG_ReadCount = nullptr;
CGameStudioModelRenderer* g_StudioRenderer = nullptr;

inline float DotProduct(const float* a, const float* b) { return (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]); };

void VectorTransform(const float* in1, float in2[3][4], float* out)
{
	out[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out[2] = DotProduct(in1, in2[2]) + in2[2][3];
}
struct extra_player_info
{
	player_anim_params_s client_anim_params;
	player_anim_params_s server_anim_params;
	size_t num_bones;
	float(bone_transform_client)[64][128][3][4];
	float(bone_transform)[128][3][4];
} extra_info[32];

void DrawString(int x, int y, float r, float g, float b, float a, DWORD alignment, const char* msg, ...)
{

	va_list va_alist;
	char buf[1024];
	va_start(va_alist, msg);
	_vsnprintf(buf, sizeof(buf), msg, va_alist);
	va_end(va_alist);
	wchar_t wbuf[1024];
	MultiByteToWideChar(CP_UTF8, 0, buf, 256, wbuf, 256);

	auto font = VGUI2_GetConsoleFont();
	int width, height;
	g_pISurface->GetTextSize(font, wbuf, width, height);

	if (alignment & FONT_RIGHT)
		x -= width;
	if (alignment & FONT_CENTER)
		x -= width / 2;

	g_pISurface->DrawSetTextFont(font);
	g_pISurface->DrawSetTextColor(r * 255, g * 255, b * 255, a * 255);
	g_pISurface->DrawSetTextPos(x, y - height / 2);
	g_pISurface->DrawPrintText(wbuf, wcslen(wbuf));
	g_pISurface->DrawFlushText();
}

struct CStudioModelRendererHook :public CGameStudioModelRenderer
{
	inline static int(CGameStudioModelRenderer::* StudioDrawPlayer_Orig)(int flags, entity_state_t* pplayer) = nullptr;
	inline static void(CGameStudioModelRenderer::* StudioRenderFinal_Orig)() = nullptr;
	inline static void(CGameStudioModelRenderer::* StudioEstimateGai_Orig)(entity_state_t* pplayer) = nullptr;
	inline static void(CGameStudioModelRenderer::* StudioYawBlend_Orig)(entity_state_t* pplayer) = nullptr;
	inline static model_t* m_pWhiteSprite = nullptr;
	inline static float m_hullcolor[8][3] =
	{
		{  1.0,  1.0,  1.0 }, // Shield  : White
		{  1.0,  0.5,  0.5 }, // Head    : Light red
		{  0.5,  1.0,  0.5 }, // Chest   : Light green
		{  1.0,  1.0,  0.5 }, // Stomach : Light yellow
		{  0.5,  0.5,  1.0 }, // Leftarm : Light blue
		{  1.0,  0.5,  1.0 }, // Rightarm: Pink
		{  0.5,  1.0,  1.0 }, // Leftleg : Aqua
		{  1.0,  1.0,  1.0 }, // Rightleg: White
	};
	inline static int m_boxpnt[6][4] =
	{
		{ 0, 4, 6, 2 },
		{ 0, 1, 5, 4 },
		{ 0, 2, 3, 1 },
		{ 7, 5, 1, 3 },
		{ 7, 3, 2, 6 },
		{ 7, 6, 4, 5 },
	};

	void StudioDrawHulls(float(*transform)[128][3][4], bool is_server = false)
	{
		if (!m_pWhiteSprite)
		{
			m_pWhiteSprite = g_pStudio->Mod_ForName("sprites/white.spr", TRUE);
		}
		int i, j;
		float lv;
		vec3_t tmp;
		vec3_t p[8];
		vec3_t pos;
		mstudiobbox_t* pbbox;
		if(!is_server) 
			pbbox = (mstudiobbox_t*)((char*)m_pStudioHeader + m_pStudioHeader->hitboxindex);
		else
		{
			auto model = g_pStudio->GetModelByIndex(m_pCurrentEntity->curstate.modelindex);

			if (model)
			{
				studiohdr_t* extra = (studiohdr_t*)g_pStudio->Mod_Extradata(model);
				if (extra)
				{
					pbbox = (mstudiobbox_t*)((char*)extra + extra->hitboxindex);
				}
			}
		}
		oEngine.pTriAPI->SpriteTexture(m_pWhiteSprite, 0);
		oEngine.pTriAPI->RenderMode(kRenderTransAlpha);
		for (i = 0; i < m_pStudioHeader->numhitboxes; i++)
		{
			// skip shield
			if (i == 20)
				continue;

			for (j = 0; j < 8; j++)
			{
				tmp[0] = (j & 1) ? pbbox[i].bbmin[0] : pbbox[i].bbmax[0];
				tmp[1] = (j & 2) ? pbbox[i].bbmin[1] : pbbox[i].bbmax[1];
				tmp[2] = (j & 4) ? pbbox[i].bbmin[2] : pbbox[i].bbmax[2];
				VectorTransform(tmp, (*transform)[pbbox[i].bone], p[j]);
			}

			j = (pbbox[i].group % 8);

			oEngine.pTriAPI->Begin(TRI_QUADS);
			if(is_server)
				oEngine.pTriAPI->Color4f(1, 0, 0, 0.5f);
			else
				oEngine.pTriAPI->Color4f(0, 1, 0, 0.5f);


			for (j = 0; j < ARRAYSIZE(m_boxpnt); j++)
			{
				tmp[0] = tmp[1] = tmp[2] = 0;
				tmp[j % 3] = (j < 3) ? 1.0 : -1.0;

				oEngine.pTriAPI->Vertex3fv(p[m_boxpnt[j][0]]);
				oEngine.pTriAPI->Vertex3fv(p[m_boxpnt[j][1]]);
				oEngine.pTriAPI->Vertex3fv(p[m_boxpnt[j][2]]);
				oEngine.pTriAPI->Vertex3fv(p[m_boxpnt[j][3]]);
			}

			oEngine.pTriAPI->End();
		}
	}

	float Length(const vec_t* v)
	{
		return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	}
	void StudioYawBlend_Hook(entity_state_t* pplayer)
	{
		static auto test = 0;
		if (test < 50)
		{
			oEngine.Con_Printf("PRE %f %f %f %f %f\n", m_pCurrentEntity->angles[1], m_pPlayerInfo->gaityaw, m_pCurrentEntity->origin[0], m_pCurrentEntity->origin[1], m_pCurrentEntity->origin[2]);
		}
		(this->*StudioYawBlend_Orig)(pplayer);

		if (m_pCurrentEntity->curstate.sequence > 100)
		{
			test = 0;
		}
		if (++test < 50)
		{
			oEngine.Con_Printf("POS %f %f\n", m_pCurrentEntity->angles[1], m_pPlayerInfo->gaityaw);
		}
	}
	void StudioEstimateGait_Hook(entity_state_t* pplayer)
	{
		StudioEstimateGait_Test(pplayer);

	}
	void StudioEstimateGait_Test(entity_state_t* pplayer)
	{
		long double v2; // fst7
		player_info_t* m_pPlayerInfo; // eax
		cl_entity_t* m_pCurrentEntity; // edx
		long double x; // fst6
		long double v6; // fst7
		long double v7; // fst7
		cl_entity_t* v8; // edx
		bool v9; // al
		bool v10; // zf
		player_info_t* v11; // eax
		long double v12; // fst6
		long double v13; // fst3
		long double v14; // fst5
		long double v15; // fst3
		char v17; // c2
		long double v18; // fst6
		long double v19; // fst5
		long double v20; // fst7
		long double v21; // fst6
		player_info_t* v22; // edx
		long double gaityaw; // fst7
		long double v24; // fst6
		long double y; // fst7
		bool v26; // cc
		player_info_t* v27; // esi
		player_info_t* v28; // eax
		float flYawDiff; // [esp+1Ch] [ebp-50h]
		float v30; // [esp+20h] [ebp-4Ch]
		float v31; // [esp+20h] [ebp-4Ch]
		float v32; // [esp+44h] [ebp-28h]
		float v33; // [esp+44h] [ebp-28h]
		float flYaw; // [esp+4Ch] [ebp-20h]
		float flYawa; // [esp+4Ch] [ebp-20h]
		vec3_t est_velocity; // [esp+50h] [ebp-1Ch] BYREF

		flYaw = this->m_clTime - this->m_clOldTime;
		flYaw = 0.01;

		if (flYaw < 0.0)
			goto LABEL_28;
		if (flYaw <= 1.0)
		{
			if (flYaw == 0.0)
			{
			LABEL_28:
				this->m_flGaitMovement = 0.0;
				return;
			}
		}
		else
		{
			flYaw = 1.0;
		}  
		v2 = flYaw;
		m_pPlayerInfo = this->m_pPlayerInfo;

		if (this->m_fGaitEstimation)
		{
			m_pCurrentEntity = this->m_pCurrentEntity;
			x = m_pCurrentEntity->origin[0];
			est_velocity[0] = x - m_pPlayerInfo->prevgaitorigin[0];
			est_velocity[1] = m_pCurrentEntity->origin[1] - m_pPlayerInfo->prevgaitorigin[1];
			est_velocity[2] = m_pCurrentEntity->origin[2] - m_pPlayerInfo->prevgaitorigin[2];
			m_pPlayerInfo->prevgaitorigin[0] = x;
			this->m_pPlayerInfo->prevgaitorigin[1] = this->m_pCurrentEntity->origin[1];
			this->m_pPlayerInfo->prevgaitorigin[2] = this->m_pCurrentEntity->origin[2];
			v30 = v2;
			v6 = Length(&est_velocity[0]);
			this->m_flGaitMovement = v6;
			if (v30 <= 0.0)
			{
				v7 = v30;
			LABEL_7:
				v8 = this->m_pCurrentEntity;
				this->m_flGaitMovement = 0.0;
				est_velocity[0] = 0.0;
				est_velocity[1] = 0.0;
				v9 = v8->curstate.sequence > 100;
				goto LABEL_8;
			}
			v26 = v6 / v30 >= 5.0;
			v7 = v30;
			if (!v26)
				goto LABEL_7;
		}
		else
		{
			VectorCopy(pplayer->velocity, est_velocity);
			v31 = v2;
			v24 = Length(&est_velocity[0]) * v31;
			v7 = v31;
			this->m_flGaitMovement = v24;
		}
		v8 = this->m_pCurrentEntity;
		v9 = v8->curstate.sequence > 100;
		if (est_velocity[1] == 0.0)
		{
			if (est_velocity[0] == 0.0)
			{
			LABEL_8:
				v10 = !v9;
				v11 = this->m_pPlayerInfo;
				if (v10)
				{
					v12 = v8->angles[1] - v11->gaityaw;
					v32 = v12 / 360.0;
					v13 = v12 - (long double)(360 * (int)v32);
					if (v13 <= 180.0)
						v14 = v13;
					else
						v14 = v13 - 360.0;
					if (v14 < -180.0)
						v14 = v14 + 360.0;
					v15 = v12;
					v15 = fmod(v15, 360.0);
					v18 = v14;
					flYawa = v15;
					v19 = flYawa;
					if (flYawa >= -180.0)
					{
						if (v19 > 180.0)
							v19 = v19 - 360.0;
					}
					else
					{
						v19 = v19 + 360.0;
					}
					if (v19 > -5.0 && v19 < 5.0)
						this->m_pCurrentEntity->baseline.fuser1 = 0.050000001;
					if (v19 < -90.0 || v19 > 90.0)
						this->m_pCurrentEntity->baseline.fuser1 = 3.5;
					if (v7 >= 0.25)
						v20 = v7 * v18;
					else
						v20 = v18 * (v7 * this->m_pCurrentEntity->baseline.fuser1);
					flYawDiff = v20;
					v21 = 0.0;
					if ((long double)(int)abs((int)flYawDiff) >= 0.1)
						v21 = v20;
					this->m_pPlayerInfo->gaityaw = v21 + this->m_pPlayerInfo->gaityaw;
					v22 = this->m_pPlayerInfo;
					gaityaw = v22->gaityaw;
					v33 = gaityaw / 360.0;
					v22->gaityaw = gaityaw - (long double)(360 * (int)v33);
					goto LABEL_28;
				}
				goto LABEL_36;
			}
			y = est_velocity[1];
		}
		else
		{
			y = est_velocity[1];
		}
		if (v8->curstate.sequence > 100)
		{
			v11 = this->m_pPlayerInfo;
		LABEL_36:
			v11->gaityaw = v11->gaityaw + v8->angles[1] - v11->gaityaw;
			return;
		}
		v27 = this->m_pPlayerInfo;
		v27->gaityaw = atan2(y, est_velocity[0]) * 180.0 / 3.141592653589793;
		v28 = this->m_pPlayerInfo;
		if (v28->gaityaw > 180.0)
		{
			v28->gaityaw = 180.0;
			v28 = this->m_pPlayerInfo;
		}
		if (v28->gaityaw < -180.0)
			v28->gaityaw = -180.0;
	}

	void StudioRenderFinal_Hook()
	{

		(this->*StudioRenderFinal_Orig)();
		if (this->m_pPlayerInfo && this->m_pCurrentEntity != oEngine.GetLocalPlayer())
		{

			auto info = g_pStudio->PlayerInfo(m_nPlayerIndex);
      extra_info[m_nPlayerIndex].client_anim_params.gaitframe = info->gaitframe;
      extra_info[m_nPlayerIndex].client_anim_params.gaityaw = info->gaityaw;
			extra_info[m_nPlayerIndex].client_anim_params.m_prevgaitorigin[0] = info->prevgaitorigin[0];
			extra_info[m_nPlayerIndex].client_anim_params.m_prevgaitorigin[1] = info->prevgaitorigin[1];
			extra_info[m_nPlayerIndex].client_anim_params.m_prevgaitorigin[2] = info->prevgaitorigin[2];
			extra_info[m_nPlayerIndex].client_anim_params.m_flGaitMovement = m_flGaitMovement;
      extra_info[m_nPlayerIndex].client_anim_params.m_clTime = m_clTime;
      extra_info[m_nPlayerIndex].client_anim_params.m_clOldTime = m_clOldTime;
			extra_info[m_nPlayerIndex].client_anim_params.angles[0] = this->m_pCurrentEntity->curstate.angles[0];
			extra_info[m_nPlayerIndex].client_anim_params.angles[1] = this->m_pCurrentEntity->curstate.angles[1];
			extra_info[m_nPlayerIndex].client_anim_params.angles[2] = this->m_pCurrentEntity->curstate.angles[2];
			StudioDrawHulls(&(extra_info[m_nPlayerIndex].bone_transform_client[cls->netchan.incoming_acknowledged & 63]), false);
			StudioDrawHulls(&(extra_info[m_nPlayerIndex].bone_transform), true);
			memcpy(&extra_info[m_nPlayerIndex].bone_transform_client[cls->netchan.outgoing_sequence & 63], m_pbonetransform, sizeof(float[128][3][4]));
		}
	}
};


typedef bool(*APPACTIVE)();
bool is_active_app()
{
	return true;
}
void Parse_Timescale()
{
	static char i_buffer[0xffff];
	static char o_buffer[0xffff];
	auto check = MSG_ReadByte_();
	if (check != 0xff)
		return;

	auto size = MSG_ReadLong_();
	MSG_ReadBuf_(size, i_buffer);
  size_t output_size = LZ4_decompress_safe((const char*)i_buffer, o_buffer, size, 0xffff);
  CUtlBuffer buf(o_buffer, output_size);
  size_t version = buf.GetChar();
  do 
  {
		uint8_t player = buf.GetUnsignedChar();
		if (player != 0xff)
		{
			extra_info[player].server_anim_params.unpack_params(buf, &extra_info[player].bone_transform, extra_info[player].num_bones);
		}
	} while (buf.TellGet() < output_size);
}


template<typename T, typename H, typename O>
void hook_vtable(T* someInstance, size_t id, H hook, O original)
{
	auto vtableCell = *reinterpret_cast<void***>(someInstance) + id;
	reinterpret_cast<void*&>(*original) = *vtableCell;
#ifdef _WIN32
	DWORD dwOldProtect;
	VirtualProtect(vtableCell, 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
#else
	auto page = reinterpret_cast<void**>(
		reinterpret_cast<uintptr_t>(vtableCell) & ~0xFFF);
	if (mprotect(page, 4096, PROT_READ | PROT_WRITE | PROT_EXEC)) {
		std::printf("%d: %s\n", errno, strerror(errno));
		exit(errno);
	}
#endif

	* vtableCell = reinterpret_cast<void*&>(hook);

#ifdef _WIN32
	VirtualProtect(vtableCell, 4, dwOldProtect, &dwOldProtect);
#else
	if (mprotect(page, 4096, PROT_READ | PROT_EXEC)) {
		std::printf("%d: %s\n", errno, strerror(errno));
		exit(errno);
	}
#endif
}

svc_func_t* EngineMsgByName(char* szMsgName)
{
	for (int i = 0; i < 61; i++) {
		if (!strcmp(cl_parsefuncs[i].pszname, szMsgName))
			return &cl_parsefuncs[i];
	}

	return nullptr;
}

pfnEngineMessage HookEngineMsg(char* szMsgName, pfnEngineMessage pfn)
{
	pfnEngineMessage Original = nullptr;

	auto Ptr = EngineMsgByName(szMsgName);

	if (Ptr != nullptr) {
		Original = Ptr->pfnParse;
		Ptr->pfnParse = pfn;
	}
	else {
		MessageBoxA(0, szMsgName, 0, MB_OK | MB_ICONERROR);
	}

	return Original;
}

int(Hud_Redraw)(float f, int force)
{
	SCREENINFO g_Screen;
	g_Screen.iSize = sizeof(g_Screen);
	oEngine.pfnGetScreenInfo(&g_Screen);

	auto index = 1;
	DrawString(50, 50, 1, 1, 1, 1, FONT_LEFT, "Client:");
	DrawString(50, 60, 1, 1, 1, 1, FONT_LEFT, "Gaitframe: %f", extra_info[index].client_anim_params.gaitframe);
	DrawString(50, 70, 1, 1, 1, 1, FONT_LEFT, "Gaityaw: %f", extra_info[index].client_anim_params.gaityaw);
	DrawString(50, 80, 1, 1, 1, 1, FONT_LEFT, "Prevgaitorigin: %f %f %f", extra_info[index].client_anim_params.m_prevgaitorigin[0], extra_info[index].client_anim_params.m_prevgaitorigin[1], extra_info[index].client_anim_params.m_prevgaitorigin[2]);
	DrawString(50, 90, 1, 1, 1, 1, FONT_LEFT, "Gaitmovement: %f", extra_info[index].client_anim_params.m_flGaitMovement);
	DrawString(50, 100, 1, 1, 1, 1, FONT_LEFT, "ClTime: %f", extra_info[index].client_anim_params.m_clTime);
	DrawString(50, 110, 1, 1, 1, 1, FONT_LEFT, "ClOldTime: %f", extra_info[index].client_anim_params.m_clOldTime);
	DrawString(50, 120, 1, 1, 1, 1, FONT_LEFT, "Angles: %f %f", extra_info[index].client_anim_params.angles[0], extra_info[index].client_anim_params.angles[1]);

	DrawString(g_Screen.iWidth - 50, 50, 1, 1, 1, 1, FONT_RIGHT, "Server:");
	DrawString(g_Screen.iWidth - 50, 60, 1, 1, 1, 1, FONT_RIGHT, "Gaitframe: %f", extra_info[index].server_anim_params.gaitframe);
	DrawString(g_Screen.iWidth - 50, 70, 1, 1, 1, 1, FONT_RIGHT, "Gaityaw: %f", extra_info[index].server_anim_params.gaityaw);

	DrawString(g_Screen.iWidth - 50, 80, 1, 1, 1, 1, FONT_RIGHT, "Prevgaitorigin: %f %f %f", extra_info[index].server_anim_params.m_prevgaitorigin[0], extra_info[index].server_anim_params.m_prevgaitorigin[1], extra_info[index].server_anim_params.m_prevgaitorigin[2]);
	DrawString(g_Screen.iWidth - 50, 90, 1, 1, 1, 1, FONT_RIGHT, "Gaitmovement: %f", extra_info[index].server_anim_params.m_flGaitMovement);
	DrawString(g_Screen.iWidth - 50, 100, 1, 1, 1, 1, FONT_RIGHT, "ClTime: %f", extra_info[index].server_anim_params.m_clTime);
	DrawString(g_Screen.iWidth - 50, 110, 1, 1, 1, 1, FONT_RIGHT, "ClOldTime: %f", extra_info[index].server_anim_params.m_clOldTime);

	return oClientDll.pHudRedrawFunc(f, force);
}

PVOID CaptureInterface(CreateInterfaceFn Interface, char* InterfaceName)
{
	PVOID dwPointer = (PVOID)(Interface(InterfaceName, 0));

	return dwPointer;
}

CreateInterfaceFn CaptureFactory(char* FactoryModule)
{
	CreateInterfaceFn Interface = 0;

	HMODULE hFactoryModule = GetModuleHandleA(FactoryModule);

	if (hFactoryModule) {
		Interface = (CreateInterfaceFn)(GetProcAddress(
			hFactoryModule, CREATEINTERFACE_PROCNAME));
	}

	return Interface;
}

void(__cdecl* pCL_ComputeClientInterpolationAmount)(usercmd_t* cmd);
void __cdecl CL_ComputeClientInterpolationAmount(usercmd_t* cmd)
{
	auto interp = oEngine.pfnGetCvarPointer("ex_interp");
	cmd->lerp_msec = interp->value * 1000;
}
void init_patchers()
{
	patchers["hw"] = std::make_unique<CDynPatcher>();
	patchers["client"] = std::make_unique<CDynPatcher>();
	patchers["sdl2"] = std::make_unique<CDynPatcher>();
	patchers["hw"]->Init("hw.dll");
	patchers["client"]->Init("client.dll");
	patchers["sdl2"]->Init("sdl2.dll");

	auto& ao = AO::Get(true);

	auto ptrScreenFade = ao.FindPattern("hw.dll", { "ScreenFade" });
	auto refScreenFade = ao.FindReference("hw.dll", { 0x68 }, ptrScreenFade);

	gEngine = ReadPtr<cl_enginefunc_t>(refScreenFade + 0x0D);
	memcpy(&oEngine, gEngine, sizeof(oEngine));

	gClientDll = ReadPtr<cldll_func_t>(refScreenFade + 0x13);
	memcpy(&oClientDll, gClientDll, sizeof(oClientDll));

	g_pStudio = (engine_studio_api_t*)ao.FindPatternIDA(
		"hw.dll",
		"68 *? ? ? ? 68 ? ? ? ? 6A ? FF D0 83 C4 ? 85 C0 75 ? 68 ? ? ? ? "
		"E8 ? ? ? ? 83 C4 ? E8 ? ? ? ? 5D");


	auto cls_signon = ao.FindPatternIDA("hw.dll", "A1 *? ? ? ? 83 C4 08 48");

	cls = (client_static_t*)(cls_signon - offsetof(client_static_t, signon));

	g_StudioRenderer = decltype(g_StudioRenderer)(ao.FindPatternIDA(
		"client.dll",
		"B9 *? ? ? ? E9 ? ? ? ? 90 90 90 90 90 90 83 7C 24 04 ?"));

	if (!g_StudioRenderer)
	{
		g_StudioRenderer = decltype(g_StudioRenderer)(ao.FindPatternIDA(
			"client.dll",
			"C7 06 *?? ?? ?? ?? 8B C6 5E C3"));
	}

	cl_parsefuncs = decltype(cl_parsefuncs)(
		(ao.FindPatternIDA("hw.dll", "BF *? ? ? ? 8B 04 B5 ? ? ? ?")) - 0x4);

	MSG_ReadByte_ = decltype(MSG_ReadByte_)(
		ao.FindPatternIDA("hw.dll", "[E8 *? ? ? ?] 88 45 F1"));
	MSG_ReadLong_ = decltype(MSG_ReadLong_)(
		ao.FindPatternIDA("hw.dll", "[E8 *? ? ? ?] F6 C3 40"));
	MSG_ReadBuf_ = decltype(MSG_ReadBuf_)(
		ao.FindPatternIDA("hw.dll", "55 8B EC A1 ?? ?? ?? ?? 8B 4D 08 8B"));

	HookEngineMsg("svc_timescale", Parse_Timescale);

	CreateInterfaceFn HW_Factory = CaptureFactory("hw.dll");
	g_pISurface = (vgui::ISurface*)(CaptureInterface(HW_Factory, VGUI_SURFACE_INTERFACE_VERSION));

	VGUI2_GetConsoleFont = decltype(VGUI2_GetConsoleFont)(ao.FindPatternIDA("hw.dll", "[E8 *?? ?? ?? ??] 50 8D 45 98"));

	patches.push_back(std::make_unique<MPatch<HUD_REDRAW_FUNC>>(
		gClientDll->pHudRedrawFunc, &Hud_Redraw, 4));
#if 1
	auto game = ao.FindPatternIDA("hw.dll", "8B 0D *?? ?? ?? ?? 8B 11 FF 52 ?? 84 C0 75 1C");
	auto appactive = (*reinterpret_cast<void***>(*(DWORD*)game) + 10);
	patches.push_back(std::make_unique<MPatch<APPACTIVE>>(
		(APPACTIVE*)appactive, &is_active_app, 4));
#endif

	oEngine.pfnRegisterVariable("hbf_debug_vis", "1.0", 0);
	size_t renderfinal_index;
	if (g_pStudio->IsHardware())
		renderfinal_index = 21;
	else
		renderfinal_index = 20;

	 hook_vtable(g_StudioRenderer,
		 renderfinal_index,
		&CStudioModelRendererHook::StudioRenderFinal_Hook,
		 &CStudioModelRendererHook::StudioRenderFinal_Orig);

#if 0
	 hook_vtable(g_StudioRenderer,
		 23,
		 &CStudioModelRendererHook::StudioEstimateGait_Hook,
		 &CStudioModelRendererHook::StudioEstimateGai_Orig);
	 hook_vtable(g_StudioRenderer,
		 26,
		 &CStudioModelRendererHook::StudioYawBlend_Hook,
		 &CStudioModelRendererHook::StudioYawBlend_Orig);


	 pCL_ComputeClientInterpolationAmount = decltype(pCL_ComputeClientInterpolationAmount)(ao.FindPatternIDA("hw.dll", "55 8B EC 51 D9 05 ?? ?? ?? ?? D8 1D ?? ?? ?? ?? 53"));

	 patchers["hw"]->HookFunctionCall(pCL_ComputeClientInterpolationAmount,
		 CL_ComputeClientInterpolationAmount);

#endif

}

static DWORD WINAPI Init(LPVOID dll)
try {
	init_patchers();

	return 0;
}
catch (const Win32Exception& e) {
	MessageBoxA(HWND_DESKTOP, e.what(), "Fatal Error", MB_ICONERROR);
	FreeLibraryAndExitThread(static_cast<HMODULE>(dll), e.code().value());
	return 0;
}
catch (const std::exception& e) {
	MessageBoxA(HWND_DESKTOP, e.what(), "Fatal Error", MB_ICONERROR);
	FreeLibraryAndExitThread(static_cast<HMODULE>(dll), -1);
	return 0;
}

BOOL Attach(HINSTANCE dll)
try {
	auto thread = CreateThread(NULL, 0, Init, dll, 0, NULL);
	if (!thread) {
		ThrowLastError();
	}
	CloseHandle(thread);

	return TRUE;
}
catch (const Win32Exception& e) {
	MessageBoxA(HWND_DESKTOP, e.what(), "Fatal Error", MB_ICONERROR);
	return FALSE;
}
catch (const std::exception& e) {
	MessageBoxA(HWND_DESKTOP, e.what(), "Fatal Error", MB_ICONERROR);
	return FALSE;
}

BOOL Detach(HINSTANCE dll)
{
	return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		return Attach(hinstDLL);
	case DLL_PROCESS_DETACH:
		return Detach(hinstDLL);
	default:
		return FALSE;
	}
}


