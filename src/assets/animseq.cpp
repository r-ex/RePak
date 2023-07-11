#include "pch.h"
#include "assets.h"
#include "public/studio.h"

void Assets::AddAnimSeqAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath)
{
    Log("Adding aseq asset '%s'\n", assetPath);

    PakAsset_t* existingAsset = pak->GetAssetByGuid(RTech::GetAssetGUIDFromString(assetPath, true), nullptr, true);
    if (existingAsset)
    {
        Warning("Tried to add animseq asset '%s' twice. Skipping redefinition...\n", assetPath);
        return;
    }

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(AnimSeqAssetHeader_t), SF_HEAD, 16);
    AnimSeqAssetHeader_t* aseqHeader = reinterpret_cast<AnimSeqAssetHeader_t*>(hdrChunk.Data());

    std::string rseqFilePath = pak->GetAssetPath() + assetPath;

    // require rseq file to exist
    REQUIRE_FILE(rseqFilePath);

    int fileNameLength = IALIGN4(strlen(assetPath) + 1);
    int rseqFileSize = Utils::GetFileSize(rseqFilePath);

    CPakDataChunk dataChunk = pak->CreateDataChunk(IALIGN4(fileNameLength + rseqFileSize), SF_CPU, 64);

    // write the rseq file path into the data buffer
    snprintf(dataChunk.Data(), fileNameLength, "%s", assetPath);

    // begin rseq input
    BinaryIO rseqInput(rseqFilePath, BinaryIOMode::Read);

    // write the rseq data into the data buffer
    rseqInput.getReader()->read(dataChunk.Data() + fileNameLength, rseqFileSize);
    rseqInput.close();

    mstudioseqdesc_t seqdesc = *reinterpret_cast<mstudioseqdesc_t*>(dataChunk.Data() + fileNameLength);

    aseqHeader->szname = dataChunk.GetPointer();

    aseqHeader->data = dataChunk.GetPointer(fileNameLength);

    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSeqAssetHeader_t, szname)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSeqAssetHeader_t, data)));

    std::vector<PakGuidRefHdr_t> guids{};

    rmem dataBuf(dataChunk.Data());
    dataBuf.seek(fileNameLength + seqdesc.autolayerindex, rseekdir::beg);

    // register autolayer aseq guids
    for (int i = 0; i < seqdesc.numautolayers; ++i)
    {
        dataBuf.seek(fileNameLength + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);

        mstudioautolayer_t* autolayer = dataBuf.get<mstudioautolayer_t>();

        if (autolayer->guid != 0)
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid)));

        PakAsset_t* asset = pak->GetAssetByGuid(autolayer->guid);

        if (asset)
            asset->AddRelation(assetEntries->size());
    }

    PakAsset_t asset;

    asset.InitAsset(assetPath, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), -1, -1, (std::uint32_t)AssetType::ASEQ);
    asset.version = 7;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    asset.EnsureUnique(assetEntries);
    assetEntries->push_back(asset);
}

void Assets::AddAnimSeqAsset_v7(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    AddAnimSeqAsset(pak, assetEntries, assetPath);
}