#pragma once

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