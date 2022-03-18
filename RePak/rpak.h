#pragma once

#include <d3d11.h>
struct Vector3
{
	float x, y, z;
};

#pragma pack(push, 1)

struct RPakPtr
{
	uint32_t Index = 0;
	uint32_t Offset = 0;
};

// Apex Legends RPak file header
struct RPakFileHeaderV8
{
	uint32_t Magic = 0x6b615052;
	uint16_t Version = 0x8;
	uint16_t Flags = 0;
	uint64_t CreatedTime; // this is actually FILETIME, but if we don't make it uint64_t here, it'll break the struct when writing

	uint64_t WhoKnows = 0;

	// size of the rpak file on disk before decompression
	uint64_t CompressedSize;

	uint64_t EmbeddedStarpakOffset = 0;
	uint64_t Padding1 = 0;

	// actual data size of the rpak file after decompression
	uint64_t DecompressedSize;
	uint64_t EmbeddedStarpakSize = 0;

	uint64_t Padding2 = 0;
	uint16_t StarpakReferenceSize = 0;
	uint16_t StarpakOptReferenceSize = 0;
	uint16_t VirtualSegmentCount;			// * 0x10
	uint16_t VirtualSegmentBlockCount;		// * 0xC
	uint32_t PatchIndex = 0;

	uint32_t DescriptorCount = 0;
	uint32_t AssetEntryCount = 0;
	uint32_t GuidDescriptorCount = 0;
	uint32_t RelationsCount = 0;

	uint8_t Unk[0x1c];
};

// Titanfall 2 RPak file header
struct RPakFileHeaderV7
{
	uint32_t Magic = 0x6b615052;
	uint16_t Version = 0x7;
	uint16_t Flags = 0;

	uint64_t CreatedTime; // this is actually FILETIME, but if we don't make it uint64_t here, it'll break the struct when writing

	uint64_t WhoKnows = 0;

	uint64_t CompressedSize;
	uint64_t Padding1 = 0;
	uint64_t DecompressedSize;
	uint64_t Padding2 = 0;

	uint16_t StarpakReferenceSize = 0;
	uint16_t VirtualSegmentCount;			// * 0x10
	uint16_t VirtualSegmentBlockCount;		// * 0xC

	uint16_t PatchIndex = 0;

	uint32_t DescriptorCount = 0;
	uint32_t AssetEntryCount = 0;
	uint32_t UnknownFifthBlockCount = 0;
	uint32_t UnknownSixedBlockCount = 0;

	uint32_t UnknownSeventhBlockCount = 0;
	uint32_t UnknownEighthBlockCount = 0;
};

// todo: document these
// segment
struct RPakVirtualSegment
{
	uint32_t DataFlag = 0; // some flags
	uint32_t Alignment;	// Thank you Respawn, very cool!
	uint64_t DataSize;	// seg data size
};

// mem page
struct RPakVirtualSegmentBlock
{
	uint32_t VirtualSegmentIndex; // which vseg is this pointing to
	uint32_t Alignment; // data alignment
	uint32_t DataSize; // vseg data size
};

// defines the location of a data "pointer" within the pak's mem pages
// allows the engine to read the index/offset pair and replace it with an actual memory pointer at runtime
struct RPakDescriptor
{
	uint32_t PageIdx;	 // page index
	uint32_t PageOffset; // offset within page
};

// same kinda thing as RPakDescriptor, but this one tells the engine where
// guid references to other assets are within mem pages
typedef RPakDescriptor RPakGuidDescriptor;

struct RPakRelationBlock
{
	uint32_t FileID;
};

// defines a bunch of values for registering/using an asset from the rpak
struct RPakAssetEntryV8
{
	RPakAssetEntryV8() = default;

