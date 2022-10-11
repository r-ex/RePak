#pragma once

// asset versions
#define TXTR_VERSION 8
#define UIMG_VERSION 10
#define DTBL_VERSION 1
#define RMDL_VERSION 10
#define MATL_VERSION 15

struct CPakFile;

namespace Assets
{
	//void AddTextureAsset(std::vector<RPakAssetEntryV7>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	//void AddUIImageAsset_r2(std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddDataTableAsset_v0(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddModelAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddMaterialAsset_v12(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);

	void AddTextureAsset_v8(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddTextureAssetList_v8(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, rapidjson::Value& mapEntry);
	void AddUIImageAsset_v10(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddDataTableAsset_v1(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddPatchAsset(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddModelAsset_v9(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddMaterialAsset_v15(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddSettingsLayoutAsset_v0(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddSettingsAsset_v1(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);

	void AddRigAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddRigAsset_v4(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddRseqAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddRseqAsset_v7(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);

	void AddShaderSetAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddShaderSetAsset_v11(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddShaderAsset_v12(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);


	extern std::string g_sAssetsDir;
};

