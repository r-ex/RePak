#pragma once
#include <Assets.h>
#include <Utils.h>

#define DEFAULT_RPAK_NAME "new"

// vectors of stuff to get written
static std::vector<RPakVirtualSegment> g_vvSegments{};
static std::vector<RPakPageInfo> g_vPages{};
static std::vector<RPakDescriptor> g_vDescriptors{};
static std::vector<RPakGuidDescriptor> g_vGuidDescriptors{};
static std::vector<RPakRelationBlock> g_vFileRelations{};
static std::vector<RPakRawDataBlock> g_vSubHeaderBlocks{};
static std::vector<RPakRawDataBlock> g_vRawDataBlocks{};

struct _vseginfo_t
{
	uint32_t index = -1;
	uint32_t size = -1;
};

namespace RePak
{
	_vseginfo_t CreateNewSegment(uint32_t size, uint32_t flags_maybe, uint32_t alignment, uint32_t vsegAlignment = -1);
	void AddStarpakReference(std::string path);
	uint64_t AddStarpakDataEntry(SRPkDataEntry block);
	void AddRawDataBlock(RPakRawDataBlock block);
	void RegisterDescriptor(uint32_t pageIdx, uint32_t pageOffset);
	void RegisterGuidDescriptor(uint32_t pageIdx, uint32_t pageOffset);
	size_t AddFileRelation(uint32_t assetIdx, uint32_t count = 1);
	RPakAssetEntry* GetAssetByGuid(std::vector<RPakAssetEntry>* assets, uint64_t guid, uint32_t* idx);
};

#define ASSET_HANDLER(ext, file, assetEntries, func_v7, func_v8) \
	if (file["$type"].GetStdString() == std::string(ext)) \
	{ \
		if(this->version == 8 && func_v8) \
			func_v8(&assetEntries, file["path"].GetString(), file); \
		if(this->version == 7 && func_v7) \
			func_v7(&assetEntries, file["path"].GetString(), file); \
	}

class RPakFileBase
{
public:
	virtual void HandleAsset(rapidjson::Value& file)
	{
		ASSET_HANDLER("txtr", file, assets, Assets::AddTextureAsset_v8, Assets::AddTextureAsset_v8);
		ASSET_HANDLER("uimg", file, assets, Assets::AddUIImageAsset_r2, Assets::AddUIImageAsset_v10);
		ASSET_HANDLER("Ptch", file, assets, Assets::AddPatchAsset, Assets::AddPatchAsset);
		ASSET_HANDLER("dtbl", file, assets, Assets::AddDataTableAsset_v0, Assets::AddDataTableAsset_v1);
		ASSET_HANDLER("rmdl", file, assets, Assets::AddModelAsset_stub, Assets::AddModelAsset_v9);
		ASSET_HANDLER("matl", file, assets, Assets::AddMaterialAsset_v12, Assets::AddMaterialAsset_v15);
	};

	virtual size_t GetAssetCount() { return assets.size(); };

	virtual void WriteAssets(BinaryIO* io)
	{
		for (auto& it : assets)
		{
			io->write(it.m_nGUID);
			io->write(it.unk0);
			io->write(it.m_nSubHeaderDataBlockIdx);
			io->write(it.m_nSubHeaderDataBlockOffset);
			io->write(it.m_nRawDataBlockIndex);
			io->write(it.m_nRawDataBlockOffset);
			io->write(it.m_nStarpakOffset);

			if(this->version == 8)
				io->write(it.m_nOptStarpakOffset);

			io->write(it.m_nPageEnd);
			io->write(it.unk1);
			io->write(it.m_nRelationsStartIdx);
			io->write(it.m_nUsesStartIdx);
			io->write(it.m_nRelationsCounts);
			io->write(it.m_nUsesCount);
			io->write(it.m_nSubHeaderSize);
			io->write(it.m_nVersion);
			io->write(it.m_nMagic);
		}
	};
	virtual void WriteHeader(BinaryIO* io)
	{
		int version = header.m_nVersion;

		io->write(header.m_nMagic);
		io->write(header.m_nVersion);
		io->write(header.m_nFlags);
		io->write(header.m_nCreatedTime);
		io->write(header.unk0);
		io->write(header.m_nSizeDisk);

		if (version == 8)
			io->write(header.m_nEmbeddedStarpakOffset);

		io->write(header.unk1);
		io->write(header.m_nSizeMemory);

		if (version == 8)
			io->write(header.m_nEmbeddedStarpakSize);

		io->write(header.unk2);
		io->write(header.m_nStarpakReferenceSize);

		if (version == 8)
			io->write(header.m_nStarpakOptReferenceSize);

		io->write(header.m_nVirtualSegmentCount);
		io->write(header.m_nPageCount);
		io->write(header.m_nPatchIndex);

		if (version == 8)
			io->write(header.alignment);

		io->write(header.m_nDescriptorCount);
		io->write(header.m_nAssetEntryCount);
		io->write(header.m_nGuidDescriptorCount);
		io->write(header.m_nRelationsCount);

		if (version == 7)
		{
			io->write(header.m_nUnknownSeventhBlockCount);
			io->write(header.m_nUnknownEighthBlockCount);
		}
		else if (version == 8)
		{
			io->write(header.unk3);
		}
	};

	virtual void SetVersion(uint32_t version)
	{
		this->header.m_nVersion = version;
		this->version = version;
	}

public:
	std::vector<RPakAssetEntry> assets{};

	uint32_t version = 0;

	RPakFileHeader header{};
};