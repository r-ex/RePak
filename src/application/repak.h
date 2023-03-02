#pragma once
#include <assets/assets.h>
#include <utils/utils.h>

// starpak data entry vector
inline std::vector<SRPkDataEntry> g_vSRPkDataEntries{};

// !TODO: remove extraneous copy constructor for 'ext'?
#define ASSET_HANDLER(ext, file, assetEntries, func_v7, func_v8) \
	if (file["$type"].GetStdString() == std::string(ext)) \
	{ \
		if(this->m_Version == 8 && func_v8) \
			func_v8(this, &assetEntries, file["path"].GetString(), file); \
		if(this->m_Version == 7 && func_v7) \
			func_v7(this, &assetEntries, file["path"].GetString(), file); \
	}
