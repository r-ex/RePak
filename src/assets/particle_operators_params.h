#pragma once
#include "math/vector.h"
#include "math/color.h"
#include "utils/DmxTools.h"
#include "public/particle_effect.h"

struct C_OP_GraphVector_Params
{
	Vector4 m_graph[16];
	bool m_isLooping;
	float m_graphTime;
	bool m_graphTimeIsInLifespans;
	bool m_bUseLocalSpace;
	int m_nFieldOutput;
	int m_outputScaleType;
	int m_LocalCtrlPoint;
	int m_inputTimeCtrlPoint;
	Vector3 m_vecOutputMin;
	Vector3 m_vecOutputMax;
};

struct C_OP_RenderPoints_Params
{
};

struct C_OP_RenderLightSource_Params
{
	float m_flColorScale;
	bool m_bColorScaleByAlpha;
	float m_flRadiusScale;
	bool m_bIsCockpit;
	float m_flPriorityMultiplier;
	float m_flFalloffHalfwayFrac;
};

struct C_OP_RenderScreenVelocityRotate_Params
{
	float m_flRotateRateDegrees;
	float m_flForwardDegrees;
};

extern void ParticleEffect_BakeParams(DmSymbolTable& symbolTable, const DmElement_s& elem, ParticleDefintionParams_s& params);
