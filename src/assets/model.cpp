#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"

char* Model_ReadRMDLFile(const std::string& path, const uint64_t alignment = 64)
{
    BinaryIO modelFile;

    if (!modelFile.Open(path, BinaryIO::Mode_e::Read))
        Error("Failed to open model file \"%s\".\n", path.c_str());

    const size_t fileSize = modelFile.GetSize();

    if (fileSize < sizeof(studiohdr_t))
        Error("Invalid model file \"%s\"; must be at least %zu bytes, found %zu.\n", path.c_str(), sizeof(studiohdr_t), fileSize);

    char* const buf = new char[IALIGN(fileSize, alignment)];
    modelFile.Read(buf, fileSize);

    studiohdr_t* const pHdr = reinterpret_cast<studiohdr_t*>(buf);

    if (pHdr->id != 'TSDI') // "IDST"
        Error("Invalid model file \"%s\"; expected magic %x, found %x.\n", path.c_str(), 'TSDI', pHdr->id);

    if (pHdr->version != 54)
        Error("Invalid model file \"%s\"; expected version %i, found %i.\n", path.c_str(), 54, pHdr->version);

    if (pHdr->length > fileSize)
        Error("Invalid model file \"%s\"; studiohdr->length(%zu) > fileSize(%zu).\n", path.c_str(), (size_t)pHdr->length, fileSize);

    return buf;
}

static char* Model_ReadVGFile(const std::string& path, size_t* const pFileSize)
{
    BinaryIO vgFile;

    if (!vgFile.Open(path, BinaryIO::Mode_e::Read))
        Error("Failed to open vertex group file \"%s\".\n", path.c_str());

    const size_t fileSize = vgFile.GetSize();

    if (fileSize < sizeof(VertexGroupHeader_t))
        Error("Invalid vertex group file \"%s\"; must be at least %zu bytes, found %zu.\n", path.c_str(), sizeof(VertexGroupHeader_t), fileSize);

    char* const buf = new char[fileSize];
    vgFile.Read(buf, fileSize);

    VertexGroupHeader_t* const pHdr = reinterpret_cast<VertexGroupHeader_t*>(buf);

    if (pHdr->id != 'GVt0') // "0tVG"
        Error("Invalid vertex group file \"%s\"; expected magic %x, found %x.\n", path.c_str(), 'GVt0', pHdr->id);

    // not sure if this is actually version but i've also never seen it != 1
    if (pHdr->version != 1)
        Error("Invalid vertex group file \"%s\"; expected version %i, found %i.\n", path.c_str(), 1, pHdr->version);

    *pFileSize = fileSize;
    return buf;
}

static PakGuid_t* Model_AddAnimRigRefs(uint32_t* const animrigCount, const rapidjson::Value& mapEntry)
{
    rapidjson::Value::ConstMemberIterator it;

    if (!JSON_GetIterator(mapEntry, "$animrigs", JSONFieldType_e::kArray, it))
        return nullptr;

    const rapidjson::Value::ConstArray animrigs = it->value.GetArray();

    if (animrigs.Empty())
        return nullptr;

    const size_t numAnimrigs = animrigs.Size();
    PakGuid_t* const guidBuf = new PakGuid_t[numAnimrigs];

    int i = -1;
    for (const auto& animrig : animrigs)
    {
        i++;
        const PakGuid_t guid = Pak_ParseGuid(animrig);

        if (!guid)
            Error("Unable to parse animrig #%i.\n", i);

        guidBuf[i] = guid;
    }

    (*animrigCount) = static_cast<uint32_t>(animrigs.Size());
    return guidBuf;
}

