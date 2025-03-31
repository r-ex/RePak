//=============================================================================//
//
// purpose: Microsoft DirectX utilities
//
//=============================================================================//
#include "pch.h"
#include "dxutils.h"

struct LegacyDDS
{
	DXGI_FORMAT     format;
	DDS_PIXELFORMAT ddpf;
};

const LegacyDDS g_LegacyDDSMap[] =
{
	{ DXGI_FORMAT_BC1_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','1'), 0, 0, 0, 0, 0 } }, // D3DFMT_DXT1
	{ DXGI_FORMAT_BC2_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','3'), 0, 0, 0, 0, 0 } }, // D3DFMT_DXT3
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','5'), 0, 0, 0, 0, 0 } }, // D3DFMT_DXT5

	{ DXGI_FORMAT_BC2_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','2'), 0, 0, 0, 0, 0 } }, // D3DFMT_DXT2
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','4'), 0, 0, 0, 0, 0 } }, // D3DFMT_DXT4

	// These DXT5 variants have various swizzled channels. They are returned 'as is' to the client as BC3.
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', '2', 'D', '5'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('x', 'G', 'B', 'R'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R', 'x', 'B', 'G'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R', 'B', 'x', 'G'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('x', 'R', 'B', 'G'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R', 'G', 'x', 'B'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('x', 'G', 'x', 'R'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('G', 'X', 'R', 'B'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('G', 'R', 'X', 'B'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R', 'X', 'G', 'B'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC3_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B', 'R', 'G', 'X'), 0, 0, 0, 0, 0 } },

	{ DXGI_FORMAT_BC4_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','4','U'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC4_SNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','4','S'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC5_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','5','U'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC5_SNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','5','S'), 0, 0, 0, 0, 0 } },

	{ DXGI_FORMAT_BC4_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '1'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC5_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '2'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC5_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', '2', 'X', 'Y'), 0, 0, 0, 0, 0 } },

	{ DXGI_FORMAT_BC6H_UF16, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B', 'C', '6', 'H'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC7_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B', 'C', '7', 'L'), 0, 0, 0, 0, 0 } },
	{ DXGI_FORMAT_BC7_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B', 'C', '7', '\0'), 0, 0, 0, 0, 0 } },

	{ DXGI_FORMAT_R8G8_B8G8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R','G','B','G'), 0, 0, 0, 0, 0 } }, // D3DFMT_R8G8_B8G8
	{ DXGI_FORMAT_G8R8_G8B8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('G','R','G','B'), 0, 0, 0, 0, 0 } }, // D3DFMT_G8R8_G8B8

	{ DXGI_FORMAT_B8G8R8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 } }, // D3DFMT_A8R8G8B8 (uses DXGI 1.1 format)
	{ DXGI_FORMAT_B8G8R8X8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0 } }, // D3DFMT_X8R8G8B8 (uses DXGI 1.1 format)
	{ DXGI_FORMAT_R8G8B8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 } }, // D3DFMT_A8B8G8R8
	{ DXGI_FORMAT_R8G8B8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0 } }, // D3DFMT_X8B8G8R8
	{ DXGI_FORMAT_R16G16_UNORM,   { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x0000ffff, 0xffff0000, 0, 0 } }, // D3DFMT_G16R16

	{ DXGI_FORMAT_R10G10B10A2_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000 } }, // D3DFMT_A2R10G10B10 (D3DX reversal issue)
	{ DXGI_FORMAT_R10G10B10A2_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 } }, // D3DFMT_A2B10G10R10 (D3DX reversal issue)

	{ DXGI_FORMAT_R8G8B8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0xff0000, 0x00ff00, 0x0000ff, 0 } }, // D3DFMT_R8G8B8

	{ DXGI_FORMAT_B5G6R5_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0 } }, // D3DFMT_R5G6B5
	{ DXGI_FORMAT_B5G5R5A1_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000 } }, // D3DFMT_A1R5G5B5
	{ DXGI_FORMAT_B5G5R5A1_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0 } }, // D3DFMT_X1R5G5B5

	{ DXGI_FORMAT_R8G8B8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00e0, 0x001c, 0x0003, 0xff00 } }, // D3DFMT_A8R3G3B2
	{ DXGI_FORMAT_B5G6R5_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0xe0, 0x1c, 0x03, 0 } }, // D3DFMT_R3G3B2

	{ DXGI_FORMAT_R8_UNORM,   { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 8, 0xff, 0, 0, 0 } }, // D3DFMT_L8
	{ DXGI_FORMAT_R16_UNORM,  { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 16, 0xffff, 0, 0, 0 } }, // D3DFMT_L16
	{ DXGI_FORMAT_R8G8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 16, 0x00ff, 0, 0, 0xff00 } }, // D3DFMT_A8L8
	{ DXGI_FORMAT_R8G8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x00ff, 0, 0, 0xff00 } }, // D3DFMT_A8L8 (alternative bitcount)

	// NVTT v1 wrote these with RGB instead of LUMINANCE
	{ DXGI_FORMAT_R8_UNORM,   { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0xff, 0, 0, 0 } }, // D3DFMT_L8
	{ DXGI_FORMAT_R16_UNORM,  { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0xffff, 0, 0, 0 }  }, // D3DFMT_L16
	{ DXGI_FORMAT_R8G8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00ff, 0, 0, 0xff00 } }, // D3DFMT_A8L8

	{ DXGI_FORMAT_A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_ALPHA, 0, 8, 0, 0, 0, 0xff }   }, // D3DFMT_A8

	{ DXGI_FORMAT_R16G16B16A16_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,   36,  0, 0, 0, 0, 0 } }, // D3DFMT_A16B16G16R16
	{ DXGI_FORMAT_R16G16B16A16_SNORM, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  110,  0, 0, 0, 0, 0 } }, // D3DFMT_Q16W16V16U16
	{ DXGI_FORMAT_R16_FLOAT,          { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  111,  0, 0, 0, 0, 0 } }, // D3DFMT_R16F
	{ DXGI_FORMAT_R16G16_FLOAT,       { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  112,  0, 0, 0, 0, 0 } }, // D3DFMT_G16R16F
	{ DXGI_FORMAT_R16G16B16A16_FLOAT, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  113,  0, 0, 0, 0, 0 } }, // D3DFMT_A16B16G16R16F
	{ DXGI_FORMAT_R32_FLOAT,          { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  114,  0, 0, 0, 0, 0 } }, // D3DFMT_R32F
	{ DXGI_FORMAT_R32G32_FLOAT,       { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  115,  0, 0, 0, 0, 0 } }, // D3DFMT_G32R32F
	{ DXGI_FORMAT_R32G32B32A32_FLOAT, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  116,  0, 0, 0, 0, 0 } }, // D3DFMT_A32B32G32R32F

	{ DXGI_FORMAT_R32_FLOAT, { sizeof(DDS_PIXELFORMAT), DDS_RGB,       0, 32, 0xffffffff, 0, 0, 0 } }, // D3DFMT_R32F (D3DX uses FourCC 114 instead)

	{ DXGI_FORMAT_R8G8B8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_PAL8A,     0, 16, 0, 0, 0, 0xff00 } }, // D3DFMT_A8P8
	{ DXGI_FORMAT_R8G8B8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_PAL8,      0,  8, 0, 0, 0, 0 } }, // D3DFMT_P8

	{ DXGI_FORMAT_B4G4R4A4_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000 } }, // D3DFMT_A4R4G4B4 (uses DXGI 1.2 format)
	{ DXGI_FORMAT_B4G4R4A4_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x0f00, 0x00f0, 0x000f, 0 } }, // D3DFMT_X4R4G4B4 (uses DXGI 1.2 format)
	{ DXGI_FORMAT_B4G4R4A4_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x0f, 0, 0, 0xf0 } }, // D3DFMT_A4L4 (uses DXGI 1.2 format)

	{ DXGI_FORMAT_YUY2, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('Y','U','Y','2'), 0, 0, 0, 0, 0 } }, // D3DFMT_YUY2 (uses DXGI 1.2 format)
	{ DXGI_FORMAT_YUY2, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('U','Y','V','Y'), 0, 0, 0, 0, 0 } }, // D3DFMT_UYVY (uses DXGI 1.2 format)

	{ DXGI_FORMAT_R8G8_SNORM,     { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 16, 0x00ff, 0xff00, 0, 0 } },     // D3DFMT_V8U8
	{ DXGI_FORMAT_R8G8B8A8_SNORM, { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 } }, // D3DFMT_Q8W8V8U8
	{ DXGI_FORMAT_R16G16_SNORM,   { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x0000ffff, 0xffff0000, 0, 0 } },   // D3DFMT_V16U16

	{ DXGI_FORMAT_R8G8B8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_BUMPLUMINANCE, 0, 16, 0x001f, 0x03e0, 0xfc00, 0 } },      // D3DFMT_L6V5U5
	{ DXGI_FORMAT_R8G8B8A8_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_BUMPLUMINANCE, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0 } },    // D3DFMT_X8L8V8U8
	{ DXGI_FORMAT_R10G10B10A2_UNORM, { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDVA, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 } }, // D3DFMT_A2W10V10U10
};

