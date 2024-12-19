#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"
#include <public/animrig.h>

extern PakGuid_t* AnimSeq_AutoAddSequenceRefs(CPakFile* const pak, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry);

// anim rigs are stored in rmdl's. use this to read it out.
extern char* Model_ReadRMDLFile(const std::string& path, const uint64_t alignment);

// page chunk structure and order:
// - header HEAD        (align=8)
// - data   CPU         (align=8) name, rmdl then refs. name and rmdl are aligned to 1 byte, refs are 8 (padded from rmdl buffer)
void Assets::AddAnimRigAsset_v4(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    short internalDependencyCount = 0; // number of dependencies inside this pak

    // deal with dependencies first; auto-add all animation sequences.
    uint32_t sequenceCount = 0;
    PakGuid_t* const sequenceRefs = AnimSeq_AutoAddSequenceRefs(pak, &sequenceCount, mapEntry);

    // from here we start with creating chunks for the target animrig asset.
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(AnimRigAssetHeader_t), SF_HEAD, 8);
    AnimRigAssetHeader_t* const pHdr = reinterpret_cast<AnimRigAssetHeader_t*>(hdrChunk.Data());

    // open and validate file to get buffer
    char* const animRigFileBuffer = Model_ReadRMDLFile(pak->GetAssetPath() + assetPath, 8);
    const studiohdr_t* const studiohdr = reinterpret_cast<const studiohdr_t*>(animRigFileBuffer);

    // note: both of these are aligned to 1 byte, but we pad the rmdl buffer as
    // the guid ref block needs to be aligned to 8 bytes.
    const size_t assetNameBufLen = strlen(assetPath) + 1;
    const size_t rmdlBufLen = IALIGN8(studiohdr->length);

    const size_t sequenceRefBufLen = sequenceCount * sizeof(PakGuid_t);

    CPakDataChunk rigChunk = pak->CreateDataChunk(assetNameBufLen + rmdlBufLen + sequenceRefBufLen, SF_CPU, 8);
    char* const nameBuf = rigChunk.Data();

    memcpy(nameBuf, assetPath, assetNameBufLen);
    pHdr->name = rigChunk.GetPointer();
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, name)));

    studiohdr_t* const studioBuf = reinterpret_cast<studiohdr_t*>(&rigChunk.Data()[assetNameBufLen]);

    memcpy(studioBuf, animRigFileBuffer, studiohdr->length);
    pHdr->data = rigChunk.GetPointer(assetNameBufLen);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, data)));

    delete[] animRigFileBuffer;
    std::vector<PakGuidRefHdr_t> guids;

    if (sequenceRefs)
    {
        const size_t base = assetNameBufLen + rmdlBufLen;
        PakGuid_t* const sequenceRefBuf = reinterpret_cast<PakGuid_t*>(&rigChunk.Data()[base]);

        memcpy(sequenceRefBuf, sequenceRefs, sequenceRefBufLen);
        delete[] sequenceRefs;

        pHdr->sequenceCount = sequenceCount;
        pHdr->pSequences = rigChunk.GetPointer(base);

        pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, pSequences)));
        guids.resize(sequenceCount);

        for (uint32_t i = 0; i < sequenceCount; ++i)
        {
            const size_t offset = base + (i * sizeof(PakGuid_t));
            const PakGuid_t guid = *reinterpret_cast<PakGuid_t*>(&rigChunk.Data()[offset]);

            if (pak->GetAssetByGuid(guid))
                internalDependencyCount++;

            guids[i] = rigChunk.GetPointer(offset);
        }
    }

    PakAsset_t asset;

    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::ARIG);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = 4;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = internalDependencyCount + 1;

    asset.AddGuids(&guids);

    pak->PushAsset(asset);
}