#include "pch.h"
#include "assets.h"
#include "public/studio.h"

static void AnimSeq_InternalAddAnimSeq(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
    const std::string rseqFilePath = pak->GetAssetPath() + assetPath;

    // begin rseq input
    BinaryIO rseqInput;

    if (!rseqInput.Open(rseqFilePath, BinaryIO::Mode_e::Read))
    {
        Error("Failed to open animseq asset \"%s\".\n", assetPath);
        return;
    }

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(AnimSeqAssetHeader_t), SF_HEAD, 16);

    const size_t rseqNameLenAligned = IALIGN4(strlen(assetPath) + 1);
    const size_t rseqFileSize = rseqInput.GetSize();

    CPakDataChunk dataChunk = pak->CreateDataChunk(IALIGN4(rseqNameLenAligned + rseqFileSize), SF_CPU, 64);

    // write the rseq file path into the data buffer
    snprintf(dataChunk.Data(), rseqNameLenAligned, "%s", assetPath);

    // write the rseq data into the data buffer
    rseqInput.Read(dataChunk.Data() + rseqNameLenAligned, rseqFileSize);
    rseqInput.Close();

    const mstudioseqdesc_t& seqdesc = *reinterpret_cast<mstudioseqdesc_t*>(dataChunk.Data() + rseqNameLenAligned);
    
    AnimSeqAssetHeader_t* const aseqHeader = reinterpret_cast<AnimSeqAssetHeader_t*>(hdrChunk.Data());
    aseqHeader->szname = dataChunk.GetPointer();

    aseqHeader->data = dataChunk.GetPointer(rseqNameLenAligned);

    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSeqAssetHeader_t, szname)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSeqAssetHeader_t, data)));

    std::vector<PakGuidRefHdr_t> guids;

    rmem dataBuf(dataChunk.Data());
    dataBuf.seek(rseqNameLenAligned + seqdesc.autolayerindex, rseekdir::beg);

    // Iterate over each of the sequence's autolayers to register each of the autolayer GUIDs
    // This is required as otherwise the game will crash while trying to dereference a non-converted GUID.
    for (int i = 0; i < seqdesc.numautolayers; ++i)
    {
        dataBuf.seek(rseqNameLenAligned + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);

        const mstudioautolayer_t* const autolayer = dataBuf.get<const mstudioautolayer_t>();

        if (autolayer->guid != 0)
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid)));

        PakAsset_t* asset = pak->GetAssetByGuid(autolayer->guid);

        // If the autolayer's guid is present in the same RPak, add this ASEQ asset to the referenced asset's dependents.
        if (asset)
            pak->SetCurrentAssetAsDependentForAsset(asset);
    }

    PakAsset_t asset;
    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::ASEQ);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = 7;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    pak->PushAsset(asset);
}

bool AnimSeq_AddSequenceRefs(CPakDataChunk* const chunk, CPakFile* const pak, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry)
{
    rapidjson::Value::ConstMemberIterator sequencesIt;
    const bool hasSequences = JSON_GetIterator(mapEntry, "$sequences", JSONFieldType_e::kArray, sequencesIt);

    if (!hasSequences)
        return false;

    const rapidjson::Value::ConstArray sequencesArray = sequencesIt->value.GetArray();
    const size_t numSequences = sequencesArray.Size();

    (*chunk) = pak->CreateDataChunk(sizeof(PakGuid_t) * numSequences, SF_CPU, 64);
    (*sequenceCount) = static_cast<uint32_t>(numSequences);

    PakGuid_t* const pGuids = reinterpret_cast<PakGuid_t*>(chunk->Data());

    int seqIndex = -1;
    for (const auto& sequence : sequencesArray)
    {
        seqIndex++;
        PakGuid_t guid;

        if (!JSON_ParseNumber(sequence, guid))
        {
            if (!sequence.IsString())
                Error("Sequence #%i is of unsupported type; expected %s or %s, found %s.\n", seqIndex,
                    JSON_TypeToString(JSONFieldType_e::kUint64), JSON_TypeToString(JSONFieldType_e::kString),
                    JSON_TypeToString(JSON_ExtractType(sequence)));

            if (sequence.GetStringLength() == 0)
                Error("Sequence #%i was defined as an invalid empty string.\n", seqIndex);

            const char* const sequencePath = sequence.GetString();
            Log("Auto-adding 'aseq' asset \"%s\".\n", sequencePath);

            guid = RTech::StringToGuid(sequencePath);
            const PakAsset_t* const existingAsset = pak->GetAssetByGuid(guid, nullptr, true);

            if (!existingAsset)
                AnimSeq_InternalAddAnimSeq(pak, guid, sequencePath);
        }

        pGuids[seqIndex] = guid;
    }

    return true;
}

void Assets::AddAnimSeqAsset_v7(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    AnimSeq_InternalAddAnimSeq(pak, assetGuid, assetPath);
}