#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"

void Assets::AddModelAsset_stub(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Error("RPak version 7 (Titanfall 2) cannot contain models");
}

void Assets::AddModelAsset_v9(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding mdl_ asset '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ModelHeader), SF_HEAD, 16);
    ModelHeader* pHdr = reinterpret_cast<ModelHeader*>(hdrChunk.Data());

    std::string rmdlFilePath = pak->GetAssetPath() + sAssetName;

    // VG is a "fake" file extension that's used to store model streaming data (name came from the magic '0tVG')
    // this data is a combined mutated version of the data from .vtx and .vvd in regular source models
    std::string vgFilePath = Utils::ChangeExtension(rmdlFilePath, "vg");

    // add required files
    REQUIRE_FILE(rmdlFilePath);
    REQUIRE_FILE(vgFilePath);

    // begin rmdl input
    BinaryIO rmdlInput;
    rmdlInput.open(rmdlFilePath, BinaryIOMode::Read);

    studiohdr_t mdlhdr = rmdlInput.read<studiohdr_t>();

    if (mdlhdr.id != 0x54534449) // "IDST"
        Error("invalid file magic for model asset '%s'. expected %x, found %x\n", sAssetName.c_str(), 0x54534449, mdlhdr.id);

    if (mdlhdr.version != 54)
        Error("invalid version for model asset '%s'. expected %i, found %i\n", sAssetName.c_str(), 54, mdlhdr.version);

    ///--------------------
    // Add VG data
    BinaryIO vgInput;
    vgInput.open(vgFilePath, BinaryIOMode::Read);

    BasicRMDLVGHeader bvgh = vgInput.read<BasicRMDLVGHeader>();

    if (bvgh.magic != 0x47567430)
        Error("invalid vg file magic for model asset '%s'. expected %x, found %x\n", sAssetName.c_str(), 0x47567430, bvgh.magic);

    if (bvgh.version != 1)
        Error("invalid vg version for model asset '%s'. expected %i, found %i\n", sAssetName.c_str(), 1, bvgh.version);

    vgInput.seek(0, std::ios::end);

    uint32_t vgFileSize = vgInput.tell();
    char* pVGBuf = new char[vgFileSize];

    vgInput.seek(0);
    vgInput.getReader()->read(pVGBuf, vgFileSize);
    vgInput.close();

    //
    // Physics
    //
    size_t phyFileSize = 0;

    CPakDataChunk phyChunk;
    if (mapEntry.HasMember("usePhysics") && mapEntry["usePhysics"].GetBool())
    {
        BinaryIO phyInput;
        phyInput.open(Utils::ChangeExtension(rmdlFilePath, "phy"), BinaryIOMode::Read);

        phyInput.seek(0, std::ios::end);

        phyFileSize = phyInput.tell();

        phyChunk = pak->CreateDataChunk(phyFileSize, SF_CPU, 64);

        phyInput.seek(0);
        phyInput.getReader()->read(phyChunk.Data(), phyFileSize);
        phyInput.close();
    }

    //
    // Anim Rigs
    //
    CPakDataChunk animRigsChunk;

    if (mapEntry.HasMember("animrigs"))
    {
        rapidjson::Value& animrigs = mapEntry["animrigs"];

        if (!animrigs.IsArray())
            Error("found field 'animrigs' on model asset '%s' with invalid type. expected 'array'\n", assetPath);

        pHdr->animRigCount = animrigs.Size();

        animRigsChunk = pak->CreateDataChunk(animrigs.Size() * sizeof(uint64_t), SF_CPU, 64);

        rmem arigBuf(animRigsChunk.Data());

        int i = 0;
        for (auto& it : animrigs.GetArray())
        {
            if (!it.IsString())
                Error("invalid animrig entry for model '%s'\n", assetPath);

            if (it.GetStringLength() == 0)
                Error("anim rig #%i for model '%s' was defined as an invalid empty string\n", i, assetPath);

            uint64_t guid = RTech::StringToGuid(it.GetStdString().c_str());

            arigBuf.write<uint64_t>(guid);

            // check if anim rig is a local asset so that the relation can be added
            PakAsset_t* asset = pak->GetAssetByGuid(guid);

            if (asset)
                asset->AddRelation(assetEntries->size());

            i++;
        }
    }

    //
    // Starpak
    //
    std::string starpakPath = pak->GetPrimaryStarpakPath();

    if (starpakPath.length() == 0)
        Error("attempted to add asset '%s' as a streaming asset, but no starpak files were available.\n-- to fix: add 'starpakPath' as an rpak-wide variable\n", assetPath);

    pak->AddStarpakReference(starpakPath);

    StreamableDataEntry de{ 0, vgFileSize, (uint8_t*)pVGBuf };
    de = pak->AddStarpakDataEntry(de);

    pHdr->alignedStreamingSize = de.m_nDataSize;

    size_t extraDataSize = 0;

    if (mdlhdr.flags & 0x10) // STATIC_PROP
    {
        extraDataSize = vgFileSize;
    }

    int fileNameDataSize = sAssetName.length() + 1;

    CPakDataChunk dataChunk = pak->CreateDataChunk(mdlhdr.length + fileNameDataSize + extraDataSize, SF_CPU, 64);
    char* pDataBuf = dataChunk.Data();

    // write the model file path into the data buffer
    snprintf(pDataBuf + mdlhdr.length, fileNameDataSize, "%s", sAssetName.c_str());

    // copy rmdl data into data buffer
    {
        // go back to the beginning of the file to read all the data
        rmdlInput.seek(0);

        // write the skeleton data into the data buffer
        rmdlInput.getReader()->read(pDataBuf, mdlhdr.length);
        rmdlInput.close();
    }

    // copy static prop data into data buffer (if needed)
    if (mdlhdr.flags & 0x10) // STATIC_PROP
    {
        memcpy_s(pDataBuf + fileNameDataSize + mdlhdr.length, vgFileSize, de.m_nDataPtr, vgFileSize);
    }

    pHdr->pName = dataChunk.GetPointer(mdlhdr.length);

    pHdr->pRMDL = dataChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelHeader, pRMDL)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelHeader, pName)));

    if (mdlhdr.flags & 0x10) // STATIC_PROP
    {
        pHdr->pStaticPropVtxCache = dataChunk.GetPointer(fileNameDataSize + mdlhdr.length);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelHeader, pStaticPropVtxCache)));
    }

    std::vector<PakGuidRefHdr_t> guids{};

    if (phyFileSize > 0)
    {
        pHdr->pPhyData = phyChunk.GetPointer();
        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelHeader, pPhyData)));
    }

    if (mapEntry.HasMember("animrigs"))
    {
        pHdr->pAnimRigs = animRigsChunk.GetPointer();
        pak->AddPointer(hdrChunk.GetPointer(offsetof(ModelHeader, pAnimRigs)));

        for (uint32_t i = 0; i < pHdr->animRigCount; ++i)
        {
            pak->AddGuidDescriptor(&guids, animRigsChunk.GetPointer(sizeof(uint64_t) * i));
        }
    }

    rmem dataBuf(pDataBuf);
    dataBuf.seek(mdlhdr.textureindex, rseekdir::beg);

    bool hasMaterialOverrides = mapEntry.HasMember("materials");

    // handle material overrides register all material guids
    for (int i = 0; i < mdlhdr.numtextures; ++i)
    {
        dataBuf.seek(mdlhdr.textureindex + (i * sizeof(materialref_t)), rseekdir::beg);

        materialref_t* material = dataBuf.get<materialref_t>();

        // if material overrides are possible and this material has an entry in the array
        if (hasMaterialOverrides && mapEntry["materials"].GetArray().Size() > i)
        {
            auto& matlEntry = mapEntry["materials"].GetArray()[i];

            // if string, calculate the guid
            if (matlEntry.IsString())
            {
                if (matlEntry.GetStringLength() != 0) // if no material path, use the original model material
                    material->guid = RTech::StringToGuid(std::string("material/" + matlEntry.GetStdString() + ".rpak").c_str()); // use user provided path
            }
            // if uint64, treat the value as the guid
            else if (matlEntry.IsUint64())
                material->guid = matlEntry.GetUint64();
        }

        if (material->guid != 0)
        {
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(dataBuf.getPosition() + offsetof(materialref_t, guid)));

            PakAsset_t* asset = pak->GetAssetByGuid(material->guid);

            if (asset)
                asset->AddRelation(assetEntries->size());
        }
    }

    PakAsset_t asset;

    asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), de.m_nOffset, -1, (std::uint32_t)AssetType::RMDL);
    asset.version = RMDL_VERSION;
    // i have literally no idea what these are
    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);
}