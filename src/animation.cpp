#include "precompiled.h"
#define PITCH 0	// up/down
#define YAW   1 // left/right
#define ROLL  2 // fall over

#define NUM_BLENDING                9

#define ANIM_SWIM_1                 8
#define ANIM_SWIM_2                 9
#define ANIM_WALK_SEQUENCE          3
#define ANIM_JUMP_SEQUENCE        6
#define ANIM_FIRST_DEATH_SEQUENCE   101

typedef float real_t;

extern player_anim_params_s player_params[MAX_CLIENTS];
int player = -1;
edict_t* m_pCurrentEntity;
extern qboolean nofind;

int VectorCompare(const vec_t* v1, const vec_t* v2)
{
	for (int i = 0; i < 3; i++)
	{
		if (v1[i] != v2[i])
			return 0;
	}

	return 1;
}


#define SV_BLENDING_INTERFACE_VERSION 1

server_studio_api_t IEngineStudio;
studiohdr_t* g_pstudiohdr;


float(*g_pRotationMatrix)[3][4];
float(*g_pBoneTransform)[128][3][4];

int ExtractBbox(void* pmodel, int sequence, float* mins, float* maxs)
{
	studiohdr_t* pstudiohdr = (studiohdr_t*)pmodel;

	if (!pstudiohdr)
	{
		return 0;
	}

	mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex);

	mins[0] = pseqdesc[sequence].bbmin[0];
	mins[1] = pseqdesc[sequence].bbmin[1];
	mins[2] = pseqdesc[sequence].bbmin[2];

	maxs[0] = pseqdesc[sequence].bbmax[0];
	maxs[1] = pseqdesc[sequence].bbmax[1];
	maxs[2] = pseqdesc[sequence].bbmax[2];

	return 1;
}

NOXREF void GetEyePosition(void* pmodel, float* vecEyePosition)
{
	studiohdr_t* pstudiohdr;

	pstudiohdr = (studiohdr_t*)pmodel;

	if (!pstudiohdr)
	{
		ALERT(at_console, "GetEyePosition() Can't get pstudiohdr ptr!\n");
		return;
	}

	vecEyePosition[0] = pstudiohdr->eyeposition[0];
	vecEyePosition[1] = pstudiohdr->eyeposition[1];
	vecEyePosition[2] = pstudiohdr->eyeposition[2];
}


int FindTransition(void* pmodel, int iEndingAnim, int iGoalAnim, int* piDir)
{
	studiohdr_t* pstudiohdr = (studiohdr_t*)pmodel;
	if (!pstudiohdr)
	{
		return iGoalAnim;
	}

	mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex);

	// bail if we're going to or from a node 0
	if (pseqdesc[iEndingAnim].entrynode == 0 || pseqdesc[iGoalAnim].entrynode == 0)
	{
		return iGoalAnim;
	}

	int iEndNode;

	if (*piDir > 0)
	{
		iEndNode = pseqdesc[iEndingAnim].exitnode;
	}
	else
	{
		iEndNode = pseqdesc[iEndingAnim].entrynode;
	}

	if (iEndNode == pseqdesc[iGoalAnim].entrynode)
	{
		*piDir = 1;
		return iGoalAnim;
	}

	byte* pTransition = ((byte*)pstudiohdr + pstudiohdr->transitionindex);

	int iInternNode = pTransition[(iEndNode - 1) * pstudiohdr->numtransitions + (pseqdesc[iGoalAnim].entrynode - 1)];

	if (iInternNode == 0)
	{
		return iGoalAnim;
	}

	// look for someone going
	for (int i = 0; i < pstudiohdr->numseq; i++)
	{
		if (pseqdesc[i].entrynode == iEndNode && pseqdesc[i].exitnode == iInternNode)
		{
			*piDir = 1;
			return i;
		}
		if (pseqdesc[i].nodeflags)
		{
			if (pseqdesc[i].exitnode == iEndNode && pseqdesc[i].entrynode == iInternNode)
			{
				*piDir = -1;
				return i;
			}
		}
	}

	ALERT(at_console, "error in transition graph");
	return iGoalAnim;
}

