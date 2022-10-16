#include "pch.h"
#include "Assets.h"
#include "assets/model.h"
#include "assets/animation.h"

void Assets::AddRigAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	Error("unsupported asset type 'rrig' for version 7");
}

void Assets::AddRseqAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	Error("unsupported asset type 'aseq' for version 7");
}

void Assets::AddRigAsset_v4(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	std::vector<RPakGuidDescriptor> guids{};

	Log("\n==============================\n");
	Log("Asset arig -> '%s'\n", assetPath);

	std::string sAssetName = assetPath;
	std::string skelFilePath = g_sAssetsDir + sAssetName;

	REQUIRE_FILE(skelFilePath);

	AnimRigHeader* pHdr = new AnimRigHeader();

	std::vector<std::string> AseqList;
	if (mapEntry.HasMember("animseqs") && mapEntry["animseqs"].IsArray())
	{
		pHdr->AseqRefCount = mapEntry["animseqs"].Size();

		if (pHdr->AseqRefCount == 0)
			Error("invalid animseq count must not be 0 for arig '%s'\n", assetPath);


		for (auto& entry : mapEntry["animseqs"].GetArray())
		{
			if (entry.IsString() && !pak->DoesAssetExist(RTech::StringToGuid(entry.GetString())))
				AseqList.push_back(entry.GetString());
				//Assets::AddRseqAsset_v7(pak, assetEntries, entry.GetString(), mapEntry);
		}

		AddRseqListAsset_v7(pak, assetEntries, mapEntry, g_sAssetsDir, AseqList);
	}

	
	// begin rrig input
	BinaryIO skelInput;
	skelInput.open(skelFilePath, BinaryIOMode::Read);

	studiohdr_t mdlhdr = skelInput.read<studiohdr_t>();

	if (mdlhdr.id != 0x54534449) // "IDST"
	{
		Warning("invalid file magic for arig asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x54534449, mdlhdr.id);
		return;
	}

	if (mdlhdr.version != 54)
	{
		Warning("invalid version for arig asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 54, mdlhdr.version);
		return;
	}

	uint32_t NameDataSize = sAssetName.length() + 1;
	uint32_t NameAlignment = NameDataSize % 4;
	NameDataSize += NameAlignment;

	size_t DataBufferSize = NameDataSize + mdlhdr.length + (pHdr->AseqRefCount * sizeof(uint64_t));
	char* pDataBuf = new char[DataBufferSize];

	// go back to the beginning of the file to read all the data
	skelInput.seek(0);

	// write the skeleton data into the data buffer
	skelInput.getReader()->read(pDataBuf + NameDataSize, mdlhdr.length);
	skelInput.close();

	// Segments
	// asset header
	_vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(AnimRigHeader), SF_HEAD, 16);

	// data segment
	_vseginfo_t dataseginfo = pak->CreateNewSegment(DataBufferSize, SF_CPU, 64);

	// write the rrig file path into the data buffer
	snprintf(pDataBuf, NameDataSize, "%s", sAssetName.c_str());

	rmem DataWriter(pDataBuf);
	//DataWriter.seek(NameDataSize + mdlhdr.length, rseekdir::beg);

	uint32_t SegmentOffset = 0;
	pHdr->pName = { dataseginfo.index, SegmentOffset };

	SegmentOffset += NameDataSize;
	pHdr->pSkeleton = { dataseginfo.index, SegmentOffset };

	SegmentOffset += mdlhdr.length;
	pHdr->pAseqRefs = { dataseginfo.index, SegmentOffset };

	pak->AddPointer(subhdrinfo.index, offsetof(AnimRigHeader, pName));
	pak->AddPointer(subhdrinfo.index, offsetof(AnimRigHeader, pSkeleton));
	pak->AddPointer(subhdrinfo.index, offsetof(AnimRigHeader, pAseqRefs));

	for (int i = 0; i < pHdr->AseqRefCount; i++)
	{
		rapidjson::Value& Entry = mapEntry["animseqs"].GetArray()[i];

		if (!Entry.IsString())
			Error("invalid animseq entry for arig '%s'\n", assetPath);

		uint64_t Offset = SegmentOffset + (i * sizeof(uint64_t));
		uint64_t GUID = RTech::StringToGuid(Entry.GetString());

		if (pak->DoesAssetExist(GUID))
		{
			DataWriter.write<uint64_t>(GUID, Offset);
			pak->AddGuidDescriptor(&guids, dataseginfo.index, Offset);

			pak->GetAssetByGuid(GUID)->AddRelation(assetEntries->size());
		}
	}

	pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr });
	pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf });

	RPakAssetEntry asset;
	asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), subhdrinfo.index, 0, subhdrinfo.size, -1, 0, -1, -1, (std::uint32_t)AssetType::ARIG);

	asset.version = 4;

	asset.pageEnd = dataseginfo.index + 1;

	asset.relationCount = pHdr->AseqRefCount + 1;
	asset.unk1 = guids.size() + 1; // uses + 1

	asset.AddGuids(&guids);
	assetEntries->push_back(asset);
}

