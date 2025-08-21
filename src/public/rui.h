#pragma once


#define RUI_PACKAGE_MAGIC ('R' | ('U' << 8) | ('I' << 16) | ('P' << 24))
#define RUI_PACKAGE_VERSION 1

#pragma pack(push)
struct RuiPackageHeader_v1_t {
	uint32_t magic;
	uint16_t packageVersion;
	uint16_t ruiVersion;
	uint64_t nameOffset;
	float elementWidth;
	float elementHeight;
	float elementWidthRcp;
	float elementHeightRcp;
	uint16_t defaultValuesSize;
	uint16_t dataStructSize;
	uint16_t styleDescriptorCount;
	uint16_t unk_A4;//unused in r2
	uint16_t renderJobCount;
	uint16_t argClusterCount;
	uint16_t argCount;
	uint16_t mappingCount;
	uint16_t transformDataSize;
	uint16_t nameSize;
	uint16_t rpakPointersInDefaltDataCount;
	uint8_t pad[2];
	uint32_t argNamesSize;
	uint32_t renderJobSize;
	uint32_t mappingSize;
	uint32_t defaultStringsSize;
	uint64_t argNamesOffset;//debug only
	uint64_t argClusterOffset;
	uint64_t argumentsOffset;
	uint64_t styleDescriptorOffset;
	uint64_t renderJobOffset;
	uint64_t mappingOffset;
	uint64_t transformDataOffset;
	uint64_t defaultValuesOffset;
	uint64_t defaultStringDataOffset;
	uint64_t rpakPointersInDefaultDataOffset;
	uint64_t defaultStringsDataSize;
};
#pragma pack(pop)

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
	short mappingCount;
	uint16_t dataStructSize;
	uint16_t dataStructInitSize;
	uint16_t styleDescriptorCount;
	uint16_t maxTransformIndex;
	uint16_t renderJobCount;
	uint16_t argClusterCount;
	StyleDescriptor_v30_s* styleDescriptors;
	uint8_t* renderJobData; 
	void* valueMappings; // maps values to others through linar/qubicSpline regression
};

struct RuiPackage {
	RuiPackageHeader_v1_t hdr{};

	RuiPackage(const fs::path& inputPath);
	RuiHeader_v30_s CreateRuiHeader_v30();
	std::vector<char> name;
	std::vector<char> defaultData;
	std::vector<char> defaultStrings;
	std::vector<uint16_t> defaultStringOffsets;
	std::vector<char> transformData;
	std::vector<char> renderJobs;
	std::vector<Argument_s> arguments;
	std::vector<ArgCluster_s> argCluster;
	std::vector<char> styleDescriptors;
	
};