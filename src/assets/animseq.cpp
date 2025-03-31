#include "pch.h"
#include "assets.h"
#include "public/studio.h"

static void AnimSeq_ParseDependencies(const uint8_t* const data, std::set<PakGuid_t>& dependencies)
{
    const mstudioseqdesc_t& seqdesc = *reinterpret_cast<const mstudioseqdesc_t*>(data);

    for (int i = 0; i < seqdesc.numevents; i++)
    {
        const mstudioevent_t* const event = seqdesc.pEvent(i);

        const char* const depStart = strchr(event->options, '@');

        if (!depStart || *(depStart + 1) == '\0')
            continue; // '@' not found or nothing after '@'.

        const char* start = strchr(depStart + 1, ' ');

        if (!start || *(start + 1) == '\0')
            continue; // Start of asset name not found or empty.

        // Advance over the ' ' so it point directly at the
        // start of the asset name.
        start++;

        const char* end = strchr(start, ' ');
        if (!end)
            end = event->options + strlen(event->options);

        if (start == end)
            continue; // Empty dependency name.

        std::string dependencyName(start, end);
        dependencies.insert(RTech::StringToGuid(dependencyName.c_str()));
    }
}

// page chunk structure and order:
// - header HEAD        (align=8)
// - data   CPU         (align=1?8) dependencies, name, then rmdl. unlike models, this is aligned to 1 since we don't have BVH4 collision data here, aligned to 8 if we have dependencies.
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

    const size_t rseqFileSize = rseqInput.GetSize();
    uint8_t* const tempAseqBuf = new uint8_t[rseqFileSize];

    // write the rseq data into the data buffer
    rseqInput.Read(tempAseqBuf, rseqFileSize);
    rseqInput.Close();

    // Parse out the dependencies which we need to know in advance.
    // NOTE: original paks duplicate the dependencies for animation
    // sequences, but this is not necessary, so we drop duplicates.
    std::set<PakGuid_t> dependencies;
    AnimSeq_ParseDependencies(tempAseqBuf, dependencies);

    const size_t numDependencies = dependencies.size();

    const size_t dependenciesBufSize = numDependencies * sizeof(PakGuid_t);
    const size_t rseqNameBufLen = strlen(assetPath) + 1;

    PakPageLump_s dataLump = pak->CreatePageLump((dependenciesBufSize + rseqNameBufLen + rseqFileSize), SF_CPU, numDependencies > 0 ? 8 : 1);

    for (size_t i = 0; i < numDependencies; i++)
    {
        std::set<PakGuid_t>::iterator it = dependencies.begin();
        std::advance(it, i);

        const PakGuid_t guidToCopy = *it;
        reinterpret_cast<PakGuid_t*>(dataLump.data)[i] = guidToCopy;

        Pak_RegisterGuidRefAtOffset(guidToCopy, i * sizeof(PakGuid_t), dataLump, asset);
    }

    const size_t nameOffset = dependenciesBufSize;
    const size_t dataOffset = dependenciesBufSize + rseqNameBufLen;

    // write the rseq file path into the data buffer
    memcpy(dataLump.data + nameOffset, assetPath, rseqNameBufLen);
    memcpy(dataLump.data + dataOffset, tempAseqBuf, rseqFileSize);

    delete[] tempAseqBuf; // No longer needed.

    const mstudioseqdesc_t& seqdesc = *reinterpret_cast<mstudioseqdesc_t*>(dataLump.data + dataOffset);

    pak->AddPointer(hdrLump, offsetof(AnimSeqAssetHeader_t, szname), dataLump, nameOffset);
    pak->AddPointer(hdrLump, offsetof(AnimSeqAssetHeader_t, data), dataLump, dataOffset);

    if (seqdesc.numautolayers > 0)
        asset.ExpandGuidBuf(seqdesc.numautolayers);

    rmem dataBuf(dataLump.data);
    dataBuf.seek(dataOffset + seqdesc.autolayerindex, rseekdir::beg);

    // Iterate over each of the sequence's autolayers to register each of the autolayer GUIDs
    // This is required as otherwise the game will crash while trying to dereference a non-converted GUID.
    for (int i = 0; i < seqdesc.numautolayers; ++i)
    {
        dataBuf.seek(dataOffset + seqdesc.autolayerindex + (i * sizeof(mstudioautolayer_t)), rseekdir::beg);
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