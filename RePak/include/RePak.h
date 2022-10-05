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
inline std::vector<SRPkDataEntry> g_vSRPkDataEntries{};

struct _vseginfo_t
{
	uint32_t index = -1;
	uint32_t size = -1;
};

namespace RePak
{
	_vseginfo_t CreateNewSegment(uint32_t size, uint32_t flags, uint32_t alignment, uint32_t vsegAlignment = -1);
	void AddStarpakReference(std::string path);
	void AddOptStarpakReference(std::string path);
	SRPkDataEntry AddStarpakDataEntry(SRPkDataEntry block);
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
	RPakFileBase(int version)
	{
		this->header.fileVersion = version;
		this->version = version;
	}

	void AddAsset(rapidjson::Value& file)
	{
		ASSET_HANDLER("txtr", file, assets, Assets::AddTextureAsset_v8, Assets::AddTextureAsset_v8);
		ASSET_HANDLER("uimg", file, assets, Assets::AddUIImageAsset_v10, Assets::AddUIImageAsset_v10);
		ASSET_HANDLER("Ptch", file, assets, Assets::AddPatchAsset, Assets::AddPatchAsset);
		ASSET_HANDLER("dtbl", file, assets, Assets::AddDataTableAsset_v0, Assets::AddDataTableAsset_v1);
		ASSET_HANDLER("rmdl", file, assets, Assets::AddModelAsset_stub, Assets::AddModelAsset_v9);
		ASSET_HANDLER("matl", file, assets, Assets::AddMaterialAsset_v12, Assets::AddMaterialAsset_v15);
	};

	size_t GetAssetCount() { return assets.size(); };

	void SetVersion(uint32_t version)
	{
		this->header.fileVersion = version;
		this->version = version;
	}

	void SetStarpakPathsSize(int len, int optLen)
	{
		this->header.starpakPathsSize = len;
		this->header.optStarpakPathsSize = optLen;
	}

	void WriteAssets(BinaryIO* io)
	{
		int i = 0;
		for (auto& it : assets)
		{
			io->write(it.guid);
			io->write(it.unk0);
			io->write(it.headIdx);
			io->write(it.headOffset);
			io->write(it.cpuIdx);
			io->write(it.cpuOffset);
			io->write(it.starpakOffset);

			if(this->version == 8)
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

			i++;
		}

		// update header asset count with the assets we've just written
		this->header.assetCount += i;
	};

	void WriteHeader(BinaryIO* io)
	{
		int version = header.fileVersion;

		io->write(header.magic);
		io->write(header.fileVersion);
		io->write(header.flags);
		io->write(header.fileTime);
		io->write(header.unk0);
		io->write(header.compressedSize);

		if (version == 8)
			io->write(header.embeddedStarpakOffset);

		io->write(header.unk1);
		io->write(header.decompressedSize);

		if (version == 8)
			io->write(header.embeddedStarpakSize);

		io->write(header.unk2);
		io->write(header.starpakPathsSize);

		if (version == 8)
			io->write(header.optStarpakPathsSize);

		io->write(header.virtualSegmentCount);
		io->write(header.pageCount);
		io->write(header.patchIndex);

		if (version == 8)
			io->write(header.alignment);

		io->write(header.descriptorCount);
		io->write(header.assetCount);
		io->write(header.guidDescriptorCount);
		io->write(header.relationCount);

		if (version == 7)
		{
			io->write(header.externalAssetsCount);
			io->write(header.externalAssetsSize);
		}
		else if (version == 8)
		{
			io->write(header.unk3);
		}
	};



public:
	std::vector<RPakAssetEntry> assets{};

	uint32_t version = 0;

	RPakFileHeader header{};
};