#pragma once

#define MAX_PERM_MIP_SIZE	0x3FFF // "Any MIP below 64kiB is permanent."
#define MAX_STREAM_MIP_SIZE	0x100000

static const std::pair<uint8_t, uint8_t> s_pBytesPerPixel[] =
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
  { uint8_t(0u),  uint8_t(0u) },
  { uint8_t(0u),  uint8_t(0u) },
  { uint8_t(5u),  uint8_t(0u) },
  { uint8_t(0u),  uint8_t(0u) },
  { uint8_t(5u),  uint8_t(0u) },
  { uint8_t(0u),  uint8_t(0u) },
  { uint8_t(1u),  uint8_t(0u) },
  { uint8_t(0u),  uint8_t(0u) },
  { uint8_t(2u),  uint8_t(0u) },
  { uint8_t(0u),  uint8_t(0u) },
  { uint8_t(0u),  uint8_t(0u) },
  { uint8_t(0u),  uint8_t(0u) },
  { uint8_t(1u),  uint8_t(0u) },
  { uint8_t(0u),  uint8_t(0u) }
};

enum mipType_t : unsigned char
{
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
	mipType_t mipType;
};

#pragma pack(push, 1)
struct TextureAssetHeader_t
{
	uint64_t guid = 0;
	PagePtr_t  pName;

	uint16_t width = 0;
	uint16_t height = 0;

	uint16_t unk0 = 0;
	uint16_t imgFormat = 0;

	uint32_t dataSize; // total data size across all sources
	uint8_t  unk1;
	uint8_t  optStreamedMipLevels; // why is this here and not below? respawn moment

	// d3d11 texture desc params
	uint8_t  arraySize;
	uint8_t  layerCount;

	uint8_t  unk2;
	uint8_t  mipLevels;
	uint8_t  streamedMipLevels;
	uint8_t  unk3[0x15];
};

