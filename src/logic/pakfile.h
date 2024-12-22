#pragma once
#include "public/rpak.h"
#include "pakpage.h"

#define PAK_MAX_PAGE_MERGE_SIZE 0xffff

struct _vseginfo_t
{
	int index = -1;
	int size = 0;
};

class CPakFileBuilder
{
public:
	CPakFileBuilder(short version)
	{
		m_Header.fileVersion = version;
	};

	//----------------------------------------------------------------------------
	// assets
	//----------------------------------------------------------------------------

	typedef void(*AssetTypeFunc_t)(CPakFileBuilder*, const PakGuid_t, const char*, const rapidjson::Value&);

	bool AddJSONAsset(const char* const targetType, const char* const assetType, const char* const assetPath,
					  const rapidjson::Value& file, AssetTypeFunc_t func_r2 = nullptr, AssetTypeFunc_t func_r5 = nullptr);
	void AddAsset(const rapidjson::Value& file);
	void AddPointer(PagePtr_t ptr);
	void AddPointer(int pageIdx, int pageOffset);

	void AddStarpakReference(const std::string& path);
	void AddOptStarpakReference(const std::string& path);

	void AddStreamingDataEntry(PakStreamSetEntry_s& block, const uint8_t* const data, const PakStreamSet_e set);

	//----------------------------------------------------------------------------
	// inlines
	//----------------------------------------------------------------------------
	inline bool IsFlagSet(int flag) const { return m_Flags & flag; };

	inline size_t GetAssetCount() const { return m_Assets.size(); };
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

	inline std::string GetPath() const { return m_Path; }
	inline void SetPath(const std::string& path) { m_Path = path; }

	inline std::string GetAssetPath() const { return m_AssetPath; }
	inline void SetAssetPath(const std::string& assetPath) { m_AssetPath = assetPath; }

	inline size_t GetNumStarpakPaths() const { return m_mandatoryStreamFilePaths.size(); }

	inline size_t GetCompressedSize() const { return m_Header.compressedSize; }
	inline size_t GetDecompressedSize() const { return m_Header.decompressedSize; }

	inline void SetCompressedSize(size_t size) { m_Header.compressedSize = size; }
	inline void SetDecompressedSize(size_t size) { m_Header.decompressedSize = size; }

	inline FILETIME GetFileTime() const { return m_Header.fileTime; }
	inline void SetFileTime(FILETIME fileTime) { m_Header.fileTime = fileTime; }

	inline void AddFlags(int flags) { m_Flags |= flags; }
	inline void RemoveFlags(int flags) { m_Flags &= ~flags; }

	//----------------------------------------------------------------------------
	// rpak
	//----------------------------------------------------------------------------
	void WriteHeader(BinaryIO& io);
	void WriteAssets(BinaryIO& io);

	size_t WriteStarpakPaths(BinaryIO& out, const PakStreamSet_e set);
	void WritePakDescriptors(BinaryIO& out);

	size_t EncodeStreamAndSwap(BinaryIO& io, const int compressLevel, const int workerCount);

	void GenerateInternalDependencies();
	void GenerateFileRelations();
	void GenerateGuidData();

	PakPageLump_s CreatePageLump(const size_t size, const int flags, const int alignment, void* const buf = nullptr);

	PakAsset_t* GetAssetByGuid(const PakGuid_t guid, uint32_t* const idx = nullptr, const bool silent = false);

	FORCEINLINE void RequireUniqueAssetGUID(const PakAsset_t& asset)
	{
		uint32_t assetIdx = UINT32_MAX;
		PakAsset_t* match = GetAssetByGuid(asset.guid, &assetIdx, true);
		if (match != nullptr)
		{
			Error("Attempted to create asset with a non-unique GUID.\n"
				"Assets at index %u (%s) and %u (%s) have the same GUID (%llx).\n",
				assetIdx, match->name.c_str(),
				static_cast<uint32_t>(m_Assets.size()), asset.name.c_str(),
				asset.guid
			);
		}
	}

	FORCEINLINE void PushAsset(const PakAsset_t& asset)
	{
		RequireUniqueAssetGUID(asset);
		m_Assets.push_back(asset);
	};

	void CreateStreamFileStream(const char* const path, const PakStreamSet_e set);
	void FinishStreamFileStream(const PakStreamSet_e set);

	void BuildFromMap(const string& mapPath);

private:
	friend class CPakPage;

	int m_Flags = 0;
	PakHdr_t m_Header;

	std::string m_Path;
	std::string m_AssetPath;
	std::string m_OutputPath;

	CPakPageBuilder m_pageBuilder;

	std::vector<PakAsset_t> m_Assets;
	std::vector<PakPointerHdr_t> m_vPakDescriptors;
	std::vector<PakGuidRefHdr_t> m_vGuidDescriptors;
	std::vector<uint32_t> m_vFileRelations;

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
inline PakAsset_t* Pak_RegisterGuidRefAtOffset(CPakFileBuilder* const pak, const PakGuid_t guid, const size_t offset, 
	PakPageLump_s& chunk, PakAsset_t& asset, PakAsset_t* targetAsset = nullptr)
{
	// NULL guids should never be added. we check it here because otherwise we
	// have to do a check at call site, and if we miss one we will end up with
	// a hard to track bug. so always call this function, even if your guid
	// might be NULL.
	if (guid == 0)
		return nullptr;

	asset.AddGuid(chunk.GetPointer(offset), guid);

	if (!targetAsset)
	{
		targetAsset = pak->GetAssetByGuid(guid, nullptr, true);

		if (!targetAsset)
			return nullptr;
	}

	return targetAsset;
}
