#pragma once

#include <d3d11.h>
#include "math/vector.h"
#include "math/color.h"
#include "public/starpak.h"

#ifndef MAKE_FOURCC
#define MAKE_FOURCC(a,b,c,d) ((d<<24)+(c<<16)+(b<<8)+a)
#endif

#define RPAK_MAGIC (('k'<<24)+('a'<<16)+('P'<<8)+'R')
#define RPAK_EXTENSION ".rpak"

#define PAK_HEADER_FLAGS_COMPRESSED      (1<<8)
#define PAK_HEADER_FLAGS_ZSTREAM_ENCODED (1<<9) // use the ZStd decoder instead of the RTech one

#define PAK_MAX_STEM_PATH 512

// max amount of streaming files that could be opened per set for a pak, so if a
// pak uses more than one set, this number would be used per set.
#define PAK_MAX_STREAMING_FILE_HANDLES_PER_SET 4

#define TYPE_TXTR	MAKE_FOURCC('t', 'x', 't', 'r') // txtr
#define TYPE_TXAN	MAKE_FOURCC('t', 'x', 'a', 'n') // txan
#define TYPE_RMDL	MAKE_FOURCC('m', 'd', 'l', '_') // mdl_
#define TYPE_UIMG	MAKE_FOURCC('u', 'i', 'm', 'g') // uimg
#define TYPE_PTCH	MAKE_FOURCC('P', 't', 'c', 'h') // Ptch
#define TYPE_DTBL	MAKE_FOURCC('d', 't', 'b', 'l') // dtbl
#define TYPE_MATL	MAKE_FOURCC('m', 'a', 't', 'l') // matl
#define TYPE_ASEQ	MAKE_FOURCC('a', 's', 'e', 'q') // aseq
#define TYPE_ARIG	MAKE_FOURCC('a', 'r', 'i', 'g') // arig
#define TYPE_SHDS	MAKE_FOURCC('s', 'h', 'd', 's') // shds
#define TYPE_SHDR	MAKE_FOURCC('s', 'h', 'd', 'r') // shdr

enum class AssetType : uint32_t
{
	NONE = 0, // !!!INVALID TYPE!!!

	TXTR = TYPE_TXTR, // texture
	TXAN = TYPE_TXAN, // texture animation
	RMDL = TYPE_RMDL, // model
	UIMG = TYPE_UIMG, // ui image atlas
	PTCH = TYPE_PTCH, // patch
	DTBL = TYPE_DTBL, // datatable
	MATL = TYPE_MATL, // material
	ASEQ = TYPE_ASEQ, // animation sequence
	ARIG = TYPE_ARIG, // animation rig
	SHDS = TYPE_SHDS, // shaderset
	SHDR = TYPE_SHDR, // shader
};

#pragma pack(push, 1)

// represents a "pointer" into a mempage by page index and offset
// when loaded, these get converted to a real pointer in the runtime.
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

	uint16_t fileVersion = 0x8;
	uint16_t flags = 0;
	FILETIME fileTime{};
	char  unk0[0x8]{};
	uint64_t compressedSize = 0; // size of the rpak file on disk before decompression
	uint64_t embeddedStarpakOffset = 0;
	char  unk1[0x8]{};
	uint64_t decompressedSize = 0; // actual data size of the rpak file after decompression
	uint64_t embeddedStarpakSize = 0;
	char  unk2[0x8]{};
	uint16_t starpakPathsSize = 0; // size in bytes of the section containing mandatory starpak paths
	uint16_t optStarpakPathsSize = 0; // size in bytes of the section containing optional starpak paths
	uint16_t memSlabCount = 0;
	uint16_t memPageCount = 0; // number of "mempages" in the rpak
	uint16_t patchIndex = 0;
	uint16_t alignment = 0;
	uint32_t pointerCount = 0;
	uint32_t assetCount = 0;
	uint32_t usesCount = 0;
	uint32_t dependentsCount = 0;

	// only in tf2, related to external
	uint32_t unk7count = 0;
	uint32_t unk8count = 0;

	// only in apex
	char  unk3[0x1c]{};
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
struct PakSlabHdr_s
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
struct PakPageHdr_s
{
	int slabIndex; // index into vseg array
	int alignment; // alignment size when buffer is allocated
	int dataSize; // actual size of page in bytes
};
#pragma pack(pop)

struct PakGuidRef_s
{
	inline bool operator<(const PakGuidRef_s& b) const
	{
		return (ptr < b.ptr);
	}

	PagePtr_t ptr;

	// this field is only used by repak.
	PakGuid_t guid;
};

// defines a bunch of values for registering/using an asset from the rpak
struct PakAsset_t
{
	PakAsset_t()
	{
		// the asset always depends on itself, and therefore this value
		// should always be at least 1 if the asset is added.
		internalDependencyCount = 1;
	};

