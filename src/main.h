#pragma once


bool OnMetaAttach();
void OnMetaDetach();
void pfnUpdateClientData(const struct edict_s *ent, int sendweapons, struct clientdata_s *cd);
void pfnPlaybackEvent(int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
void C_ServerActivate(edict_t *pEdictList, int edictCount, int clientMax);
void pfnCmdStart(const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed);
extern int VectorCompare(const vec_t* v1, const vec_t* v2);

#define VectorSubtract(a,b,c) {(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}
#define VectorAdd(a,b,c) {(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];}
#define VectorCopy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}
#define VectorClear(a) {(a)[0]=0.0;(a)[1]=0.0;(a)[2]=0.0;}