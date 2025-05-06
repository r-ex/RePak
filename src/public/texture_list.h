#pragma once
#include "logic/rtech.h"

struct TextureListHeader_s
{
	const PakGuid_t** textureGuids;
	const char** textureNames;
	size_t numTextures;
};
