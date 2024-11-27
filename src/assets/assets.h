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
	void AddPatchAsset(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);

	void AddTextureAsset(CPakFile* pak, uint64_t guid, const char* assetPath, bool forceDisableStreaming, bool materialGeneratedTexture);
	void AddTextureAsset_v8(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);

	void AddMaterialAsset_v12(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);
	void AddMaterialAsset_v15(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);

	void AddUIImageAsset_v10(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);

	void AddDataTableAsset(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);
	void AddModelAsset_v9(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);

	void AddAnimSeqAsset(CPakFile* pak, const char* assetPath);
	void AddAnimSeqAsset_v7(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);
	void AddAnimRigAsset_v4(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);

	void AddShaderSetAsset_v8(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);
	void AddShaderSetAsset_v11(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);
	void AddShaderAsset_v8(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);
	void AddShaderAsset_v12(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry);

};
