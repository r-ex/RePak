#pragma once
#include "material.h"

struct MaterialForAspect_s
{
	// Each shader type has a material, but they can be 0 for some.
	// It depends on the mt4a asset and what types it supports.
	PakGuid_t materials[MaterialShaderType_e::_TYPE_COUNT];
};
