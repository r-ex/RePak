#pragma once
struct Vector3
{
	float x, y, z;
};

#pragma pack(push, 1)

struct RPakPtr
{
	uint32_t Index;
	uint32_t Offset;
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

// segment
struct RPakVirtualSegment
{
	uint32_t DataFlag = 0; // some flags
	uint32_t DataType;	// type of data contained in the segment
	uint64_t DataSize;	// seg data size
};

// mem page
struct RPakVirtualSegmentBlock
{
	uint32_t VirtualSegmentIndex; // which vseg is this pointing to
	uint32_t DataType; // type of the referenced vseg
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

struct RPakUnknownBlockFive
{
	uint32_t Unk1;
	uint32_t Flags;
};

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

	uint16_t HighestPageNum = 0;
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
	uint32_t vsegIdx;
	uint64_t dataSize;
	uint8_t* dataPtr;
};

enum class SegmentType : uint32_t
{
	Unknown1 = 0x4,
	AssetSubHeader = 0x8,
	AssetRawData = 0x10,
	Unknown2 = 0x20,
};

enum class AssetType : uint32_t
{
	TEXTURE = 0x72747874, // b'txtr'
	MODEL = 0x5f6c646d,   // b'mdl_'
	UIMG = 0x676d6975,    // b'uimg'
	PTCH = 0x68637450,	  // b'Ptch'
	DTBL = 0x6c627464,    // b'dtbl'
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

	uint8_t Un1 = 0;
	uint8_t Un2 = 0;
	uint16_t Format = 0;		// Maps to a DXGI format

	uint32_t DataSize;	// This is the total amount of image data across all banks
	uint32_t Unknown2 = 0;
	uint8_t Unknown3 = 0;
	uint8_t MipLevels = 0;
	uint8_t MipLevelsStreamed = 0;

	uint8_t UnknownPad[0x15];
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
#pragma pack(pop)

struct PtchEntry
{
	std::string FileName = "";
	uint8_t PatchNum = 0;
	uint32_t FileNamePageOffset = 0;
};