void Assets::AddRseqAsset_v7(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	std::string sAssetName = assetPath;
	std::string FilePath = g_sAssetsDir + sAssetName;

	REQUIRE_FILE(FilePath);

	uint64_t GUID = RTech::StringToGuid(sAssetName.c_str());

	//Log("\n==============================\n");
	Log("Asset aseq -> 0x%llX -> '%s'\n", GUID, assetPath);

	AnimHeader* pHdr = new AnimHeader();

	std::string ActivityOverride = "";

	if (mapEntry.HasMember("activity") && mapEntry["activity"].IsString())
		ActivityOverride = mapEntry["activity"].GetStdString();

	uint32_t activityNameDataSize = ActivityOverride.length() + 1;
	uint32_t fileNameDataSize = sAssetName.length() + 1;
	uint32_t rseqFileSize = (uint32_t)Utils::GetFileSize(FilePath);
	uint32_t bufAlign = 4 - (fileNameDataSize + rseqFileSize) % 4;

	size_t DataBufferSize = fileNameDataSize + rseqFileSize + bufAlign + 2;
	char* pDataBuf = new char[DataBufferSize + activityNameDataSize];
	rmem dataBuf(pDataBuf);

	// write the aseq file path into the data buffer
	dataBuf.writestring(sAssetName, 0);

	BinaryIO rseqInput;
	rseqInput.open(FilePath, BinaryIOMode::Read);
	rseqInput.getReader()->read(pDataBuf + fileNameDataSize, DataBufferSize);
	rseqInput.close();

	std::vector<RPakGuidDescriptor> guids{};

	dataBuf.seek(fileNameDataSize, rseekdir::beg);
	mstudioseqdesc_t seqdesc = dataBuf.read<mstudioseqdesc_t>();

	bool OverrideHeader = false;

	if (mapEntry.HasMember("activity") && mapEntry["activity"].IsString())
	{
		dataBuf.writestring(ActivityOverride, fileNameDataSize + rseqFileSize);
		seqdesc.szactivitynameindex = rseqFileSize ;
		OverrideHeader = true;
	}

	// Segments
	// asset header
	_vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(AnimHeader), SF_HEAD, 16);

	// data segment
	_vseginfo_t dataseginfo = pak->CreateNewSegment(DataBufferSize + activityNameDataSize, SF_CPU, 64);

	pHdr->pName = { dataseginfo.index, 0 };

	pHdr->pAnimation = { dataseginfo.index, fileNameDataSize };

	pak->AddPointer(subhdrinfo.index, offsetof(AnimHeader, pName));
	pak->AddPointer(subhdrinfo.index, offsetof(AnimHeader, pAnimation));

	if (mapEntry.HasMember("autolayers") && mapEntry["autolayers"].IsBool() && !mapEntry["autolayers"].GetBool())
	{
		seqdesc.numautolayers = 0;
		OverrideHeader = true;
	}

	if (OverrideHeader)
		memcpy(pDataBuf + fileNameDataSize, &seqdesc, sizeof(mstudioseqdesc_t));

	dataBuf.seek(fileNameDataSize + seqdesc.autolayerindex, rseekdir::beg);
	// register autolayer aseq guids
	for (int i = 0; i < seqdesc.numautolayers; ++i)
	{
		dataBuf.seek(fileNameDataSize + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);

		mstudioautolayer_t* autolayer = dataBuf.get<mstudioautolayer_t>();

		if (autolayer->guid != 0)
			pak->AddGuidDescriptor(&guids, dataseginfo.index, dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid));

		if (pak->DoesAssetExist(autolayer->guid))
			pak->GetAssetByGuid(autolayer->guid)->AddRelation(assetEntries->size());
		//else Warning("\n==============================\n0x%llX AutoLayer dependancy not found -> 0x%llX\n==============================\n", GUID, autolayer->guid);
	}

	pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr });
	pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf });

	RPakAssetEntry asset;
	asset.InitAsset(GUID, subhdrinfo.index, 0, subhdrinfo.size, -1, 0, -1, -1, (std::uint32_t)AssetType::ASEQ);

	asset.version = 7;

	asset.pageEnd = dataseginfo.index + 1;
	asset.unk1 = guids.size() + 1; // uses + 1

	asset.AddGuids(&guids);
	assetEntries->push_back(asset);
}