void SetBodygroup(void* pmodel, entvars_t* pev, int iGroup, int iValue)
{
	studiohdr_t* pstudiohdr = (studiohdr_t*)pmodel;
	if (!pstudiohdr)
	{
		return;
	}

	if (iGroup > pstudiohdr->numbodyparts)
	{
		return;
	}

	mstudiobodyparts_t* pbodypart = (mstudiobodyparts_t*)((byte*)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if (iValue >= pbodypart->nummodels)
	{
		return;
	}

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;
	pev->body += (iValue - iCurrent) * pbodypart->base;
}

int GetBodygroup(void* pmodel, entvars_t* pev, int iGroup)
{
	studiohdr_t* pstudiohdr = (studiohdr_t*)pmodel;

	if (!pstudiohdr || iGroup > pstudiohdr->numbodyparts)
	{
		return 0;
	}

	mstudiobodyparts_t* pbodypart = (mstudiobodyparts_t*)((byte*)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if (pbodypart->nummodels <= 1)
		return 0;

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;
	return iCurrent;
}


#if defined(REGAMEDLL_FIXES) && defined(HAVE_SSE) // SSE2 version
void AngleQuaternion(vec_t* angles, vec_t* quaternion)
{
	static const ALIGN16_BEG size_t ps_signmask[4] ALIGN16_END = { 0x80000000, 0, 0x80000000, 0 };

	__m128 a = _mm_loadu_ps(angles);
	a = _mm_mul_ps(a, _mm_load_ps(_ps_0p5)); //a *= 0.5
	__m128 s, c;
	sincos_ps(a, &s, &c);

	__m128 im1 = _mm_shuffle_ps(s, c, _MM_SHUFFLE(1, 0, 1, 0)); //im1 =  {sin[0], sin[1], cos[0], cos[1] }
	__m128 im2 = _mm_shuffle_ps(c, s, _MM_SHUFFLE(2, 2, 2, 2)); //im2 =  {cos[2], cos[2], sin[2], sin[2] }

	__m128 part1 = _mm_mul_ps(
		_mm_shuffle_ps(im1, im1, _MM_SHUFFLE(1, 2, 2, 0)),
		_mm_shuffle_ps(im1, im1, _MM_SHUFFLE(0, 3, 1, 3))
	);
	part1 = _mm_mul_ps(part1, im2);

	__m128 part2 = _mm_mul_ps(
		_mm_shuffle_ps(im1, im1, _MM_SHUFFLE(2, 1, 0, 2)),
		_mm_shuffle_ps(im1, im1, _MM_SHUFFLE(3, 0, 3, 1))
	);

	part2 = _mm_mul_ps(part2, _mm_shuffle_ps(im2, im2, _MM_SHUFFLE(0, 0, 2, 2)));

	__m128 signmask = _mm_load_ps((float*)ps_signmask);
	part2 = _mm_xor_ps(part2, signmask);

	__m128 res = _mm_add_ps(part1, part2);
	_mm_storeu_ps(quaternion, res);
}
#else // REGAMEDLL_FIXES
void AngleQuaternion(vec_t* angles, vec_t* quaternion)
{
	real_t sy, cy, sp_, cp;
	real_t angle;
	float sr, cr;

	float ftmp0;
	float ftmp1;
	float ftmp2;

	angle = angles[ROLL] * 0.5;
	sy = sin(angle);
	cy = cos(angle);

	angle = angles[YAW] * 0.5;
	sp_ = sin(angle);
	cp = cos(angle);

	angle = angles[PITCH] * 0.5;
	sr = sin(angle);
	cr = cos(angle);

	ftmp0 = sr * cp;
	ftmp1 = cr * sp_;

	*quaternion = ftmp0 * cy - ftmp1 * sy;
	quaternion[1] = ftmp1 * cy + ftmp0 * sy;

	ftmp2 = cr * cp;
	quaternion[2] = ftmp2 * sy - sp_ * sr * cy;
	quaternion[3] = sp_ * sr * sy + ftmp2 * cy;
}
#endif // REGAMEDLL_FIXES

void QuaternionSlerp(vec_t* p, vec_t* q, float t, vec_t* qt)
{
	int i;
	real_t a = 0;
	real_t b = 0;

	for (i = 0; i < 4; i++)
	{
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}

	if (a > b)
	{
		for (i = 0; i < 4; i++)
			q[i] = -q[i];
	}

	float sclp, sclq;
	real_t cosom = (p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3]);

	if ((1.0 + cosom) > 0.000001)
	{
		if ((1.0 - cosom) > 0.000001)
		{
			real_t cosomega = acos(real_t(cosom));

			float omega = cosomega;
			float sinom = sin(cosomega);

			sclp = sin((1.0f - t) * omega) / sinom;
			sclq = sin(real_t(omega * t)) / sinom;
		}
		else
		{
			sclq = t;
			sclp = 1.0f - t;
		}

		for (i = 0; i < 4; i++)
			qt[i] = sclp * p[i] + sclq * q[i];
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];

		sclp = sin((1.0f - t) * (0.5f * M_PI));
		sclq = sin(t * (0.5f * M_PI));

		for (i = 0; i < 3; i++)
			qt[i] = sclp * p[i] + sclq * qt[i];
	}
}

