#pragma once

#define DEFAULT_RPAK_NAME "new"

namespace RePak
{
	uint32_t CreateNewSegment(uint64_t size, uint32_t flags_maybe, SegmentType type, RPakVirtualSegment& seg, uint32_t vsegType=-1);
	void AddStarpakReference(std::string path);
	uint64_t AddStarpakDataEntry(SRPkDataEntry block);
	void AddRawDataBlock(RPakRawDataBlock block);
	void RegisterDescriptor(uint32_t pageIdx, uint32_t pageOffset);
	void RegisterGuidDescriptor(uint32_t pageIdx, uint32_t pageOffset);
	size_t AddFileRelation(uint32_t assetIdx, uint32_t count = 1);
	RPakAssetEntryV8* GetAssetByGuid(std::vector<RPakAssetEntryV8>* assets, uint64_t guid, uint32_t* idx);
};

#define ASSET_HANDLER(ext, file, assetEntries, func) \
	if (file["$type"].GetStdString() == std::string(ext)) \
		func(&assetEntries, file["path"].GetString(), file);