static void Model_AllocateIntermediateDataChunk(CPakFileBuilder* const pak, PakPageLump_s& hdrChunk, ModelAssetHeader_t* const pHdr,
    PakGuid_t* const animrigRefs, const uint32_t animrigCount, PakGuid_t* const sequenceRefs, const uint32_t sequenceCount, 
    const char* const assetPath, PakAsset_t& asset)
{
    // the model name is aligned to 1 byte, but the guid ref block is aligned
    // to 8, we have to pad the name buffer to align the guid ref block. if
    // we have no guid ref blocks, the entire lump will be aligned to 1 byte.
    const size_t modelNameBufLen = strlen(assetPath) + 1;
    const size_t alignedNameBufLen = IALIGN8(modelNameBufLen);

    const size_t animRigRefsBufLen = animrigCount * sizeof(PakGuid_t);
    const size_t sequenceRefsBufLen = sequenceCount * sizeof(PakGuid_t);

    const bool hasGuidRefs = animrigRefs || sequenceRefs;

    PakPageLump_s intermediateChunk = pak->CreatePageLump(alignedNameBufLen + animRigRefsBufLen + sequenceRefsBufLen, SF_CPU, hasGuidRefs ? 8 : 1);
    memcpy(intermediateChunk.data, assetPath, modelNameBufLen); // Write the null-terminated asset path to the chunk buffer.

    pak->AddPointer(hdrChunk, offsetof(ModelAssetHeader_t, pName), intermediateChunk, 0);

    if (hasGuidRefs)
    {
        asset.ExpandGuidBuf(animrigCount + sequenceCount);
        uint64_t curIndex = 0;

        if (animrigRefs)
        {
            const size_t base = alignedNameBufLen;

            memcpy(&intermediateChunk.data[base], animrigRefs, animRigRefsBufLen);
            delete[] animrigRefs;

            pHdr->animRigCount = animrigCount;
            pak->AddPointer(hdrChunk, offsetof(ModelAssetHeader_t, pAnimRigs), intermediateChunk, base);

            for (uint32_t i = 0; i < animrigCount; ++i)
            {
                const size_t offset = base + (i * sizeof(PakGuid_t));
                const PakGuid_t guid = *reinterpret_cast<PakGuid_t*>(&intermediateChunk.data[offset]);

                Pak_RegisterGuidRefAtOffset(guid, offset, intermediateChunk, asset);
                curIndex++;
            }
        }

        if (sequenceRefs)
        {
            const size_t base = alignedNameBufLen + (curIndex * sizeof(PakGuid_t));

            memcpy(&intermediateChunk.data[base], sequenceRefs, sequenceRefsBufLen);
            delete[] sequenceRefs;

            pHdr->sequenceCount = sequenceCount;
            pak->AddPointer(hdrChunk, offsetof(ModelAssetHeader_t, pSequences), intermediateChunk, base);

            for (uint32_t i = 0; i < sequenceCount; ++i)
            {
                const size_t offset = base + (i * sizeof(PakGuid_t));
                const PakGuid_t guid = *reinterpret_cast<PakGuid_t*>(&intermediateChunk.data[offset]);

                Pak_RegisterGuidRefAtOffset(guid, offset, intermediateChunk, asset);
            }
        }
    }
}

static uint64_t Model_InternalAddVertexGroupData(CPakFileBuilder* const pak, PakPageLump_s* const hdrChunk, ModelAssetHeader_t* const modelHdr, studiohdr_t* const studiohdr, const std::string& rmdlFilePath)
{
    modelHdr->totalVertexDataSize = studiohdr->vtxsize + studiohdr->vvdsize + studiohdr->vvcsize + studiohdr->vvwsize;

    ///--------------------
    // Add VG data
    // VG is a "fake" file extension that's used to store model streaming data (name came from the magic '0tVG')
    // this data is a combined mutated version of the data from .vtx and .vvd in regular source models
    const std::string vgFilePath = Utils::ChangeExtension(rmdlFilePath, ".vg");

    size_t vgFileSize = 0;
    char* const vgBuf = Model_ReadVGFile(vgFilePath, &vgFileSize);

    PakStreamSetEntry_s de{ 0, vgFileSize };
    pak->AddStreamingDataEntry(de, (uint8_t*)vgBuf, STREAMING_SET_MANDATORY);

    assert(de.dataSize <= UINT32_MAX);
    modelHdr->streamedVertexDataSize = static_cast<uint32_t>(de.dataSize);

    // static props must have their vertex group data copied as permanent data in the pak file.
    if (studiohdr->IsStaticProp())
    {
        PakPageLump_s vgLump = pak->CreatePageLump(vgFileSize, SF_CPU | SF_TEMP | SF_CLIENT, 1, vgBuf);
        pak->AddPointer(*hdrChunk, offsetof(ModelAssetHeader_t, pStaticPropVtxCache), vgLump, 0);
    }
    else
        delete[] vgBuf;

    return de.offset;
}

extern PakGuid_t* AnimSeq_AutoAddSequenceRefs(CPakFileBuilder* const pak, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry);