void QuaternionMatrix(vec_t* quaternion, float(*matrix)[4])
{
	matrix[0][0] = 1.0 - 2.0 * quaternion[1] * quaternion[1] - 2.0 * quaternion[2] * quaternion[2];
	matrix[1][0] = 2.0 * quaternion[0] * quaternion[1] + 2.0 * quaternion[3] * quaternion[2];
	matrix[2][0] = 2.0 * quaternion[0] * quaternion[2] - 2.0 * quaternion[3] * quaternion[1];

	matrix[0][1] = 2.0 * quaternion[0] * quaternion[1] - 2.0 * quaternion[3] * quaternion[2];
	matrix[1][1] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[2] * quaternion[2];
	matrix[2][1] = 2.0 * quaternion[1] * quaternion[2] + 2.0 * quaternion[3] * quaternion[0];

	matrix[0][2] = 2.0 * quaternion[0] * quaternion[2] + 2.0 * quaternion[3] * quaternion[1];
	matrix[1][2] = 2.0 * quaternion[1] * quaternion[2] - 2.0 * quaternion[3] * quaternion[0];
	matrix[2][2] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[1] * quaternion[1];
}

mstudioanim_t* StudioGetAnim(model_t* m_pSubModel, mstudioseqdesc_t* pseqdesc)
{
	mstudioseqgroup_t* pseqgroup;
	cache_user_t* paSequences;

	pseqgroup = (mstudioseqgroup_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqgroupindex) + pseqdesc->seqgroup;

	if (pseqdesc->seqgroup == 0)
	{
		return (mstudioanim_t*)((byte*)g_pstudiohdr + pseqdesc->animindex);
	}

	paSequences = (cache_user_t*)m_pSubModel->submodels;

	if (!paSequences)
	{
		paSequences = (cache_user_t*)IEngineStudio.Mem_Calloc(16, sizeof(cache_user_t)); // UNDONE: leak!
		m_pSubModel->submodels = (dmodel_t*)paSequences;
	}
	if (!IEngineStudio.Cache_Check((struct cache_user_s*)&(paSequences[pseqdesc->seqgroup])))
	{
		IEngineStudio.LoadCacheFile(pseqgroup->name, (struct cache_user_s*)&paSequences[pseqdesc->seqgroup]);
	}
	return (mstudioanim_t*)((byte*)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

mstudioanim_t* LookupAnimation(model_t* model, mstudioseqdesc_t* pseqdesc, int index)
{
	mstudioanim_t* panim = StudioGetAnim(model, pseqdesc);
	if (index >= 0 && index <= (pseqdesc->numblends - 1))
		panim += index * g_pstudiohdr->numbones;

	return panim;
}

void StudioCalcBoneAdj(float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen)
{
	int i, j;
	float value;
	mstudiobonecontroller_t* pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t*)((byte*)g_pstudiohdr + g_pstudiohdr->bonecontrollerindex);

	for (j = 0; j < g_pstudiohdr->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if (pbonecontroller[j].type & STUDIO_RLOOP)
			{
				if (abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					int a, b;
					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
				else
				{
					value = (pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt)) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
			}
			else
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				value = clamp(value, 0.0f, 1.0f);
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
		}
		else
		{
			value = mouthopen / 64.0;

			if (value > 1.0)
				value = 1.0;

			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
		}
		switch (pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

void StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q)
{
	int j, k;
	vec4_t q1, q2;
	vec3_t angle1, angle2;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j + 3] == 0)
		{
			// default
			angle2[j] = angle1[j] = pbone->value[j + 3];
		}
		else
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j + 3]);
			k = frame;

			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;

			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}

			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k + 1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k + 2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
			angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
		}

		if (pbone->bonecontroller[j + 3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if (!VectorCompare(angle1, angle2))
	{
		AngleQuaternion(angle1, q1);
		AngleQuaternion(angle2, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
	else
		AngleQuaternion(angle1, q);
}

void StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos)
{
	int j, k;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		// default;
		pos[j] = pbone->value[j];
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j]);

			k = frame;

			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;

			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}

			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
					pos[j] += (panimvalue[k + 1].value * (1.0 - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
				else
					pos[j] += panimvalue[k + 1].value * pbone->scale[j];
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];

				else
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
			}
		}

		if (pbone->bonecontroller[j] != -1 && adj)
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

