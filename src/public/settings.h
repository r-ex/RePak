#pragma once
//#include <pch.h>

struct SettingsAssetHeader_s
{
	// This field becomes a pointer to the settings layout
	// in the runtime.
	PakGuid_t settingsLayoutGuid;

	void* valueData;
	char* name;

	char* stringData;
	uint32_t uniqueId;

	char unk_24[4]; // padding most likely

	char** modNames;

	void* unk_30;

	int valueBufSize;
	char unk_38[12];
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
