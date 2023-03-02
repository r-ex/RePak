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
	inline bool IsFlagSet(int flag) const { return m_Flags & flag; };

	inline std::string GetPath() const { return m_Path; }
	inline size_t GetAssetCount() const { return m_Assets.size(); };

	inline std::string GetStarpakPath(int i) const
	{
		if (i >= 0 && i < m_vStarpakPaths.size())
			return m_vStarpakPaths[i];
		else
			return ""; // if invalid starpak is requested, return empty string
	};

	inline void SetVersion(uint32_t version)
	{
		m_Header.fileVersion = version;
		m_Version = version;
	}

	inline void SetStarpakPathsSize(int len, int optLen)
	{
		m_Header.starpakPathsSize = len;
		m_Header.optStarpakPathsSize = optLen;
	}

	inline void SetPath(const std::string& path)
	{
		m_Path = path;
	}

	void WriteHeader(BinaryIO* io);
	void WriteAssets(BinaryIO* io);
	void WriteRPakRawDataBlocks(BinaryIO& out);

	// purpose: populates m_vFileRelations vector with combined asset relation data
	void GenerateFileRelations();
	void GenerateGuidData();

	_vseginfo_t CreateNewSegment(uint32_t size, uint32_t flags, uint32_t alignment, uint32_t vsegAlignment = -1);
	RPakAssetEntry* GetAssetByGuid(uint64_t guid, uint32_t* idx = nullptr);

private:
	RPakVirtualSegment GetMatchingSegment(uint32_t flags, uint32_t alignment, uint32_t* segidx);

	// next available starpak data offset
	uint64_t m_NextStarpakOffset = 0x1000;

public: // !TODO: Make private
	int m_Version = 0;
	int m_Flags = 0;

	RPakFileHeader m_Header;

	std::string m_Path;
	std::string m_PrimaryStarpakPath;

	std::vector<RPakAssetEntry> m_Assets;

	std::vector<std::string> m_vStarpakPaths;
	std::vector<std::string> m_vOptStarpakPaths;

	std::vector<RPakVirtualSegment> m_vVirtualSegments;
	std::vector<RPakPageInfo> m_vPages;
	std::vector<RPakDescriptor> m_vDescriptors;
	std::vector<RPakGuidDescriptor> m_vGuidDescriptors;
	std::vector<uint32_t> m_vFileRelations;

	std::vector<RPakRawDataBlock> m_vRawDataBlocks;
	std::vector<SRPkDataEntry> m_vStarpakDataBlocks;
};