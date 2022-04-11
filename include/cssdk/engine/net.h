#ifndef _NET_H_
#define _NET_H_
#ifdef _WIN32
#pragma once
#endif

#define MAX_FLOWS 2
#define MAX_STREAMS 2
#define MAX_LATENT 32

#define CS_SIZE 260
#define FRAGMENT_SIZE 1400
#define NET_MAX_PAYLOAD 3990

#include "netadr.h"
#include "common_rehlds.h"

typedef struct
{
	int size;
	double time;
} flowstats_t;

typedef struct
{
	flowstats_t stats[MAX_LATENT];
	int current;
	double nextcompute;
	float kbytespersec;
	float avgkbytespersec;
} flow_t;

typedef struct fragbuf_s
{
	fragbuf_s *next;
	int bufferid;
	sizebuf_t frag_message;
	byte frag_message_buf[FRAGMENT_SIZE];
	qboolean isfile;
	qboolean isbuffer;
	qboolean iscompressed;
	char filename[CS_SIZE];
	int foffset;
	int size;
} fragbuf_t;

typedef struct fragbufwaiting_s
{
	struct fragbufwaiting_s *next;
	int fragbufcount;
	fragbuf_t *fragbufs;
} fragbufwaiting_t;

// Used as array indexer
typedef enum netsrc_s
{
	NS_CLIENT = 0,
	NS_SERVER,
	NS_MULTICAST,	// xxxMO
	NS_MAX
} netsrc_t;

struct netchan_t
{
	netsrc_t sock;
	netadr_t remote_address;
	unsigned int player_slot;
	float last_received;
	float connected_time;
	double rate;
	double cleartime;

	int incoming_sequence;
	int incoming_acknowledged;
	int incoming_reliable_acknowledged;
	int incoming_reliable_sequence;

	int outgoing_sequence;
	int reliable_sequence;
	int last_reliable_sequence;

	void *connection_status;
	int (*pfnNetchan_Blocksize)(void *);

	sizebuf_t message;
	byte message_buf[NET_MAX_PAYLOAD];

	int reliable_length;
	byte reliable_buf[NET_MAX_PAYLOAD];

	fragbufwaiting_t *waitlist[MAX_STREAMS];

	int reliable_fragment[MAX_STREAMS];
	unsigned int reliable_fragid[MAX_STREAMS];

	fragbuf_t *fragbufs[MAX_STREAMS];
	int fragbufcount[MAX_STREAMS];

	short int frag_startpos[MAX_STREAMS];
	short int frag_length[MAX_STREAMS];

	fragbuf_t *incomingbufs[MAX_STREAMS];
	qboolean incomingready[MAX_STREAMS];

	char incomingfilename[CS_SIZE];
	void *temp_buffer;
	int tempbuffersize;

	flow_t flow[MAX_FLOWS];
};

#endif // _NET_H_
