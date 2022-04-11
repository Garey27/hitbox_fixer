#ifndef _DELTA_PACKET_H_
#define _DELTA_PACKET_H_
#ifdef _WIN32
#pragma once
#endif

typedef struct packet_entities_s
{
	int num_entities;
	unsigned char flags[32];
	entity_state_t *entities;
} packet_entities_t;

#endif //_DELTA_PACKET_H_
