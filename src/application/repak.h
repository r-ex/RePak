#pragma once
#include <assets/assets.h>
#include <utils/utils.h>
#include "public/rpak.h"

// starpak data entry vector
inline std::vector<StreamableDataEntry> g_vSRPkDataEntries{};

#define ASSET_HANDLER(ext, file, assetEntries, func_v7, func_v8) \
	if (file["$type"].GetStdString() == ext) \
	{ \
		if(this->m_Header.fileVersion == 8 && func_v8) \
			func_v8(this, &assetEntries, file["path"].GetString(), file); \
		if(this->m_Header.fileVersion == 7 && func_v7) \
			func_v7(this, &assetEntries, file["path"].GetString(), file); \
	}
