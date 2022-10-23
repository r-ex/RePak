#include "pch.h"
#include "rpak.h"


void Assets::AddAnimSeqAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	Error("unsupported asset type 'aseq' for version 7\n");
}

void Assets::AddAnimSeqAsset_v7(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding aseq asset '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    AnimSequenceHeader* pHdr = new AnimSequenceHeader();

    std::string rseqFilePath = g_sAssetsDir + sAssetName;

    // require rseq file to exist
    REQUIRE_FILE(rseqFilePath);

    uint32_t fileNameDataSize = sAssetName.length() + 1;
    uint32_t rseqFileSize = (uint32_t)Utils::GetFileSize(rseqFilePath);

    uint32_t bufAlign = 4 - (fileNameDataSize + rseqFileSize) % 4;

    char* pDataBuf = new char[fileNameDataSize + rseqFileSize + bufAlign]{};

    // write the rseq file path into the data buffer
    snprintf(pDataBuf, fileNameDataSize, "%s", sAssetName.c_str());

    // begin rseq input
    BinaryIO rseqInput;
    rseqInput.open(rseqFilePath, BinaryIOMode::Read);

    // go back to the beginning of the file to read all the data
    rseqInput.seek(0);

    // write the rseq data into the data buffer
    rseqInput.getReader()->read(pDataBuf + fileNameDataSize, rseqFileSize);
    rseqInput.close();

    mstudioseqdesc_t seqdesc = *reinterpret_cast<mstudioseqdesc_t*>(pDataBuf + fileNameDataSize);

    // Segments
    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(AnimSequenceHeader), SF_HEAD, 16);

    // data segment
    _vseginfo_t dataseginfo = pak->CreateNewSegment(rseqFileSize + fileNameDataSize + bufAlign, SF_CPU, 64);

    pHdr->szname = { dataseginfo.index, 0 };

    pHdr->data = { dataseginfo.index, fileNameDataSize };

    pak->AddPointer(subhdrinfo.index, offsetof(AnimSequenceHeader, szname));
    pak->AddPointer(subhdrinfo.index, offsetof(AnimSequenceHeader, data));

    std::vector<RPakGuidDescriptor> guids{};

    rmem dataBuf(pDataBuf);
    dataBuf.seek(fileNameDataSize + seqdesc.autolayerindex, rseekdir::beg);

    // register autolayer aseq guids
    for (int i = 0; i < seqdesc.numautolayers; ++i)
    {
        dataBuf.seek(fileNameDataSize + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);

        mstudioautolayer_t* autolayer = dataBuf.get<mstudioautolayer_t>();

        if (autolayer->guid != 0)
            pak->AddGuidDescriptor(&guids, dataseginfo.index, dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid));

        RPakAssetEntry* asset = pak->GetAssetByGuid(autolayer->guid);

        if (asset)
            asset->AddRelation(assetEntries->size());
    }

    RPakRawDataBlock shdb{ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr };
    pak->AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf };
    pak->AddRawDataBlock(rdb);

    uint32_t lastPageIdx = dataseginfo.index;

    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), subhdrinfo.index, 0, subhdrinfo.size, -1, 0, -1, -1, (std::uint32_t)AssetType::ASEQ);
    asset.version = 7;
    // i have literally no idea what these are
    asset.pageEnd = lastPageIdx + 1;
    asset.unk1 = 2;

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);
}