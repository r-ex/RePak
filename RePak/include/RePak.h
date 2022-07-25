#pragma once
#include <Assets.h>

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

class RPak
{
public:
	virtual void HandleAsset(rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>& asset) {};
	virtual std::vector<char> GetHeaderBuffer() { return std::vector<char>(); };
	virtual std::vector<char> GetAssetsBuffer() { return std::vector<char>(); };

	virtual void m_nCreatedTime(uint64_t val) {};
	virtual void m_nSizeDisk(uint64_t val) {};
	virtual void m_nSizeMemory(uint64_t val) {};
	virtual void m_nVirtualSegmentCount(uint16_t val) {};
	virtual void m_nPageCount(uint16_t val) {};
	virtual void m_nDescriptorCount(uint32_t val) {};
	virtual void m_nGuidDescriptorCount(uint32_t val) {};
	virtual void m_nRelationsCounts(uint32_t val) {};
	virtual void m_nAssetEntryCount() {};
	virtual void m_nStarpakReferenceSize(uint16_t val) {};
	virtual void m_nStarpakOptReferenceSize(uint16_t val) {};
	virtual int thing() { return 15; };
};

class RPakV7 : public RPak
{
protected:
	RPakFileHeaderV7 header{};
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

	std::vector<char> GetHeaderBuffer() override
	{
		auto const ptr = reinterpret_cast<char*>(&header);
		std::vector<char> buffer(ptr, ptr + sizeof header);
		return buffer;
	}

	std::vector<char> GetAssetsBuffer() override
	{
		std::vector<char> ret;
		for (auto i : assets)
		{
			auto const ptr = reinterpret_cast<char*>(&i);
			std::vector<char> buffer(ptr, ptr + sizeof i);
			ret.insert(ret.end(), buffer.begin(), buffer.end());
		}
		//auto const ptr = reinterpret_cast<char*>(&assets);
		//std::vector<char> buffer(ptr, ptr + sizeof assets);
		//return buffer;
		return ret;
	};
	// look idk how i can make header public because its type can be RPakFileHeaderV7 or it can be RPakFileHeaderV8, so I have to make setters for every single part of the header
	// maybe we could make make one generic header struct and make a function that sorta takes the information that we want (using the existing structs maybe?) and turns it into the vector to be returned?
	void m_nCreatedTime(uint64_t val) override { header.m_nCreatedTime = val; };
	void m_nSizeDisk(uint64_t val) override { header.m_nSizeDisk = val; };
	void m_nSizeMemory(uint64_t val) override { header.m_nSizeMemory = val; };
	void m_nVirtualSegmentCount(uint16_t val) override { header.m_nVirtualSegmentCount = val; };
	void m_nPageCount(uint16_t val) override { header.m_nPageCount = val; };
	void m_nDescriptorCount(uint32_t val) override { header.m_nDescriptorCount = val; };
	void m_nGuidDescriptorCount(uint32_t val) override { header.m_nGuidDescriptorCount = val; };
	void m_nRelationsCounts(uint32_t val) override { header.m_nRelationsCounts = val; };
	void m_nAssetEntryCount() override { header.m_nAssetEntryCount = (uint32_t)assets.size(); };
	void m_nStarpakReferenceSize(uint16_t val) override { header.m_nStarpakReferenceSize = val; };
	void m_nStarpakOptReferenceSize(uint16_t val) override { }; // v7 doesnt have these

	int thing() override { return 10; };
};

class RPakV8 : public RPak
{
protected:
	RPakFileHeaderV8 header{};
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

	std::vector<char> GetHeaderBuffer() override
	{
		auto const ptr = reinterpret_cast<char*>(&header);
		std::vector<char> buffer(ptr, ptr + sizeof header);
		return buffer;
	}

	std::vector<char> GetAssetsBuffer() override
	{
		std::vector<char> ret;
		for (auto i : assets)
		{
			auto const ptr = reinterpret_cast<char*>(&i);
			std::vector<char> buffer(ptr, ptr + sizeof i);
			ret.insert(ret.end(), buffer.begin(), buffer.end());
		}
		//auto const ptr = reinterpret_cast<char*>(&assets);
		//std::vector<char> buffer(ptr, ptr + sizeof assets);
		//return buffer;
		return ret;
	};
	// look idk how i can make header public because its type can be RPakFileHeaderV7 or it can be RPakFileHeaderV8, so I have to make setters for every single part of the header
	// maybe we could make make one generic header struct and make a function that sorta takes the information that we want (using the existing structs maybe?) and turns it into the vector to be returned?
	void m_nCreatedTime(uint64_t val) override { header.m_nCreatedTime = val; };
	void m_nSizeDisk(uint64_t val) override { header.m_nSizeDisk = val; };
	void m_nSizeMemory(uint64_t val) override { header.m_nSizeMemory = val; };
	void m_nVirtualSegmentCount(uint16_t val) override { header.m_nVirtualSegmentCount = val; };
	void m_nPageCount(uint16_t val) override { header.m_nPageCount = val; };
	void m_nDescriptorCount(uint32_t val) override { header.m_nDescriptorCount = val; };
	void m_nGuidDescriptorCount(uint32_t val) override { header.m_nGuidDescriptorCount = val; };
	void m_nRelationsCounts(uint32_t val) override { header.m_nRelationsCounts = val; };
	void m_nAssetEntryCount() override { header.m_nAssetEntryCount = (uint32_t)assets.size(); };
	void m_nStarpakReferenceSize(uint16_t val) override { header.m_nStarpakReferenceSize = val; };
	void m_nStarpakOptReferenceSize(uint16_t val) override { header.m_nStarpakOptReferenceSize = val; };

	int thing() override { return 5; };
};