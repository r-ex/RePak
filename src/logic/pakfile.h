#pragma once

struct _vseginfo_t
{
	unsigned int index = 0xFFFFFFFF;
	unsigned int size = 0;
};

class CPakFile
{
public:
	CPakFile(int version);

	void AddAsset(rapidjson::Value& file);
	void AddPointer(unsigned int pageIdx, unsigned int pageOffset);
	void AddGuidDescriptor(std::vector<RPakGuidDescriptor>* guids, unsigned int idx, unsigned int offset);
	void AddRawDataBlock(RPakRawDataBlock block);

	// starpaks
	void AddStarpakReference(const std::string& path);
	void AddOptStarpakReference(const std::string& path);
	SRPkDataEntry AddStarpakDataEntry(SRPkDataEntry block);

	// inlines
	inline bool IsFlagSet(int flag) { return this->m_Flags & flag; };
	inline size_t GetAssetCount() { return m_Assets.size(); };

	std::string GetStarpakPath(int i)
	{
		if (i >= 0 && i < m_vStarpakPaths.size())
			return m_vStarpakPaths[i];
		else
			return ""; // if invalid starpak is requested, return empty string
	};

	inline std::string GetPath()
	{
		return this->m_Path;
	}

	inline void SetVersion(uint32_t version)
	{
		this->m_Header.fileVersion = version;
		this->m_Version = version;
	}

	inline void SetStarpakPathsSize(int len, int optLen)
	{
		this->m_Header.starpakPathsSize = len;
		this->m_Header.optStarpakPathsSize = optLen;
	}

	inline void SetPath(std::string& path)
	{
		this->m_Path = path;
	}

	void WriteRPakRawDataBlocks(BinaryIO& out);
	void WriteAssets(BinaryIO* io);
	void WriteHeader(BinaryIO* io);

	// purpose: populates m_vFileRelations vector with combined asset relation data
	void GenerateFileRelations();
	void GenerateGuidData();

	_vseginfo_t CreateNewSegment(uint32_t size, uint32_t flags, uint32_t alignment, uint32_t vsegAlignment = -1);
	RPakAssetEntry* GetAssetByGuid(uint64_t guid, uint32_t* idx = nullptr);

private:
	RPakVirtualSegment GetMatchingSegment(uint32_t flags, uint32_t alignment, uint32_t* segidx);

	// next available starpak data offset
	unsigned __int64 m_NextStarpakOffset = 0x1000;

public:
	int m_Version = 0;
	int m_Flags = 0;

	RPakFileHeader m_Header{};

	std::vector<RPakAssetEntry> m_Assets{};

	std::string primaryStarpakPath;

	std::vector<std::string> m_vStarpakPaths{};
	std::vector<std::string> m_vOptStarpakPaths{};

	std::vector<RPakVirtualSegment> m_vVirtualSegments{};
	std::vector<RPakPageInfo> m_vPages{};
	std::vector<RPakDescriptor> m_vDescriptors{};
	std::vector<RPakGuidDescriptor> m_vGuidDescriptors{};
	std::vector<uint32_t> m_vFileRelations{};

	std::vector<RPakRawDataBlock> m_vRawDataBlocks{};
	std::vector<SRPkDataEntry> m_vStarpakDataBlocks{};

	std::string m_Path;
};