#pragma once

std::string g_sAssetsDir;

namespace Assets
{
	void AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath);

	void AddDataTableAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath);
};

