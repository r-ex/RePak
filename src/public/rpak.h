#pragma once

#include <d3d11.h>
#include "math/vector.h"
#include "math/color.h"
#include "public/starpak.h"

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
#define TYPE_ARIG	(('g'<<24)+('i'<<16)+('r'<<8)+'a') // aseq

enum class AssetType : uint32_t
{
	TXTR = TYPE_TXTR, // texture
	RMDL = TYPE_RMDL, // model
	UIMG = TYPE_UIMG, // ui image atlas
	PTCH = TYPE_PTCH, // patch
	DTBL = TYPE_DTBL, // datatable
	MATL = TYPE_MATL, // material
	ASEQ = TYPE_ASEQ, // animation sequence
	ARIG = TYPE_ARIG, // animation rig
};

#pragma pack(push, 1)

// represents a "pointer" into a mempage by page index and offset
// when loaded, these usually get converted to a real pointer
struct PagePtr_t
{
	int index = 0;
	int offset = 0;

	static PagePtr_t NullPtr()
	{
		return { -1, 0 };
	}

	size_t value() const { return (static_cast<size_t>(index) << 32) | offset; };

	bool operator<(const PagePtr_t& a) const
	{
		return value() < a.value();
	}
};

// generic header struct for both apex and titanfall 2
// contains all the necessary members for both, RPakFileBase::WriteHeader decides
// which should be written depending on the version
struct PakHdr_t
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
static_assert(sizeof(PakHdr_t) == 136);

struct PakPatchFileHdr_t // follows immediately after the file header in patch rpaks
{
	uint64_t compressedSize;
	uint64_t decompressedSize;
};

// segment
// these probably aren't actually called virtual segments
// this struct doesn't really describe any real data segment, but collects info
// about the size of pages that are using specific flags/types/whatever
struct PakSegmentHdr_t
{
	int flags = 0;
	int alignment = 0;
	uint64_t dataSize = 0;
};

// mem page
// describes an actual section in the file data. all pages are sequential
// with page at idx 0 being just after the asset relation data
// in patched rpaks (e.g. common(01).rpak), these sections don't fully line up with the data,
// because of both the patch edit stream and also missing pages that are only present in the base rpak
struct PakPageHdr_t
{
	int segIdx; // index into vseg array
	int pageAlignment; // alignment size when buffer is allocated
	int dataSize; // actual size of page in bytes
};
#pragma pack(pop)

// defines the location of a data "pointer" within the pak's mem pages
// allows the engine to read the index/offset pair and replace it with an actual memory pointer at runtime
typedef PagePtr_t PakPointerHdr_t;

// same kinda thing as RPakDescriptor, but this one tells the engine where
// guid references to other assets are within mem pages
typedef PakPointerHdr_t PakGuidRefHdr_t;

// defines a bunch of values for registering/using an asset from the rpak
struct PakAsset_t
{
	PakAsset_t() = default;

	void InitAsset(const std::string& name,
		PagePtr_t headPtr,
		uint32_t headerSize,
		PagePtr_t cpuPtr,
		uint64_t starpakOffset,
		uint64_t optStarpakOffset,
		uint32_t type)
	{
		this->name = name;
		this->guid = RTech::StringToGuid(name.c_str());
		this->headPtr = headPtr;
		this->cpuPtr = cpuPtr;
		this->starpakOffset = starpakOffset;
		this->optStarpakOffset = optStarpakOffset;
		this->headDataSize = headerSize;
		this->id = type;
	}