void StudioSlerpBones(vec4_t* q1, float pos1[][3], vec4_t* q2, float pos2[][3], float s)
{
	int i;
	vec4_t q3;
	float s1;

	s = clamp(s, 0.0f, 1.0f);
	s1 = 1.0f - s;

	for (i = 0; i < g_pstudiohdr->numbones; i++)
	{
		QuaternionSlerp(q1[i], q2[i], s, q3);

		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];

		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

void StudioCalcRotations(mstudiobone_t* pbones, int* chain, int chainlength, float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f)
{
	int i;
	int j;
	int frame;
	float		adj[MAXSTUDIOCONTROLLERS];
	mstudiobone_t* pbone;
	float		s, dadt;

	if (f > pseqdesc->numframes - 1)
	{
		f = 0;	// bah, fix this bug with changing sequences too fast
	}

	// BUG (somewhere else) but this code should validate this data.
	// This could cause a crash if the frame # is negative, so we'll go ahead
	// and clamp it here
	else if (f < -0.01)
	{
		f = -0.01;
	}

	frame = (int)f;

	dadt = gpGlobals->frametime;
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t*)((byte*)g_pstudiohdr + g_pstudiohdr->boneindex);

	StudioCalcBoneAdj(dadt, adj, player_params[player].prevcontroller, player_params[player].controller, 0);


	for (i = chainlength - 1; i >= 0; i--)
	{
		j = chain[i];
		StudioCalcBoneQuaterion((int)frame, s, &pbones[j], &panim[j], adj, q[j]);
		StudioCalcBonePosition((int)frame, s, &pbones[j], &panim[j], adj, pos[j]);
	}

	if (pseqdesc->motiontype & STUDIO_X)
		pos[pseqdesc->motionbone][0] = 0.0f;

	if (pseqdesc->motiontype & STUDIO_Y)
		pos[pseqdesc->motionbone][1] = 0.0f;

	if (pseqdesc->motiontype & STUDIO_Z)
		pos[pseqdesc->motionbone][2] = 0.0f;

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * player_params[player].framerate;

	if (pseqdesc->motiontype & STUDIO_LX)
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];

	if (pseqdesc->motiontype & STUDIO_LY)
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];

	if (pseqdesc->motiontype & STUDIO_LZ)
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];

}

void ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];

	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];

	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}

void AngleMatrix(const vec_t* angles, float(*matrix)[4])
{
	real_t angle;
	real_t  sr, sp, sy, cr, cp, cy;

	angle = real_t(angles[ROLL] * (M_PI * 2 / 360));
	sy = sin(angle);
	cy = cos(angle);

	angle = real_t(angles[YAW] * (M_PI * 2 / 360));
	sp = sin(angle);
	cp = cos(angle);

	angle = real_t(angles[PITCH] * (M_PI * 2 / 360));
	sr = sin(angle);
	cr = cos(angle);

	matrix[0][0] = cr * cp;
	matrix[1][0] = cr * sp;
	matrix[2][0] = -sr;

	matrix[0][1] = (sy * sr) * cp - cy * sp;
	matrix[1][1] = (sy * sr) * sp + cy * cp;
	matrix[2][1] = sy * cr;

	matrix[0][2] = (cy * sr) * cp + sy * sp;
	matrix[1][2] = (cy * sr) * sp - sy * cp;
	matrix[2][2] = cy * cr;

	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
}

real_t StudioEstimateFrame(float frame, mstudioseqdesc_t* pseqdesc)
{
	if (pseqdesc->numframes <= 1)
		return 0;

	return real_t(pseqdesc->numframes - 1) * frame / 256;
}


float StudioEstimateFrame(mstudioseqdesc_t* pseqdesc)
{
	double dfdt, f;


	if (1)
	{
		if (player_params[player].m_clTime < player_params[player].animtime)
		{
			dfdt = 0;
		}
		else
		{
			dfdt = (player_params[player].m_clTime - player_params[player].animtime) * player_params[player].framerate * pseqdesc->fps;
		}
	}
	else
	{
		dfdt = 0;
	}

	if (pseqdesc->numframes <= 1)
	{
		f = 0;
	}
	else
	{
		f = (player_params[player].frame * (pseqdesc->numframes - 1)) / 256.0;
	}

	f += dfdt;

	if (pseqdesc->flags & STUDIO_LOOPING)
	{
		if (pseqdesc->numframes > 1)
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) * (pseqdesc->numframes - 1);
		}

		if (f < 0)
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else
	{
		if (f >= pseqdesc->numframes - 1.001)
		{
			f = pseqdesc->numframes - 1.001;
		}

		if (f < 0.0)
		{
			f = 0.0;
		}
	}

	return f;
}

/*
====================
StudioEstimateFrame
====================
*/
float HL_StudioEstimateFrame(mstudioseqdesc_t* pseqdesc)
{
	double	dfdt, f;


	if (1)
	{
		if (player_params[player].m_clTime < player_params[player].animtime)
		{
			dfdt = 0;
		}
		else
		{
			dfdt = (player_params[player].m_clTime - player_params[player].animtime) * player_params[player].framerate * pseqdesc->fps;
		}
	}
	else
	{
		dfdt = 0;
	}


	if (pseqdesc->numframes <= 1)
	{
		f = 0;
	}
	else
	{
		f = (player_params[player].frame * (pseqdesc->numframes - 1)) / 256.0;
	}

	f += dfdt;

	if (pseqdesc->flags & STUDIO_LOOPING)
	{
		if (pseqdesc->numframes > 1)
			f -= (int)(f / (pseqdesc->numframes - 1)) * (pseqdesc->numframes - 1);
		if (f < 0) f += (pseqdesc->numframes - 1);
	}
	else
	{
		if (f >= pseqdesc->numframes - 1.001)
			f = pseqdesc->numframes - 1.001;
		if (f < 0.0)  f = 0.0;
	}
	return f;
}

