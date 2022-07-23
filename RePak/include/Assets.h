#pragma once

// asset versions
#define TXTR_VERSION 8
#define UIMG_VERSION 10
#define DTBL_VERSION 1
#define RMDL_VERSION 9
#define MATL_VERSION 15

namespace Assets
{
	void AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddUIImageAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddDataTableAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddPatchAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddModelAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddMaterialAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);

	extern std::string g_sAssetsDir;
	extern std::vector<std::string> g_vsStarpakPaths;
	extern std::vector<std::string> g_vsOptStarpakPaths;
	extern std::vector<SRPkDataEntry> g_vSRPkDataEntries;
};

