#pragma once

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
	uint64_t NameHash;
	uint64_t Padding = 0;

	uint32_t SubHeaderDataBlockIndex;
	uint32_t SubHeaderDataBlockOffset;
	uint32_t RawDataBlockIndex;
	uint32_t RawDataBlockOffset;

	uint64_t StarpakOffset = -1;
	uint64_t OptionalStarpakOffset = -1;

	uint16_t Un1 = 0;
	uint16_t Un2 = 0;

	uint32_t DataSpliceIndex = 0; // what?

	uint32_t Un4 = 0;
	uint32_t Un5 = 0;
	uint32_t Un6 = 0;

	uint32_t SubHeaderSize;
	uint32_t Flags = 0;
	uint32_t Magic;
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
	uint8_t Format = 0;		// Maps to a DXGI format
	uint8_t Un3 = 0;

	uint32_t DataSize;	// This is the total amount of image data across all banks
	uint32_t Unknown2 = 0;
	uint8_t Unknown3 = 0;
	uint8_t MipLevels;
	uint8_t MipLevelsStreamed = 0;

	uint8_t UnknownPad[0x15];
};
#pragma pack(pop)