/*
====================
StudioEstimateInterpolant
====================
*/
float HL_StudioEstimateInterpolant()
{
	float	dadt = 1.0f;

	if ((player_params[player].m_clTime >= player_params[player].m_clOldTime + 0.01f))
	{
		dadt = (player_params[player].m_clTime - player_params[player].m_clOldTime) / 0.1f;
		if (dadt > 2.0f) dadt = 2.0f;
	}

	return dadt;
}

void EXT_FUNC HL_StudioSetupBones(model_t* pModel, float frame, int sequence, const vec_t* angles, const vec_t* origin, const byte* pcontroller, const byte* pblending, int iBone, const edict_t* pEdict)
{
	int i, j, chainlength = 0;
	int chain[MAXSTUDIOBONES];
	double		f;
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;
	float bonematrix[3][4];
	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];

	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];

	byte prevcontroller[4];
	byte controller[4];
	vec3_t temp_angles;
	vec3_t temp_origin;
	if (!phf_hitbox_fix->value)
	{
		orig_interface.SV_StudioSetupBones(pModel, frame, sequence, angles, origin, pcontroller, pblending, iBone, pEdict);
		return;
	}

	if (ENTINDEX(pEdict) <= api->GetMaxClients())
	{
		player = ENTINDEX(pEdict) - 1;
		sequence = player_params[player].sequence;
		frame = player_params[player].frame;
		temp_angles = player_params[player].angles;
		temp_origin = player_params[player].origin;
		controller[0] = player_params[player].controller[0];
		controller[1] = player_params[player].controller[1];
		controller[2] = player_params[player].controller[2];
		controller[3] = player_params[player].controller[3];
		prevcontroller[0] = player_params[player].prevcontroller[0];
		prevcontroller[1] = player_params[player].prevcontroller[1];
		prevcontroller[2] = player_params[player].prevcontroller[2];
		prevcontroller[3] = player_params[player].prevcontroller[3];
	}
	else
	{
		// todo implement non-players backtracked animation
		orig_interface.SV_StudioSetupBones(pModel, frame, sequence, angles, origin, pcontroller, pblending, iBone, pEdict);
		return;
	}

	g_pstudiohdr = (studiohdr_t*)IEngineStudio.Mod_Extradata(pModel);

	if (!g_pstudiohdr)
	{
		return;
	}

	if (sequence >= g_pstudiohdr->numseq)
		sequence = 0;


	pbones = (mstudiobone_t*)((byte*)g_pstudiohdr + g_pstudiohdr->boneindex);

	if (iBone < -1 || iBone >= g_pstudiohdr->numbones)
		iBone = 0;


	if (iBone == -1)
	{
		chainlength = g_pstudiohdr->numbones;

		for (i = 0; i < chainlength; i++)
			chain[(chainlength - i) - 1] = i;
	}
	else
	{
		// only the parent bones
		for (i = iBone; i != -1; i = pbones[i].parent)
			chain[chainlength++] = i;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + sequence;

	f = HL_StudioEstimateFrame(pseqdesc);

	panim = StudioGetAnim(pModel, pseqdesc);
	StudioCalcRotations(pbones, chain, chainlength, pos, q, pseqdesc, panim, f);

	if (pseqdesc->numblends > 1)
	{
		float	s;
		float	dadt;

		panim += g_pstudiohdr->numbones;
		StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, f);

		dadt = HL_StudioEstimateInterpolant();
		s = (player_params[player].blending[0] * dadt + player_params[player].prevblending[0] * (1.0f - dadt)) / 255.0f;

		StudioSlerpBones(q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += g_pstudiohdr->numbones;
			StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, f);

			panim += g_pstudiohdr->numbones;
			StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, f);

			s = (player_params[player].blending[0] * dadt + player_params[player].prevblending[0] * (1.0f - dadt)) / 255.0f;
			StudioSlerpBones(q3, pos3, q4, pos4, s);

			s = (player_params[player].blending[1] * dadt + player_params[player].prevblending[1] * (1.0f - dadt)) / 255.0f;
			StudioSlerpBones(q, pos, q3, pos3, s);
		}
	}

	if (player_params[player].sequencetime && (player_params[player].sequencetime + 0.2f > player_params[player].m_clTime) && (player_params[player].prevsequence < g_pstudiohdr->numseq))
	{
		// blend from last sequence
		static float	pos1b[MAXSTUDIOBONES][3];
		static vec4_t	q1b[MAXSTUDIOBONES];
		float		s;


		pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + player_params[player].prevsequence;
		panim = StudioGetAnim(pModel, pseqdesc);

		// clip prevframe
		StudioCalcRotations(pbones, chain, chainlength, pos1b, q1b, pseqdesc, panim, player_params[player].prevframe);

		if (pseqdesc->numblends > 1)
		{
			panim += g_pstudiohdr->numbones;
			StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, player_params[player].prevframe);

			s = (player_params[player].prevseqblending[0]) / 255.0f;
			StudioSlerpBones(q1b, pos1b, q2, pos2, s);

			if (pseqdesc->numblends == 4)
			{
				panim += g_pstudiohdr->numbones;
				StudioCalcRotations(pbones, chain, chainlength,  pos3, q3, pseqdesc, panim, player_params[player].prevframe);

				panim += g_pstudiohdr->numbones;
				StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, player_params[player].prevframe);

				s = (player_params[player].prevseqblending[0]) / 255.0f;
				StudioSlerpBones(q3, pos3, q4, pos4, s);

				s = (player_params[player].prevseqblending[1]) / 255.0f;
				StudioSlerpBones(q1b, pos1b, q3, pos3, s);
			}
		}

		s = 1.0f - (player_params[player].m_clTime - player_params[player].sequencetime) / 0.2f;
		StudioSlerpBones(q, pos, q1b, pos1b, s);
	}
	else
	{
		// store prevframe otherwise
		player_params[player].prevframe = f;
	}

	pbones = (mstudiobone_t*)((byte*)g_pstudiohdr + g_pstudiohdr->boneindex);

	// calc gait animation
	if (player_params[player].gaitsequence != 0)
	{
		qboolean	copy_bones = true;

		if (player_params[player].gaitsequence >= g_pstudiohdr->numseq)
			player_params[player].gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + player_params[player].gaitsequence;

		panim = StudioGetAnim(pModel, pseqdesc);
		StudioCalcRotations(pbones, chain, chainlength,  pos2, q2, pseqdesc, panim, player_params[player].gaitframe);

		for (i = 0; i < g_pstudiohdr->numbones; i++)
		{
			if (!strcmp(pbones[i].name, "Bip01 Spine"))
				copy_bones = false;
			else if (!strcmp(pbones[pbones[i].parent].name, "Bip01 Pelvis"))
				copy_bones = true;

			if (!copy_bones) continue;

			if (copy_bones)
			{
				memcpy(pos[i], pos2[i], sizeof(pos[i]));
				memcpy(q[i], q2[i], sizeof(q[i]));
			}
		}
	}
	AngleMatrix(temp_angles, (*g_pRotationMatrix));

	(*g_pRotationMatrix)[0][3] = temp_origin[0];
	(*g_pRotationMatrix)[1][3] = temp_origin[1];
	(*g_pRotationMatrix)[2][3] = temp_origin[2];

	for (i = chainlength - 1; i >= 0; i--)
	{
		j = chain[i];
		QuaternionMatrix(q[j], bonematrix);

		bonematrix[0][3] = pos[j][0];
		bonematrix[1][3] = pos[j][1];
		bonematrix[2][3] = pos[j][2];

		if (pbones[j].parent == -1)
			ConcatTransforms((*g_pRotationMatrix), bonematrix, (*g_pBoneTransform)[j]);
		else
			ConcatTransforms((*g_pBoneTransform)[pbones[j].parent], bonematrix, (*g_pBoneTransform)[j]);
	}
}

