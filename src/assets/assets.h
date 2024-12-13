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
	void AddPatchAsset(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);

	void AddTextureAsset_v8(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);

	void AddMaterialAsset_v12(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);
	void AddMaterialAsset_v15(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);

	void AddUIImageAsset_v10(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);

	void AddDataTableAsset(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);
	void AddModelAsset_v9(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);

	void AddAnimSeqAsset_v7(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);
	void AddAnimRigAsset_v4(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);

	void AddShaderSetAsset_v8(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);
	void AddShaderSetAsset_v11(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);
	void AddShaderAsset_v8(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);
	void AddShaderAsset_v12(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry);
};
