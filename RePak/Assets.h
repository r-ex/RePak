#pragma once

namespace Assets
{
	void AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath);
	void AddDataTableAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath);
	extern std::string g_sAssetsDir;
};

