#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"
#include <public/animrig.h>

bool AnimRig_AddSequenceRefs(CPakDataChunk* chunk, CPakFile* pak, AnimRigAssetHeader_t* hdr, const rapidjson::Value& mapEntry)
{
    rapidjson::Value::ConstMemberIterator it;

    if (!JSON_GetIterator(mapEntry, "sequences", JSONFieldType_e::kArray, it))
        return false;

    std::vector<PakGuid_t> sequenceGuids;

    for (const auto& sequenceElem : it->value.GetArray())
    {
        if (!sequenceElem.IsString())
            continue;

        if (sequenceElem.GetStringLength() == 0)
            continue;

        PakGuid_t guid;

        if (!JSON_ParseNumber(sequenceElem, guid))
        {
            guid = RTech::StringToGuid(sequenceElem.GetString());
            Assets::AddAnimSeqAsset(pak, guid, sequenceElem.GetString());
        }

        sequenceGuids.emplace_back(guid);
        hdr->sequenceCount++;
    }

    CPakDataChunk guidsChunk = pak->CreateDataChunk(sizeof(PakGuid_t) * sequenceGuids.size(), SF_CPU, 64);

    PakGuid_t* const pGuids = reinterpret_cast<PakGuid_t*>(guidsChunk.Data());
    for (int i = 0; i < sequenceGuids.size(); ++i)
    {
        pGuids[i] = sequenceGuids[i];
    }

    *chunk = guidsChunk;
    return true;
}

// anim rigs are stored in rmdl's. use this to read it out.
extern char* Model_ReadRMDLFile(const std::string& path);

void Assets::AddAnimRigAsset_v4(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    // open and validate file to get buffer
    const char* const animRigFileBuffer = Model_ReadRMDLFile(pak->GetAssetPath() + assetPath);
    const studiohdr_t* const studiohdr = reinterpret_cast<const studiohdr_t*>(animRigFileBuffer);

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(AnimRigAssetHeader_t), SF_HEAD, 16);

    const size_t assetNameLength = strlen(assetPath);

    CPakDataChunk nameChunk = pak->CreateDataChunk(assetNameLength + 1, SF_CPU, 1); // [rika]: only aligned to 1 byte in season 3 paks
    memcpy_s(nameChunk.Data(), assetNameLength, assetPath, assetNameLength);

    CPakDataChunk rigChunk = pak->CreateDataChunk(studiohdr->length, SF_CPU, 64);
    memcpy_s(rigChunk.Data(), studiohdr->length, animRigFileBuffer, studiohdr->length);

    AnimRigAssetHeader_t* const pHdr = reinterpret_cast<AnimRigAssetHeader_t*>(hdrChunk.Data());
    pHdr->data = rigChunk.GetPointer();
    pHdr->name = nameChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, data)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, name)));

    std::vector<PakGuidRefHdr_t> guids{};
    CPakDataChunk guidsChunk;
    if (AnimRig_AddSequenceRefs(&guidsChunk, pak, pHdr, mapEntry))
    {
        pHdr->pSequences = guidsChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, pSequences)));

        for (int i = 0; i < pHdr->sequenceCount; ++i)
        {
            guids.emplace_back(guidsChunk.GetPointer(8 * i));
        }
    }

    delete[] animRigFileBuffer;

    PakAsset_t asset;

    asset.InitAsset(assetPath, Pak_GetGuidOverridable(mapEntry, assetPath), hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::ARIG);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = 4;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    pak->PushAsset(asset);
}