	void InitAsset(uint64_t nGUID,
		uint32_t nSubHeaderBlockIdx,
		uint32_t nSubHeaderBlockOffset,
		uint32_t nSubHeaderSize,
		uint32_t nRawDataBlockIdx,
		uint32_t nRawDataBlockOffset,
		uint64_t nStarpakOffset,
		uint64_t nOptStarpakOffset,
		uint32_t Type)
	{
		this->GUID = nGUID;
		this->SubHeaderDataBlockIndex = nSubHeaderBlockIdx;
		this->SubHeaderDataBlockOffset = nSubHeaderBlockOffset;
		this->RawDataBlockIndex = nRawDataBlockIdx;
		this->RawDataBlockOffset = nRawDataBlockOffset;
		this->StarpakOffset = nStarpakOffset;
		this->OptionalStarpakOffset = nOptStarpakOffset;
		this->SubHeaderSize = nSubHeaderSize;
		this->Magic = Type;
	}

	// hashed version of the asset path
	// used for referencing the asset from elsewhere
	//
	// - when referenced from other assets, the GUID is used directly
	// - when referenced from scripts, the GUID is calculated from the original asset path
	//   by a function such as RTech::StringToGuid
	uint64_t GUID = 0;
	uint64_t Padding = 0;

	// page index and offset for where this asset's header is located
	uint32_t SubHeaderDataBlockIndex = 0;
	uint32_t SubHeaderDataBlockOffset = 0;

	// page index and offset for where this asset's data is located
	// note: this may not always be used for finding the data:
	//		 some assets use their own idx/offset pair from within the subheader
	//		 when adding pairs like this, you MUST register it as a descriptor
	//		 otherwise the pointer won't be converted
	uint32_t RawDataBlockIndex = 0;
	uint32_t RawDataBlockOffset = 0;

	// offset to any available streamed data
	// StarpakOffset         = "mandatory" starpak file offset
	// OptionalStarpakOffset = "optional" starpak file offset
	// 
	// in reality both are mandatory but respawn likes to do a little trolling
	// so "opt" starpaks are a thing
	uint64_t StarpakOffset = -1;
	uint64_t OptionalStarpakOffset = -1;

	uint16_t HighestPageNum = 0; // might be called pageEnd? - number of the highest mem page that is used by this asset
	uint16_t Un2 = 0;

	uint32_t RelationsStartIndex = 0;

	uint32_t UsesStartIndex = 0;
	uint32_t RelationsCount = 0;
	uint32_t UsesCount = 0; // number of other assets that this asset uses

	// size of the asset header
	uint32_t SubHeaderSize = 0;

	// this isn't always changed when the asset gets changed
	// but respawn calls it a version so i will as well
	uint32_t Version = 0; 

	uint32_t Magic = 0;
};
#pragma pack(pop)

struct RPakRawDataBlock
{
	uint32_t pageIdx;
	uint64_t dataSize;
	uint8_t* dataPtr;
};

struct SRPkDataEntry
{
	uint64_t offset = -1; // set when added
	uint64_t dataSize = 0;
	uint8_t* dataPtr;
};

enum class AssetType : uint32_t
{
	TEXTURE = 0x72747874, // b'txtr' - texture
	RMDL = 0x5f6c646d,    // b'mdl_' - model
	UIMG = 0x676d6975,    // b'uimg' - ui image atlas
	PTCH = 0x68637450,	  // b'Ptch' - patch
	DTBL = 0x6c627464,    // b'dtbl' - datatable
	MATL = 0x6c74616d,    // b'matl' - material
};

enum class DataTableColumnDataType : uint32_t
{
	Bool,
	Int,
	Float,
	Vector,
	StringT,
	Asset,
	AssetNoPrecache
};

//
//	Assets
//
#pragma pack(push, 1)
struct TextureHeader
{
	uint64_t NameHash = 0;
	uint32_t NameIndex = 0;
	uint32_t NameOffset = 0;

	uint16_t Width = 0;
	uint16_t Height = 0;

	uint16_t Un1 = 0;
	uint16_t Format = 0;		// Maps to a DXGI format

	uint32_t DataSize;	// This is the total amount of image data across all banks
	uint8_t Unknown2;
	uint8_t MipLevelsStreamedOpt;
	uint8_t ArraySize;
	uint8_t LayerCount;
	uint8_t Unknown4;
	uint8_t MipLevels;
	uint8_t MipLevelsStreamed;
	uint8_t UnknownPad[21];
};

