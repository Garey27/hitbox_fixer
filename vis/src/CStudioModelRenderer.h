#pragma once
#include <rehlds/engine/client.h>
#include <rehlds/public/rehlds/model.h>
#include <rehlds/public/rehlds/studio.h>

class CStudioModelRenderer
{
public:
	CStudioModelRenderer(void);
public:
	virtual ~CStudioModelRenderer();
	virtual void Init(void);

public:
	virtual int StudioDrawModel(int flags);
	virtual int StudioDrawPlayer(int flags, struct entity_state_s* pplayer);

public:
	virtual mstudioanim_t* StudioGetAnim(model_t* m_pSubModel, mstudioseqdesc_t* pseqdesc);
	virtual void StudioSetUpTransform(int trivial_accept);
	virtual void StudioSetupBones(void);
	virtual void StudioCalcAttachments(void);
	virtual void StudioSaveBones(void);
	virtual void StudioMergeBones(model_t* m_pSubModel);
	virtual float StudioEstimateInterpolant(void);
	virtual float StudioEstimateFrame(mstudioseqdesc_t* pseqdesc);
	virtual void StudioFxTransform(cl_entity_t* ent, float transform[3][4]);
	virtual void StudioSlerpBones(vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s);
	virtual void StudioCalcBoneAdj(float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen);
	virtual void StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q);
	virtual void StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos);
	virtual void StudioCalcRotations(float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f);
	virtual void StudioRenderModel(void);
	virtual void StudioRenderFinal(void);
	virtual void StudioRenderFinal_Software(void);
	virtual void StudioRenderFinal_Hardware(void);
	virtual void StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch);
	virtual void StudioEstimateGait(entity_state_t* pplayer);
	virtual void StudioProcessGait(entity_state_t* pplayer);

public:
	void StudioSetShadowSprite(int iSprite);
	void StudioDrawShadow(float* origin, float scale);

public:
	static int m_iShadowSprite;
	static int m_boxpnt[6][4];
	static vec3_t m_hullcolor[8];

	double m_clTime;
	double m_clOldTime;

	int m_fDoInterp;
	int m_fGaitEstimation;

	int m_nFrameCount;

	cvar_t* m_pCvarHiModels;
	cvar_t* m_pCvarDeveloper;
	cvar_t* m_pCvarDrawEntities;

	cl_entity_t* m_pCurrentEntity;
	model_t* m_pRenderModel;
	player_info_t* m_pPlayerInfo;

	int m_nPlayerIndex;
	float m_flGaitMovement;

	studiohdr_t* m_pStudioHeader;
	mstudiobodyparts_t* m_pBodyPart;
	mstudiomodel_t* m_pSubModel;

	int m_nTopColor;
	int m_nBottomColor;

	model_t* m_pChromeSprite;

	int m_nCachedBones;
	char m_nCachedBoneNames[MAXSTUDIOBONES][32];
	float m_rgCachedBoneTransform[MAXSTUDIOBONES][3][4];
	float m_rgCachedLightTransform[MAXSTUDIOBONES][3][4];

	float m_fSoftwareXScale, m_fSoftwareYScale;

	float m_vUp[3];
	float m_vRight[3];
	float m_vNormal[3];

	float m_vRenderOrigin[3];

	int* m_pStudioModelCount;
	int* m_pModelsDrawn;

	float(*m_protationmatrix)[3][4];
	float(*m_paliastransform)[3][4];

	float(*m_pbonetransform)[MAXSTUDIOBONES][3][4];
	float(*m_plighttransform)[MAXSTUDIOBONES][3][4];

public:
	static int s_iShadowSprite;
};

enum BoneIndex
{
	BONE_HEAD,
	BONE_PELVIS,
	BONE_SPINE1,
	BONE_SPINE2,
	BONE_SPINE3,
	BONE_MAX,
};

class CGameStudioModelRenderer : public CStudioModelRenderer
{
public:
	CGameStudioModelRenderer(void);

public:
	virtual void StudioSetupBones(void);
	virtual void StudioEstimateGait(entity_state_t* pplayer);
	virtual void StudioProcessGait(entity_state_t* pplayer);
	virtual int StudioDrawPlayer(int flags, entity_state_t* pplayer);
	virtual int _StudioDrawPlayer(int flags, entity_state_t* pplayer);
	virtual void StudioFxTransform(cl_entity_t* ent, float transform[3][4]);
	virtual void StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch);
	virtual void CalculateYawBlend(entity_state_t* pplayer);
	virtual void CalculatePitchBlend(entity_state_t* pplayer);

private:
	void SavePlayerState(entity_state_t* pplayer);
	void SetupClientAnimation(entity_state_t* pplayer);
	void RestorePlayerState(entity_state_t* pplayer);
	mstudioanim_t* LookupAnimation(mstudioseqdesc_t* pseqdesc, int index);
	void CachePlayerBoneIndices(void);
	int GetPlayerBoneIndex(BoneIndex whichBone);
	bool GetPlayerBoneWorldPosition(BoneIndex whichBone, vec3_t* pos);

private:
	int m_nPlayerGaitSequences[MAX_CLIENTS];

private:
	bool m_bLocal;
	int m_boneIndexCache[BONE_MAX];
	bool m_isBoneCacheValid;
};
