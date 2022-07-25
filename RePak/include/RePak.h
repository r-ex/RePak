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
	RPakAssetEntryV7* GetAssetByGuid(std::vector<RPakAssetEntryV7>* assets, uint64_t guid, uint32_t* idx);
	RPakAssetEntryV8* GetAssetByGuid(std::vector<RPakAssetEntryV8>* assets, uint64_t guid, uint32_t* idx);
};

#define ASSET_HANDLER(ext, file, assetEntries, func) \
	if (file["$type"].GetStdString() == std::string(ext)) \
		func(&assetEntries, file["path"].GetString(), file);

class RPakFileBase
{
public:
	virtual void HandleAsset(rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>& asset) {};

	virtual size_t GetAssetCount() { return 0; };

	virtual void WriteAssets(BinaryIO* io) { };
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

	RPakFileHeader header{};
};

class RPakFileV7 : public RPakFileBase
{
protected:
	std::vector<RPakAssetEntryV7> assets{};
public:
	void HandleAsset(rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>& file) override
	{
		ASSET_HANDLER("txtr", file, assets, Assets::AddTextureAsset);
		ASSET_HANDLER("uimg", file, assets, Assets::AddUIImageAsset);
		ASSET_HANDLER("Ptch", file, assets, Assets::AddPatchAsset);
		ASSET_HANDLER("dtbl", file, assets, Assets::AddDataTableAsset);
		ASSET_HANDLER("rmdl", file, assets, Assets::AddModelAsset);
		ASSET_HANDLER("matl", file, assets, Assets::AddMaterialAsset);
	};

	size_t GetAssetCount() override
	{
		return assets.size();
	};

	void WriteAssets(BinaryIO* io) override
	{
		WRITE_VECTOR_PTRIO(io, assets);
	};
};

class RPakFileV8 : public RPakFileBase
{
protected:
	std::vector<RPakAssetEntryV8> assets{};
public:
	void HandleAsset(rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>& file) override
	{
		ASSET_HANDLER("txtr", file, assets, Assets::AddTextureAsset);
		ASSET_HANDLER("uimg", file, assets, Assets::AddUIImageAsset);
		ASSET_HANDLER("Ptch", file, assets, Assets::AddPatchAsset);
		ASSET_HANDLER("dtbl", file, assets, Assets::AddDataTableAsset);
		ASSET_HANDLER("rmdl", file, assets, Assets::AddModelAsset);
		ASSET_HANDLER("matl", file, assets, Assets::AddMaterialAsset);
	};

	size_t GetAssetCount() override
	{
		return assets.size();
	};

	void WriteAssets(BinaryIO* io) override
	{
		WRITE_VECTOR_PTRIO(io, assets);
	};
};