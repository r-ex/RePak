#pragma once

// dds id
#define DDS_MAGIC ((' '<<24)+('S'<<16)+('D'<<8)+'D') // dds 'magic', placed before 'dwSize'

// Shader structure reference: https://github.com/microsoft/D3D12TranslationLayer/blob/master/DxbcParser/include/BlobContainer.h
// D3D12TranslationLayer is licensed under the MIT License, Copyright (c) Microsoft Corporation.
// Read the license at https://github.com/microsoft/D3D12TranslationLayer/blob/master/LICENSE
#define DXBC_FOURCC_NAME      (('C'<<24)+('B'<<16)+('X'<<8)+'D')
#define DXBC_FOURCC_RDEF_NAME (('F'<<24)+('E'<<16)+('D'<<8)+'R')

// pixelformat flags
#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#define DDPF_YUV 0x200
#define DDPF_LUMINANCE 0x20000

// dds flags
#define DDS_FOURCC DDPF_FOURCC
#define DDS_ALPHA DDPF_ALPHA
#define DDS_RGB DDPF_RGB
#define DDS_LUMINANCE DDPF_LUMINANCE
#define DDS_RGBA DDPF_RGB | DDPF_ALPHAPIXELS

struct DDS_PIXELFORMAT {
	DWORD dwSize;
	DWORD dwFlags;
	uint32_t dwFourCC;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	DWORD dwABitMask;
};

typedef struct {
	DWORD           dwSize;
	DWORD           dwFlags;
	DWORD           dwHeight;
	DWORD           dwWidth;
	DWORD           dwPitchOrLinearSize;
	DWORD           dwDepth;
	DWORD           dwMipMapCount;
	DWORD           dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	DWORD           dwCaps;
	DWORD           dwCaps2;
	DWORD           dwCaps3;
	DWORD           dwCaps4;
	DWORD           dwReserved2;
} DDS_HEADER;

struct DDS_HEADER_DXT10 {
	DXGI_FORMAT              dxgiFormat;
	D3D10_RESOURCE_DIMENSION resourceDimension;
	UINT                     miscFlag;
	UINT                     arraySize;
	UINT                     miscFlags2;
};


// DXBC Types

enum DXShaderType_e : uint16_t
{
	ComputeShader = 0x4353,
	DomainShader = 0x4453,
	GeometryShader = 0x4753,
	HullShader = 0x4853,
	VertexShader = 0xFFFE,
	PixelShader = 0xFFFF,
};

#define DXBC_HASH_SIZE 16
typedef struct DXBCHash
{
	unsigned char Digest[DXBC_HASH_SIZE];
} DXBCHash;

typedef struct DXBCVersion
{
	UINT16 Major;
	UINT16 Minor;
} DXBCVersion;

struct DXBCBlobHeader
{
	uint32_t    BlobFourCC;
	uint32_t    BlobSize;    // Byte count for BlobData

	void* const GetBlobData() const { return reinterpret_cast<void*>((char*)this + sizeof(DXBCBlobHeader)); };
};

typedef struct DXBCHeader
{
	UINT        DXBCHeaderFourCC;
	DXBCHash    Hash;
	DXBCVersion Version;
	UINT32      ContainerSizeInBytes; // Count from start of this header, including all blobs
	UINT32      BlobCount;
	// Structure is followed by UINT32[BlobCount] (the blob index, storing offsets from start of container in bytes 
	//                                             to the start of each blob's header)

	inline bool isValid() const { return DXBCHeaderFourCC == DXBC_FOURCC_NAME ? true : false; }
	uint32_t* const pBlobOffset(uint32_t i) const { assert(i < BlobCount && "index exceeded BlobCount"); return reinterpret_cast<uint32_t*>((char*)this + sizeof(DXBCHeader)) + i; }
	uint32_t BlobOffset(uint32_t i) const { return *pBlobOffset(i); }
	DXBCBlobHeader* const pBlob(uint32_t i) const { return reinterpret_cast<DXBCBlobHeader*>((char*)this + BlobOffset(i)); }

} DXBCHeader;

static const char* s_SRVDimensionNames[] = {
	"UNKNOWN",
	"Buffer",
	"Texture1D",
	"Texture1DArray",
	"Texture2D",
	"Texture2DArray",
	"Texture2DMS",
	"Texture2DMSArray",
	"Texture3D",
	"Texture3DArray",
	"Texture3DCube",
	"Texture3DCubeArray",
	"BufferEx",
};


struct RDefResourceBinding_t
{
	uint32_t NameOffset; // from start of RDEFBlobHeader
	D3D_SHADER_INPUT_TYPE Type;
	D3D_RESOURCE_RETURN_TYPE ReturnType;
	D3D10_SRV_DIMENSION Dimension;
	uint32_t NumSamples;
	uint32_t BindPoint;
	uint32_t BindCount;
	D3D_SHADER_INPUT_FLAGS Flags;

	inline const char* name(void* rdefBlob) const { return reinterpret_cast<const char*>((char*)rdefBlob + NameOffset); }

	inline const char* dimensionName() const { return s_SRVDimensionNames[Dimension]; };
};

struct RDefBlobHeader_t
{
	uint32_t ConstBufferCount;
	uint32_t ConstBufferOffset;
	uint32_t BoundResourceCount;
	uint32_t BoundResourceOffset;
	uint8_t  VersionMinor;
	uint8_t  VersionMajor;
	DXShaderType_e ShaderType;
	uint32_t CompilerFlags;
	uint32_t CreatorOffset; // name of compiler?
	uint32_t ID; // 'RD11'
	uint32_t unk_20[7];

	const RDefResourceBinding_t* const resource(uint32_t i) const { return reinterpret_cast<const RDefResourceBinding_t*>((char*)this + BoundResourceOffset) + i; };
	const char* const compilerName() const { return reinterpret_cast<const char*>(this) + CreatorOffset; };
};


#define SHDR_FOUND_RDEF (1 << 0)
struct ParsedDXShaderData_t
{
	UINT foundFlags;
	int numTextureResources;
	uint8_t pakShaderType; // eShaderType
	uint8_t mtlTexSlotCount; // bind point for the last material-provided texture

	void EnableFlag(UINT flag) { foundFlags |= flag; };
};

class DXUtils
{
public:
	static DXGI_FORMAT GetFormatFromHeader(const DDS_HEADER& hdr);
	static const char* GetFormatAsString(DXGI_FORMAT fmt);

	static bool GetParsedShaderData(const char* bytecode, size_t bytecodeLen, ParsedDXShaderData_t* outData);

private:
	static DXGI_FORMAT GetFormatFromHeaderEx(const DDS_HEADER& hdr);
};