void AddRseqListAsset_v7(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, rapidjson::Value& mapEntry, std::string sAssetsDir, std::vector<std::string> AseqList)
{
	// calculate databuf size
	size_t DataSize = 0;
	for (auto& AseqName : AseqList)
	{
		uint64_t GUID = RTech::StringToGuid(AseqName.c_str());
		std::vector<RPakGuidDescriptor> guids{};

		std::string FilePath = sAssetsDir + AseqName;
		REQUIRE_FILE(FilePath);


		uint32_t fileNameDataSize = AseqName.length() + 1;
		uint32_t rseqFileSize = (uint32_t)Utils::GetFileSize(FilePath);
		uint32_t bufAlign = 4 - (fileNameDataSize + rseqFileSize) % 4;
		DataSize += fileNameDataSize + rseqFileSize + bufAlign;
	}

	size_t HeaderSize = sizeof(AnimHeader) * AseqList.size();
	char* pHeaderBuf = new char[HeaderSize];
	rmem hdrBuf(pHeaderBuf);

	char* pDataBuf = new char[DataSize];
	rmem dataBuf(pDataBuf);


	// asset header
	_vseginfo_t subhdrinfo = pak->CreateNewSegment(HeaderSize, SF_HEAD, 16);

	// data segment
	_vseginfo_t dataseginfo = pak->CreateNewSegment(DataSize, SF_CPU, 64);

	uint32_t headeroffset = 0;
	uint32_t baseoffset = 0;
	for (auto& AseqName : AseqList)
	{
		std::vector<RPakGuidDescriptor> guids{};
		uint64_t GUID = RTech::StringToGuid(AseqName.c_str());
		std::string FilePath = sAssetsDir + AseqName;

		uint32_t fileNameDataSize = AseqName.length() + 1;
		uint32_t rseqFileSize = (uint32_t)Utils::GetFileSize(FilePath);
		uint32_t bufAlign = 4 - (fileNameDataSize + rseqFileSize) % 4;
		size_t DataBufferSize = fileNameDataSize + rseqFileSize + bufAlign;

		// write the aseq file path into the data buffer
		dataBuf.writestring(AseqName, baseoffset);

		BinaryIO rseqInput;
		rseqInput.open(FilePath, BinaryIOMode::Read);
		rseqInput.getReader()->read(pDataBuf + baseoffset + fileNameDataSize, DataBufferSize);
		rseqInput.close();

		dataBuf.seek(baseoffset + fileNameDataSize, rseekdir::beg);
		mstudioseqdesc_t seqdesc = dataBuf.read<mstudioseqdesc_t>();

		AnimHeader pHdr;

		pHdr.pName = { dataseginfo.index, baseoffset };
		pHdr.pAnimation = { dataseginfo.index, baseoffset + fileNameDataSize };

		pak->AddPointer( subhdrinfo.index, headeroffset + offsetof(AnimHeader, pName) );
		pak->AddPointer( subhdrinfo.index, headeroffset + offsetof(AnimHeader, pAnimation) );

		uint64_t AutoLayerOffset = baseoffset + fileNameDataSize + seqdesc.autolayerindex;

		dataBuf.seek(AutoLayerOffset, rseekdir::beg);

		// register autolayer aseq guids
		for (int i = 0; i < seqdesc.numautolayers; ++i)
		{
			dataBuf.seek(AutoLayerOffset + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);

			mstudioautolayer_t* autolayer = dataBuf.get<mstudioautolayer_t>();

			if (autolayer->guid != 0)
				pak->AddGuidDescriptor(&guids, dataseginfo.index, dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid));

			if (pak->DoesAssetExist(autolayer->guid))
				pak->GetAssetByGuid(autolayer->guid)->AddRelation(assetEntries->size());
		}


		memcpy(pHeaderBuf + headeroffset, &pHdr, sizeof(AnimHeader));


		RPakAssetEntry asset;
		asset.InitAsset(GUID, subhdrinfo.index, headeroffset, sizeof(AnimHeader), -1, 0, -1, -1, (std::uint32_t)AssetType::ASEQ);

		asset.version = 7;

		asset.pageEnd = dataseginfo.index + 1;
		asset.unk1 = guids.size() + 1; // uses + 1

		asset.AddGuids(&guids);
		assetEntries->push_back(asset);

		headeroffset += sizeof(AnimHeader);
		baseoffset += DataBufferSize;
		dataBuf.seek(baseoffset, rseekdir::beg);
	}

	pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHeaderBuf });
	pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf });
}