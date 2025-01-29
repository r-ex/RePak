#pragma once

enum SettingsFieldType_e : unsigned short
{
	ST_Bool,
	ST_Int,
	ST_Float,
	ST_Float2,
	ST_Float3,
	ST_String,
	ST_Asset,
	ST_Asset_2,
	ST_StaticArray,
	ST_DynamicArray,

	ST_Invalid = 0xFFFF,
};

struct SettingsField_s
{
	SettingsFieldType_e type;
	uint16_t nameOffset;
	uint32_t valueOffset : 24;
	uint32_t subLayoutIndex : 8;
};

struct SettingsDynamicArray_s
{
	uint32_t arraySize;   // Can be 0, since dynamic arrays are meant to be growable in the runtime.
	uint32_t arrayOffset; // The offset is always relative from this SettingsDynamicArray_s struct.
};
static_assert(sizeof(SettingsDynamicArray_s) == 8);

struct SettingsLayoutHeader_s
{
	PagePtr_t name;
	PagePtr_t fieldMap;
	PagePtr_t unk_10; // seems unused, needs research.

	uint32_t hashTableSize;
	uint32_t fieldCount;
	uint32_t extraDataSizeIndex;
	uint32_t hashStepScale;
	uint32_t hashSeed;

	// -1 on dynamic arrays?
	int arrayElemCount;
	int layoutSize;
	int unk_34; // seems unused, needs research.
	PagePtr_t fieldNames;
	PagePtr_t subHeaders; // points to an array of SettingsLayoutHeader_s
};
static_assert(sizeof(SettingsLayoutHeader_s) == 72);

struct SettingsLayoutParseResult_s
{
	SettingsLayoutParseResult_s()
		: arrayElemCount(1)
		, subLayoutCount(0)
		, highestSubLayoutIndex(0)
		, extraDataSizeIndex(0)
		, usedValueBufferSize(0)
		, totalValueBufferSize(0)
		, hashSeed(0)
		, hashTableSize(0)
		, hashStepScale(0)
	{}

	std::vector<std::string> fieldNames;
	std::vector<std::string> typeNames;
	std::vector<uint32_t> offsetMap;
	std::vector<uint32_t> indexMap;
	std::vector<uint32_t> bucketMap;
	std::vector<SettingsFieldType_e> typeMap;

	int arrayElemCount;
	uint32_t subLayoutCount;

	uint32_t highestSubLayoutIndex;
	uint32_t extraDataSizeIndex;

	uint32_t usedValueBufferSize;
	uint32_t totalValueBufferSize;

	// These vars are for fast field name lookup in the runtime.
	uint32_t hashSeed;
	uint32_t hashTableSize; // Number of hash buckets, must be a power of 2.
	uint32_t hashStepScale;
};

struct SettingsLayoutAsset_s
{
	SettingsLayoutParseResult_s rootLayout;
	std::vector<SettingsLayoutAsset_s> subLayouts;
};

static const char* s_settingsFieldTypeNames[] = {
	"bool",
	"int",
	"float",
	"float2",
	"float3",
	"string",
	"asset",
	"asset_noprecache",
	"array",
	"array_dynamic",
};

extern uint32_t SettingsLayout_GetFieldSizeForType(const SettingsFieldType_e type);
extern uint32_t SettingsLayout_GetFieldAlignmentForType(const SettingsFieldType_e type);
extern SettingsFieldType_e SettingsLayout_GetFieldTypeForString(const char* const typeName);
