#pragma once
#define SETTINGS_MAX_MODS UINT16_MAX

enum SettingsModType_e : unsigned short
{
	kIntPlus = 0x0,
	kIntMultiply = 0x1,
	kFloatPlus = 0x2,
	kFloatMultiply = 0x3,
	kBool = 0x4,
	kNumber = 0x5,
	kString = 0x6,

	SETTINGS_MOD_COUNT
};

inline const char* const g_settingsModType[SETTINGS_MOD_COUNT] =
{
	"int_plus",
	"int_multipy",
	"float_plus",
	"float_multipy",
	"bool",
	"number",
	"string"
};

union SettingsModValue_u
{
	bool boolValue;
	int intValue;
	float floatValue;
	uint32_t stringOffset;
};

struct SettingsMod_s
{
	uint16_t nameIndex; // Index into mod names array.
	SettingsModType_e type;
	int valueOffset;
	SettingsModValue_u value;
};

struct SettingsAssetHeader_s
{
	// This field becomes a pointer to the settings layout
	// in the runtime.
	PakGuid_t settingsLayoutGuid;

	void* valueData;
	char* name;

	char* stringData;
	uint32_t uniqueId;

	char padding[4]; // padding most likely

	const char** modNames;
	SettingsMod_s* modValues;

	uint32_t valueBufSize;
	uint32_t modFlags;
	uint32_t modNameCount;
	uint32_t modValuesCount;
};
static_assert(sizeof(SettingsAssetHeader_s) == 72);

struct SettingsAsset_s
{
	const rapidjson::Value* value;
	const SettingsLayoutAsset_s* layout;

	size_t bufferBase;

	std::vector<int64_t> fieldIndexMap;
	std::vector<SettingsAsset_s> subAssets;
};