// Note that many common DDS reader/writers (including D3DX) swap the
// the RED/BLUE masks for 10:10:10:2 formats. We assume
// below that the 'backwards' header mask is being used since it is most
// likely written by D3DX. The more robust solution is to use the 'DX10'
// header extension and specify the DXGI_FORMAT_R10G10B10A2_UNORM format directly

// We do not support the following legacy Direct3D 9 formats:
//      D3DFMT_D16_LOCKABLE (DDPF_ZBUFFER: 0x00000400)
//      FourCC 82 D3DFMT_D32F_LOCKABLE
//      FourCC 117 D3DFMT_CxV8U8

// We do not support the following known FourCC codes:
//      FourCC CTX1 (Xbox 360 only)
//      FourCC EAR, EARG, ET2, ET2A (Ericsson Texture Compression)
//      FourCC MET1 (a.k.a. D3DFMT_MULTI2_ARGB8; rarely supported by any hardware)

DXGI_FORMAT MakeSRGB(_In_ DXGI_FORMAT format) noexcept
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM_SRGB;

	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM_SRGB;

	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM_SRGB;

	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

	case DXGI_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM_SRGB;

	default:
		return format;
	}
}

//-----------------------------------------------------------------------------
// purpose: gets the dxgi format from header
// returns: DXGI_FORMAT
// NOTE: this code is taken from texdiag from Microsoft's DirectXTex,
// all code that was unnecessary for repak to function has been commented out.
// Read the license: https://github.com/microsoft/DirectXTex/blob/main/LICENSE
//-----------------------------------------------------------------------------
DXGI_FORMAT DXUtils::GetFormatFromHeader(const DDS_HEADER& hdr) noexcept
{
	const DDS_PIXELFORMAT& ddpf = hdr.ddspf;

	uint32_t ddpfFlags = ddpf.dwFlags;
	if (hdr.dwReserved1[9] == MAKEFOURCC('N', 'V', 'T', 'T'))
	{
		// Clear out non-standard nVidia DDS flags
		ddpfFlags &= ~0xC0000000 /* DDPF_SRGB | DDPF_NORMAL */;
	}

	constexpr size_t MAP_SIZE = sizeof(g_LegacyDDSMap) / sizeof(LegacyDDS);
	size_t index = 0;
	if (ddpf.dwSize == 0 && ddpf.dwFlags == 0 && ddpf.dwFourCC != 0)
	{
		// Handle some DDS files where the DDPF_PIXELFORMAT is mostly zero
		for (index = 0; index < MAP_SIZE; ++index)
		{
			const LegacyDDS* entry = &g_LegacyDDSMap[index];

			if (entry->ddpf.dwFlags & DDS_FOURCC)
			{
				if (ddpf.dwFourCC == entry->ddpf.dwFourCC)
					break;
			}
		}
	}
	else
	{
		for (index = 0; index < MAP_SIZE; ++index)
		{
			const LegacyDDS* entry = &g_LegacyDDSMap[index];

			if ((ddpfFlags & DDS_FOURCC) && (entry->ddpf.dwFlags & DDS_FOURCC))
			{
				// In case of FourCC codes, ignore any other bits in ddpf.dwFlags
				if (ddpf.dwFourCC == entry->ddpf.dwFourCC)
					break;
			}
			else if ((ddpfFlags == entry->ddpf.dwFlags) && (ddpf.dwRGBBitCount == entry->ddpf.dwRGBBitCount))
			{
				if (entry->ddpf.dwFlags & DDS_PAL8)
				{
					// PAL8 / PAL8A
					break;
				}
				else if (entry->ddpf.dwFlags & DDS_ALPHA)
				{
					if (ddpf.dwABitMask == entry->ddpf.dwABitMask)
						break;
				}
				else if (entry->ddpf.dwFlags & DDS_LUMINANCE)
				{
					if (entry->ddpf.dwFlags & DDS_ALPHAPIXELS)
					{
						// LUMINANCEA
						if (ddpf.dwRBitMask == entry->ddpf.dwRBitMask
							&& ddpf.dwABitMask == entry->ddpf.dwABitMask)
							break;
					}
					else
					{
						// LUMINANCE
						if (ddpf.dwRBitMask == entry->ddpf.dwRBitMask)
							break;
					}
				}
				else if (entry->ddpf.dwFlags & DDS_BUMPDUDV)
				{
					if (entry->ddpf.dwFlags & DDS_ALPHAPIXELS)
					{
						// BUMPDUDVA
						if (ddpf.dwRBitMask == entry->ddpf.dwRBitMask
							&& ddpf.dwABitMask == entry->ddpf.dwABitMask)
						{
							//flags &= ~DDS_FLAGS_NO_R10B10G10A2_FIXUP;
							break;
						}
					}
					else
					{
						// BUMPDUDV
						if (ddpf.dwRBitMask == entry->ddpf.dwRBitMask)
							break;
					}
				}
				else if (entry->ddpf.dwFlags & DDS_ALPHAPIXELS)
				{
					// RGBA
					if (ddpf.dwRBitMask == entry->ddpf.dwRBitMask
						&& ddpf.dwGBitMask == entry->ddpf.dwGBitMask
						&& ddpf.dwBBitMask == entry->ddpf.dwBBitMask
						&& ddpf.dwABitMask == entry->ddpf.dwABitMask)
						break;
				}
				else
				{
					// RGB
					if (ddpf.dwRBitMask == entry->ddpf.dwRBitMask
						&& ddpf.dwGBitMask == entry->ddpf.dwGBitMask
						&& ddpf.dwBBitMask == entry->ddpf.dwBBitMask)
						break;
				}
			}
		}
	}

	if (index >= MAP_SIZE)
		return DXGI_FORMAT_UNKNOWN;

	DXGI_FORMAT format = g_LegacyDDSMap[index].format;

	//if ((cflags & CONV_FLAGS_EXPAND) && (flags & DDS_FLAGS_NO_LEGACY_EXPANSION))
	//	return DXGI_FORMAT_UNKNOWN;

	//if ((format == DXGI_FORMAT_R10G10B10A2_UNORM) && (flags & DDS_FLAGS_NO_R10B10G10A2_FIXUP))
	//{
	//	cflags ^= CONV_FLAGS_SWIZZLE;
	//}

	if ((hdr.dwReserved1[9] == MAKEFOURCC('N', 'V', 'T', 'T'))
		&& (ddpf.dwFlags & 0x40000000 /* DDPF_SRGB */))
	{
		format = MakeSRGB(format);
	}

	//convFlags = cflags;

	return format;
}

