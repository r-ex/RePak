#pragma once
#include "logic/pakfile.h"

// asset versions
#define TXTR_VERSION 8
#define UIMG_VERSION 10
//#define DTBL_VERSION 1
#define RMDL_VERSION 10
#define MATL_VERSION 15

namespace Assets
{
	void AddMaterialAsset_v12(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);

	void AddTextureAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, bool forceDisableStreaming);
	void AddTextureAsset_v8(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);

	void AddUIImageAsset_v10(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddDataTableAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddPatchAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddModelAsset_v9(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddMaterialAsset_v15(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	
	void AddAnimSeqAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath);
	void AddAnimSeqAsset_v7(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
	void AddAnimRigAsset_v4(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry);
};
