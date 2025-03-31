#include "pch.h"
#include "assets.h"
#include "public/studio.h"

// page chunk structure and order:
// - header HEAD        (align=8)
// - data   CPU         (align=1) name, then rmdl. unlike models, this is aligned to 1 since we don't have BVH4 collision data here.
static void AnimSeq_InternalAddAnimSeq(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
    const std::string rseqFilePath = pak->GetAssetPath() + assetPath;

    // begin rseq input
    BinaryIO rseqInput;

    if (!rseqInput.Open(rseqFilePath, BinaryIO::Mode_e::Read))
    {
        Error("Failed to open animseq asset \"%s\".\n", assetPath);
        return;
    }

    PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(AnimSeqAssetHeader_t), SF_HEAD, 8);

    const size_t rseqNameBufLen = strlen(assetPath) + 1;
    const size_t rseqFileSize = rseqInput.GetSize();

    PakPageLump_s dataLump = pak->CreatePageLump(rseqNameBufLen + rseqFileSize, SF_CPU, 1);

    // write the rseq file path into the data buffer
    memcpy(dataLump.data, assetPath, rseqNameBufLen);

    // write the rseq data into the data buffer
    rseqInput.Read(dataLump.data + rseqNameBufLen, rseqFileSize);
    rseqInput.Close();

    const mstudioseqdesc_t& seqdesc = *reinterpret_cast<mstudioseqdesc_t*>(dataLump.data + rseqNameBufLen);

    pak->AddPointer(hdrLump, offsetof(AnimSeqAssetHeader_t, szname), dataLump, 0);
    pak->AddPointer(hdrLump, offsetof(AnimSeqAssetHeader_t, data), dataLump, rseqNameBufLen);

    if (seqdesc.numautolayers > 0)
        asset.ExpandGuidBuf(seqdesc.numautolayers);

    rmem dataBuf(dataLump.data);
    dataBuf.seek(rseqNameBufLen + seqdesc.autolayerindex, rseekdir::beg);

    // Iterate over each of the sequence's autolayers to register each of the autolayer GUIDs
    // This is required as otherwise the game will crash while trying to dereference a non-converted GUID.
    for (int i = 0; i < seqdesc.numautolayers; ++i)
    {
        dataBuf.seek(rseqNameBufLen + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);
        const mstudioautolayer_t* const autolayer = dataBuf.get<const mstudioautolayer_t>();

        const size_t offset = dataBuf.getPosition() + offsetof(mstudioautolayer_t, guid);
        Pak_RegisterGuidRefAtOffset(autolayer->guid, offset, dataLump, asset);
    }

    asset.InitAsset(hdrLump.GetPointer(), sizeof(AnimSeqAssetHeader_t), PagePtr_t::NullPtr(), ASEQ_VERSION, AssetType::ASEQ);
    asset.SetHeaderPointer(hdrLump.data);

    pak->FinishAsset();
}

PakGuid_t* AnimSeq_AutoAddSequenceRefs(CPakFileBuilder* const pak, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry)
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

        char buffer[32]; const char* base = "sequence #";
        char* current = std::copy(base, base + 10, buffer);
        std::to_chars_result result = std::to_chars(current, buffer + sizeof(buffer), seqIndex);

        *result.ptr = '\0';

        const char* sequenceName = nullptr;
        const PakGuid_t guid = Pak_ParseGuidFromObject(sequence, buffer, sequenceName);

        if (sequenceName)
        {
            const PakAsset_t* const existingAsset = pak->GetAssetByGuid(guid, nullptr, true);

            if (!existingAsset)
            {
                Debug("Auto-adding 'aseq' asset \"%s\".\n", sequenceName);
                AnimSeq_InternalAddAnimSeq(pak, guid, sequenceName);
            }
        }

        guidBuf[seqIndex] = guid;
    }

    (*sequenceCount) = static_cast<uint32_t>(numSequences);
    return guidBuf;
}

void Assets::AddAnimSeqAsset_v7(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    AnimSeq_InternalAddAnimSeq(pak, assetGuid, assetPath);
}