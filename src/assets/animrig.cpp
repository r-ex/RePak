#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"
#include <public/animrig.h>

extern bool AnimSeq_AddSequenceRefs(CPakFile* const pak, CPakDataChunk* const chunk, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry);

// anim rigs are stored in rmdl's. use this to read it out.
extern char* Model_ReadRMDLFile(const std::string& path, const uint64_t alignment);

void Assets::AddAnimRigAsset_v4(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    // deal with dependencies first; auto-add all animation sequences.
    CPakDataChunk sequenceRefChunk; uint32_t sequenceCount;
    const bool hasAnimSeqRefs = AnimSeq_AddSequenceRefs(pak, &sequenceRefChunk, &sequenceCount, mapEntry);

    // from here we start with creating chunks for the target animrig asset.
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(AnimRigAssetHeader_t), SF_HEAD, 8);
    const size_t assetNameLength = strlen(assetPath);

    CPakDataChunk nameChunk = pak->CreateDataChunk(assetNameLength + 1, SF_CPU, 1); // [rika]: only aligned to 1 byte in season 3 paks
    memcpy_s(nameChunk.Data(), assetNameLength, assetPath, assetNameLength);

    // open and validate file to get buffer
    char* const animRigFileBuffer = Model_ReadRMDLFile(pak->GetAssetPath() + assetPath, 8);
    const studiohdr_t* const studiohdr = reinterpret_cast<const studiohdr_t*>(animRigFileBuffer);

    CPakDataChunk rigChunk = pak->CreateDataChunk(studiohdr->length, SF_CPU, 8, animRigFileBuffer);

    AnimRigAssetHeader_t* const pHdr = reinterpret_cast<AnimRigAssetHeader_t*>(hdrChunk.Data());
    pHdr->data = rigChunk.GetPointer();
    pHdr->name = nameChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, data)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, name)));

    std::vector<PakGuidRefHdr_t> guids;

    if (hasAnimSeqRefs)
    {
        guids.resize(sequenceCount);

        pHdr->sequenceCount = sequenceCount;
        pHdr->pSequences = sequenceRefChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, pSequences)));

        for (uint32_t i = 0; i < sequenceCount; ++i)
        {
            guids[i] = sequenceRefChunk.GetPointer(i * sizeof(PakGuid_t));
        }
    }

    PakAsset_t asset;

    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::ARIG);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = 4;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    pak->PushAsset(asset);
}