#include "pch.h"
#include "assets.h"
#include "public/studio.h"

void Assets::AddAnimSeqAsset(CPakFile* pak, const char* assetPath)
{
    Log("Adding aseq asset '%s'\n", assetPath);

    const PakAsset_t* existingAsset = pak->GetAssetByGuid(RTech::GetAssetGUIDFromString(assetPath, true), nullptr, true);
    if (existingAsset)
    {
        Warning("Tried to add animseq asset '%s' twice. Skipping redefinition...\n", assetPath);
        return;
    }

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(AnimSeqAssetHeader_t), SF_HEAD, 16);

    const std::string rseqFilePath = pak->GetAssetPath() + assetPath;

    // require rseq file to exist
    REQUIRE_FILE(rseqFilePath);

    const size_t rseqNameLenAligned = IALIGN4(strlen(assetPath) + 1);
    const size_t rseqFileSize = Utils::GetFileSize(rseqFilePath);

    CPakDataChunk dataChunk = pak->CreateDataChunk(IALIGN4(rseqNameLenAligned + rseqFileSize), SF_CPU, 64);

    // write the rseq file path into the data buffer
    snprintf(dataChunk.Data(), rseqNameLenAligned, "%s", assetPath);

    // begin rseq input
    BinaryIO rseqInput(rseqFilePath, BinaryIOMode::Read);

    // write the rseq data into the data buffer
    rseqInput.getReader()->read(dataChunk.Data() + rseqNameLenAligned, rseqFileSize);
    rseqInput.close();

    mstudioseqdesc_t seqdesc = *reinterpret_cast<mstudioseqdesc_t*>(dataChunk.Data() + rseqNameLenAligned);
    
    AnimSeqAssetHeader_t* const aseqHeader = reinterpret_cast<AnimSeqAssetHeader_t*>(hdrChunk.Data());
    aseqHeader->szname = dataChunk.GetPointer();

    aseqHeader->data = dataChunk.GetPointer(rseqNameLenAligned);

    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSeqAssetHeader_t, szname)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSeqAssetHeader_t, data)));

    std::vector<PakGuidRefHdr_t> guids{};

    rmem dataBuf(dataChunk.Data());
    dataBuf.seek(rseqNameLenAligned + seqdesc.autolayerindex, rseekdir::beg);

    // Iterate over each of the sequence's autolayers to register each of the autolayer GUIDs
    // This is required as otherwise the game will crash while trying to dereference a non-converted GUID.
    for (int i = 0; i < seqdesc.numautolayers; ++i)
    {
        dataBuf.seek(rseqNameLenAligned + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);

        const mstudioautolayer_t* autolayer = dataBuf.get<const mstudioautolayer_t>();

        if (autolayer->guid != 0)
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid)));

        PakAsset_t* asset = pak->GetAssetByGuid(autolayer->guid);

        // If the autolayer's guid is present in the same RPak, add this ASEQ asset to the referenced asset's dependents.
        if (asset)
            pak->SetCurrentAssetAsDependentForAsset(asset);
    }

    PakAsset_t asset;
    asset.InitAsset(assetPath, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::ASEQ);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = 7;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    pak->PushAsset(asset);
}

void Assets::AddAnimSeqAsset_v7(CPakFile* pak, const char* assetPath, rapidjson::Value& /*mapEntry*/)
{
    AddAnimSeqAsset(pak, assetPath);
}