#pragma once

#include <d3d11.h>
#include <pch.h>

#define SF_HEAD   0 // :skull:
#define SF_CPU    (1 << 0)
#define SF_TEMP   (1 << 1)
#define SF_SERVER (1 << 5)
#define SF_CLIENT (1 << 6)
#define SF_DEV    (1 << 7)

struct Vector2
{
	float x, y;

	Vector2(float x, float y) {
		this->x = x;
		this->y = y;
	}

	Vector2() {};
};

struct Vector3
{
	float x, y, z;

	Vector3(float x, float y, float z) {
		this->x = x;
		this->y = y;
		this->z = z;
	}

	Vector3() {};
};

struct Quaternion
{
	float x, y, z, w;

	Quaternion(float x, float y, float z, float w) {
		this->x = x;
		this->y = y;
		this->z = z;
		this->w = w;
	}

	Quaternion() {};
};


enum class AssetType : uint32_t
{
	RMDL = '_ldm', // mdl_ - 0x5F6C646D
	TXTR = 'rtxt', // txtr - 0x72747874
	TXAN = 'naxt', // txan - 0x6e617874
	UIIA = 'aiiu', // uiia - 0x61696975
	DTBL = 'lbtd', // dtbl - 0x6C627464
	STGS = 'sgts', // stgs - 0x73677473
	STLT = 'tlts', // stlt - 0x746c7473
	MATL = 'ltam', // matl - 0x6C74616D
	ARIG = 'gira', // arig - 0x67697261
	ASEQ = 'qesa', // aseq - 0x71657361
	SUBT = 'tbus', // subt - 0x74627573
	SHDS = 'sdhs', // shds - 0x73646873
	SHDR = 'rdhs', // shdr - 0x72646873
	UIMG = 'gmiu', // uimg - 0x676D6975
	RSON = 'nosr', // rson - 0x72736F6E
	PTCH = 'hctp',  // Ptch - 0x68637450
	UI = 'iu' // ui - 0x75690000
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
	uint64_t fileTime; // this is actually FILETIME, but if we don't make it uint64_t here, it'll break the struct when writing
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

// internal data structure for referencing streaming data to be written
struct SRPkDataEntry
{
	uint64_t m_nOffset = -1; // set when added
	uint64_t m_nDataSize = 0;
	uint8_t* m_nDataPtr = nullptr;
};