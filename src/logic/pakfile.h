#pragma once
#include "public/rpak.h"
#include "pakpage.h"
#include "buildsettings.h"
#include "streamfile.h"

struct PakStreamSetEntry_s
{
	PakStreamSetEntry_s()
	{
		streamOffset = -1;
		streamIndex = -1;
	}

	int64_t streamOffset : 52;
	int64_t streamIndex : 12;
};

class CPakFileBuilder
{
	friend class CPakPage;

	enum class AssetScope_e
	{
		kServerOnly,
		kClientOnly,
		kAll,
	};

public:
	CPakFileBuilder(const CBuildSettings* const buildSettings, CStreamFileBuilder* const streamBuilder);

	//----------------------------------------------------------------------------
	// assets
	//----------------------------------------------------------------------------

	typedef void(*AssetTypeFunc_t)(CPakFileBuilder*, const PakGuid_t, const char*, const rapidjson::Value&);

	bool AddJSONAsset(const char* const targetType, const char* const assetType, const char* const assetPath,
			const AssetScope_e assetScope, const rapidjson::Value& file, AssetTypeFunc_t func_r2 = nullptr, AssetTypeFunc_t func_r5 = nullptr);
	void AddAsset(const rapidjson::Value& file);

	void AddPointer(PakPageLump_s& pointerLump, const size_t pointerOffset, const PakPageLump_s& dataLump, const size_t dataOffset);
	void AddPointer(PakPageLump_s& pointerLump, const size_t pointerOffset);

	int64_t AddStreamingFileReference(const char* const path, const bool mandatory);

	PakStreamSetEntry_s AddStreamingDataEntry(const int64_t size, const uint8_t* const data, const PakStreamSet_e set);

	//----------------------------------------------------------------------------
	// inlines
	//----------------------------------------------------------------------------
	inline bool IsFlagSet(const int flag) const { return m_buildSettings->IsFlagSet(flag); };

	inline size_t GetAssetCount() const { return m_assets.size(); };
	inline uint16_t GetNumPages() const { return m_pageBuilder.GetPageCount(); };

	inline uint16_t GetVersion() const { return m_Header.fileVersion; }
	void SetVersion(const uint16_t version);

	inline size_t GetMaxStreamingFileHandlesPerSet() const
	{
		return GetVersion() == 7
			? PAK_MAX_STREAMING_FILE_HANDLES_PER_SET_V7
			: PAK_MAX_STREAMING_FILE_HANDLES_PER_SET_V8;
	}

	inline void SetStarpakPathsSize(uint16_t len, uint16_t optLen)
	{
		m_Header.starpakPathsSize = len;
		m_Header.optStarpakPathsSize = optLen;
	}

	inline std::string GetPath() const { return m_pakFilePath; }
	inline void SetPath(const std::string& path) { m_pakFilePath = path; }

	inline std::string GetAssetPath() const { return m_assetPath; }
	inline void SetAssetPath(const std::string& assetPath) { m_assetPath = assetPath; }

	inline size_t GetCompressedSize() const { return m_Header.compressedSize; }
	inline size_t GetDecompressedSize() const { return m_Header.decompressedSize; }

	inline void SetCompressedSize(size_t size) { m_Header.compressedSize = size; }
	inline void SetDecompressedSize(size_t size) { m_Header.decompressedSize = size; }

	inline FILETIME GetFileTime() const { return m_Header.fileTime; }
	inline void SetFileTime(FILETIME fileTime) { m_Header.fileTime = fileTime; }

	//----------------------------------------------------------------------------
	// rpak
	//----------------------------------------------------------------------------
	void WriteHeader(BinaryIO& io);
	void WriteAssetDescriptors(BinaryIO& io);
	void WriteAssetUses(BinaryIO& io);
	void WriteAssetDependents(BinaryIO& io);

	size_t WriteStarpakPaths(BinaryIO& out, const PakStreamSet_e set);
	void WritePagePointers(BinaryIO& out);