// page chunk structure and order:
// - header        HEAD        (align=8)
// - intermediate  CPU         (align=1?8) name, animrig refs then animseqs refs. aligned to 1 if we don't have any refs.
// - vphysics      TEMP        (align=1)
// - vertex groups TEMP_CLIENT (align=1)
// - rmdl          CPU         (align=64) 64 bit aligned because collision data is loaded with aligned SIMD instructions.
void Assets::AddModelAsset_v9(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    PakAsset_t asset;

    // deal with dependencies first; auto-add all animation sequences.
    uint32_t sequenceCount = 0;
    PakGuid_t* const sequenceRefs = AnimSeq_AutoAddSequenceRefs(pak, &sequenceCount, mapEntry);

    // this function only creates the arig guid refs, it does not auto-add.
    uint32_t animrigCount = 0;
    PakGuid_t* const animrigRefs = Model_AddAnimRigRefs(&animrigCount, mapEntry);

    // from here we start with creating chunks for the target model asset.
    PakPageLump_s hdrChunk = pak->CreatePageLump(sizeof(ModelAssetHeader_t), SF_HEAD, 8);
    ModelAssetHeader_t* const pHdr = reinterpret_cast<ModelAssetHeader_t*>(hdrChunk.data);

    //
    // Name, Anim Rigs and Animseqs, these all share 1 data chunk.
    //
    Model_AllocateIntermediateDataChunk(pak, hdrChunk, pHdr, animrigRefs, animrigCount, sequenceRefs, sequenceCount, assetPath, asset);

    const std::string rmdlFilePath = pak->GetAssetPath() + assetPath;

    char* const rmdlBuf = Model_ReadRMDLFile(rmdlFilePath);
    studiohdr_t* const studiohdr = reinterpret_cast<studiohdr_t*>(rmdlBuf);

    //
    // Physics
    //
    if (studiohdr->vphysize)
    {
        BinaryIO phyInput;
        const std::string physicsFile = Utils::ChangeExtension(rmdlFilePath, ".phy");

        if (!phyInput.Open(physicsFile, BinaryIO::Mode_e::Read))
            Error("Failed to open physics asset \"%s\".\n", physicsFile.c_str());

        const size_t phyFileSize = phyInput.GetSize();

        if (studiohdr->vphysize != phyFileSize)
            Error("Expected physics file size is %zu, found physics asset of size %zu.\n", (size_t)studiohdr->vphysize, phyFileSize);

        PakPageLump_s phyChunk = pak->CreatePageLump(phyFileSize, SF_CPU | SF_TEMP, 1);
        phyInput.Read(phyChunk.data, phyFileSize);

        pak->AddPointer(hdrChunk, offsetof(ModelAssetHeader_t, pPhyData), phyChunk, 0);
    }

    //
    // Starpak
    //
    uint64_t streamedVgOffset;

    if (pak->IsFlagSet(PF_KEEP_CLIENT))
        streamedVgOffset = Model_InternalAddVertexGroupData(pak, &hdrChunk, pHdr, studiohdr, rmdlFilePath);
    else
        streamedVgOffset = UINT64_MAX;

    // the last chunk is the actual data chunk that contains the rmdl
    PakPageLump_s dataChunk = pak->CreatePageLump(studiohdr->length, SF_CPU, 64, rmdlBuf);
    pak->AddPointer(hdrChunk, offsetof(ModelAssetHeader_t, pData), dataChunk, 0);

    // Material Overrides Handling
    rapidjson::Value::ConstMemberIterator materialsIt;

    // todo(amos): do we even want material overrides? shouldn't these need to
    // be fixed in the studiomdl itself? there are reports of this causing many
    // errors as the game tries to read the path from the mdl itself which this
    // loop below doesn't update.
    const bool hasMaterialOverrides = JSON_GetIterator(mapEntry, "$materials", JSONFieldType_e::kArray, materialsIt);
    const rapidjson::Value* materialOverrides = hasMaterialOverrides ? &materialsIt->value : nullptr;

    // handle material overrides register all material guids
    for (int i = 0; i < studiohdr->numtextures; ++i)
    {
        mstudiotexture_t* const tex = studiohdr->pTexture(i);

        if (hasMaterialOverrides)
        {
            rapidjson::Value::ConstArray materialArray = materialOverrides->GetArray();

            if (materialArray.Size() > i)
            {
                const PakGuid_t guid = Pak_ParseGuid(materialArray[i]);

                if (!guid)
                    Error("Unable to parse material #%i.\n", i);

                tex->guid = guid;
            }
        }

        const size_t pos = (char*)tex - dataChunk.data;
        const size_t offset = pos + offsetof(mstudiotexture_t, guid);

        Pak_RegisterGuidRefAtOffset(tex->guid, offset, dataChunk, asset);
        const PakAsset_t* const internalAsset = pak->GetAssetByGuid(tex->guid);

        if (internalAsset)
        {
            // make sure referenced asset is a material for sanity
            internalAsset->EnsureType(TYPE_MATL);

            // model assets don't exist on r2 so we can be sure that this is a v8 pak (and therefore has v15 materials)
            MaterialAssetHeader_v15_t* const matlHdr = reinterpret_cast<MaterialAssetHeader_v15_t*>(internalAsset->header);

            if (matlHdr->materialType != studiohdr->materialType(i))
            {
                Error("Unexpected shader type for material in slot #%i, expected '%s', found '%s'.\n",
                    i, s_materialShaderTypeNames[studiohdr->materialType(i)], s_materialShaderTypeNames[matlHdr->materialType]);
            }
        }
    }

    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.size, PagePtr_t::NullPtr(), streamedVgOffset, UINT64_MAX, AssetType::RMDL);
    asset.SetHeaderPointer(hdrChunk.data);
  
    asset.version = RMDL_VERSION;
    asset.pageEnd = pak->GetNumPages();

    pak->PushAsset(asset);
}