struct UIImageAtlasHeader_t
{
	float widthRatio; // 1 / m_nWidth
	float heightRatio; // 1 / m_nHeight
	uint16_t width = 1;
	uint16_t height = 1;
	uint16_t textureCount = 0;
	uint16_t unkCount = 0;
	PagePtr_t pTextureOffsets{};
	PagePtr_t pTextureDimensions{};
	uint32_t unk1 = 0;
	uint32_t unk2 = 0;
	PagePtr_t pTextureHashes{};
	PagePtr_t pTextureNames{};
	uint64_t atlasGUID = 0;
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
#pragma pack(pop)

// map of dxgi format to the corresponding txtr asset format value
static const std::map<DXGI_FORMAT, uint16_t> s_txtrFormatMap{
	{ DXGI_FORMAT_BC1_UNORM,                  uint16_t(0) },
	{ DXGI_FORMAT_BC1_UNORM_SRGB,             uint16_t(1) },
	{ DXGI_FORMAT_BC2_UNORM,                  uint16_t(2) },
	{ DXGI_FORMAT_BC2_UNORM_SRGB,             uint16_t(3) },
	{ DXGI_FORMAT_BC3_UNORM,                  uint16_t(4) },
	{ DXGI_FORMAT_BC3_UNORM_SRGB,             uint16_t(5) },
	{ DXGI_FORMAT_BC4_UNORM,                  uint16_t(6) },
	{ DXGI_FORMAT_BC4_SNORM,                  uint16_t(7) },
	{ DXGI_FORMAT_BC5_UNORM,                  uint16_t(8) },
	{ DXGI_FORMAT_BC5_SNORM,                  uint16_t(9) },
	{ DXGI_FORMAT_BC6H_UF16,                  uint16_t(10) },
	{ DXGI_FORMAT_BC6H_SF16,                  uint16_t(11) },
	{ DXGI_FORMAT_BC7_UNORM,                  uint16_t(12) },
	{ DXGI_FORMAT_BC7_UNORM_SRGB,             uint16_t(13) },
	{ DXGI_FORMAT_R32G32B32A32_FLOAT,         uint16_t(14) },
	{ DXGI_FORMAT_R32G32B32A32_UINT,          uint16_t(15) },
	{ DXGI_FORMAT_R32G32B32A32_SINT,          uint16_t(16) },
	{ DXGI_FORMAT_R32G32B32_FLOAT,            uint16_t(17) },
	{ DXGI_FORMAT_R32G32B32_UINT,             uint16_t(18) },
	{ DXGI_FORMAT_R32G32B32_SINT,             uint16_t(19) },
	{ DXGI_FORMAT_R16G16B16A16_FLOAT,         uint16_t(20) },
	{ DXGI_FORMAT_R16G16B16A16_UNORM,         uint16_t(21) },
	{ DXGI_FORMAT_R16G16B16A16_UINT,          uint16_t(22) },
	{ DXGI_FORMAT_R16G16B16A16_SNORM,         uint16_t(23) },
	{ DXGI_FORMAT_R16G16B16A16_SINT,          uint16_t(24) },
	{ DXGI_FORMAT_R32G32_FLOAT,               uint16_t(25) },
	{ DXGI_FORMAT_R32G32_UINT,                uint16_t(26) },
	{ DXGI_FORMAT_R32G32_SINT,                uint16_t(27) },
	{ DXGI_FORMAT_R10G10B10A2_UNORM,          uint16_t(28) },
	{ DXGI_FORMAT_R10G10B10A2_UINT,           uint16_t(29) },
	{ DXGI_FORMAT_R11G11B10_FLOAT,            uint16_t(30) },
	{ DXGI_FORMAT_R8G8B8A8_UNORM,             uint16_t(31) },
	{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,        uint16_t(32) },
	{ DXGI_FORMAT_R8G8B8A8_UINT,              uint16_t(33) },
	{ DXGI_FORMAT_R8G8B8A8_SNORM,             uint16_t(34) },
	{ DXGI_FORMAT_R8G8B8A8_SINT,              uint16_t(35) },
	{ DXGI_FORMAT_R16G16_FLOAT,               uint16_t(36) },
	{ DXGI_FORMAT_R16G16_UNORM,               uint16_t(37) },
	{ DXGI_FORMAT_R16G16_UINT,                uint16_t(38) },
	{ DXGI_FORMAT_R16G16_SNORM,               uint16_t(39) },
	{ DXGI_FORMAT_R16G16_SINT,                uint16_t(40) },
	{ DXGI_FORMAT_R32_FLOAT,                  uint16_t(41) },
	{ DXGI_FORMAT_R32_UINT,                   uint16_t(42) },
	{ DXGI_FORMAT_R32_SINT,                   uint16_t(43) },
	{ DXGI_FORMAT_R8G8_UNORM,                 uint16_t(44) },
	{ DXGI_FORMAT_R8G8_UINT,                  uint16_t(45) },
	{ DXGI_FORMAT_R8G8_SNORM,                 uint16_t(46) },
	{ DXGI_FORMAT_R8G8_SINT,                  uint16_t(47) },
	{ DXGI_FORMAT_R16_FLOAT,                  uint16_t(48) },
	{ DXGI_FORMAT_R16_UNORM,                  uint16_t(49) },
	{ DXGI_FORMAT_R16_UINT,                   uint16_t(50) },
	{ DXGI_FORMAT_R16_SNORM,                  uint16_t(51) },
	{ DXGI_FORMAT_R16_SINT,                   uint16_t(52) },
	{ DXGI_FORMAT_R8_UNORM,                   uint16_t(53) },
	{ DXGI_FORMAT_R8_UINT,                    uint16_t(54) },
	{ DXGI_FORMAT_R8_SNORM,                   uint16_t(55) },
	{ DXGI_FORMAT_R8_SINT,                    uint16_t(56) },
	{ DXGI_FORMAT_A8_UNORM,                   uint16_t(57) },
	{ DXGI_FORMAT_R9G9B9E5_SHAREDEXP,         uint16_t(58) },
	{ DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, uint16_t(59) },
	{ DXGI_FORMAT_D32_FLOAT,                  uint16_t(60) },
	{ DXGI_FORMAT_D16_UNORM,                  uint16_t(61) },
};
