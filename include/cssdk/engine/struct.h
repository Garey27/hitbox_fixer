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
	int sequence;
	int gaitsequence;
	int m_nPlayerGaitSequences;
	
	float frame;
	float prevframe;
	float gaitframe;
	float gaityaw;
	Vector origin;
	Vector m_prevgaitorigin;
	Vector prevangles;
	Vector angles;
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


#endif // _STRUCT_H_