//-----------------------------------------------------------------------------
// purpose: gets the dxgi format as string
// returns: dxgi format as const char*
//-----------------------------------------------------------------------------
const char* DXUtils::GetFormatAsString(DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_UNKNOWN: return "DXGI_FORMAT_UNKNOWN";
	case DXGI_FORMAT_R32G32B32A32_TYPELESS: return "DXGI_FORMAT_R32G32B32A32_TYPELESS";
	case DXGI_FORMAT_R32G32B32A32_FLOAT: return "DXGI_FORMAT_R32G32B32A32_FLOAT";
	case DXGI_FORMAT_R32G32B32A32_UINT: return "DXGI_FORMAT_R32G32B32A32_UINT";
	case DXGI_FORMAT_R32G32B32A32_SINT: return "DXGI_FORMAT_R32G32B32A32_SINT";
	case DXGI_FORMAT_R32G32B32_TYPELESS: return "DXGI_FORMAT_R32G32B32_TYPELESS";
	case DXGI_FORMAT_R32G32B32_FLOAT: return "DXGI_FORMAT_R32G32B32_FLOAT";
	case DXGI_FORMAT_R32G32B32_UINT: return "DXGI_FORMAT_R32G32B32_UINT";
	case DXGI_FORMAT_R32G32B32_SINT: return "DXGI_FORMAT_R32G32B32_SINT";
	case DXGI_FORMAT_R16G16B16A16_TYPELESS: return "DXGI_FORMAT_R16G16B16A16_TYPELESS";
	case DXGI_FORMAT_R16G16B16A16_FLOAT: return "DXGI_FORMAT_R16G16B16A16_FLOAT";
	case DXGI_FORMAT_R16G16B16A16_UNORM: return "DXGI_FORMAT_R16G16B16A16_UNORM";
	case DXGI_FORMAT_R16G16B16A16_UINT: return "DXGI_FORMAT_R16G16B16A16_UINT";
	case DXGI_FORMAT_R16G16B16A16_SNORM: return "DXGI_FORMAT_R16G16B16A16_SNORM";
	case DXGI_FORMAT_R16G16B16A16_SINT: return "DXGI_FORMAT_R16G16B16A16_SINT";
	case DXGI_FORMAT_R32G32_TYPELESS: return "DXGI_FORMAT_R32G32_TYPELESS";
	case DXGI_FORMAT_R32G32_FLOAT: return "DXGI_FORMAT_R32G32_FLOAT";
	case DXGI_FORMAT_R32G32_UINT: return "DXGI_FORMAT_R32G32_UINT";
	case DXGI_FORMAT_R32G32_SINT: return "DXGI_FORMAT_R32G32_SINT";
	case DXGI_FORMAT_R32G8X24_TYPELESS: return "DXGI_FORMAT_R32G8X24_TYPELESS";
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return "DXGI_FORMAT_D32_FLOAT_S8X24_UINT";
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: return "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS";
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT";
	case DXGI_FORMAT_R10G10B10A2_TYPELESS: return "DXGI_FORMAT_R10G10B10A2_TYPELESS";
	case DXGI_FORMAT_R10G10B10A2_UNORM: return "DXGI_FORMAT_R10G10B10A2_UNORM";
	case DXGI_FORMAT_R10G10B10A2_UINT: return "DXGI_FORMAT_R10G10B10A2_UINT";
	case DXGI_FORMAT_R11G11B10_FLOAT: return "DXGI_FORMAT_R11G11B10_FLOAT";
	case DXGI_FORMAT_R8G8B8A8_TYPELESS: return "DXGI_FORMAT_R8G8B8A8_TYPELESS";
	case DXGI_FORMAT_R8G8B8A8_UNORM: return "DXGI_FORMAT_R8G8B8A8_UNORM";
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
	case DXGI_FORMAT_R8G8B8A8_UINT: return "DXGI_FORMAT_R8G8B8A8_UINT";
	case DXGI_FORMAT_R8G8B8A8_SNORM: return "DXGI_FORMAT_R8G8B8A8_SNORM";
	case DXGI_FORMAT_R8G8B8A8_SINT: return "DXGI_FORMAT_R8G8B8A8_SINT";
	case DXGI_FORMAT_R16G16_TYPELESS: return "DXGI_FORMAT_R16G16_TYPELESS";
	case DXGI_FORMAT_R16G16_FLOAT: return "DXGI_FORMAT_R16G16_FLOAT";
	case DXGI_FORMAT_R16G16_UNORM: return "DXGI_FORMAT_R16G16_UNORM";
	case DXGI_FORMAT_R16G16_UINT: return "DXGI_FORMAT_R16G16_UINT";
	case DXGI_FORMAT_R16G16_SNORM: return "DXGI_FORMAT_R16G16_SNORM";
	case DXGI_FORMAT_R16G16_SINT: return "DXGI_FORMAT_R16G16_SINT";
	case DXGI_FORMAT_R32_TYPELESS: return "DXGI_FORMAT_R32_TYPELESS";
	case DXGI_FORMAT_D32_FLOAT: return "DXGI_FORMAT_D32_FLOAT";
	case DXGI_FORMAT_R32_FLOAT: return "DXGI_FORMAT_R32_FLOAT";
	case DXGI_FORMAT_R32_UINT: return "DXGI_FORMAT_R32_UINT";
	case DXGI_FORMAT_R32_SINT: return "DXGI_FORMAT_R32_SINT";
	case DXGI_FORMAT_R24G8_TYPELESS: return "DXGI_FORMAT_R24G8_TYPELESS";
	case DXGI_FORMAT_D24_UNORM_S8_UINT: return "DXGI_FORMAT_D24_UNORM_S8_UINT";
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: return "DXGI_FORMAT_R24_UNORM_X8_TYPELESS";
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return "DXGI_FORMAT_X24_TYPELESS_G8_UINT";
	case DXGI_FORMAT_R8G8_TYPELESS: return "DXGI_FORMAT_R8G8_TYPELESS";
	case DXGI_FORMAT_R8G8_UNORM: return "DXGI_FORMAT_R8G8_UNORM";
	case DXGI_FORMAT_R8G8_UINT: return "DXGI_FORMAT_R8G8_UINT";
	case DXGI_FORMAT_R8G8_SNORM: return "DXGI_FORMAT_R8G8_SNORM";
	case DXGI_FORMAT_R8G8_SINT: return "DXGI_FORMAT_R8G8_SINT";
	case DXGI_FORMAT_R16_TYPELESS: return "DXGI_FORMAT_R16_TYPELESS";
	case DXGI_FORMAT_R16_FLOAT: return "DXGI_FORMAT_R16_FLOAT";
	case DXGI_FORMAT_D16_UNORM: return "DXGI_FORMAT_D16_UNORM";
	case DXGI_FORMAT_R16_UNORM: return "DXGI_FORMAT_R16_UNORM";
	case DXGI_FORMAT_R16_UINT: return "DXGI_FORMAT_R16_UINT";
	case DXGI_FORMAT_R16_SNORM: return "DXGI_FORMAT_R16_SNORM";
	case DXGI_FORMAT_R16_SINT: return "DXGI_FORMAT_R16_SINT";
	case DXGI_FORMAT_R8_TYPELESS: return "DXGI_FORMAT_R8_TYPELESS";
	case DXGI_FORMAT_R8_UNORM: return "DXGI_FORMAT_R8_UNORM";
	case DXGI_FORMAT_R8_UINT: return "DXGI_FORMAT_R8_UINT";
	case DXGI_FORMAT_R8_SNORM: return "DXGI_FORMAT_R8_SNORM";
	case DXGI_FORMAT_R8_SINT: return "DXGI_FORMAT_R8_SINT";
	case DXGI_FORMAT_A8_UNORM: return "DXGI_FORMAT_A8_UNORM";
	case DXGI_FORMAT_R1_UNORM: return "DXGI_FORMAT_R1_UNORM";
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: return "DXGI_FORMAT_R9G9B9E5_SHAREDEXP";
	case DXGI_FORMAT_R8G8_B8G8_UNORM: return "DXGI_FORMAT_R8G8_B8G8_UNORM";
	case DXGI_FORMAT_G8R8_G8B8_UNORM: return "DXGI_FORMAT_G8R8_G8B8_UNORM";
	case DXGI_FORMAT_BC1_TYPELESS: return "DXGI_FORMAT_BC1_TYPELESS";
	case DXGI_FORMAT_BC1_UNORM: return "DXGI_FORMAT_BC1_UNORM";
	case DXGI_FORMAT_BC1_UNORM_SRGB: return "DXGI_FORMAT_BC1_UNORM_SRGB";
	case DXGI_FORMAT_BC2_TYPELESS: return "DXGI_FORMAT_BC2_TYPELESS";
	case DXGI_FORMAT_BC2_UNORM: return "DXGI_FORMAT_BC2_UNORM";
	case DXGI_FORMAT_BC2_UNORM_SRGB: return "DXGI_FORMAT_BC2_UNORM_SRGB";
	case DXGI_FORMAT_BC3_TYPELESS: return "DXGI_FORMAT_BC3_TYPELESS";
	case DXGI_FORMAT_BC3_UNORM: return "DXGI_FORMAT_BC3_UNORM";
	case DXGI_FORMAT_BC3_UNORM_SRGB: return "DXGI_FORMAT_BC3_UNORM_SRGB";
	case DXGI_FORMAT_BC4_TYPELESS: return "DXGI_FORMAT_BC4_TYPELESS";
	case DXGI_FORMAT_BC4_UNORM: return "DXGI_FORMAT_BC4_UNORM";
	case DXGI_FORMAT_BC4_SNORM: return "DXGI_FORMAT_BC4_SNORM";
	case DXGI_FORMAT_BC5_TYPELESS: return "DXGI_FORMAT_BC5_TYPELESS";
	case DXGI_FORMAT_BC5_UNORM: return "DXGI_FORMAT_BC5_UNORM";
	case DXGI_FORMAT_BC5_SNORM: return "DXGI_FORMAT_BC5_SNORM";
	case DXGI_FORMAT_B5G6R5_UNORM: return "DXGI_FORMAT_B5G6R5_UNORM";
	case DXGI_FORMAT_B5G5R5A1_UNORM: return "DXGI_FORMAT_B5G5R5A1_UNORM";
	case DXGI_FORMAT_B8G8R8A8_UNORM: return "DXGI_FORMAT_B8G8R8A8_UNORM";
	case DXGI_FORMAT_B8G8R8X8_UNORM: return "DXGI_FORMAT_B8G8R8X8_UNORM";
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: return "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM";
	case DXGI_FORMAT_B8G8R8A8_TYPELESS: return "DXGI_FORMAT_B8G8R8A8_TYPELESS";
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB";
	case DXGI_FORMAT_B8G8R8X8_TYPELESS: return "DXGI_FORMAT_B8G8R8X8_TYPELESS";
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB";
	case DXGI_FORMAT_BC6H_TYPELESS: return "DXGI_FORMAT_BC6H_TYPELESS";
	case DXGI_FORMAT_BC6H_UF16: return "DXGI_FORMAT_BC6H_UF16";
	case DXGI_FORMAT_BC6H_SF16: return "DXGI_FORMAT_BC6H_SF16";
	case DXGI_FORMAT_BC7_TYPELESS: return "DXGI_FORMAT_BC7_TYPELESS";
	case DXGI_FORMAT_BC7_UNORM: return "DXGI_FORMAT_BC7_UNORM";
	case DXGI_FORMAT_BC7_UNORM_SRGB: return "DXGI_FORMAT_BC7_UNORM_SRGB";
	case DXGI_FORMAT_AYUV: return "DXGI_FORMAT_AYUV";
	case DXGI_FORMAT_Y410: return "DXGI_FORMAT_Y410";
	case DXGI_FORMAT_Y416: return "DXGI_FORMAT_Y416";
	case DXGI_FORMAT_NV12: return "DXGI_FORMAT_NV12";
	case DXGI_FORMAT_P010: return "DXGI_FORMAT_P010";
	case DXGI_FORMAT_P016: return "DXGI_FORMAT_P016";
	case DXGI_FORMAT_420_OPAQUE: return "DXGI_FORMAT_420_OPAQUE";
	case DXGI_FORMAT_YUY2: return "DXGI_FORMAT_YUY2";
	case DXGI_FORMAT_Y210: return "DXGI_FORMAT_Y210";
	case DXGI_FORMAT_Y216: return "DXGI_FORMAT_Y216";
	case DXGI_FORMAT_NV11: return "DXGI_FORMAT_NV11";
	case DXGI_FORMAT_AI44: return "DXGI_FORMAT_AI44";
	case DXGI_FORMAT_IA44: return "DXGI_FORMAT_IA44";
	case DXGI_FORMAT_P8: return "DXGI_FORMAT_P8";
	case DXGI_FORMAT_A8P8: return "DXGI_FORMAT_A8P8";
	case DXGI_FORMAT_B4G4R4A4_UNORM: return "DXGI_FORMAT_B4G4R4A4_UNORM";
	case DXGI_FORMAT_P208: return "DXGI_FORMAT_P208";
	case DXGI_FORMAT_V208: return "DXGI_FORMAT_V208";
	case DXGI_FORMAT_V408: return "DXGI_FORMAT_V408";
	case DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE: return "DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE";
	case DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE: return "DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE";
	case DXGI_FORMAT_FORCE_UINT: return "DXGI_FORMAT_FORCE_UINT";
	default: return "DXGI_FORMAT_UNKNOWN";
	}
}