	void GenerateInternalDependencies();
	void GenerateAssetDependents();
	void GenerateAssetUses();

	PakPageLump_s CreatePageLump(const size_t size, const int flags, const int alignment, void* const buf = nullptr);
	PakAsset_t* GetAssetByGuid(const PakGuid_t guid, size_t* const idx = nullptr, const bool silent = false);

	FORCEINLINE PakAsset_t& BeginAsset(const PakGuid_t assetGuid, const char* const assetPath)
	{
		// Only one asset can be processed at a time! This only asserts when
		// another asset is being created while we are still working on one,
		// or when 'FinishAsset()' wasn't called after everything was done.
		assert(!m_processingAsset);

		size_t assetIdx = SIZE_MAX;
		const PakAsset_t* const match = GetAssetByGuid(assetGuid, &assetIdx, true);

		if (match) // Asset was already added or GUID is colliding.
		{
			if (match->name.compare(assetPath) == 0)
			{
				Error("Asset \"%s\" was already added at index #%zu!\n",
					assetPath, assetIdx);
			}
			else // Collision (could be non-unique GUID override or an actual collision).
			{
				Error("Asset \"%s\" has GUID %llX which collides with asset \"%s\" at index #%zu!\n",
					assetPath, assetGuid, match->name.c_str(), assetIdx);
			}
		}

		m_processingAsset = true;
		PakAsset_t& asset = m_assets.emplace_back();

		asset.guid = assetGuid;
		asset.name = assetPath;

		return asset;
	}

	FORCEINLINE void FinishAsset()
	{
		PakAsset_t& asset = m_assets.back();
		asset.pageEnd = GetNumPages();

		m_processingAsset = false;
	};

	void BuildFromMap(const js::Document& doc);

private:
	const CBuildSettings* m_buildSettings;
	CStreamFileBuilder* m_streamBuilder;

	bool m_processingAsset = false;

	PakHdr_t m_Header;

	std::string m_pakFilePath;
	std::string m_assetPath;

	std::vector<PakAsset_t> m_assets;
	std::vector<PagePtr_t> m_pagePointers;

	CPakPageBuilder m_pageBuilder;

	std::vector<std::string> m_mandatoryStreamFilePaths;
	std::vector<std::string> m_optionalStreamFilePaths;
};

// if the asset already existed, the function will return true.
inline bool Pak_RegisterGuidRefAtOffset(const PakGuid_t guid, const size_t offset, 
	PakPageLump_s& chunk, PakAsset_t& asset)
{
	// NULL guids should never be added. we check it here because otherwise we
	// have to do a check at call site, and if we miss one we will end up with
	// a hard to track bug. so always call this function, even if your guid
	// might be NULL.
	if (guid == 0)
		return false;

	asset.AddGuid(chunk.GetPointer(offset), guid);
	return true;
}

// gets the pak file header size based on pak version
inline size_t Pak_GetHeaderSize(const uint16_t version)
{
	switch (version)
	{
		// todo(amos): we probably should import headers for both
		// versions and do a sizeof here.
	case 7: return 0x58;
	case 8: return 0x80;
	default: assert(0); return 0;
	};
}

static inline bool Pak_IsVersionSupported(const int version)
{
	switch (version)
	{
	case 7:
	case 8:
		return true;
	default:
		return false;
	}
}

inline const char* Pak_EncodeAlgorithmToString(const uint16_t flags)
{
	if (flags & PAK_HEADER_FLAGS_RTECH_ENCODED)
		return "RTech";
	if (flags & PAK_HEADER_FLAGS_OODLE_ENCODED)
		return "Oodle";
	if (flags & PAK_HEADER_FLAGS_ZSTD_ENCODED)
		return "ZStd";

	return "an unknown algorithm";
}

extern size_t Pak_EncodeStreamAndSwap(BinaryIO& io, const int compressLevel, const int workerCount, const uint16_t pakVersion, const char* const pakPath);
extern size_t Pak_DecodeStreamAndSwap(BinaryIO& io, const uint16_t pakVersion, const char* const pakPath);
