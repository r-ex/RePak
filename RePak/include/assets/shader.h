#pragma once

#include <pch.h>

#pragma pack(push, 2)
enum ShaderType : uint8_t
{
	Pixel,
	Vertex,
	Geometry,
	Hardware,
	Domain,
	Compute
};

struct TextureSlotData
{
	uint16_t unk1;
	uint16_t unk2;
	uint16_t unk3;
	uint16_t unk4;

	uint16_t unk5;
	uint16_t unk6;
	uint16_t unk7;
	uint16_t unk8;
};

// --- shdr ---
struct ShaderHeader
{
	RPakPtr pName{};

	ShaderType ShaderType;
	uint8_t unk = 255;
	uint16_t min_widthheight = 1;

	uint16_t max_width = 512;
	uint16_t max_height = 512;

	RPakPtr pIndex0{};

	RPakPtr pTextureSlotData{};
};

int size = sizeof(ShaderHeader);

struct TextureSlot
{
};

// --- shds ---
struct ShaderSetHeader {
	uint64_t VTablePadding;

	RPakPtr pName{};

	uint8_t Unknown1[0x8];
	uint16_t Count1;  // if Count3 = 3 then Count1 = TextureInputCount
	uint16_t TextureInputCount;
	uint16_t Count3 = 3;
	uint8_t Byte1 = 0;
	uint8_t Byte2 = 0;

	uint8_t Unknown2[0x10];

	uint64_t VertexShaderGUID;
	uint64_t PixelShaderGUID;
};

struct ShaderSetHeaderTF {
	uint64_t VTablePadding;

	RPakPtr pName{};

	uint8_t Unknown1[0x8];
	uint16_t Count1;
	uint16_t TextureInputCount;
	uint16_t Count3;
	uint8_t Byte1;
	uint8_t Byte2;

	uint8_t Unknown2[0x28];

	// only used for version 12+
	uint64_t VertexShaderGUID;
	uint64_t PixelShaderGUID;
};

#pragma pack(pop)

struct RShaderImage
{
	RPakPtr pData{};

	uint32_t DataSize;
	uint32_t DataSize2;

	RPakPtr pData2{};
};

struct ShaderDataHeader
{
	RPakPtr pData{};

	uint32_t DataSize;
};