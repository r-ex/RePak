#pragma once
#include <cstdint>

struct ShaderSetAssetHeader_v8_t
{
	uint64_t reserved_vftable;

	PagePtr_t name; // const char*

	uint64_t reserved_inputFlags; // unknown data type, but definitely 8 bytes

	uint16_t textureInputCounts[2];

	uint16_t numSamplers; // number of samplers used by the pixel shader

	uint16_t firstResourceBindPoint;
	uint16_t numResources;

	uint8_t unk_20[32];

	uint64_t vertexShader;
	uint64_t pixelShader;
};
static_assert(sizeof(ShaderSetAssetHeader_v8_t) == 88);
static_assert(offsetof(ShaderSetAssetHeader_v8_t, vertexShader) == 72);

struct ShaderSetAssetHeader_v11_t
{
	uint64_t reserved_vftable;

	PagePtr_t name; // const char*

	uint64_t reserved_inputFlags; // stores some calculated value of vertex shader input flags

	uint16_t textureInputCounts[2];

	uint16_t numSamplers; // number of samplers used by the pixel shader

	uint8_t firstResourceBindPoint;
	uint8_t numResources;

	uint8_t unk_20[16]; // at least some of this is reserved, potentially all of it

	uint64_t vertexShader;
	uint64_t pixelShader;
};
static_assert(sizeof(ShaderSetAssetHeader_v11_t) == 64);
static_assert(offsetof(ShaderSetAssetHeader_v11_t, vertexShader) == 48);
