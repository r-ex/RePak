#pragma once
#include "public/rpak.h"

struct _vseginfo_t
{
	int index = -1;
	int size = 0;
};

#define MAX_PAK_PAGE_SIZE 0xffff

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
	CPakPage(CPakFile* pak, int segIdx, int pageIdx, int flags, int align) :
		pak(pak), segmentIndex(segIdx), pageIndex(pageIdx), flags(flags), alignment(align), dataSize(0) {};

private:
	CPakFile* pak;

	int segmentIndex; // index of the virtual data segment that this page is part of
	int pageIndex; // index of this page in all of the pak's pages

	int flags;
	int alignment; // required memory alignment for all data in this page

	std::vector<CPakDataChunk> chunks;

	int dataSize;

public:
	inline int GetIndex() { return pageIndex; };
	inline int GetFlags() { return flags; };
	inline int GetAlignment() { return alignment; };

	PakPageHdr_t GetHeader() { return { segmentIndex, alignment, dataSize }; };
	int GetSize() { return dataSize; };

	void AddDataChunk(CPakDataChunk& chunk);
	void PadPageToChunkAlignment(uint8_t alignment);
};

class CPakDataChunk
{
	friend class CPakPage;

public:
	CPakDataChunk() = default;

	CPakDataChunk(size_t size, uint8_t alignment, char* data) : size((int)size), alignment(alignment), pData(data), released(false) {};
	CPakDataChunk(int pageIndex, int pageOffset, size_t size, uint8_t alignment, char* data) : pageIndex(pageIndex), pageOffset(pageOffset), size((int)size), alignment(alignment), pData(data), released(false) {};

private:
	int pageIndex;
	int pageOffset;
	int size;

	char* pData;
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

	typedef void(*AssetTypeFunc_t)(CPakFile*, const char*, rapidjson::Value&);

	bool AddJSONAsset(const char* type, rapidjson::Value& file, AssetTypeFunc_t func_r2 = nullptr, AssetTypeFunc_t func_r5 = nullptr);
	void AddAsset(rapidjson::Value& file);
	void AddPointer(PagePtr_t ptr);
	void AddPointer(int pageIdx, int pageOffset);
	void AddGuidDescriptor(std::vector<PakGuidRefHdr_t>* guids, const PagePtr_t& ptr);
	void AddGuidDescriptor(std::vector<PakGuidRefHdr_t>* guids, int idx, int offset);

	FORCEINLINE void AddDependentToAsset(PakAsset_t* dependency, size_t dependentAssetIndex)
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
	StreamableDataEntry AddStarpakDataEntry(StreamableDataEntry block);

	//----------------------------------------------------------------------------
	// inlines
	//----------------------------------------------------------------------------
	inline bool IsFlagSet(int flag) const { return m_Flags & flag; };

	inline size_t GetAssetCount() const { return m_Assets.size(); };
	inline size_t GetStreamingAssetCount() const { return m_vStarpakDataBlocks.size(); };
	inline size_t GetNumPages() const { return m_vPages.size(); };

	inline uint32_t GetVersion() const { return m_Header.fileVersion; }
	inline void SetVersion(int version) { m_Header.fileVersion = (short)version; }

	inline void SetStarpakPathsSize(uint16_t len, uint16_t optLen)
	{
		m_Header.starpakPathsSize = len;
		m_Header.optStarpakPathsSize = optLen;
	}

	inline std::string GetPath() const { return m_Path; }
	inline void SetPath(const std::string& path) { m_Path = path; }

	inline std::string GetAssetPath() const { return m_AssetPath; }
	inline void SetAssetPath(const std::string& assetPath) { m_AssetPath = assetPath; }

	inline std::string GetStarpakPath(int i) const
	{
		if (i >= 0 && i < m_vStarpakPaths.size())
			return m_vStarpakPaths[i];
		else
			return ""; // if invalid starpak is requested, return empty string
	};

	inline std::string GetPrimaryStarpakPath() const { return m_PrimaryStarpakPath; };
	inline size_t GetNumStarpakPaths() const { return m_vStarpakPaths.size(); }
	inline void SetPrimaryStarpakPath(const std::string& path) { m_PrimaryStarpakPath = path; }

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

	size_t WriteStarpakPaths(BinaryIO& out, bool optional = false);

	void WriteSegmentHeaders(BinaryIO& out);
	void WriteMemPageHeaders(BinaryIO& out);
	void WritePakDescriptors(BinaryIO& out);

	//----------------------------------------------------------------------------
	// starpak
	//----------------------------------------------------------------------------
	void WriteStarpakDataBlocks(BinaryIO& io);
	void WriteStarpakSortsTable(BinaryIO& io);

	void FreeStarpakDataBlocks();

	// purpose: populates m_vFileRelations vector with combined asset relation data
	void GenerateFileRelations();
	void GenerateGuidData();

	CPakPage& FindOrCreatePage(int flags, int alignment, size_t newDataSize);

	CPakDataChunk CreateDataChunk(size_t size, int flags, int alignment);
	//_vseginfo_t CreateNewSegment(int size, uint32_t flags, uint32_t alignment, uint32_t vsegAlignment = -1);
	CPakVSegment& FindOrCreateSegment(int flags, int alignment);

	PakAsset_t* GetAssetByGuid(uint64_t guid, uint32_t* idx = nullptr, bool silent = false);

	FORCEINLINE void RequireUniqueAssetGUID(const PakAsset_t& asset)
	{
		uint32_t assetIdx = UINT32_MAX;
		PakAsset_t* match = GetAssetByGuid(asset.guid, &assetIdx, true);
		if (match != nullptr)
		{
			Error("Attempted to create asset with a non-unique GUID."
				"\nAssets at index %u (%s) and %u (%s) have the same GUID (%llx).\n",
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

	void BuildFromMap(const string& mapPath);

private:
	friend class CPakPage;

	// next available starpak data offset
	uint64_t m_NextStarpakOffset = 0x1000;
	int m_Flags = 0;

	PakHdr_t m_Header;

	std::string m_Path;
	std::string m_AssetPath;
	std::string m_PrimaryStarpakPath;

	std::vector<PakAsset_t> m_Assets;

	std::vector<std::string> m_vStarpakPaths;
	std::vector<std::string> m_vOptStarpakPaths;

	std::vector<CPakVSegment> m_vVirtualSegments;
	std::vector<CPakPage> m_vPages;
	std::vector<PakPointerHdr_t> m_vPakDescriptors;
	std::vector<PakGuidRefHdr_t> m_vGuidDescriptors;
	std::vector<uint32_t> m_vFileRelations;

	std::vector<StreamableDataEntry> m_vStarpakDataBlocks;
};