	void InitAsset(
		const PagePtr_t pHeadPtr,
		const uint32_t nHeaderSize,
		const PagePtr_t pCpuPtr,
		const uint32_t nVersion,
		const AssetType type,
		const int64_t nStarpakOffset = -1,
		const int64_t nStarpakIndex = -1,
		const int64_t nOptStarpakOffset = -1,
		const int64_t nOptStarpakIndex = -1
	)
	{
		this->headPtr = pHeadPtr;
		this->cpuPtr = pCpuPtr;
		this->starpakOffset = nStarpakOffset;
		this->starpakIndex = nStarpakIndex;
		this->optStarpakOffset = nOptStarpakOffset;
		this->optStarpakIndex = nOptStarpakIndex;
		this->headDataSize = nHeaderSize;
		this->version = nVersion;
		this->id = type;
	}

	// hashed version of the asset path
	// used for referencing the asset from elsewhere
	//
	// - when referenced from other assets, the GUID is used directly
	// - when referenced from scripts, the GUID is calculated from the original asset path
	//   by a function such as RTech::StringToGuid
	PakGuid_t guid = 0;
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
	int64_t starpakOffset : 52 = -1;
	int64_t starpakIndex : 12 = -1;
	int64_t optStarpakOffset : 52 = -1;
	int64_t optStarpakIndex : 12 = -1;

	uint16_t pageEnd = 0; // highest mem page used by this asset

	// internal asset dependency count, which counts the total number of assets
	// that are in the same pak as this asset, and are needed for this asset
	// to work. the dependency count also includes the asset itself; the asset
	// depends on itself. the runtime decrements this value atomically when
	// processing the internal dependencies until it reaches 1, and then starts
	// loading this asset.
	short internalDependencyCount;

	// start index for this asset's dependents/dependencies in respective arrays
	uint32_t dependentsIndex = 0;
	uint32_t usesIndex = 0;

	uint32_t dependentsCount = 0; // number of local assets that use this asset
	uint32_t usesCount = 0; // number of local assets that are used by this asset

	// size of the asset header
	uint32_t headDataSize = 0;

	// this isn't always changed when the asset gets changed
	// but respawn calls it a version so i will as well
	uint32_t version = 0;

	// see AssetType enum
	AssetType id = AssetType::NONE;

	// internal
public:
	int _assetidx;
	std::string name;

	void* header = nullptr;

	// Extra information about the asset that is made available to other assets when being created.
	std::shared_ptr<void> _publicData;

	// vector of indexes for local assets that use this asset
	std::vector<unsigned int> _dependents;

	// guid reference pointers
	std::vector<PakGuidRef_s> _uses;

	FORCEINLINE void SetHeaderPointer(void* pHeader) { this->header = pHeader; };

	template <typename T>
	inline void SetPublicData(T* const data)
	{
		std::shared_ptr<T> ptr(data);
		_publicData = std::move(ptr);
	}

	char* const PublicData() { return reinterpret_cast<char*>(_publicData.get()); };

	FORCEINLINE void AddDependent(const unsigned int idx) { _dependents.push_back({ idx }); };
	FORCEINLINE void AddDependent(const size_t idx) { _dependents.push_back({ static_cast<unsigned int>(idx) }); };

	FORCEINLINE void AddGuid(const PagePtr_t desc, const PakGuid_t assetGuid) { _uses.push_back({ desc, assetGuid }); };
	FORCEINLINE void ExpandGuidBuf(const size_t amount) { _uses.reserve(_uses.size() + amount); }

	FORCEINLINE bool IsType(uint32_t type) const
	{
		return static_cast<uint32_t>(id) == type;
	}

	FORCEINLINE void EnsureType(uint32_t type) const
	{
		if (!IsType(type))
		{
			Utils::FourCCString_t expected;
			Utils::FourCCString_t found;

			Utils::FourCCToString(expected, type);
			Utils::FourCCToString(found, type);

			Error("Unexpected asset type for \"%s\". Expected '%.4s', found '%.4s'.\n", this->name.c_str(), expected, found);
		}
	}

	FORCEINLINE int64_t GetPackedStreamOffset() const { return (starpakOffset & 0xFFFFFFFFFFFFF000) | (starpakIndex & 0xFFF); }
	FORCEINLINE int64_t GetPackedOptStreamOffset() const { return (optStarpakOffset & 0xFFFFFFFFFFFFF000) | (optStarpakIndex & 0xFFF); }
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
	std::string pakFileName;
	uint8_t highestPatchNum = 0;
	uint32_t pakFileNameOffset = 0;
};

#define SF_HEAD   0 // :skull:
#define SF_CPU    (1 << 0)
#define SF_TEMP   (1 << 1)
#define SF_SERVER (1 << 5)
#define SF_CLIENT (1 << 6)
#define SF_DEV    (1 << 7)
