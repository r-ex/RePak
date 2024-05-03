#pragma once
#include <cstdint>

struct ShaderSetAssetHeader_v8_t
{
	uint64_t reservedVtbl;

	PagePtr_t name; // const char*

	uint64_t unk_10; // unknown data type, but definitely 8 bytes

	uint16_t unk_18; // count

	uint16_t textureInputCount;

	uint16_t samplerCount; // number of samplers used by the pixel shader

	uint8_t unk_1E;
	uint8_t unk_1F;

	uint8_t unk_20[40];

	uint64_t vertexShader;
	uint64_t pixelShader;
};
static_assert(offsetof(ShaderSetAssetHeader_v8_t, vertexShader) == 72);
static_assert(offsetof(ShaderSetAssetHeader_v8_t, unk_1E) == 0x1e);
static_assert(sizeof(ShaderSetAssetHeader_v8_t) == 88);
