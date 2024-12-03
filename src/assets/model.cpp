#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"

char* Model_ReadRMDLFile(const std::string& path)
{
    BinaryIO modelFile;

    if (!modelFile.Open(path, BinaryIO::Mode_e::Read))
        Error("Failed to open model file \"%s\"\n", path.c_str());

    const size_t fileSize = modelFile.GetSize();

    if (fileSize < sizeof(studiohdr_t))
        Error("Invalid model file \"%s\"; must be at least %zu bytes, found %zu\n", path.c_str(), sizeof(studiohdr_t), fileSize);

    char* const buf = new char[fileSize];
    modelFile.Read(buf, fileSize);

    studiohdr_t* const pHdr = reinterpret_cast<studiohdr_t*>(buf);

    if (pHdr->id != 'TSDI') // "IDST"
        Error("Invalid model file \"%s\"; expected magic %x, found %x\n", path.c_str(), 'TSDI', pHdr->id);

    if (pHdr->version != 54)
        Error("Invalid model file \"%s\"; expected version %i, found %i\n", path.c_str(), 54, pHdr->version);

    if (pHdr->length > fileSize)
        Error("Invalid model file \"%s\"; studiohdr->length(%i) > fileSize(%i)\n", path.c_str(), pHdr->length, fileSize);

    return buf;
}

char* Model_ReadVGFile(const std::string& path, size_t* const pFileSize)
{
    BinaryIO vgFile;

    if (!vgFile.Open(path, BinaryIO::Mode_e::Read))
        Error("Failed to open vertex group file \"%s\"\n", path.c_str());

    const size_t fileSize = vgFile.GetSize();

    if (fileSize < sizeof(VertexGroupHeader_t))
        Error("Invalid vertex group file \"%s\"; must be at least %zu bytes, found %zu\n", path.c_str(), sizeof(VertexGroupHeader_t), fileSize);

    char* const buf = new char[fileSize];
    vgFile.Read(buf, fileSize);

    VertexGroupHeader_t* const pHdr = reinterpret_cast<VertexGroupHeader_t*>(buf);

    if (pHdr->id != 'GVt0') // "0tVG"
        Error("Invalid vertex group file \"%s\"; expected magic %x, found %x\n", path.c_str(), 'GVt0', pHdr->id);

    // not sure if this is actually version but i've also never seen it != 1
    if (pHdr->version != 1)
        Error("Invalid vertex group file \"%s\"; expected version %i, found %i\n", path.c_str(), 1, pHdr->version);

    *pFileSize = fileSize;
    return buf;
}

static void Model_CheckAssetRef(const rapidjson::Value& val, const char* assetType, const int index)
{
    if (!val.IsNumber())
    {
        if (!val.IsString())
            Error("%s #%i is of unsupported type; expected %s or %s, found %s\n", assetType, index,
                JSON_TypeToString(JSONFieldType_e::kUint64), JSON_TypeToString(JSONFieldType_e::kString),
                JSON_TypeToString(JSON_ExtractType(val)));

        if (val.GetStringLength() == 0)
            Error("%s #%i was defined as an invalid empty string\n", assetType, index);
    }
}

bool Model_AddSequenceRefs(CPakDataChunk* chunk, CPakFile* pak, ModelAssetHeader_t* hdr, const rapidjson::Value& mapEntry)
{
    rapidjson::Value::ConstMemberIterator sequencesIt;
    const bool hasSequences = JSON_GetIterator(mapEntry, "$sequences", JSONFieldType_e::kArray, sequencesIt);

    if (!hasSequences)
        return false;

    const rapidjson::Value::ConstArray sequencesArray = sequencesIt->value.GetArray();
    std::vector<PakGuid_t> sequenceGuids;

    int seqIndex = -1;
    for (const auto& sequence : sequencesArray)
    {
        seqIndex++;
        Model_CheckAssetRef(sequence, "sequence", seqIndex);

        PakGuid_t guid;

        if (!JSON_ParseNumber(sequence, guid))
        {
            const char* const sequencePath = sequence.GetString();
            Log("Auto-adding aseq asset \"%s\".\n", sequencePath);

            guid = RTech::StringToGuid(sequencePath);
            Assets::AddAnimSeqAsset(pak, guid, sequencePath);
        }

        sequenceGuids.emplace_back(guid);
        hdr->sequenceCount++;
    }

    CPakDataChunk guidsChunk = pak->CreateDataChunk(sizeof(PakGuid_t) * sequenceGuids.size(), SF_CPU, 64);

    PakGuid_t* pGuids = reinterpret_cast<PakGuid_t*>(guidsChunk.Data());
    for (size_t i = 0; i < sequenceGuids.size(); ++i)
    {
        pGuids[i] = sequenceGuids[i];
    }

    *chunk = guidsChunk;
    return true;
}

