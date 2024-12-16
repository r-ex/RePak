#pragma once
#include "public/rpak.h"

#define MAX_PAK_PAGE_SIZE 0xffff

class CPakFile;

struct _vseginfo_t
{
	int index = -1;
	int size = 0;
};

class CPakVSegment
{
	friend class CPakFile;

public:
	CPakVSegment() = default;
	CPakVSegment(int index, int flags, int align, size_t initialSize) : index(index), flags(flags), alignment(align), dataSize(initialSize) {};
private:
	int index;

	int flags;
	int alignment;

	size_t dataSize;

public:

	inline int GetIndex() { return index; };
	inline int GetFlags() { return flags; };
	inline int GetAlignment() { return alignment; };

	inline void AddToDataSize(size_t size) { dataSize += size; };
	inline size_t GetDataSize() { return dataSize; };

	inline PakSegmentHdr_t GetHeader() { return { flags, alignment, dataSize }; };
};

class CPakDataChunk;

class CPakPage
{
	friend class CPakFile;

public:
	CPakPage() = default;
	CPakPage(int segIdx, int pageIdx, int flags, int align)
		: segmentIndex(segIdx), pageIndex(pageIdx), flags(flags), alignment(align), dataSize(0) {};

private:
	int segmentIndex; // index of the virtual data segment that this page is part of
	int pageIndex; // index of this page in all of the pak's pages

	int flags;
	int alignment; // required memory alignment for all data in this page

	std::vector<CPakDataChunk> chunks;

	int dataSize;

public:
	inline int GetIndex() const { return pageIndex; };
	inline int GetFlags() const { return flags; };
	inline int GetAlignment() const { return alignment; };

	PakPageHdr_t GetHeader() const { return { segmentIndex, alignment, dataSize }; };
	int GetSize() const { return dataSize; };
};

class CPakDataChunk
{
	friend class CPakFile;
	friend class CPakPage;

public:
	CPakDataChunk() = default;

	CPakDataChunk(size_t size, uint8_t alignment, char* data) : pageIndex(0), pageOffset(0), size((int)size), alignment(alignment), pData(data), released(false) {};
	CPakDataChunk(int pageIndex, int pageOffset, size_t size, uint8_t alignment, char* data) : pageIndex(pageIndex), pageOffset(pageOffset), size((int)size), alignment(alignment), pData(data), released(false) {};

private:
	char* pData;
	int pageIndex;
	int pageOffset;
	int size;
	uint8_t alignment;
	bool released;
public:

	inline PagePtr_t GetPointer(size_t offset=0) { return { pageIndex, static_cast<int>(pageOffset + offset) }; };

	inline int GetIndex() const { return pageIndex; };
	inline char* Data() { return pData; };
	inline int GetSize() const { return size; };
	inline bool IsReleased() const { return released; };

	inline void Release()
	{
		delete[] pData;
		
		this->pData = nullptr;
		this->released = true;
	}
};

class CPakFile
{
public:
	CPakFile(short version)
	{
		m_Header.fileVersion = version;
	};

	//----------------------------------------------------------------------------
	// assets
	//----------------------------------------------------------------------------

	typedef void(*AssetTypeFunc_t)(CPakFile*, const PakGuid_t, const char*, const rapidjson::Value&);

	bool AddJSONAsset(const char* const targetType, const char* const assetType, const char* const assetPath,
					  const rapidjson::Value& file, AssetTypeFunc_t func_r2 = nullptr, AssetTypeFunc_t func_r5 = nullptr);
	void AddAsset(const rapidjson::Value& file);
	void AddPointer(PagePtr_t ptr);
	void AddPointer(int pageIdx, int pageOffset);
	void AddGuidDescriptor(std::vector<PakGuidRefHdr_t>* guids, const PagePtr_t& ptr);
	void AddGuidDescriptor(std::vector<PakGuidRefHdr_t>* guids, int idx, int offset);

	FORCEINLINE void AddDependentToAsset(PakAsset_t* const dependency, const size_t dependentAssetIndex)
	{
		if(dependency)
			dependency->AddRelation(dependentAssetIndex);
	}

	// Assumes that the function is being called for the currently processing asset
	// Records the currently processing asset as a dependent asset for the provided dependency asset
	FORCEINLINE void SetCurrentAssetAsDependentForAsset(PakAsset_t* dependency)
	{
		AddDependentToAsset(dependency, m_Assets.size());
	}

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

	inline size_t GetNumPages() const { return m_vPages.size(); };

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
	void WritePageData(BinaryIO& out);

	size_t WriteStarpakPaths(BinaryIO& out, const PakStreamSet_e set);

	void WriteSegmentHeaders(BinaryIO& out);
	void WriteMemPageHeaders(BinaryIO& out);
	void WritePakDescriptors(BinaryIO& out);

	size_t EncodeStreamAndSwap(BinaryIO& io, const int compressLevel, const int workerCount);

	//----------------------------------------------------------------------------
	// starpak
	//----------------------------------------------------------------------------
	// purpose: populates m_vFileRelations vector with combined asset relation data
	void GenerateFileRelations();
	void GenerateGuidData();

	CPakPage& FindOrCreatePage(const int flags, const int alignment, const size_t newDataSize);

	CPakDataChunk CreateDataChunk(const size_t size, const int flags, const int alignment);
	//_vseginfo_t CreateNewSegment(int size, uint32_t flags, uint32_t alignment, uint32_t vsegAlignment = -1);
	CPakVSegment& FindOrCreateSegment(int flags, int alignment);

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

	std::vector<PakAsset_t> m_Assets;

	std::vector<CPakVSegment> m_vVirtualSegments;
	std::vector<CPakPage> m_vPages;
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
