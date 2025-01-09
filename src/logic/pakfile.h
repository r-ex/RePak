#pragma once
#include "public/rpak.h"
#include "pakpage.h"

class CPakFileBuilder
{
	enum class AssetScope_e
	{
		kServerOnly,
		kClientOnly,
		kAll,
	};

public:
	CPakFileBuilder() {};

	//----------------------------------------------------------------------------
	// assets
	//----------------------------------------------------------------------------

	typedef void(*AssetTypeFunc_t)(CPakFileBuilder*, const PakGuid_t, const char*, const rapidjson::Value&);

	bool AddJSONAsset(const char* const targetType, const char* const assetType, const char* const assetPath,
			const AssetScope_e assetScope, const rapidjson::Value& file, AssetTypeFunc_t func_r2 = nullptr, AssetTypeFunc_t func_r5 = nullptr);
	void AddAsset(const rapidjson::Value& file);

	void AddPointer(PakPageLump_s& pointerLump, const size_t pointerOffset, const PakPageLump_s& dataLump, const size_t dataOffset);
	void AddPointer(PakPageLump_s& pointerLump, const size_t pointerOffset);

	void AddStarpakReference(const std::string& path);
	void AddOptStarpakReference(const std::string& path);

	void AddStreamingDataEntry(PakStreamSetEntry_s& block, const uint8_t* const data, const PakStreamSet_e set);

	//----------------------------------------------------------------------------
	// inlines
	//----------------------------------------------------------------------------
	inline bool IsFlagSet(int flag) const { return m_buildFlags & flag; };

	inline size_t GetAssetCount() const { return m_assets.size(); };
	inline size_t GetMandatoryStreamingAssetCount() const { return m_mandatoryStreamingDataBlocks.size(); };
	inline size_t GetOptionalStreamingAssetCount() const { return m_optionalStreamingDataBlocks.size(); };

	inline uint16_t GetNumPages() const { return m_pageBuilder.GetPageCount(); };

	inline uint32_t GetVersion() const { return m_Header.fileVersion; }
	inline void SetVersion(const uint16_t version)
	{
		if ((version != 7) && (version != 8))
		{
			Error("Unsupported pak file version %hu.\n", version);
		}

		m_Header.fileVersion = version;
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

	inline size_t GetNumStarpakPaths() const { return m_mandatoryStreamFilePaths.size(); }

	inline size_t GetCompressedSize() const { return m_Header.compressedSize; }
	inline size_t GetDecompressedSize() const { return m_Header.decompressedSize; }

	inline void SetCompressedSize(size_t size) { m_Header.compressedSize = size; }
	inline void SetDecompressedSize(size_t size) { m_Header.decompressedSize = size; }

	inline FILETIME GetFileTime() const { return m_Header.fileTime; }
	inline void SetFileTime(FILETIME fileTime) { m_Header.fileTime = fileTime; }

	inline void AddFlags(int flags) { m_buildFlags |= flags; }
	inline void RemoveFlags(int flags) { m_buildFlags &= ~flags; }

	//----------------------------------------------------------------------------
	// rpak
	//----------------------------------------------------------------------------
	void WriteHeader(BinaryIO& io);
	void WriteAssetDescriptors(BinaryIO& io);
	void WriteAssetUses(BinaryIO& io);
	void WriteAssetDependents(BinaryIO& io);

	size_t WriteStarpakPaths(BinaryIO& out, const PakStreamSet_e set);
	void WritePagePointers(BinaryIO& out);

	size_t EncodeStreamAndSwap(BinaryIO& io, const int compressLevel, const int workerCount);

	void GenerateInternalDependencies();
	void GenerateAssetDependents();
	void GenerateAssetUses();

	PakPageLump_s CreatePageLump(const size_t size, const int flags, const int alignment, void* const buf = nullptr);

	PakAsset_t* GetAssetByGuid(const PakGuid_t guid, uint32_t* const idx = nullptr, const bool silent = false);

	FORCEINLINE void RequireUniqueAssetGUID(const PakAsset_t& asset)
	{
		uint32_t assetIdx = UINT32_MAX;
		PakAsset_t* match = GetAssetByGuid(asset.guid, &assetIdx, true);
		if (match != nullptr && match != &asset)
		{
			Error("Attempted to create asset with a non-unique GUID.\n"
				"Assets at index %u (%s) and %u (%s) have the same GUID (%llx).\n",
				assetIdx, match->name.c_str(),
				static_cast<uint32_t>(m_assets.size()), asset.name.c_str(),
				asset.guid
			);
		}
	}

	FORCEINLINE PakAsset_t& BeginAsset(const PakGuid_t assetGuid, const char* const assetPath)
	{
		// Only one asset can be processed at a time! This only asserts when
		// another asset is being created while we are still working on one,
		// or when 'FinishAsset()' wasn't called after everything was done.
		assert(!m_processingAsset);
		m_processingAsset = true;

		PakAsset_t& asset = m_assets.emplace_back();

		asset.guid = assetGuid;
		asset.name = assetPath;

		RequireUniqueAssetGUID(asset);

		return asset;
	}

	FORCEINLINE void FinishAsset()
	{
		PakAsset_t& asset = m_assets.back();
		asset.pageEnd = GetNumPages();

		m_processingAsset = false;
	};

	void CreateStreamFileStream(const char* const path, const PakStreamSet_e set);
	void FinishStreamFileStream(const PakStreamSet_e set);

	void BuildFromMap(const string& mapPath);

private:
	friend class CPakPage;

	int m_buildFlags = 0;
	bool m_processingAsset = false;

	PakHdr_t m_Header;

	std::string m_pakFilePath;
	std::string m_assetPath;
	std::string m_outputPath;

	std::vector<PakAsset_t> m_assets;
	std::vector<PagePtr_t> m_pagePointers;

	CPakPageBuilder m_pageBuilder;

	std::vector<std::string> m_mandatoryStreamFilePaths;
	std::vector<std::string> m_optionalStreamFilePaths;

	std::vector<PakStreamSetEntry_s> m_mandatoryStreamingDataBlocks;
	std::vector<PakStreamSetEntry_s> m_optionalStreamingDataBlocks;

	BinaryIO m_mandatoryStreamFile;
	BinaryIO m_optionalStreamFile;

	// next available starpak data offset
	uint64_t m_nextMandatoryStarpakOffset = STARPAK_DATABLOCK_ALIGNMENT;
	uint64_t m_nextOptionalStarpakOffset = STARPAK_DATABLOCK_ALIGNMENT;
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
