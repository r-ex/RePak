#pragma once



enum class VariableType : uint8_t {
	NONE = 0x0,
	STRING = 0x1,
	ASSET = 0x2,
	BOOL = 0x3,
	INT = 0x4,
	FLOAT = 0x5,
	FLOAT2 = 0x6,
	FLOAT3 = 0x7,
	COLOR_ALPHA = 0x8,
	GAMETIME = 0x9,
	FLOAT_UNK = 0xA,
	IMAGE = 0xB

};

struct Argument_s
{
	VariableType type;

	uint8_t unk_1;

	uint16_t dataOffset;
	uint16_t nameOffset;

	uint16_t shortHash;
};

struct ArgCluster_s
{
	uint16_t argIndex;
	uint16_t argCount;

	uint8_t byte_4;
	uint8_t byte_5;

	uint16_t short_6;
	uint16_t valueSize;
	uint16_t dataStructSize;
	uint16_t short_C;
	uint16_t short_E;
	uint16_t renderJobCount;
};

struct IndexedColor_s {
	uint16_t red;
	uint16_t green;
	uint16_t blue;
	uint16_t alpha;
};

struct StyleDescriptor_v30_s {
	uint16_t type;
	IndexedColor_s color0;
	IndexedColor_s color1;
	IndexedColor_s color2;
	uint16_t blend;
	uint16_t premul;
	uint16_t val_1E;
	uint16_t val_20;
	uint16_t val_22;
	uint16_t val_24;
	uint16_t val_26;
	uint16_t val_28;
	uint16_t val_2A;
	uint16_t val_2C;
	uint16_t val_2E;
	uint16_t val_30;
	uint16_t val_32;
};


struct RuiHeader_v30_s
{
	const char* name;
	uint8_t* dataStructInitData;
	uint8_t* transformData;
	float elementWidth;
	float elementHeight;
	float elementWidthRcp;
	float elementHeightRcp;
	char* argNames;
	ArgCluster_s* argClusters;
	Argument_s* arguments;
	short argumentCount; // number of slots for arguments. not all are used. has to be power of 2
	short keyframingCount;
	uint16_t dataStructSize;
	uint16_t dataStructInitSize;
	uint16_t styleDescriptorCount;
	uint16_t maxTransformIndex;
	uint16_t renderJobCount;
	uint16_t argClusterCount;
	StyleDescriptor_v30_s* styleDescriptors;
	uint8_t* renderJobData; 
	void* keyframings; // maps values to others through linar/qubicSpline regression
};

