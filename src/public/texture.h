#pragma once

#define MAX_STREAMED_TEXTURE_MIPS 4
#define MAX_MIPS_PER_TEXTURE 13 // max total mips per texture

#define MAX_PERM_MIP_SIZE	0x3FFF // "Any MIP below 64kiB is permanent."
#define MAX_STREAM_MIP_SIZE	0xFFFFF

#define TEXTURE_INVALID_FORMAT_INDEX 0xFFFF

struct TextureBytesPerPixel_t
{
	uint8_t x;
	uint8_t y;
};

static inline const TextureBytesPerPixel_t s_pBytesPerPixel[] =
{
  { uint8_t(8u),  uint8_t(4u) },
  { uint8_t(8u),  uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(8u),  uint8_t(4u) },
  { uint8_t(8u),  uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(4u) },
  { uint8_t(16u), uint8_t(1u) },
  { uint8_t(16u), uint8_t(1u) },
  { uint8_t(16u), uint8_t(1u) },
  { uint8_t(12u), uint8_t(1u) },
  { uint8_t(12u), uint8_t(1u) },
  { uint8_t(12u), uint8_t(1u) },
  { uint8_t(8u),  uint8_t(1u) },
  { uint8_t(8u),  uint8_t(1u) },
  { uint8_t(8u),  uint8_t(1u) },
  { uint8_t(8u),  uint8_t(1u) },
  { uint8_t(8u),  uint8_t(1u) },
  { uint8_t(8u),  uint8_t(1u) },
  { uint8_t(8u),  uint8_t(1u) },
  { uint8_t(8u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
  { uint8_t(1u),  uint8_t(1u) },
  { uint8_t(1u),  uint8_t(1u) },
  { uint8_t(1u),  uint8_t(1u) },
  { uint8_t(1u),  uint8_t(1u) },
  { uint8_t(1u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(4u),  uint8_t(1u) },
  { uint8_t(2u),  uint8_t(1u) },
};

enum class mipType_e : char
{
	INVALID = -1,
	STATIC = 0,
	STREAMED,
	STREAMED_OPT,
	_COUNT
};

struct mipLevel_t
{
	size_t mipOffset; // offset into dds
	size_t mipSize;
	size_t mipSizeAligned; // aligned for rpak
	unsigned short mipWidth;
	unsigned short mipHeight;
	unsigned char mipLevel;
	mipType_e mipType;
};

struct TextureDesc_t
{
	PakGuid_t guid;
	PagePtr_t name;
	uint16_t width;
	uint16_t height;
	uint16_t depth;
	uint16_t imageFormat;
};

struct TextureAssetHeader_t : public TextureDesc_t
{
	uint32_t dataSize; // total data size across all sources
	uint8_t  compressionType; // 0 PC, 8 PS4, 9 Switch
	uint8_t  optStreamedMipLevels; // why is this here and not below? respawn moment

	// d3d11 texture desc params
	uint8_t  arraySize;
	uint8_t  layerCount;

	uint8_t  usageFlags;
	uint8_t  mipLevels;
	uint8_t  streamedMipLevels;

	// info about the first or last mip isn't stored? this array is always
	// populated up to total mip count -1, but it appears unused by the
	// runtime. possibly internal tools only.
	uint8_t unkPerMip[MAX_MIPS_PER_TEXTURE-1];
	int64_t numPixels; // reserved for the runtime.
};

struct UIImageAtlasHeader_t
{
	float widthRatio; // 1 / m_nWidth
	float heightRatio; // 1 / m_nHeight

	uint16_t width = 1;
	uint16_t height = 1;

	uint16_t imageCount = 0;
	uint16_t unkCount = 0;

	PagePtr_t pImageOffsets{};
	PagePtr_t pImageDimensions{};

	PagePtr_t unknown{}; // something with UV's?

	PagePtr_t pImageHashes{};
	PagePtr_t pImagesNames{};

	PakGuid_t atlasGUID = 0;
};

struct UIImageUV
{
	void InitUIImageUV(float startX, float startY, float width, float height)
	{
		this->uv0x = startX;
		this->uv1x = width;
		this->uv0y = startY;
		this->uv1y = height;
	}
	// maybe the uv coords for top left?
	// just leave these as 0 and it should be fine
	float uv0x = 0;
	float uv0y = 0;

	// these two seem to be the uv coords for the bottom right corner
	// examples:
	// uv1x = 10;
	// | | | | | | | | | |
	// uv1x = 5;
	// | | | | |
	float uv1x = 1.f;
	float uv1y = 1.f;
};

// examples of changes from these values: https://imgur.com/a/l1YDXaz
struct UIImageOffset
{
	void InitUIImageOffset(float flStartX, float flStartY, float flEndX, float flEndY)
	{
		this->startX = flStartX;
		this->startY = flStartY;
		this->endX = flEndX;
		this->endY = flEndY;
		//this->unkX = 1 - 2 * startX; // doesn't seem to always 100% of the time match up but its very close
		//this->unkY = 1 - 2 * startY;
	}
	// these don't seem to matter all that much as long as they are a valid float number
	float f0 = 0.f;
	float f1 = 0.f;

	// endX and endY define where the edge of the image is, with 1.f being the full length of the image and 0.5f being half of the image
	float endX = 1.f;
	float endY = 1.f;

	// startX and startY define where the top left corner is in proportion to the full image dimensions
	float startX = 0.f;
	float startY = 0.f;

	// changing these 2 values causes the image to be distorted on each axis
	float unkX = 1.f;
	float unkY = 1.f;
};

// map of dxgi format to the corresponding txtr asset format value
static inline uint16_t Texture_DXGIToImageFormat(const uint32_t format)
{
	switch (format)
	{
	case DXGI_FORMAT_BC1_UNORM:                  return uint16_t(0);
	case DXGI_FORMAT_BC1_UNORM_SRGB:             return uint16_t(1);
	case DXGI_FORMAT_BC2_UNORM:                  return uint16_t(2);
	case DXGI_FORMAT_BC2_UNORM_SRGB:             return uint16_t(3);
	case DXGI_FORMAT_BC3_UNORM:                  return uint16_t(4);
	case DXGI_FORMAT_BC3_UNORM_SRGB:             return uint16_t(5);
	case DXGI_FORMAT_BC4_UNORM:                  return uint16_t(6);
	case DXGI_FORMAT_BC4_SNORM:                  return uint16_t(7);
	case DXGI_FORMAT_BC5_UNORM:                  return uint16_t(8);
	case DXGI_FORMAT_BC5_SNORM:                  return uint16_t(9);
	case DXGI_FORMAT_BC6H_UF16:                  return uint16_t(10);
	case DXGI_FORMAT_BC6H_SF16:                  return uint16_t(11);
	case DXGI_FORMAT_BC7_UNORM:                  return uint16_t(12);
	case DXGI_FORMAT_BC7_UNORM_SRGB:             return uint16_t(13);
	case DXGI_FORMAT_R32G32B32A32_FLOAT:         return uint16_t(14);
	case DXGI_FORMAT_R32G32B32A32_UINT:          return uint16_t(15);
	case DXGI_FORMAT_R32G32B32A32_SINT:          return uint16_t(16);
	case DXGI_FORMAT_R32G32B32_FLOAT:            return uint16_t(17);
	case DXGI_FORMAT_R32G32B32_UINT:             return uint16_t(18);
	case DXGI_FORMAT_R32G32B32_SINT:             return uint16_t(19);
	case DXGI_FORMAT_R16G16B16A16_FLOAT:         return uint16_t(20);
	case DXGI_FORMAT_R16G16B16A16_UNORM:         return uint16_t(21);
	case DXGI_FORMAT_R16G16B16A16_UINT:          return uint16_t(22);
	case DXGI_FORMAT_R16G16B16A16_SNORM:         return uint16_t(23);
	case DXGI_FORMAT_R16G16B16A16_SINT:          return uint16_t(24);
	case DXGI_FORMAT_R32G32_FLOAT:               return uint16_t(25);
	case DXGI_FORMAT_R32G32_UINT:                return uint16_t(26);
	case DXGI_FORMAT_R32G32_SINT:                return uint16_t(27);
	case DXGI_FORMAT_R10G10B10A2_UNORM:          return uint16_t(28);
	case DXGI_FORMAT_R10G10B10A2_UINT:           return uint16_t(29);
	case DXGI_FORMAT_R11G11B10_FLOAT:            return uint16_t(30);
	case DXGI_FORMAT_R8G8B8A8_UNORM:             return uint16_t(31);
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:        return uint16_t(32);
	case DXGI_FORMAT_R8G8B8A8_UINT:              return uint16_t(33);
	case DXGI_FORMAT_R8G8B8A8_SNORM:             return uint16_t(34);
	case DXGI_FORMAT_R8G8B8A8_SINT:              return uint16_t(35);
	case DXGI_FORMAT_R16G16_FLOAT:               return uint16_t(36);
	case DXGI_FORMAT_R16G16_UNORM:               return uint16_t(37);
	case DXGI_FORMAT_R16G16_UINT:                return uint16_t(38);
	case DXGI_FORMAT_R16G16_SNORM:               return uint16_t(39);
	case DXGI_FORMAT_R16G16_SINT:                return uint16_t(40);
	case DXGI_FORMAT_R32_FLOAT:                  return uint16_t(41);
	case DXGI_FORMAT_R32_UINT:                   return uint16_t(42);
	case DXGI_FORMAT_R32_SINT:                   return uint16_t(43);
	case DXGI_FORMAT_R8G8_UNORM:                 return uint16_t(44);
	case DXGI_FORMAT_R8G8_UINT:                  return uint16_t(45);
	case DXGI_FORMAT_R8G8_SNORM:                 return uint16_t(46);
	case DXGI_FORMAT_R8G8_SINT:                  return uint16_t(47);
	case DXGI_FORMAT_R16_FLOAT:                  return uint16_t(48);
	case DXGI_FORMAT_R16_UNORM:                  return uint16_t(49);
	case DXGI_FORMAT_R16_UINT:                   return uint16_t(50);
	case DXGI_FORMAT_R16_SNORM:                  return uint16_t(51);
	case DXGI_FORMAT_R16_SINT:                   return uint16_t(52);
	case DXGI_FORMAT_R8_UNORM:                   return uint16_t(53);
	case DXGI_FORMAT_R8_UINT:                    return uint16_t(54);
	case DXGI_FORMAT_R8_SNORM:                   return uint16_t(55);
	case DXGI_FORMAT_R8_SINT:                    return uint16_t(56);
	case DXGI_FORMAT_A8_UNORM:                   return uint16_t(57);
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:         return uint16_t(58);
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: return uint16_t(59);
	case DXGI_FORMAT_D32_FLOAT:                  return uint16_t(60);
	case DXGI_FORMAT_D16_UNORM:                  return uint16_t(61);

	default: return TEXTURE_INVALID_FORMAT_INDEX;
	}
};
