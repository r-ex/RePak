#pragma once

struct Vector3
{
	float x, y, z;
};

#pragma pack(push, 1)
struct RPakFileHeaderV8
{
	uint32_t Magic = 0x6b615052;
	uint16_t Version = 0x8;
	uint16_t Flags = 0;
	uint64_t CreatedTime; // this is actually FILETIME, but if we don't make it uint64_t here, it'll break the struct when writing

	uint64_t WhoKnows = 0;
	uint64_t CompressedSize;

	uint64_t EmbeddedStarpakOffset = 0;
	uint64_t Padding1 = 0;

	uint64_t DecompressedSize;
	uint64_t EmbeddedStarpakSize = 0;

	uint64_t Padding2 = 0;
	uint16_t StarpakReferenceSize = 0;
	uint16_t StarpakOptReferenceSize = 0;
	uint16_t VirtualSegmentCount;			// * 0x10
	uint16_t VirtualSegmentBlockCount;		// * 0xC
	uint32_t PatchIndex = 0;

	uint32_t UnknownThirdBlockCount = 0;
	uint32_t AssetEntryCount;
	uint32_t UnknownFifthBlockCount = 0;
	uint32_t RelationsCount = 0;

	uint8_t Unk[0x1c];
};

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

	uint32_t UnknownThirdBlockCount = 0;
	uint32_t AssetEntryCount = 0;
	uint32_t UnknownFifthBlockCount = 0;
	uint32_t UnknownSixedBlockCount = 0;

	uint32_t UnknownSeventhBlockCount = 0;
	uint32_t UnknownEighthBlockCount = 0;
};

struct RPakVirtualSegment
{
	uint32_t DataFlag = 0; // some flags
	uint32_t DataType;	// type of data contained in the segment
	uint64_t DataSize;	// seg data size
};

struct RPakVirtualSegmentBlock
{
	uint32_t VirtualSegmentIndex; // which vseg is this pointing to
	uint32_t DataType; // type of the referenced vseg
	uint32_t DataSize; // vseg data size
};

struct RPakUnknownBlockThree
{
	uint32_t DataEntryIndex;	// Corrosponds to a data entry
	uint32_t Offset;			// Offset in the data entry, never > DataEntry.DataSize, possibly a 48bit integer packed....
};

struct RPakUnknownBlockFive
{
	uint32_t Unk1;
	uint32_t Flags;
};

struct RPakRelationBlock
{
	uint32_t FileID;
};

struct RPakAssetEntryV8
{
	RPakAssetEntryV8() = default;

	void InitAsset(uint64_t nNameHash,
		uint32_t nSubHeaderBlockIdx,
		uint32_t nSubHeaderBlockOffset,
		uint32_t nSubHeaderSize,
		uint32_t nRawDataBlockIdx,
		uint32_t nRawDataBlockOffset,
		uint64_t nStarpakOffset,
		uint64_t nOptStarpakOffset,
		uint32_t Type)
	{
		this->NameHash = nNameHash;
		this->SubHeaderDataBlockIndex = nSubHeaderBlockIdx;
		this->SubHeaderDataBlockOffset = nSubHeaderBlockOffset;
		this->RawDataBlockIndex = nRawDataBlockIdx;
		this->RawDataBlockOffset = nRawDataBlockOffset;
		this->StarpakOffset = nStarpakOffset;
		this->OptionalStarpakOffset = nOptStarpakOffset;
		this->SubHeaderSize = nSubHeaderSize;
		this->Magic = Type;
	}

	uint64_t NameHash = 0;
	uint64_t Padding = 0;

	uint32_t SubHeaderDataBlockIndex = 0;
	uint32_t SubHeaderDataBlockOffset = 0;
	uint32_t RawDataBlockIndex = 0;
	uint32_t RawDataBlockOffset = 0;

	uint64_t StarpakOffset = -1;
	uint64_t OptionalStarpakOffset = -1;

	uint16_t Un1 = 0;
	uint16_t Un2 = 0;

	uint32_t RelationsStartIndex = 0;

	uint32_t UsesStartIndex = 0;
	uint32_t RelationsCount = 0;
	uint32_t UsesCount = 0;

	uint32_t SubHeaderSize = 0;
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
	AssetSubHeader = 0x8,
	AssetRawData   = 0x10,
};

enum class AssetType : uint32_t
{
	TEXTURE = 0x72747874,
	MODEL = 0x5f6c646d,
	UIMG = 0x676d6975
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
	uint64_t NameHash;
	uint32_t NameIndex;
	uint32_t NameOffset;

	uint16_t Width;
	uint16_t Height;

	uint8_t Un1 = 0;
	uint8_t Un2 = 0;
	uint16_t Format = 0;		// Maps to a DXGI format

	uint32_t DataSize;	// This is the total amount of image data across all banks
	uint32_t Unknown2 = 0;
	uint8_t Unknown3 = 0;
	uint8_t MipLevels;
	uint8_t MipLevelsStreamed = 0;

	uint8_t UnknownPad[0x15];
};

struct DataTableColumn
{
	uint32_t NameIndex;
	uint32_t NameOffset;
	DataTableColumnDataType Type;
	uint32_t RowOffset;
};

struct DataTableHeader
{
	uint32_t ColumnCount;
	uint32_t RowCount;

	uint32_t ColumnHeaderBlock;
	uint32_t ColumnHeaderOffset;
	uint32_t RowHeaderBlock;
	uint32_t RowHeaderOffset;
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
#pragma pack(pop)