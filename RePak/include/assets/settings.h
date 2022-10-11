#pragma once

#include <pch.h>

#pragma pack(push, 2)
// this is currently unused, but could probably be used just fine if you copy stuff over from the RPak V7 material function in material.cpp
enum class SettingsFieldType : uint16_t
{
	Bool,
	Int,
	Float,
	Float2,
	Float3,
	String,
	Asset,
	Asset2,
	Array,
	Array2,
};

// --- stgs ---
struct SettingsHeader
{
	uint64_t LayoutGUID;

	RPakPtr Values{};

	RPakPtr Name{};

	RPakPtr StringBuf{};

	uint64_t Unk1;

	RPakPtr ModNames{};

	RPakPtr Unk2{};

	uint32_t KvpBufferSize = 0x60;

	uint64_t Unk3;
	uint32_t Unk4;
};

struct SettingsKeyValue
{
	uint32_t BlockIndex;
	uint32_t BlockOffset;
};

struct SettingsKeyValuePair
{
	SettingsKeyValue Key;
	SettingsKeyValue Value;
};

struct SettingsLayoutHeader
{
	RPakPtr pName{};
	RPakPtr pItems{};
	RPakPtr unk2{};
	uint32_t unk3 = 0;
	uint32_t itemsCount = 0;
	uint32_t unk4 = 0;
	uint32_t unk5 = 0;
	uint32_t unk6 = 0;
	uint32_t unk7 = 0;
	uint32_t unk8 = 0;
	uint32_t unk9 = 0;
	RPakPtr pStringBuf{};
	RPakPtr unk11{};
};

struct SettingsLayoutItem
{
	SettingsFieldType type = SettingsFieldType::String;
	uint16_t NameOffset; // offset from start of stgs value buffer
	uint32_t ValueOffset;
};

struct SettingsLayout
{
	std::string name;
	unsigned int itemsCount;
	std::vector<SettingsLayoutItem> items;
};
#pragma pack(pop)