struct UIImageHeader
{
	uint64_t Unk0 = 0;
	uint16_t Width = 1;
	uint16_t Height = 1;
	uint16_t TextureOffsetsCount = 0;
	uint16_t TextureCount = 0;
	uint32_t TextureOffsetsIndex = 0;
	uint32_t TextureOffsetsOffset = 0;
	uint32_t TextureDimsIndex = 0;
	uint32_t TextureDimsOffset = 0;
	uint32_t Unk20 = 0;
	uint32_t Unk24 = 0;
	uint32_t TextureHashesIndex = 0;
	uint32_t TextureHashesOffset = 0;
	uint32_t TextureNamesIndex = 0;
	uint32_t TextureNamesOffset = 0;
	uint64_t TextureGuid = 0;
};

struct UIImageUV
{
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

struct DataTableColumn
{
	RPakPtr Name;
	DataTableColumnDataType Type;
	uint32_t RowOffset;
};

struct DataTableHeader
{
	uint32_t ColumnCount;
	uint32_t RowCount;

	RPakPtr ColumnHeaderPtr;
	RPakPtr RowHeaderPtr;
	uint32_t UnkHash;

	uint16_t Un1 = 0;
	uint16_t Un2 = 0;

	uint32_t RowStride;	// Number of bytes per row
	uint32_t Padding;
};

struct DataTableColumnData
{
	DataTableColumnDataType Type;
	bool bValue = 0;
	int iValue = -1;
	float fValue = -1;
	Vector3 vValue;
	std::string stringValue;
	std::string assetValue;
	std::string assetNPValue;
};

struct PtchHeader
{
	uint32_t Unk0 = 255; // always FF 00 00 00?
	uint32_t EntryCount = 0;

	RPakPtr EntryNames;

	RPakPtr EntryPatchNums;
};

// size: 0x78 (120 bytes)
struct ModelHeader
{
	// IDST data
	// .mdl
	RPakPtr SkeletonPtr;
	uint64_t Padding = 0;

	// model path
	// e.g. mdl/vehicle/goblin_dropship/goblin_dropship.rmdl
	RPakPtr NamePtr;
	uint64_t Padding2 = 0;

	// .phy
	RPakPtr PhyPtr;
	uint64_t Padding3 = 0;

	// this data is usually kept in a mandatory starpak, but there's also fallback(?) data here
	// in rmdl v8, this is literally just .vtx and .vvd stuck together
	// in v9+ it's an increasingly mutated version, although it contains roughly the same data
	RPakPtr VGPtr;

	// pointer to data for the model's arig guid(s?)
	RPakPtr AnimRigRefPtr;

	// this is a guess based on the above ptr's data. i think this is == to the number of guids at where the ptr points to
	uint32_t AnimRigCount = 0;

	// size of the data kept in starpak
	uint32_t StreamedDataSize = 0;
	uint32_t DataCacheSize = 0;
	uint64_t Padding6 = 0;

	// number of anim sequences directly associated with this model
	uint32_t AnimSequenceCount = 0;
	RPakPtr AnimSequencePtr;

	uint64_t Padding7 = 0;
	uint64_t Padding8 = 0;
	uint64_t Padding9 = 0;
};

// only the first few members of the skeleton header struct.
// used for verifying the input file when adding an rmdl asset
struct BasicRMDLSkeletonHeader
{
	uint32_t magic; // IDST
	uint32_t version; // 6
	uint32_t checksum;
	uint32_t stringTableOffset;
	char modelName[64];
	uint32_t dataSize;
};

struct studiohdr_t
{
	uint32_t Magic;
	uint32_t Version;
	uint32_t Hash;
	uint32_t NameTableOffset;

	char SkeletonName[0x40];

	uint32_t DataSize;

	float EyePosition[3];
	float IllumPosition[3];
	float HullMin[3];
	float HullMax[3];
	float ViewBBMin[3];
	float ViewBBMax[3];

	uint32_t Flags; // 0x9c

	uint32_t BoneCount; // 0xa0
	uint32_t BoneDataOffset; // 0xa4

	uint32_t BoneControllerCount;
	uint32_t BoneControllerOffset;

	uint32_t HitboxCount;
	uint32_t HitboxOffset;

