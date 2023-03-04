#pragma once

#include <d3d11.h>
#include "math/vector.h"
#include "math/color.h"

#define RPAK_MAGIC		(('k'<<24)+('a'<<16)+('P'<<8)+'R')
#define RPAK_EXTENSION ".rpak"

#define STARPAK_MAGIC	(('k'<<24)+('P'<<16)+('R'<<8)+'S')
#define STARPAK_VERSION	1
#define STARPAK_EXTENSION ".starpak"

// data blocks in starpaks are all aligned to 4096 bytes, including
// the header which gets filled with 0xCB after the magic and version
#define STARPAK_DATABLOCK_ALIGNMENT 4096
#define STARPAK_DATABLOCK_ALIGNMENT_PADDING 0xCB

#define TYPE_TXTR	(('r'<<24)+('t'<<16)+('x'<<8)+'t') // txtr
#define TYPE_RMDL	(('_'<<24)+('l'<<16)+('d'<<8)+'m') // mdl_
#define TYPE_UIMG	(('g'<<24)+('m'<<16)+('i'<<8)+'u') // uimg
#define TYPE_PTCH	(('h'<<24)+('c'<<16)+('t'<<8)+'P') // Ptch
#define TYPE_DTBL	(('l'<<24)+('b'<<16)+('t'<<8)+'d') // dtbl
#define TYPE_MATL	(('l'<<24)+('t'<<16)+('a'<<8)+'m') // matl
#define TYPE_ASEQ	(('q'<<24)+('e'<<16)+('s'<<8)+'a') // aseq

enum class AssetType : uint32_t
{
	TXTR = TYPE_TXTR, // texture
	RMDL = TYPE_RMDL, // model
	UIMG = TYPE_UIMG, // ui image atlas
	PTCH = TYPE_PTCH, // patch
	DTBL = TYPE_DTBL, // datatable
	MATL = TYPE_MATL, // material
	ASEQ = TYPE_ASEQ, // animation sequence
};

#pragma pack(push, 1)

// represents a "pointer" into a mempage by page index and offset
// when loaded, these usually get converted to a real pointer
struct RPakPtr
{
	uint32_t index = 0;
	uint32_t offset = 0;
};

// generic header struct for both apex and titanfall 2
// contains all the necessary members for both, RPakFileBase::WriteHeader decides
// which should be written depending on the version
struct RPakFileHeader
{
	DWORD magic = 0x6b615052;

	short fileVersion = 0x8;
	char  flags[0x2];
	FILETIME fileTime;
	char  unk0[0x8];
	uint64_t compressedSize; // size of the rpak file on disk before decompression
	uint64_t embeddedStarpakOffset = 0;
	char  unk1[0x8];
	uint64_t decompressedSize; // actual data size of the rpak file after decompression
	uint64_t embeddedStarpakSize = 0;
	char  unk2[0x8];
	uint16_t starpakPathsSize = 0; // size in bytes of the section containing mandatory starpak paths
	uint16_t optStarpakPathsSize = 0; // size in bytes of the section containing optional starpak paths
	uint16_t virtualSegmentCount = 0;
	uint16_t pageCount = 0; // number of "mempages" in the rpak
	uint16_t patchIndex = 0;
	uint16_t alignment = 0;
	uint32_t descriptorCount = 0;
	uint32_t assetCount = 0;
	uint32_t guidDescriptorCount = 0;
	uint32_t relationCount = 0;

	// only in tf2, related to external
	uint32_t unk7count = 0;
	uint32_t unk8count = 0;

	// only in apex
	char  unk3[0x1c];
};
static_assert(sizeof(RPakFileHeader) == 136);

struct RPakPatchCompressedHeader // follows immediately after the file header in patch rpaks
{
	uint64_t compressedSize;
	uint64_t decompressedSize;
};

// segment
// these probably aren't actually called virtual segments
// this struct doesn't really describe any real data segment, but collects info
// about the size of pages that are using specific flags/types/whatever
struct RPakVirtualSegment
{
	uint32_t flags = 0; // not sure what this actually is, doesn't seem to be used in that many places
	uint32_t alignment = 0;
	uint64_t dataSize = 0;
};

// mem page
// describes an actual section in the file data. all pages are sequential
// with page at idx 0 being just after the asset relation data
// in patched rpaks (e.g. common(01).rpak), these sections don't fully line up with the data,
// because of both the patch edit stream and also missing pages that are only present in the base rpak
struct RPakPageInfo
{
	uint32_t segIdx; // index into vseg array
	uint32_t pageAlignment; // alignment size when buffer is allocated
	uint32_t dataSize; // actual size of page in bytes
};