void Assets::AddModelAsset_v9(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ModelAssetHeader_t), SF_HEAD, 16);
    ModelAssetHeader_t* pHdr = reinterpret_cast<ModelAssetHeader_t*>(hdrChunk.Data());

    std::vector<PakGuidRefHdr_t> guids;

    std::string rmdlFilePath = pak->GetAssetPath() + assetPath;
    char* rmdlBuf = Model_ReadRMDLFile(rmdlFilePath);

    studiohdr_t* studiohdr = reinterpret_cast<studiohdr_t*>(rmdlBuf);

    ///--------------------
    // Add VG data
    // VG is a "fake" file extension that's used to store model streaming data (name came from the magic '0tVG')
    // this data is a combined mutated version of the data from .vtx and .vvd in regular source models
    std::string vgFilePath = Utils::ChangeExtension(rmdlFilePath, "vg");

    size_t vgFileSize = 0;
    char* const vgBuf = Model_ReadVGFile(vgFilePath, &vgFileSize);

    //
    // Physics
    //
    size_t phyFileSize = 0;

    CPakDataChunk phyChunk;
    
    if (studiohdr->vphyindex)
    {
        BinaryIO phyInput;
        const std::string physicsFile = Utils::ChangeExtension(rmdlFilePath, "phy");

        if (!phyInput.Open(physicsFile, BinaryIO::Mode_e::Read))
            Error("Failed to open physics asset '%s'\n", physicsFile.c_str());

        phyFileSize = phyInput.GetSize();
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

        if (numAnimrigs != pHdr->animRigCount)
            Warning("model expects %u animrigs, but only %u were provided; model may not work correctly\n", pHdr->animRigCount, numAnimrigs);

        pHdr->animRigCount = numAnimrigs;

        animRigsChunk = pak->CreateDataChunk(numAnimrigs * sizeof(PakGuid_t), SF_CPU, 64);

        rmem arigBuf(animRigsChunk.Data());

        int i = -1;
        for (const auto& animrig : animrigs)
        {
            i++;
            Model_CheckAssetRef(animrig, "animrig", i);

            PakGuid_t guid;

            if (!JSON_ParseNumber(animrig, guid))
                guid = RTech::StringToGuid(animrig.GetString());

            arigBuf.write<PakGuid_t>(guid);

            // check if anim rig is a local asset so that the relation can be added
            PakAsset_t* const animRigAsset = pak->GetAssetByGuid(guid);

            pak->SetCurrentAssetAsDependentForAsset(animRigAsset);
        }

        pHdr->pAnimRigs = animRigsChunk.GetPointer();
        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelAssetHeader_t, pAnimRigs)));

        for (uint32_t j = 0; j < pHdr->animRigCount; ++j)
        {
            guids.emplace_back(animRigsChunk.GetPointer(sizeof(uint64_t) * j));
        }
    }

    CPakDataChunk sequencesChunk;
    if (Model_AddSequenceRefs(&sequencesChunk, pak, pHdr, mapEntry))
    {
        pHdr->pSequences = sequencesChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelAssetHeader_t, pSequences)));

        for (uint32_t i = 0; i < pHdr->sequenceCount; ++i)
        {
            guids.emplace_back(sequencesChunk.GetPointer(8 * i));
        }
    }

    //
    // Starpak
    //
    StreamableDataEntry de{ 0, vgFileSize, (uint8_t*)vgBuf };
    pak->AddStarpakDataEntry(de);

    assert(de.dataSize <= UINT32_MAX);
    pHdr->alignedStreamingSize = static_cast<uint32_t>(de.dataSize);

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
        memcpy_s(pDataBuf + fileNameDataSize + studiohdr->length, vgFileSize, de.pData, vgFileSize);
    }

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
                const rapidjson::Value& matlEntry = materialArray[i];
                Model_CheckAssetRef(matlEntry, "material", i);

                PakGuid_t guid;

                if (!JSON_ParseNumber(matlEntry, guid))
                    guid = RTech::StringToGuid(matlEntry.GetString());

                tex->guid = guid;
            }
        }

        if (tex->guid != 0)
        {
            size_t pos = (char*)tex - pDataBuf;
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(static_cast<int>(pos) + offsetof(mstudiotexture_t, guid)));

            PakAsset_t* asset = pak->GetAssetByGuid(tex->guid);

            if (asset)
            {
                // make sure referenced asset is a material for sanity
                asset->EnsureType(TYPE_MATL);

                // model assets don't exist on r2 so we can be sure that this is a v8 pak (and therefore has v15 materials)
                MaterialAssetHeader_v15_t* matlHdr = reinterpret_cast<MaterialAssetHeader_v15_t*>(asset->header);

                if (matlHdr->materialType != studiohdr->materialType(i))
                {
                    Warning("Setting material of unexpected type in material slot %i for model asset '%s'. Expected type '%s', found material with type '%s'.\n",
                        i, assetPath, s_materialShaderTypeNames[studiohdr->materialType(i)], s_materialShaderTypeNames[matlHdr->materialType]);
                }

                pak->SetCurrentAssetAsDependentForAsset(asset);
            }
        }
    }

    PakAsset_t asset;

    asset.InitAsset(assetPath, Pak_GetGuidOverridable(mapEntry, assetPath), hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), de.offset, UINT64_MAX, AssetType::RMDL);
    asset.SetHeaderPointer(hdrChunk.Data());
  
    asset.version = RMDL_VERSION;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    pak->PushAsset(asset);
}