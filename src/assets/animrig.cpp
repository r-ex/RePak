#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"
#include <public/animrig.h>

extern bool AnimSeq_AddSequenceRefs(CPakDataChunk* const chunk, CPakFile* const pak, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry);

// anim rigs are stored in rmdl's. use this to read it out.
extern char* Model_ReadRMDLFile(const std::string& path);

void Assets::AddAnimRigAsset_v4(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(AnimRigAssetHeader_t), SF_HEAD, 8);
    const size_t assetNameLength = strlen(assetPath);

    CPakDataChunk nameChunk = pak->CreateDataChunk(assetNameLength + 1, SF_CPU, 1); // [rika]: only aligned to 1 byte in season 3 paks
    memcpy_s(nameChunk.Data(), assetNameLength, assetPath, assetNameLength);

    // open and validate file to get buffer
    const char* const animRigFileBuffer = Model_ReadRMDLFile(pak->GetAssetPath() + assetPath);
    const studiohdr_t* const studiohdr = reinterpret_cast<const studiohdr_t*>(animRigFileBuffer);

    CPakDataChunk rigChunk = pak->CreateDataChunk(studiohdr->length, SF_CPU, 64);
    memcpy_s(rigChunk.Data(), studiohdr->length, animRigFileBuffer, studiohdr->length);

    delete[] animRigFileBuffer;

    AnimRigAssetHeader_t* const pHdr = reinterpret_cast<AnimRigAssetHeader_t*>(hdrChunk.Data());
    pHdr->data = rigChunk.GetPointer();
    pHdr->name = nameChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, data)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, name)));

    std::vector<PakGuidRefHdr_t> guids{};
    CPakDataChunk guidsChunk;
    if (AnimSeq_AddSequenceRefs(&guidsChunk, pak, &pHdr->sequenceCount, mapEntry))
    {
        pHdr->pSequences = guidsChunk.GetPointer();
        pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, pSequences)));

        for (uint32_t i = 0; i < pHdr->sequenceCount; ++i)
        {
            guids.emplace_back(guidsChunk.GetPointer(i * sizeof(PakGuid_t)));
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