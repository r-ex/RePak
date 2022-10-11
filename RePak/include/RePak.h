#pragma once
#include <Assets.h>
#include <Utils.h>

#define DEFAULT_RPAK_NAME "new"

// starpak data entry vector
inline std::vector<SRPkDataEntry> g_vSRPkDataEntries{};

struct _vseginfo_t
{
	unsigned int index = 0xFFFFFFFF;
	unsigned int size = 0;
};

#define ASSET_HANDLER(ext, file, assetEntries, func_v7, func_v8) \
	if (file["$type"].GetStdString() == std::string(ext)) \
	{ \
		if(this->m_Version == 8 && func_v8) \
			func_v8(this, &assetEntries, file["path"].GetString(), file); \
		if(this->m_Version == 7 && func_v7) \
			func_v7(this, &assetEntries, file["path"].GetString(), file); \
	}

class CPakFile
{
public:
	CPakFile(int version)
	{
		this->m_Header.fileVersion = version;
		this->m_Version = version;
	}

	void AddAsset(rapidjson::Value& file)
	{
		ASSET_HANDLER("txtr", file, m_Assets, Assets::AddTextureAsset_v8, Assets::AddTextureAsset_v8);
		ASSET_HANDLER("uimg", file, m_Assets, Assets::AddUIImageAsset_v10, Assets::AddUIImageAsset_v10);
		ASSET_HANDLER("Ptch", file, m_Assets, Assets::AddPatchAsset, Assets::AddPatchAsset);
		ASSET_HANDLER("dtbl", file, m_Assets, Assets::AddDataTableAsset_v0, Assets::AddDataTableAsset_v1);
		ASSET_HANDLER("rmdl", file, m_Assets, Assets::AddModelAsset_stub, Assets::AddModelAsset_v9);
		ASSET_HANDLER("matl", file, m_Assets, Assets::AddMaterialAsset_v12, Assets::AddMaterialAsset_v15);
		ASSET_HANDLER("rseq", file, m_Assets, Assets::AddAnimSeqAsset_stub, Assets::AddAnimSeqAsset_v7);
	};

	inline bool IsFlagSet(int flag) { return this->flags & flag; };

	inline size_t GetAssetCount() { return m_Assets.size(); };

	std::string GetStarpakPath(int i) {
		if (m_vStarpakPaths.size() > i)
			return m_vStarpakPaths[i];
		else
			return ""; // if invalid starpak is requested, return empty string
	};

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

	void WriteRPakRawDataBlocks(BinaryIO& out)
	{
		for (auto it = m_vRawDataBlocks.begin(); it != m_vRawDataBlocks.end(); ++it)
		{
			out.getWriter()->write((char*)it->m_nDataPtr, it->m_nDataSize);
		}
	}

	void WriteAssets(BinaryIO* io)
	{
		for (auto& it : m_Assets)
		{
			io->write(it.guid);
			io->write(it.unk0);
			io->write(it.headIdx);
			io->write(it.headOffset);
			io->write(it.cpuIdx);
			io->write(it.cpuOffset);
			io->write(it.starpakOffset);

			if (this->m_Version == 8)
				io->write(it.optStarpakOffset);

			io->write(it.pageEnd);
			io->write(it.unk1);
			io->write(it.relStartIdx);
			io->write(it.usesStartIdx);
			io->write(it.relationCount);
			io->write(it.usesCount);
			io->write(it.headDataSize);
			io->write(it.version);
			io->write(it.id);
		}

		// update header asset count with the assets we've just written
		this->m_Header.assetCount = m_Assets.size();
	};

	void WriteHeader(BinaryIO* io)
	{
		m_Header.virtualSegmentCount = m_vVirtualSegments.size();
		m_Header.pageCount = m_vPages.size();
		m_Header.descriptorCount = m_vDescriptors.size();
		m_Header.guidDescriptorCount = m_vGuidDescriptors.size();
		m_Header.relationCount = m_vFileRelations.size();

		int version = m_Header.fileVersion;

		io->write(m_Header.magic);
		io->write(m_Header.fileVersion);
		io->write(m_Header.flags);
		io->write(m_Header.fileTime);
		io->write(m_Header.unk0);
		io->write(m_Header.compressedSize);

		if (version == 8)
			io->write(m_Header.embeddedStarpakOffset);

		io->write(m_Header.unk1);
		io->write(m_Header.decompressedSize);

		if (version == 8)
			io->write(m_Header.embeddedStarpakSize);

		io->write(m_Header.unk2);
		io->write(m_Header.starpakPathsSize);

		if (version == 8)
			io->write(m_Header.optStarpakPathsSize);

		io->write(m_Header.virtualSegmentCount);
		io->write(m_Header.pageCount);
		io->write(m_Header.patchIndex);

		if (version == 8)
			io->write(m_Header.alignment);

		io->write(m_Header.descriptorCount);
		io->write(m_Header.assetCount);
		io->write(m_Header.guidDescriptorCount);
		io->write(m_Header.relationCount);

		if (version == 7)
		{
			io->write(m_Header.unk7count);
			io->write(m_Header.unk8count);
		}
		else if (version == 8)
			io->write(m_Header.unk3);
	};

