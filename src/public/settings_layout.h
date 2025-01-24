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

struct SettingsLayoutParseResult_s
{
	SettingsLayoutParseResult_s()
		:totalValueBufferSize(0)
	{}

	std::vector<std::string> fieldNames;
	std::vector<std::string> typeNames;
	std::vector<uint32_t> offsetMap;

	std::vector<SettingsFieldType_e> typeMap;

	uint32_t totalValueBufferSize;
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
extern SettingsFieldType_e SettingsLayout_GetFieldTypeForString(const char* const typeName);