// See copyright and license notice in dxutils.h
bool DXUtils::GetParsedShaderData(const char* bytecode, size_t /*bytecodeLen*/, ParsedDXShaderData_t* outData)
{
	const DXBCHeader* fileHeader = reinterpret_cast<const DXBCHeader*>(bytecode);

	if (fileHeader->DXBCHeaderFourCC != DXBC_FOURCC_NAME)
		return false;

	if (!outData)
		return false;

	outData->numTextureResources = 0;
	outData->mtlTexSlotCount = 0;
	for (uint32_t i = 0; i < fileHeader->BlobCount; ++i)
	{
		const DXBCBlobHeader* blob = fileHeader->pBlob(i);

		if (blob->BlobFourCC == DXBC_FOURCC_RDEF_NAME)
		{
			outData->EnableFlag(SHDR_FOUND_RDEF);

			const RDefBlobHeader_t* rdef = reinterpret_cast<const RDefBlobHeader_t*>(blob->GetBlobData());
			Debug("Shader built by \"%s\".\n", rdef->compilerName());

			switch (rdef->ShaderType)
			{
			case PixelShader:
				outData->pakShaderType = 0; // eShaderType::Pixel
				break;
			case VertexShader:
				outData->pakShaderType = 1; // eShaderType::Vertex
				break;
			case GeometryShader:
				outData->pakShaderType = 2; // eShaderType::Geometry
				break;
			case HullShader:
				outData->pakShaderType = 3; // eShaderType::Hull
				break;
			case DomainShader:
				outData->pakShaderType = 4; // eShaderType::Domain
				break;
			case ComputeShader:
				outData->pakShaderType = 5; // eShaderType::Compute
				break;
			default:
				Error("Unknown shader type: %X.\n", rdef->ShaderType);
				break;
			}

			for (uint32_t j = 0; j < rdef->BoundResourceCount; ++j)
			{
				const RDefResourceBinding_t* resource = rdef->resource(j);
				//printf("%s %s (%X).\n", resource->dimensionName(), resource->name(blob->GetBlobData()), resource->Flags);

				if (resource->Type == D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE)
				{
					outData->numTextureResources++;

					if (resource->BindPoint < 40)
						outData->mtlTexSlotCount = static_cast<uint8_t>(resource->BindPoint) + 1;
				}

				if (outData->numTextureResources > 0 && resource->Type != D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE)
					break;
			}
		}
	}

	return true;
}