	uint32_t LocalAnimCount;
	uint32_t LocalAnimOffset;

	uint32_t LocalSeqCount;
	uint32_t LocalSeqOffset;

	uint32_t ActivityListVersion;
	uint32_t EventsIndexed;

	uint32_t MaterialCount;
	uint32_t MaterialOffset;

	uint32_t TextureDirCount;
	uint32_t TextureDirOffset;

	uint32_t SkinReferenceCount;	// Total number of references (submeshes)
	uint32_t SkinFamilyCount;		// Total skins per reference
	uint32_t SkinReferenceOffset;	// Offset to data

	uint32_t BodyPartCount;
	uint32_t BodyPartOffset;

	uint32_t AttachmentCount;
	uint32_t AttachmentOffset;

	uint8_t Unknown2[0x14];

	uint32_t SubmeshLodsOffset;

	uint8_t Unknown3[0x64];
	uint32_t OffsetToBoneRemapInfo;
	uint32_t BoneRemapCount;
};

struct BasicRMDLVGHeader
{
	uint32_t magic;
	uint32_t version;
};

// asset path
// texture guids
// unknown section with same length as texture guids
// surface name


struct UnknownMaterialSection
{
	// required but seems to follow a pattern. maybe related to "Unknown2" above?
	// nulling these bytes makes the material stop drawing entirely
	uint32_t Unknown5[8]{};

	uint32_t Unknown6 = 0;

	// both of these are required

	// seems to be some kind of render/visibility flag.
	uint16_t Flags1 = 0x17;
	uint16_t Flags2 = 0x6; // i'm not sure about what this one does exactly

	uint64_t Padding = 0;
};

struct MaterialHeader
{
	uint64_t VtblPtrPad = 0; // Gets set to CMaterialGlue vtbl ptr
	uint8_t Padding[0x8]{}; // Un-used.
	uint64_t AssetGUID = 0; // This materials GUID.

	RPakPtr Name{}; // Asset path
	RPakPtr SurfaceName{}; // Surface name (as defined in surfaceproperties.rson)
	RPakPtr SurfaceName2{}; // Surface name 2 

	// IDX 1: DepthShadow
	// IDX 2: DepthPrepass
	// IDX 3: DepthVSM
	// IDX 4: DepthShadowTight
	// IDX 5: ColPass
	// They seem to be the exact same for all materials throughout the game.
	uint64_t GUIDRefs[5]{}; // Required to have proper textures.
	uint64_t ShaderSetGUID = 0; // Shaderset guid / CShaderGlue guid

	RPakPtr TextureGUIDs{}; // TextureGUID Map 1
	RPakPtr TextureGUIDs2{}; // TextureGUID Map 2

	int16_t UnknownSignature = 0x4; //This seems to be the start of a modified VTF Header, I have no clue what this member does. 
	int16_t Width = 2048;
	int16_t Height = 2048;
	int16_t Unknown = 0;

	// Not fucking optional, its responsible for clamping and stretching aka to tile properly.
	uint32_t ImageFlags = 0x1D0300; 
	uint32_t Unknown1 = 0;

	uint32_t Unknown2 = 0x1F5A92BD; // REQUIRED but why?

	uint32_t Alignment = 0;

	// neither of these 2 seem to be required
	uint32_t something = 0;
	uint32_t something2 = 0;

