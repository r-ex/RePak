#pragma once

#include <pch.h>

// DX Shader Types
enum ShaderType : uint16_t
{
	ComputeShader = 0x4353,
	DomainShader = 0x4453,
	GeometryShader = 0x4753,
	HullShader = 0x4853,
	VertexShader = 0xFFFE,
	PixelShader = 0xFFFF,
};

enum HdrShaderType : uint8_t
{
	Pixel,
	Vertex,
	Geometry,
	Hull,
	Domain,
	Compute
};

struct DXBCHeader
{
	uint32_t Magic;
	uint32_t Checksum[4];
	uint32_t One;
	uint32_t DataSize;
	uint32_t ChunkCount;
};

struct RDefHeader
{
	uint32_t Magic; // RDEF
	uint32_t DataSize;
	uint32_t ConstBufferCount;
	uint32_t ConstBufferOffset;
	uint32_t ResBindingCount;
	uint32_t ResBindingOffset;
	uint8_t  MinorVersion;
	uint8_t  MajorVersion;
	ShaderType ShaderType;
	uint32_t Flags;
	uint32_t CompilerStringOffset;
};

#pragma pack(push, 1)
// shader "DXBC" header
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

	HdrShaderType ShaderType;
	uint8_t unk = 255;
	uint16_t min_widthheight = 1;

	uint16_t max_width = 512;
	uint16_t max_height = 512;

	RPakPtr pIndex0{};

	RPakPtr pTextureSlotData{};
};

struct TextureSlot
{
};

struct ShaderDataHeader
{
	RPakPtr pData{};

	uint32_t DataSize;
};

// --- shds ---
struct ShaderSetHeader {
	uint64_t VTablePadding;

	RPakPtr pName{};

	uint8_t pad_0008[8];        // Dispatcher Context, Some SEH try and catch thing.
	uint16_t Count1;            // TextureInputCount
	uint16_t TextureInputCount = 7;
	uint16_t NumSamplers = 3;   // Used by ID3D11DeviceContext::PSSetSamplers to set NumSamplers
	uint8_t StartSlot;       // Used by ID3D11DeviceContext::PSSetShaderResources to set StartSlot.
	uint8_t NumViews;        // Used by ID3D11DeviceContext::PSSetShaderResources to set NumViews.
	uint8_t Byte1;           //

	uint8_t pad_0021[15]; //0x0021

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