void CS_StudioProcessParams(int player, player_anim_params_s& params)
{
	auto cl = api->GetClient(player);
	if (!cl || !cl->edict)
		return;
	auto pModel = api->GetModel(cl->edict->v.modelindex);
	if (!pModel)
		return;
	g_pstudiohdr = (studiohdr_t*)IEngineStudio.Mod_Extradata(pModel);

	if (!g_pstudiohdr)
	{
		return;
	}
	// Bound sequence number
	if (params.sequence < 0)
		params.sequence = 0;

	if (params.sequence >= g_pstudiohdr->numseq)
		return;

	auto pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + params.sequence;

	
	if (params.m_nPlayerGaitSequences != ANIM_JUMP_SEQUENCE && params.gaitsequence == ANIM_JUMP_SEQUENCE)
	{
		params.gaitframe = 0;
	}

	params.m_nPlayerGaitSequences = params.gaitsequence;
	

	params.f = StudioEstimateFrame(pseqdesc);


	if (params.gaitsequence == ANIM_WALK_SEQUENCE)
	{
		if (params.blending[0] > 26)
		{
			params.blending[0] -= 26;
		}
		else
		{
			params.blending[0] = 0;
		}
		params.prevseqblending[0] = params.blending[0];
	}
	
}
void EXT_FUNC CS_StudioSetupBones(model_t* pModel, float frame, int sequence, const vec_t* angles, const vec_t* origin, const byte* pcontroller, const byte* pblending, int iBone, const edict_t* pEdict)
{
	int i, j, chainlength = 0;
	int chain[MAXSTUDIOBONES];
	double f;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;
	byte prevcontroller[4];
	byte controller[4];
	float bonematrix[3][4];
	vec3_t temp_angles;
	vec3_t temp_origin;

	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];

	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];

	if (!phf_hitbox_fix->value || nofind)
	{
		orig_interface.SV_StudioSetupBones(pModel, frame, sequence, angles, origin, pcontroller, pblending, iBone, pEdict);
		return;
	}
	auto entId = ENTINDEX(pEdict);
	if (entId > 0 && entId <= api->GetMaxClients())
	{
		player = entId - 1;
		sequence = player_params[player].sequence;
		frame = player_params[player].frame;
		temp_origin = player_params[player].origin;
		temp_angles = player_params[player].angles;
		controller[0] = player_params[player].controller[0];
		controller[1] = player_params[player].controller[1];
		controller[2] = player_params[player].controller[2];
		controller[3] = player_params[player].controller[3];
		prevcontroller[0] = player_params[player].prevcontroller[0];
		prevcontroller[1] = player_params[player].prevcontroller[1];
		prevcontroller[2] = player_params[player].prevcontroller[2];
		prevcontroller[3] = player_params[player].prevcontroller[3];
		f = player_params[player].f;
	}
	else
	{
		// todo implement non-players backtracked animation
		orig_interface.SV_StudioSetupBones(pModel, frame, sequence, angles, origin, pcontroller, pblending, iBone, pEdict);
		return;
	}
	g_pstudiohdr = (studiohdr_t*)IEngineStudio.Mod_Extradata(pModel);

	if (!g_pstudiohdr)
	{
		return;
	}
	// Bound sequence number
	if (sequence < 0 || sequence >= g_pstudiohdr->numseq)
		sequence = 0;

	pbones = (mstudiobone_t*)((byte*)g_pstudiohdr + g_pstudiohdr->boneindex);
	pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + sequence;
	panim = StudioGetAnim(pModel, pseqdesc);

	if (iBone < -1 || iBone >= g_pstudiohdr->numbones)
		iBone = 0;

	if (iBone == -1)
	{
		chainlength = g_pstudiohdr->numbones;

		for (i = 0; i < chainlength; i++)
			chain[(chainlength - i) - 1] = i;
	}
	else
	{
		// only the parent bones
		for (i = iBone; i != -1; i = pbones[i].parent)
			chain[chainlength++] = i;
	}

	if (pseqdesc->numblends != NUM_BLENDING)
	{
		StudioCalcRotations(pbones, chain, chainlength, pos, q, pseqdesc, panim, f);
	}
	// This game knows how to do nine way blending
	else
	{
		static float pos3[MAXSTUDIOBONES][3] = {}, pos4[MAXSTUDIOBONES][3] = {};
		static float q3[MAXSTUDIOBONES][4] = {}, q4[MAXSTUDIOBONES][4] = {};

		real_t s, t;
		s = player_params[player].blending[0];
		t = player_params[player].blending[1];


		// Blending is 0-127 == Left to Middle, 128 to 255 == Middle to right
		if (s <= 127.0f)
		{
			// Scale 0-127 blending up to 0-255
			s = (s * 2.0f);

			if (t <= 127.0f)
			{
				t = (t * 2.0f);

				StudioCalcRotations(pbones, chain, chainlength, pos, q, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 1);
				StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 3);
				StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 4);
				StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, f);
			}
			else
			{
				t = 2.0f * (t - 127.0f);

				panim = LookupAnimation(pModel, pseqdesc, 3);
				StudioCalcRotations(pbones, chain, chainlength, pos, q, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 4);
				StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 6);
				StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 7);
				StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, f);
			}
		}
		else
		{
			// Scale 127-255 blending up to 0-255
			s = 2.0f * (s - 127.0f);

			if (t <= 127.0f)
			{
				t = (t * 2.0f);

				panim = LookupAnimation(pModel, pseqdesc, 1);
				StudioCalcRotations(pbones, chain, chainlength, pos, q, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 2);
				StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 4);
				StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 5);
				StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, f);
			}
			else
			{
				t = 2.0f * (t - 127.0f);

				panim = LookupAnimation(pModel, pseqdesc, 4);
				StudioCalcRotations(pbones, chain, chainlength, pos, q, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 5);
				StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 7);
				StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, f);

				panim = LookupAnimation(pModel, pseqdesc, 8);
				StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, f);
			}
		}

		// Normalize interpolant
		s /= 255.0f;
		t /= 255.0f;

		// Spherically interpolate the bones
		StudioSlerpBones(q, pos, q2, pos2, s);
		StudioSlerpBones(q3, pos3, q4, pos4, s);
		StudioSlerpBones(q, pos, q3, pos3, t);
	}

	// Are we in the process of transitioning from one sequence to another.
	if (
		player_params[player].sequencetime &&
		(player_params[player].sequencetime + 0.2 > player_params[player].m_clTime) &&
		(player_params[player].prevsequence >= 0 && player_params[player].prevsequence < g_pstudiohdr->numseq))
	{
		// blend from last sequence
		static float pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];

		float s = player_params[player].prevseqblending[0];
		float t = player_params[player].prevseqblending[1];

		// Point at previous sequenece
		pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + player_params[player].prevsequence;
		panim = StudioGetAnim(pModel, pseqdesc);

		// Know how to do three way blends
		if (pseqdesc->numblends != 9)
		{
			// clip prevframe
			StudioCalcRotations(pbones, chain, chainlength, pos1b, q1b, pseqdesc, panim, player_params[player].prevframe);
		}
		else
		{
			// Blending is 0-127 == Left to Middle, 128 to 255 == Middle to right
			if (s <= 127.0f)
			{
				// Scale 0-127 blending up to 0-255
				s = (s * 2.0f);

				if (t <= 127.0f)
				{
					t = (t * 2.0f);

					StudioCalcRotations(pbones, chain, chainlength, pos1b, q1b, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 1);
					StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 3);
					StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 4);
					StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, player_params[player].prevframe);
				}
				else
				{
					t = 2.0f * (t - 127.0f);

					panim = LookupAnimation(pModel, pseqdesc, 3);
					StudioCalcRotations(pbones, chain, chainlength, pos1b, q1b, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 4);
					StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 6);
					StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 7);
					StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, player_params[player].prevframe);
				}
			}
			else
			{
				// Scale 127-255 blending up to 0-255
				s = 2.0f * (s - 127.0f);

				if (t <= 127.0f)
				{
					t = (t * 2.0f);

					panim = LookupAnimation(pModel, pseqdesc, 1);
					StudioCalcRotations(pbones, chain, chainlength, pos1b, q1b, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 2);
					StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 4);
					StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 5);
					StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, player_params[player].prevframe);
				}
				else
				{
					t = 2.0f * (t - 127.0f);

					panim = LookupAnimation(pModel, pseqdesc, 4);
					StudioCalcRotations(pbones, chain, chainlength, pos1b, q1b, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 5);
					StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 7);
					StudioCalcRotations(pbones, chain, chainlength, pos3, q3, pseqdesc, panim, player_params[player].prevframe);

					panim = LookupAnimation(pModel, pseqdesc, 8);
					StudioCalcRotations(pbones, chain, chainlength, pos4, q4, pseqdesc, panim, player_params[player].prevframe);
				}
			}

			// Normalize interpolant
			s /= 255.0f;
			t /= 255.0f;

			// Spherically interpolate the bones
			StudioSlerpBones(q1b, pos1b, q2, pos2, s);
			StudioSlerpBones(q3, pos3, q4, pos4, s);
			StudioSlerpBones(q1b, pos1b, q3, pos3, t);
		}

		// Now blend last frame of previous sequence with current sequence.
		s = 1.0 - (player_params[player].m_clTime - player_params[player].sequencetime) / 0.2;
		StudioSlerpBones(q, pos, q1b, pos1b, s);
	}
	else
	{
		player_params[player].prevframe = f;
	}
	if (player != -1 && sequence < ANIM_FIRST_DEATH_SEQUENCE && sequence != ANIM_SWIM_1 && sequence != ANIM_SWIM_2)
	{
		bool bCopy = true;

		if (player_params[player].gaitsequence < 0 || player_params[player].gaitsequence >= g_pstudiohdr->numseq)
			player_params[player].gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t*)((byte*)g_pstudiohdr + g_pstudiohdr->seqindex) + player_params[player].gaitsequence;

		panim = StudioGetAnim(pModel, pseqdesc);
		StudioCalcRotations(pbones, chain, chainlength, pos2, q2, pseqdesc, panim, player_params[player].gaitframe);

		for (i = 0; i < g_pstudiohdr->numbones; i++)
		{
			if (!strcmp(pbones[i].name, "Bip01 Spine"))
			{
				bCopy = false;
			}
			else if (!strcmp(pbones[pbones[i].parent].name, "Bip01 Pelvis"))
			{
				bCopy = true;
			}

			if (bCopy)
			{
				memcpy(pos[i], pos2[i], sizeof(pos[i]));
				memcpy(q[i], q2[i], sizeof(q[i]));
			}
		}
	}


	AngleMatrix(temp_angles, (*g_pRotationMatrix));

	(*g_pRotationMatrix)[0][3] = temp_origin[0];
	(*g_pRotationMatrix)[1][3] = temp_origin[1];
	(*g_pRotationMatrix)[2][3] = temp_origin[2];

	for (i = chainlength - 1; i >= 0; i--)
	{
		j = chain[i];
		QuaternionMatrix(q[j], bonematrix);

		bonematrix[0][3] = pos[j][0];
		bonematrix[1][3] = pos[j][1];
		bonematrix[2][3] = pos[j][2];

		if (pbones[j].parent == -1)
			ConcatTransforms((*g_pRotationMatrix), bonematrix, (*g_pBoneTransform)[j]);
		else
			ConcatTransforms((*g_pBoneTransform)[pbones[j].parent], bonematrix, (*g_pBoneTransform)[j]);
	}
}