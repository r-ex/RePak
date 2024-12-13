#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"

char* Model_ReadRMDLFile(const std::string& path)
{
    BinaryIO modelFile;

    if (!modelFile.Open(path, BinaryIO::Mode_e::Read))
        Error("Failed to open model file \"%s\".\n", path.c_str());

    const size_t fileSize = modelFile.GetSize();

    if (fileSize < sizeof(studiohdr_t))
        Error("Invalid model file \"%s\"; must be at least %zu bytes, found %zu.\n", path.c_str(), sizeof(studiohdr_t), fileSize);

    char* const buf = new char[fileSize];
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

extern bool AnimSeq_AddSequenceRefs(CPakDataChunk* const chunk, CPakFile* const pak, uint32_t* const sequenceCount, const rapidjson::Value& mapEntry);

void Assets::AddModelAsset_v9(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ModelAssetHeader_t), SF_HEAD, 16);
    ModelAssetHeader_t* pHdr = reinterpret_cast<ModelAssetHeader_t*>(hdrChunk.Data());

    std::vector<PakGuidRefHdr_t> guids;

    const std::string rmdlFilePath = pak->GetAssetPath() + assetPath;
    char* const rmdlBuf = Model_ReadRMDLFile(rmdlFilePath); // todo: free rmdl buf

    studiohdr_t* studiohdr = reinterpret_cast<studiohdr_t*>(rmdlBuf);

    //
    // Physics
    //
    size_t phyFileSize = 0;

    CPakDataChunk phyChunk;
    
    if (studiohdr->vphysize)
    {
        BinaryIO phyInput;
        const std::string physicsFile = Utils::ChangeExtension(rmdlFilePath, ".phy");

        if (!phyInput.Open(physicsFile, BinaryIO::Mode_e::Read))
            Error("Failed to open physics asset \"%s\".\n", physicsFile.c_str());

        phyFileSize = phyInput.GetSize();

        if (studiohdr->vphysize != phyFileSize)
            Error("Expected physics file size is %zu, found physics asset of size %zu.\n", (size_t)studiohdr->vphysize, phyFileSize);

        phyChunk = pak->CreateDataChunk(phyFileSize, SF_CPU, 64);
        phyInput.Read(phyChunk.Data(), phyFileSize);
    }

    //
    // Anim Rigs
    //
    CPakDataChunk animRigsChunk;

    rapidjson::Value::ConstMemberIterator it;
    const bool hasAnimRigs = JSON_GetIterator(mapEntry, "$animrigs", JSONFieldType_e::kArray, it);

    if (hasAnimRigs)
    {
        const rapidjson::Value::ConstArray animrigs = it->value.GetArray();
        const uint32_t numAnimrigs = (uint32_t)animrigs.Size();

        pHdr->animRigCount = numAnimrigs;

        animRigsChunk = pak->CreateDataChunk(numAnimrigs * sizeof(PakGuid_t), SF_CPU, 64);

        rmem arigBuf(animRigsChunk.Data());

        int i = -1;
        for (const auto& animrig : animrigs)
        {
            i++;
            const PakGuid_t guid = Pak_ParseGuid(animrig);

            if (!guid)
                Error("Unable to parse animrig #%i.\n", i);

            arigBuf.write<PakGuid_t>(guid);

            // check if anim rig is a local asset so that the relation can be added
            PakAsset_t* const animRigAsset = pak->GetAssetByGuid(guid);

            pak->SetCurrentAssetAsDependentForAsset(animRigAsset);
        }

        pHdr->pAnimRigs = animRigsChunk.GetPointer();
        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelAssetHeader_t, pAnimRigs)));

        for (uint32_t j = 0; j < pHdr->animRigCount; ++j)
        {
            guids.emplace_back(animRigsChunk.GetPointer(sizeof(PakGuid_t) * j));
        }
    }

    CPakDataChunk sequencesChunk;
    if (AnimSeq_AddSequenceRefs(&sequencesChunk, pak, &pHdr->sequenceCount, mapEntry))
    {
        pHdr->pSequences = sequencesChunk.GetPointer();
        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelAssetHeader_t, pSequences)));

        for (uint32_t i = 0; i < pHdr->sequenceCount; ++i)
        {
            guids.emplace_back(sequencesChunk.GetPointer(sizeof(PakGuid_t) * i));
        }
    }

    //
    // Starpak
    //
    pHdr->totalVertexDataSize = studiohdr->vtxsize + studiohdr->vvdsize + studiohdr->vvcsize + studiohdr->vvwsize;

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
    pHdr->streamedVertexDataSize = static_cast<uint32_t>(de.dataSize);

    size_t extraDataSize = 0;

    if (studiohdr->IsStaticProp())
    {
        extraDataSize = vgFileSize;
    }

    const size_t fileNameDataSize = strlen(assetPath) + 1;

    CPakDataChunk dataChunk = pak->CreateDataChunk(studiohdr->length + fileNameDataSize + extraDataSize, SF_CPU, 64);
    char* pDataBuf = dataChunk.Data();

    // write the model file path into the data buffer
    snprintf(pDataBuf + studiohdr->length, fileNameDataSize, "%s", assetPath);

    // copy rmdl into rpak buffer and move studiohdr ptr
    memcpy_s(pDataBuf, studiohdr->length, rmdlBuf, studiohdr->length);
    studiohdr = reinterpret_cast<studiohdr_t*>(pDataBuf);

    delete[] rmdlBuf;

    // copy static prop data into data buffer (if needed)
    if (studiohdr->IsStaticProp()) // STATIC_PROP
    {
        memcpy_s(pDataBuf + fileNameDataSize + studiohdr->length, vgFileSize, vgBuf, vgFileSize);
    }

    delete[] vgBuf;

    pHdr->pName = dataChunk.GetPointer(studiohdr->length);
    pHdr->pData = dataChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelAssetHeader_t, pData)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelAssetHeader_t, pName)));

    if (studiohdr->IsStaticProp()) // STATIC_PROP
    {
        pHdr->pStaticPropVtxCache = dataChunk.GetPointer(fileNameDataSize + studiohdr->length);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelAssetHeader_t, pStaticPropVtxCache)));
    }

    if (phyFileSize > 0)
    {
        pHdr->pPhyData = phyChunk.GetPointer();
        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelAssetHeader_t, pPhyData)));
    }

    rapidjson::Value::ConstMemberIterator materialsIt;

    // todo(amos): de we even want material overrides? shouldn't these need to
    // be fixed in the studiomdl itself? there are reports of this causing many
    // errors as the game tries to read the path from the mdl itself which this
    // loop below doesn't update.
    const bool hasMaterialOverrides = JSON_GetIterator(mapEntry, "$materials", JSONFieldType_e::kArray, materialsIt);
    const rapidjson::Value* materialOverrides = hasMaterialOverrides ? &materialsIt->value : nullptr;

    // handle material overrides register all material guids
    for (int i = 0; i < studiohdr->numtextures; ++i)
    {
        mstudiotexture_t* tex = studiohdr->pTexture(i);

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

        if (tex->guid != 0)
        {
            const size_t pos = (char*)tex - pDataBuf;
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(pos + offsetof(mstudiotexture_t, guid)));

            PakAsset_t* const asset = pak->GetAssetByGuid(tex->guid);

            if (asset)
            {
                // make sure referenced asset is a material for sanity
                asset->EnsureType(TYPE_MATL);

                // model assets don't exist on r2 so we can be sure that this is a v8 pak (and therefore has v15 materials)
                MaterialAssetHeader_v15_t* matlHdr = reinterpret_cast<MaterialAssetHeader_v15_t*>(asset->header);

                if (matlHdr->materialType != studiohdr->materialType(i))
                {
                    Error("Unexpected shader type for material in slot #%i, expected '%s', found '%s'.\n",
                        i, s_materialShaderTypeNames[studiohdr->materialType(i)], s_materialShaderTypeNames[matlHdr->materialType]);
                }

                pak->SetCurrentAssetAsDependentForAsset(asset);
            }
        }
    }

    PakAsset_t asset;

    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), de.offset, UINT64_MAX, AssetType::RMDL);
    asset.SetHeaderPointer(hdrChunk.Data());
  
    asset.version = RMDL_VERSION;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    pak->PushAsset(asset);
}