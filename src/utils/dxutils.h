#pragma once

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


class DXUtils
{
public:
	static DXGI_FORMAT GetFormatFromHeader(const DDS_HEADER& hdr);
	static const char* GetFormatAsString(DXGI_FORMAT fmt);

private:
	static DXGI_FORMAT GetFormatFromHeaderEx(const DDS_HEADER& hdr);
};