// defines the location of a data "pointer" within the pak's mem pages
// allows the engine to read the index/offset pair and replace it with an actual memory pointer at runtime
typedef RPakPtr RPakDescriptor;

// same kinda thing as RPakDescriptor, but this one tells the engine where
// guid references to other assets are within mem pages
typedef RPakDescriptor RPakGuidDescriptor;

// defines a bunch of values for registering/using an asset from the rpak
struct RPakAssetEntry
{
	RPakAssetEntry() = default;

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
		this->guid = nGUID;
		this->headIdx = nSubHeaderBlockIdx;
		this->headOffset = nSubHeaderBlockOffset;
		this->cpuIdx = nRawDataBlockIdx;
		this->cpuOffset = nRawDataBlockOffset;
		this->starpakOffset = nStarpakOffset;
		this->optStarpakOffset = nOptStarpakOffset;
		this->headDataSize = nSubHeaderSize;
		this->id = Type;
	}

	// hashed version of the asset path
	// used for referencing the asset from elsewhere
	//
	// - when referenced from other assets, the GUID is used directly
	// - when referenced from scripts, the GUID is calculated from the original asset path
	//   by a function such as RTech::StringToGuid
	uint64_t guid = 0;
	uint8_t  unk0[0x8]{};

	// page index and offset for where this asset's header is located
	int headIdx = 0;
	int headOffset = 0;

	// page index and offset for where this asset's data is located
	// note: this may not always be used for finding the data:
	//		 some assets use their own idx/offset pair from within the subheader
	//		 when adding pairs like this, you MUST register it as a descriptor
	//		 otherwise the pointer won't be converted
	int cpuIdx = 0;
	int cpuOffset = 0;

	// offset to any available streamed data
	// starpakOffset    = "mandatory" starpak file offset
	// optStarpakOffset = "optional" starpak file offset
	// 
	// in reality both are mandatory but respawn likes to do a little trolling
	// so "opt" starpaks are a thing
	__int64 starpakOffset = -1;
	__int64 optStarpakOffset = -1;

	uint16_t pageEnd = 0; // highest mem page used by this asset
	uint16_t unk1 = 0; // might be local "uses" + 1

	uint32_t relStartIdx = 0;

	uint32_t usesStartIdx = 0;
	uint32_t relationCount = 0;
	uint32_t usesCount = 0; // number of other assets that this asset uses

	// size of the asset header
	uint32_t headDataSize = 0;

	// this isn't always changed when the asset gets changed
	// but respawn calls it a version so i will as well
	int version = 0;

	// see AssetType enum below
	uint32_t id = 0;

	// internal
public:
	int _assetidx;

	// vector of indexes for local assets that use this asset
	std::vector<unsigned int> _relations{};

	inline void AddRelation(unsigned int idx) { _relations.push_back({ idx }); };

	std::vector<RPakGuidDescriptor> _guids{};

	inline void AddGuid(RPakGuidDescriptor desc) { _guids.push_back(desc); };

	inline void AddGuids(std::vector<RPakGuidDescriptor>* descs)
	{
		for (auto& it : *descs)
			_guids.push_back(it);
	};
};
#pragma pack(pop)

// internal data structure for referencing file data to be written
struct RPakRawDataBlock
{
	uint32_t m_nPageIdx;
	uint64_t m_nDataSize;
	uint8_t* m_nDataPtr;
};

// starpak header
struct StreamableSetHeader
{
	int magic;
	int version;
};

// internal data structure for referencing streaming data to be written
struct StreamableDataEntry
{
	uint64_t m_nOffset = -1; // set when added
	uint64_t m_nDataSize = 0;
	uint8_t* m_nDataPtr = nullptr;
};

//
//	Assets
//
#pragma pack(push, 1)

struct PtchHeader
{
	uint32_t unknown_1 = 255; // always FF 00 00 00?
	uint32_t patchedPakCount = 0;

	RPakPtr pPakNames;

	RPakPtr pPakPatchNums;
};

#pragma pack(pop)

// internal data structure for storing patch_master entries before being written
struct PtchEntry
{
	std::string FileName = "";
	uint8_t PatchNum = 0;
	uint32_t FileNamePageOffset = 0;
};

#define SF_HEAD   0 // :skull:
#define SF_CPU    (1 << 0)
#define SF_TEMP   (1 << 1)
#define SF_SERVER (1 << 5)
#define SF_CLIENT (1 << 6)
#define SF_DEV    (1 << 7)
