#pragma once
#include "math/vector.h"
#include "math/color.h"
#include "dmelement.h"

struct UtlStringDisk_s // CUtlString encoded on the disk.
{
	const char* memory;
	int64_t allocationCount;
	int64_t growSize;
	int64_t actualLength;
};

struct ParticleDefintionParams_s
{
	int maxParticles;
	int initialParticles;
	UtlStringDisk_s materialName;
	Vector3 boundingBoxMin;
	Vector3 boundingBoxMax;
	float cullRadius;
	int cullControlPoint;
	UtlStringDisk_s fallbackReplacementName;
	int fallbackThreshold;
	float constantRadius;
	ColorB constantColor;
	float constantColorScale;
	float constantRotation;
	float constantRotationSpeed;
	Vector3 constantNormal;
	float constantNormalSpinDegrees;
	int constantSequenceNumber;
	int constantSequenceNumber1;
	int groupId;
	float maximumTimeStep;
	float maximumSimTime;
	float minimumSimTime;
	int minimumFrames;
	int skipRenderControlPoint;
	int allowRenderControlPoint;
	float maxDrawDistance;
	float minDrawDistance;
	bool applyDrawDistanceWhenChild;
	char gapBD[3]; // padding.
	float noDrawTimeToGoToSleep;
	bool shouldSort;
	bool shouldBatch;
	bool viewModelEffect;
	bool screenSpaceEffect;
	bool drawWithScreenSpace;
	bool drawThroughLeafSystem;
	bool checkOwnerDormant;
	char gapCB; // padding.
	int maxRecursionDepth;
	float aggregateRadius;
	int aggregationMinAvailableParticles;
	float minimumTimeStep;
	int minCpuLevel;
	int minGpuLevel;
	float stopSimulationAfterTime;
	float warmUpTime;
	bool pauseAfterWarmUp;
	bool killIfOverLimit;
	bool useHeightInYaw;
	bool doDrawDuringPhaseShift;
	bool doOnlyDrawDuringPhaseShift;
	bool inheritsAlphaVisibility;
	bool inheritsRawVisibility;
	char gapF3; // padding.
	int randomSeed;
	bool renderShadows;
	bool isScripted;
	bool reserved;
	bool sortFromOrigin;
	bool lowResDrawEnable;
	char gapFD[3]; // padding.
	float lowResDist;
	bool inheritEntityScale;
	char gap105[3]; // padding.
};

struct ParticleOperatorCommonParams_s
{
	float ppStartFadeInTime;
	float opEndFadeInTime;
	float opStartFadeOutTime;
	float opEndFadeOutTime;
	float opFadeOscillatePeriod;
	int opTimeOffsetSeed;
	float opTimeOffsetMin;
	float opTimeOffsetMax;
	int opTimeScaleSeed;
	float opTimeScaleMin;
	float opTimeScaleMax;
	int opStrengthScaleSeed;
	float opStrengthMinScale;
	float opStrengthMaxScale;
	int opEndCapState;
	bool mute;
};

struct ParticleOperatorBaked_s
{
	uint16_t opTypeIndex;                  // indexes into dictionary, gets the operator type
	uint16_t opSpecificParamsDataSize;     // struct size of operator (matches with ctor for each operator, so its most likely jsut that)
	uint16_t visibilityInputsIndex;        // same stuff as source engine but baked (CParticleVisibilityInputs)
	uint16_t initializerParamsIndex;       // same stuff as source engine but baked
	uint16_t opSpecificParamsIndex;        // points to struct that is specific to this operator
	char pad[10]; // id must be aligned to 16 bytes.
	DmObjectId_s id;
	ParticleOperatorCommonParams_s common; // similar to source, but also baked
};

struct ParticleOperatorList_s
{
	ParticleOperatorBaked_s** operators;
	uint64_t count;
};

struct ParticleChildParams_s
{
	float delay;
	bool endCap;
	bool mute;
	int elemIdx;
};

struct ParticleElementDisk_s
{
	const char* name;
	DmObjectId_s id;
	ParticleChildParams_s* childRefs;
	const char** scriptRefs;
	int numChildRefs;
	int numScriptRefs;
	bool preventNameBasedLookup;
	char pad[4]; // params must be aligned to 8 bytes.
	ParticleDefintionParams_s params;
	ParticleOperatorList_s operators[6];
};

struct EffectAssetData_s
{
	const char* fileName;
	ParticleElementDisk_s* elements;
	size_t numElements;
	const char** stringDict;
	size_t numStrings;
};

struct EffectAssetHdr_s
{
	EffectAssetData_s* pcfPtr;
	size_t pcfCount;
};
