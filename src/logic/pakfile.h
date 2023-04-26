#pragma once
#include "public/rpak.h"

struct _vseginfo_t
{
	unsigned int index = 0xFFFFFFFF;
	unsigned int size = 0;
};

class CPakFile
{
public:
	CPakFile(int version);

	//----------------------------------------------------------------------------
	// assets
	//----------------------------------------------------------------------------
	void AddAsset(rapidjson::Value& file);
	void AddPointer(unsigned int pageIdx, unsigned int pageOffset);
	void AddGuidDescriptor(std::vector<PakGuidRefHdr_t>* guids, unsigned int idx, unsigned int offset);
	void AddRawDataBlock(PakRawDataBlock_t block);

	void AddStarpakReference(const std::string& path);
	void AddOptStarpakReference(const std::string& path);
	StreamableDataEntry AddStarpakDataEntry(StreamableDataEntry block);

	//----------------------------------------------------------------------------
	// inlines
	//----------------------------------------------------------------------------
	inline bool IsFlagSet(int flag) const { return m_Flags & flag; };

	inline size_t GetAssetCount() const { return m_Assets.size(); };
	inline size_t GetStreamingAssetCount() const { return m_vStarpakDataBlocks.size(); }

	inline uint32_t GetVersion() const { return m_Header.fileVersion; }
	inline void SetVersion(uint32_t version) { m_Header.fileVersion = version; }

	inline void SetStarpakPathsSize(int len, int optLen)
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
	void WriteRawDataBlocks(BinaryIO& out);

	size_t WriteStarpakPaths(BinaryIO& out, bool optional = false);

	void WriteVirtualSegments(BinaryIO& out);
	void WritePages(BinaryIO& out);
	void WritePakDescriptors(BinaryIO& out);
	void WriteGuidDescriptors(BinaryIO& out);
	void WriteFileRelations(BinaryIO& out);

	//----------------------------------------------------------------------------
	// starpak
	//----------------------------------------------------------------------------
	void WriteStarpakDataBlocks(BinaryIO& io);
	void WriteStarpakSortsTable(BinaryIO& io);

	void FreeRawDataBlocks();
	void FreeStarpakDataBlocks();

	// purpose: populates m_vFileRelations vector with combined asset relation data
	void GenerateFileRelations();
	void GenerateGuidData();

	_vseginfo_t CreateNewSegment(uint32_t size, uint32_t flags, uint32_t alignment, uint32_t vsegAlignment = -1);

	PakAsset_t* GetAssetByGuid(uint64_t guid, uint32_t* idx = nullptr);

	void BuildFromMap(const string& mapPath);

private:

	PakSegmentHdr_t GetMatchingSegment(uint32_t flags, uint32_t alignment, uint32_t* segidx);

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

	std::vector<PakSegmentHdr_t> m_vVirtualSegments;
	std::vector<PakPageHdr_t> m_vPages;
	std::vector<PakPointerHdr_t> m_vPakDescriptors;
	std::vector<PakGuidRefHdr_t> m_vGuidDescriptors;
	std::vector<uint32_t> m_vFileRelations;

	std::vector<PakRawDataBlock_t> m_vRawDataBlocks;
	std::vector<StreamableDataEntry> m_vStarpakDataBlocks;
};