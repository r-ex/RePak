#pragma once

// yeah these are totally necessary and couldn't just be used as single values :}
#define TXTR_VERSION 8
#define UIMG_VERSION 10
#define DTBL_VERSION 1

namespace Assets
{
	void AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddUIImageAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddDataTableAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddPatchAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);

	extern std::string g_sAssetsDir;
};