	void InitAsset(uint64_t guid,
		PagePtr_t headPtr,
		uint32_t headerSize,
		PagePtr_t cpuPtr,
		uint64_t starpakOffset,
		uint64_t optStarpakOffset,
		uint32_t type)
	{
		this->name = "(null)";
		this->guid = guid;
		this->headPtr = headPtr;
		this->cpuPtr = cpuPtr;
		this->starpakOffset = starpakOffset;
		this->optStarpakOffset = optStarpakOffset;
		this->headDataSize = headerSize;
		this->id = type;
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
	PagePtr_t headPtr;

	// page index and offset for where this asset's data is located
	// note: this may not always be used for finding the data:
	//		 some assets use their own idx/offset pair from within the subheader
	//		 when adding pairs like this, you MUST register it as a descriptor
	//		 otherwise the pointer won't be converted
	PagePtr_t cpuPtr;

	// offset to any available streamed data
	// starpakOffset    = "mandatory" starpak file offset
	// optStarpakOffset = "optional" starpak file offset
	// 
	// in reality both are mandatory but respawn likes to do a little trolling
	// so "opt" starpaks are a thing
	__int64 starpakOffset = -1;
	__int64 optStarpakOffset = -1;

	// this is actually uint16 in file. we store it as size_t here to avoid casts in every asset function
	size_t pageEnd = 0; // highest mem page used by this asset

	// value is decremented every time a dependency finishes processing its own dependencies
	short remainingDependencyCount = 0;

	// start index for this asset's dependents/dependencies in respective arrays
	uint32_t dependentsIndex = 0;
	uint32_t dependenciesIndex = 0;

	uint32_t dependentsCount = 0; // number of local assets that use this asset
	uint32_t dependenciesCount = 0; // number of local assets that are used by this asset

	// size of the asset header
	int headDataSize = 0;

	// this isn't always changed when the asset gets changed
	// but respawn calls it a version so i will as well
	int version = 0;

	// see AssetType enum
	uint32_t id = 0;

	// internal
public:
	int _assetidx;
	std::string name;

	// vector of indexes for local assets that use this asset
	std::vector<unsigned int> _relations{};

	inline void AddRelation(unsigned int idx) { _relations.push_back({ idx }); };
	inline void AddRelation(size_t idx) { _relations.push_back({ static_cast<unsigned int>(idx) }); };

	std::vector<PakGuidRefHdr_t> _guids{};

	inline void AddGuid(PakGuidRefHdr_t desc) { _guids.push_back(desc); };

	inline void AddGuids(std::vector<PakGuidRefHdr_t>* descs)
	{
		for (auto& it : *descs)
			_guids.push_back(it);
	};

	// ensures that this asset's guid does not already exist within a given vector of assets
	inline void EnsureUnique(std::vector<PakAsset_t>* assets)
	{
		size_t i = 0;
		for (PakAsset_t& asset : *assets)
		{
			// this check requires a fatal error as by the time this is checked (just before being added to the vector)
			// all of this asset's pages have already been created and dependencies have been processed
			// if we simply skip the asset this late, it will cause problems with unused pages and invalid dependencies
			// 
			// additionally, a duplicate asset will usually be a mistake that the user wishes to deal with by themselves, and a non-fatal error
			// may cause them to overlook the problem.
			// assets in which duplicate additions are expected (e.g. textures being auto-added from materials) already handle non-fatal duplications separately
			// from an earlier point at which asset skips are acceptable
			if (asset.guid == this->guid)
				Error("Found duplicate asset '%s'. Assets at index %lld and %lld have the same GUID (%llX). Exiting...\n", this->name.c_str(), i, assets->size(), this->guid);

			i++;
		}
	}
};

//
//	Assets
//
struct PatchAssetHeader_t
{
	uint32_t unknown_1 = 255; // always FF 00 00 00?
	uint32_t patchedPakCount = 0;

	PagePtr_t pPakNames;

	PagePtr_t pPakPatchNums;
};
static_assert(sizeof(PatchAssetHeader_t) == 24);

// internal data structure for storing patch_master entries before being written
struct PtchEntry
{
	std::string pakFileName = "";
	uint8_t highestPatchNum = 0;
	uint32_t pakFileNameOffset = 0;
};

#define SF_HEAD   0 // :skull:
#define SF_CPU    (1 << 0)
#define SF_TEMP   (1 << 1)
#define SF_SERVER (1 << 5)
#define SF_CLIENT (1 << 6)
#define SF_DEV    (1 << 7)
