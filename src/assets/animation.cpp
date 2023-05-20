#include "pch.h"
#include "assets.h"
#include "public/studio.h"


void Assets::AddAnimSeqAsset_stub(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	Error("unsupported asset type 'aseq' for version 7\n");
}

void Assets::AddAnimSeqAsset_v7(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding aseq asset '%s'\n", assetPath);

    CPakDataChunk& hdrChunk = pak->CreateDataChunk(sizeof(AnimSequenceHeader), SF_HEAD, 16);
    AnimSequenceHeader* aseqHeader = reinterpret_cast<AnimSequenceHeader*>(hdrChunk.Data());

    std::string rseqFilePath = pak->GetAssetPath() + assetPath;

    // require rseq file to exist
    REQUIRE_FILE(rseqFilePath);

    int fileNameDataSize = strlen(assetPath) + 1;
    int rseqFileSize = (int)Utils::GetFileSize(rseqFilePath);

    uint32_t bufAlign = 4 - (fileNameDataSize + rseqFileSize) % 4;

    CPakDataChunk& dataChunk = pak->CreateDataChunk(fileNameDataSize + rseqFileSize + bufAlign, SF_CPU, 64);

    // write the rseq file path into the data buffer
    snprintf(dataChunk.Data(), fileNameDataSize, "%s", assetPath);

    // begin rseq input
    BinaryIO rseqInput;
    rseqInput.open(rseqFilePath, BinaryIOMode::Read);

    // go back to the beginning of the file to read all the data
    rseqInput.seek(0);

    // write the rseq data into the data buffer
    rseqInput.getReader()->read(dataChunk.Data() + fileNameDataSize, rseqFileSize);
    rseqInput.close();

    mstudioseqdesc_t seqdesc = *reinterpret_cast<mstudioseqdesc_t*>(dataChunk.Data() + fileNameDataSize);

    aseqHeader->szname = dataChunk.GetPointer();

    aseqHeader->data = dataChunk.GetPointer(fileNameDataSize);

    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSequenceHeader, szname)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSequenceHeader, data)));

    std::vector<PakGuidRefHdr_t> guids{};

    rmem dataBuf(dataChunk.Data());
    dataBuf.seek(fileNameDataSize + seqdesc.autolayerindex, rseekdir::beg);

    // register autolayer aseq guids
    for (int i = 0; i < seqdesc.numautolayers; ++i)
    {
        dataBuf.seek(fileNameDataSize + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);

        mstudioautolayer_t* autolayer = dataBuf.get<mstudioautolayer_t>();

        if (autolayer->guid != 0)
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid)));

        PakAsset_t* asset = pak->GetAssetByGuid(autolayer->guid);

        if (asset)
            asset->AddRelation(assetEntries->size());
    }

    PakAsset_t asset;

    asset.InitAsset(RTech::StringToGuid(assetPath), hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), -1, -1, (std::uint32_t)AssetType::ASEQ);
    asset.version = 7;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);
}