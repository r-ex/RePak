#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"
#include <public/animrig.h>

extern PakGuid_t* AnimSeq_AutoAddSequenceRefs(CPakFileBuilder* const pak, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry);

// anim rigs are stored in rmdl's. use this to read it out.
extern char* Model_ReadRMDLFile(const std::string& path, const uint64_t alignment);

// page chunk structure and order:
// - header HEAD        (align=8)
// - data   CPU         (align=8) name, rmdl then refs. name and rmdl are aligned to 1 byte, refs are 8 (padded from rmdl buffer)
void Assets::AddAnimRigAsset_v4(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    // deal with dependencies first; auto-add all animation sequences.
    uint32_t sequenceCount = 0;
    PakGuid_t* const sequenceRefs = AnimSeq_AutoAddSequenceRefs(pak, &sequenceCount, mapEntry);

    // from here we start with creating lumps for the target animrig asset.
    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);

    PakPageLump_s hdrChunk = pak->CreatePageLump(sizeof(AnimRigAssetHeader_t), SF_HEAD, 8);
    AnimRigAssetHeader_t* const pHdr = reinterpret_cast<AnimRigAssetHeader_t*>(hdrChunk.data);

    // open and validate file to get buffer
    char* const animRigFileBuffer = Model_ReadRMDLFile(pak->GetAssetPath() + assetPath, 8);
    const studiohdr_t* const studiohdr = reinterpret_cast<const studiohdr_t*>(animRigFileBuffer);

    // note: both of these are aligned to 1 byte, but we pad the rmdl buffer as
    // the guid ref block needs to be aligned to 8 bytes.
    const size_t assetNameBufLen = strlen(assetPath) + 1;
    const size_t rmdlBufLen = IALIGN8(studiohdr->length);

    const size_t sequenceRefBufLen = sequenceCount * sizeof(PakGuid_t);

    PakPageLump_s rigChunk = pak->CreatePageLump(assetNameBufLen + rmdlBufLen + sequenceRefBufLen, SF_CPU, 8);
    char* const nameBuf = rigChunk.data;

    memcpy(nameBuf, assetPath, assetNameBufLen);
    pak->AddPointer(hdrChunk, offsetof(AnimRigAssetHeader_t, name), rigChunk, 0);

    studiohdr_t* const studioBuf = reinterpret_cast<studiohdr_t*>(&rigChunk.data[assetNameBufLen]);

    memcpy(studioBuf, animRigFileBuffer, studiohdr->length);
    pak->AddPointer(hdrChunk, offsetof(AnimRigAssetHeader_t, data), rigChunk, assetNameBufLen);

    delete[] animRigFileBuffer;

    if (sequenceRefs)
    {
        const size_t base = assetNameBufLen + rmdlBufLen;
        PakGuid_t* const sequenceRefBuf = reinterpret_cast<PakGuid_t*>(&rigChunk.data[base]);

        memcpy(sequenceRefBuf, sequenceRefs, sequenceRefBufLen);
        delete[] sequenceRefs;

        pHdr->sequenceCount = sequenceCount;
        pak->AddPointer(hdrChunk, offsetof(AnimRigAssetHeader_t, pSequences), rigChunk, base);

        for (uint32_t i = 0; i < sequenceCount; ++i)
        {
            const size_t offset = base + (i * sizeof(PakGuid_t));
            const PakGuid_t guid = *reinterpret_cast<PakGuid_t*>(&rigChunk.data[offset]);

            Pak_RegisterGuidRefAtOffset(guid, offset, rigChunk, asset);
        }
    }

    asset.InitAsset(hdrChunk.GetPointer(), sizeof(AnimRigAssetHeader_t), PagePtr_t::NullPtr(), ARIG_VERSION, AssetType::ARIG);
    asset.SetHeaderPointer(hdrChunk.data);

    pak->FinishAsset();
}