	inline void AddPointer(unsigned int pageIdx, unsigned int pageOffset)
	{
		m_vDescriptors.push_back({ pageIdx, pageOffset });
	}

	inline void AddGuidDescriptor(std::vector<RPakGuidDescriptor>* guids, unsigned int idx, unsigned int offset)
	{
		guids->push_back({ idx, offset });
	}

	inline void AddRawDataBlock(RPakRawDataBlock block)
	{
		m_vRawDataBlocks.push_back(block);
	};

	// purpose: populates m_vFileRelations vector with combined asset relation data
	inline void GenerateFileRelations()
	{
		for (auto& it : m_Assets)
		{
			it.relationCount = it._relations.size();
			it.relStartIdx = m_vFileRelations.size();

			for (int i = 0; i < it._relations.size(); ++i)
				m_vFileRelations.push_back( it._relations[i] );
		}
		m_Header.relationCount = m_vFileRelations.size();
	}

	inline void GenerateGuidData()
	{
		for (auto& it : m_Assets)
		{
			it.usesCount = it._guids.size();
			it.usesStartIdx = it.usesCount == 0 ? 0 : m_vGuidDescriptors.size();

			for (int i = 0; i < it._guids.size(); ++i)
				m_vGuidDescriptors.push_back({ it._guids[i] });
		}
		m_Header.guidDescriptorCount = m_vGuidDescriptors.size();
	}

	_vseginfo_t CreateNewSegment(uint32_t size, uint32_t flags, uint32_t alignment, uint32_t vsegAlignment = -1)
	{
		uint32_t vsegidx = (uint32_t)m_vVirtualSegments.size();

		// find existing "segment" with the same values or create a new one, this is to overcome the engine's limit of having max 20 of these
		// since otherwise we write into unintended parts of the stack, and that's bad
		RPakVirtualSegment seg = GetMatchingSegment(flags, vsegAlignment == -1 ? alignment : vsegAlignment, &vsegidx);

		bool bShouldAddVSeg = seg.dataSize == 0;
		seg.dataSize += size;

		if (bShouldAddVSeg)
			m_vVirtualSegments.emplace_back(seg);
		else
			m_vVirtualSegments[vsegidx] = seg;

		RPakPageInfo vsegblock{ vsegidx, alignment, size };

		m_vPages.emplace_back(vsegblock);
		unsigned int pageidx = m_vPages.size() - 1;

		return { pageidx, size };
	}

	RPakAssetEntry* GetAssetByGuid(uint64_t guid, uint32_t* idx = nullptr)
	{
		uint32_t i = 0;
		for (auto& it : m_Assets)
		{
			if (it.guid == guid)
			{
				if (idx)
					*idx = i;
				return &it;
			}
			i++;
		}
		Debug("failed to find asset with guid %llX\n", guid);
		return nullptr;
	}

	// starpaks

	// purpose: add new starpak file path to be used by the rpak
	// returns: void
	void AddStarpakReference(std::string path)
	{
		for (auto& it : m_vStarpakPaths)
		{
			if (it == path)
				return;
		}
		m_vStarpakPaths.push_back(path);
	}

	void AddOptStarpakReference(std::string path)
	{
		for (auto& it : m_vOptStarpakPaths)
		{
			if (it == path)
				return;
		}
		m_vOptStarpakPaths.push_back(path);
	}

	SRPkDataEntry AddStarpakDataEntry(SRPkDataEntry block)
	{
		// starpak data is aligned to 4096 bytes
		size_t ns = Utils::PadBuffer((char**)&block.m_nDataPtr, block.m_nDataSize, 4096);

		block.m_nDataSize = ns;
		block.m_nOffset = m_NextStarpakOffset;

		m_vStarpakDataBlocks.push_back(block);

		m_NextStarpakOffset += block.m_nDataSize;

		return block;
	}

private:
	// purpose: create page and segment with the specified parameters
	RPakVirtualSegment GetMatchingSegment(uint32_t flags, uint32_t alignment, uint32_t* segidx)
	{
		uint32_t i = 0;
		for (auto& it : m_vVirtualSegments)
		{
			if (it.flags == flags && it.alignment == alignment)
			{
				*segidx = i;
				return it;
			}
			i++;
		}

		return { flags, alignment, 0 };
	}

	// next available starpak data offset
	unsigned __int64 m_NextStarpakOffset = 0x1000;

public:
	std::vector<RPakAssetEntry> m_Assets{};

	int m_Version = 0;

	int flags = 0;

	RPakFileHeader m_Header{};

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
};

#define PF_KEEP_DEV 1 << 0