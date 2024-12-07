#pragma once

struct player_anim_params_s
{
public:
	int playerId;
	int sequence;
	int prevgaitsequence;
	int gaitsequence;
	int m_nPlayerGaitSequences;

	double f;
	float frame;
	float prevframe;
	float gaitframe;
	float gaityaw;
	vec3_t origin;
	vec3_t final_angles;
	vec3_t angles;
	vec3_t m_prevgaitorigin;
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

	void pack_params(size_t player_index, CUtlBuffer& buffer, float (*bone_transform)[MAXSTUDIOBONES][3][4], size_t num_bones)
	{
		buffer.PutUnsignedChar(player_index);

		buffer.PutFloat(gaitframe);
		buffer.PutFloat(gaityaw);
		buffer.PutFloat(m_prevgaitorigin[0]);
		buffer.PutFloat(m_prevgaitorigin[1]);
		buffer.PutFloat(m_prevgaitorigin[2]);
		buffer.PutFloat(m_flGaitMovement);
		buffer.PutFloat(m_clTime);
		buffer.PutFloat(m_clOldTime);
		buffer.PutUnsignedInt(num_bones);
		if (num_bones)
		{
			buffer.Put(bone_transform, sizeof(float[3][4]) * num_bones);
		}

	}
	void unpack_params(CUtlBuffer& buffer, float (*bone_transform)[MAXSTUDIOBONES][3][4], size_t& num_bones)
	{
		gaitframe = buffer.GetFloat();
		gaityaw = buffer.GetFloat();
		m_prevgaitorigin[0] = buffer.GetFloat();
		m_prevgaitorigin[1] = buffer.GetFloat();
		m_prevgaitorigin[2] = buffer.GetFloat();
		m_flGaitMovement = buffer.GetFloat();
		m_clTime = buffer.GetFloat();
		m_clOldTime = buffer.GetFloat();
		num_bones = buffer.GetUnsignedInt();
		if (num_bones)
		{
      buffer.Get(bone_transform, sizeof(float[3][4]) * num_bones);			
		}
	}
};