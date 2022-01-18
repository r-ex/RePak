#pragma once

// yeah these are totally necessary and couldn't just be used as single values :}
#define TXTR_VERSION 8

namespace Assets
{
	void AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath);
	void AddDataTableAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath);
	extern std::string g_sAssetsDir;
};

