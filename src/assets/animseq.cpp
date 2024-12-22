#include "pch.h"
#include "assets.h"
#include "public/studio.h"

// page chunk structure and order:
// - header HEAD        (align=8)
// - data   CPU         (align=1) name, then rmdl. unlike models, this is aligned to 1 since we don't have BVH4 collision data here.
static void AnimSeq_InternalAddAnimSeq(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
    PakAsset_t asset;
    const std::string rseqFilePath = pak->GetAssetPath() + assetPath;

    // begin rseq input
    BinaryIO rseqInput;

    if (!rseqInput.Open(rseqFilePath, BinaryIO::Mode_e::Read))
    {
        Error("Failed to open animseq asset \"%s\".\n", assetPath);
        return;
    }

    PakPageLump_s hdrChunk = pak->CreatePageLump(sizeof(AnimSeqAssetHeader_t), SF_HEAD, 8);

    const size_t rseqNameBufLen = strlen(assetPath) + 1;
    const size_t rseqFileSize = rseqInput.GetSize();

    PakPageLump_s dataChunk = pak->CreatePageLump(rseqNameBufLen + rseqFileSize, SF_CPU, 1);

    // write the rseq file path into the data buffer
    memcpy(dataChunk.data, assetPath, rseqNameBufLen);

    // write the rseq data into the data buffer
    rseqInput.Read(dataChunk.data + rseqNameBufLen, rseqFileSize);
    rseqInput.Close();

    const mstudioseqdesc_t& seqdesc = *reinterpret_cast<mstudioseqdesc_t*>(dataChunk.data + rseqNameBufLen);
    AnimSeqAssetHeader_t* const aseqHeader = reinterpret_cast<AnimSeqAssetHeader_t*>(hdrChunk.data);

    aseqHeader->szname = dataChunk.GetPointer();
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSeqAssetHeader_t, szname)));

    aseqHeader->data = dataChunk.GetPointer(rseqNameBufLen);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimSeqAssetHeader_t, data)));

    if (seqdesc.numautolayers > 0)
        asset.ExpandGuidBuf(seqdesc.numautolayers);

    rmem dataBuf(dataChunk.data);
    dataBuf.seek(rseqNameBufLen + seqdesc.autolayerindex, rseekdir::beg);

    // Iterate over each of the sequence's autolayers to register each of the autolayer GUIDs
    // This is required as otherwise the game will crash while trying to dereference a non-converted GUID.
    for (int i = 0; i < seqdesc.numautolayers; ++i)
    {
        dataBuf.seek(rseqNameBufLen + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);
        const mstudioautolayer_t* const autolayer = dataBuf.get<const mstudioautolayer_t>();

        const size_t offset = dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid);
        Pak_RegisterGuidRefAtOffset(pak, autolayer->guid, offset, dataChunk, asset);
    }

    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.size, PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::ASEQ);
    asset.SetHeaderPointer(hdrChunk.data);

    asset.version = 7;
    asset.pageEnd = pak->GetNumPages();

    pak->PushAsset(asset);
}

PakGuid_t* AnimSeq_AutoAddSequenceRefs(CPakFile* const pak, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry)
{
    rapidjson::Value::ConstMemberIterator sequencesIt;

    if (!JSON_GetIterator(mapEntry, "$sequences", JSONFieldType_e::kArray, sequencesIt))
        return nullptr;

    const rapidjson::Value::ConstArray sequencesArray = sequencesIt->value.GetArray();

    if (sequencesArray.Empty())
        return nullptr;

    const size_t numSequences = sequencesArray.Size();
    PakGuid_t* const guidBuf = new PakGuid_t[numSequences];

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
            guid = RTech::StringToGuid(sequencePath);

            const PakAsset_t* const existingAsset = pak->GetAssetByGuid(guid, nullptr, true);

            if (!existingAsset)
            {
                Log("Auto-adding 'aseq' asset \"%s\".\n", sequencePath);
                AnimSeq_InternalAddAnimSeq(pak, guid, sequencePath);
            }
        }

        guidBuf[seqIndex] = guid;
    }

    (*sequenceCount) = static_cast<uint32_t>(numSequences);
    return guidBuf;
}

void Assets::AddAnimSeqAsset_v7(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    AnimSeq_InternalAddAnimSeq(pak, assetGuid, assetPath);
}