	UnknownMaterialSection UnkSections[2]{};
};

struct MaterialCPUHeader
{
	RPakPtr Unknown{};
	uint32_t DataSize = 0;
	uint32_t VersionMaybe = 3;
};

#pragma pack(pop)

struct PtchEntry
{
	std::string FileName = "";
	uint8_t PatchNum = 0;
	uint32_t FileNamePageOffset = 0;
};

static std::map<DXGI_FORMAT, uint16_t> TxtrFormatMap{
	{ DXGI_FORMAT_BC1_UNORM, 0 },
	{ DXGI_FORMAT_BC1_UNORM_SRGB, 1 },
	{ DXGI_FORMAT_BC2_UNORM, 2 },
	{ DXGI_FORMAT_BC2_UNORM_SRGB, 3 },
	{ DXGI_FORMAT_BC3_UNORM, 4 },
	{ DXGI_FORMAT_BC3_UNORM_SRGB, 5 },
	{ DXGI_FORMAT_BC4_UNORM, 6 },
	{ DXGI_FORMAT_BC4_SNORM, 7 },
	{ DXGI_FORMAT_BC5_UNORM, 8 },
	{ DXGI_FORMAT_BC5_SNORM, 9 },
	{ DXGI_FORMAT_BC6H_UF16, 10 },
	{ DXGI_FORMAT_BC6H_SF16, 11 },
	{ DXGI_FORMAT_BC7_UNORM, 12 },
	{ DXGI_FORMAT_BC7_UNORM_SRGB, 13 },
	{ DXGI_FORMAT_R32G32B32A32_FLOAT, 14 },
	{ DXGI_FORMAT_R32G32B32A32_UINT, 15 },
	{ DXGI_FORMAT_R32G32B32A32_SINT, 16 },
	{ DXGI_FORMAT_R32G32B32_FLOAT, 17 },
	{ DXGI_FORMAT_R32G32B32_UINT, 18 },
	{ DXGI_FORMAT_R32G32B32_SINT, 19 },
	{ DXGI_FORMAT_R16G16B16A16_FLOAT, 20 },
	{ DXGI_FORMAT_R16G16B16A16_UNORM, 21 },
	{ DXGI_FORMAT_R16G16B16A16_UINT, 22 },
	{ DXGI_FORMAT_R16G16B16A16_SNORM, 23 },
	{ DXGI_FORMAT_R16G16B16A16_SINT, 24 },
	{ DXGI_FORMAT_R32G32_FLOAT, 25 },
	{ DXGI_FORMAT_R32G32_UINT, 26 },
	{ DXGI_FORMAT_R32G32_SINT, 27 },
	{ DXGI_FORMAT_R10G10B10A2_UNORM, 28 },
	{ DXGI_FORMAT_R10G10B10A2_UINT, 29 },
	{ DXGI_FORMAT_R11G11B10_FLOAT, 30 },
	{ DXGI_FORMAT_R8G8B8A8_UNORM, 31 },
	{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 32 },
	{ DXGI_FORMAT_R8G8B8A8_UINT, 33 },
	{ DXGI_FORMAT_R8G8B8A8_SNORM, 34 },
	{ DXGI_FORMAT_R8G8B8A8_SINT, 35 },
	{ DXGI_FORMAT_R16G16_FLOAT, 36 },
	{ DXGI_FORMAT_R16G16_UNORM, 37 },
	{ DXGI_FORMAT_R16G16_UINT, 38 },
	{ DXGI_FORMAT_R16G16_SNORM, 39 },
	{ DXGI_FORMAT_R16G16_SINT, 40 },
	{ DXGI_FORMAT_R32_FLOAT, 41 },
	{ DXGI_FORMAT_R32_UINT, 42 },
	{ DXGI_FORMAT_R32_SINT, 43 },
	{ DXGI_FORMAT_R8G8_UNORM, 44 },
	{ DXGI_FORMAT_R8G8_UINT, 45 },
	{ DXGI_FORMAT_R8G8_SNORM, 46 },
	{ DXGI_FORMAT_R8G8_SINT, 47 },
	{ DXGI_FORMAT_R16_FLOAT, 48 },
	{ DXGI_FORMAT_R16_UNORM, 49 },
	{ DXGI_FORMAT_R16_UINT, 50 },
	{ DXGI_FORMAT_R16_SNORM, 51 },
	{ DXGI_FORMAT_R16_SINT, 52 },
	{ DXGI_FORMAT_R8_UNORM, 53 },
	{ DXGI_FORMAT_R8_UINT, 54 },
	{ DXGI_FORMAT_R8_SNORM, 55 },
	{ DXGI_FORMAT_R8_SINT, 56 },
	{ DXGI_FORMAT_A8_UNORM, 57 },
	{ DXGI_FORMAT_R9G9B9E5_SHAREDEXP, 58 },
	{ DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, 59 },
	{ DXGI_FORMAT_D32_FLOAT, 60 },
	{ DXGI_FORMAT_D16_UNORM, 61 },
};