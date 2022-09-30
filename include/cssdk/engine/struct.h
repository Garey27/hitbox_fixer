#ifndef _STRUCT_H_
#define _STRUCT_H_

#include "net.h"
#include "progs.h"
#include "delta_packet.h"
#include "server_static.h"
#include "userid_rehlds.h"

#define MAX_INFO_STRING 256

typedef struct client_frame_s
{
	double senttime;
	float ping_time;
	clientdata_t clientdata;
	weapon_data_t weapondata[64];
	packet_entities_t entities;
} client_frame_t;

typedef struct usercmd_s
{
	short	lerp_msec;      // Interpolation time on client
	byte	msec;           // Duration in ms of command
	vec3_t	viewangles;     // Command view angles.

// intended velocities
	float	forwardmove;    // Forward velocity.
	float	sidemove;       // Sideways velocity.
	float	upmove;         // Upward velocity.
	byte	lightlevel;     // Light level at spot where we are standing.
	unsigned short  buttons;  // Attack buttons
	byte    impulse;          // Impulse command issued.
	byte	weaponselect;	// Current weapon id

// Experimental player impact stuff.
	int		impact_index;
	vec3_t	impact_position;
} usercmd_t;

struct client_t
{
	qboolean active;
	qboolean spawned;
	qboolean fully_connected;
	qboolean connected;
	qboolean uploading;
	qboolean hasusrmsgs;
	qboolean has_force_unmodified;
	netchan_t netchan;
	int chokecount;
	int delta_sequence;
	qboolean fakeclient;
	qboolean proxy;
	usercmd_t lastcmd;
	double connecttime;
	double cmdtime;
	double ignorecmdtime;
	float latency;
	float packet_loss;
	double localtime;
	double nextping;
	double svtimebase;
	sizebuf_t datagram;
	byte datagram_buf[4000];
	double connection_started;
	double next_messagetime;
	double next_messageinterval;
	qboolean send_message;
	qboolean skip_message;
	client_frame_t *frames;
	event_state_t events;
	edict_t *edict;
	const edict_t *pViewEntity;
	int userid;
	USERID_t network_userid;
	char userinfo[MAX_INFO_STRING];
	qboolean sendinfo;
	float sendinfo_time;
	char hashedcdkey[64];
	char name[32];
	int topcolor;
	int bottomcolor;
	int entityId;
	resource_t resourcesonhand;
	resource_t resourcesneeded;
	FILE *upload;
	qboolean uploaddoneregistering;
	customization_t customdata;
	int crcValue;
	int lw;
	int lc;
	char physinfo[256];
	qboolean m_bLoopback;
	uint32_t m_VoiceStreams[2];
	double m_lastvoicetime;
	int m_sendrescount;
};

struct player_anim_params_s
{
	int playerId;
	int sequence;
	int gaitsequence;
	int m_nPlayerGaitSequences;
	
	double f;
	float frame;
	float prevframe;
	float gaitframe;
	float gaityaw;
	Vector origin;
	Vector angles;
	Vector final_origin;
	Vector final_angles;
	Vector m_prevgaitorigin;
	Vector prevangles;
	int prevsequence;
	float sequencetime;
	float animtime;
	double m_clTime;
	double m_clOldTime;
	double framerate;
	unsigned char prevblending[2];
	unsigned char prevseqblending[2];
	unsigned char prevcontroller[4];
	unsigned char controller[4];
	unsigned char blending[2];
	float m_flGaitMovement;
	float m_flYawModifier;
};
typedef struct sv_adjusted_positions_s
{
	int active;
	int needrelink;
	vec3_t neworg;
	vec3_t oldorg;
	vec3_t initial_correction_org;
	vec3_t oldabsmin;
	vec3_t oldabsmax;
	int deadflag;
	vec3_t temp_org;
	int temp_org_setflag;
	player_anim_params_s extra;
} sv_adjusted_positions_t;


constexpr int MAX_CONSISTENCY_LIST = 512;

typedef struct consistency_s
{
	char* filename;
	int issound;
	int orig_index;
	int value;
	int check_type;
	float mins[3];
	float maxs[3];
} consistency_t;

typedef struct event_s
{
	unsigned short index;
	const char* filename;
	int filesize;
	const char* pszScript;
} event_t;


constexpr int NUM_BASELINES = 64;

typedef struct extra_baselines_s
{
	int number;
	int classname[NUM_BASELINES];
	entity_state_t baseline[NUM_BASELINES];
} extra_baselines_t;

constexpr int MAX_DATAGRAM = 4000;

typedef struct server_s
{
	qboolean active;
	qboolean paused;
	qboolean loadgame;
	double time;
	double oldtime;
	int lastcheck;
	double lastchecktime;
	char name[64];
	char oldname[64];
	char startspot[64];
	char modelname[64];
	struct model_s* worldmodel;
	CRC32_t worldmapCRC;
	unsigned char clientdllmd5[16];
	resource_t resourcelist[MAX_RESOURCE_LIST];
	int num_resources;
	consistency_t consistency_list[MAX_CONSISTENCY_LIST];
	int num_consistency;
	const char* model_precache[MAX_MODELS];
	struct model_s* models[MAX_MODELS];
	unsigned char model_precache_flags[MAX_MODELS];
	struct event_s event_precache[MAX_EVENTS];
	const char* sound_precache[MAX_SOUNDS];
	short int sound_precache_hashedlookup[MAX_SOUNDS_HASHLOOKUP_SIZE];
	qboolean sound_precache_hashedlookup_built;
	const char* generic_precache[MAX_GENERIC];
	char generic_precache_names[MAX_GENERIC][64];
	int num_generic_names;
	const char* lightstyles[MAX_LIGHTSTYLES];
	int num_edicts;
	int max_edicts;
	edict_t* edicts;
	struct entity_state_s* baselines;
	extra_baselines_t* instance_baselines;
	server_state_t state;
	sizebuf_t datagram;
	unsigned char datagram_buf[MAX_DATAGRAM];
	sizebuf_t reliable_datagram;
	unsigned char reliable_datagram_buf[MAX_DATAGRAM];
	sizebuf_t multicast;
	unsigned char multicast_buf[1024];
	sizebuf_t spectator;
	unsigned char spectator_buf[1024];
	sizebuf_t signon;
	unsigned char signon_data[32768];
} server_t;
#endif // _STRUCT_H_

