#include "pch.h"
#include "Assets.h"

void Assets::AddModelAsset_stub(RPakFileBase* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Error("RPak version 7 (Titanfall 2) cannot contain models");
}

void Assets::AddModelAsset_v9(RPakFileBase* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding mdl_ asset '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    ModelHeader* pHdr = new ModelHeader();

    std::string rmdlFilePath = g_sAssetsDir + sAssetName;

    // VG is a "fake" file extension that's used to store model streaming data (name came from the magic '0tVG')
    // this data is a combined mutated version of the data from .vtx and .vvd in regular source models
    std::string vgFilePath = Utils::ChangeExtension(rmdlFilePath, "vg");

    // fairly modified version of source .phy file data
    std::string phyFilePath = Utils::ChangeExtension(rmdlFilePath, "phy"); // optional (not used by all models)

    // add required files
    REQUIRE_FILE(rmdlFilePath);
    REQUIRE_FILE(vgFilePath);

    // begin rmdl input
    BinaryIO skelInput;
    skelInput.open(rmdlFilePath, BinaryIOMode::Read);

    studiohdr_t mdlhdr = skelInput.read<studiohdr_t>();

    if (mdlhdr.id != 0x54534449) // "IDST"
    {
        Warning("invalid file magic for model asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x54534449, mdlhdr.id);
        return;
    }

    if (mdlhdr.version != 54)
    {
        Warning("invalid version for model asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 54, mdlhdr.version);
        return;
    }

    uint32_t fileNameDataSize = sAssetName.length() + 1;

    char* pDataBuf = new char[fileNameDataSize + mdlhdr.length];

    // write the model file path into the data buffer
    snprintf(pDataBuf, fileNameDataSize, "%s", sAssetName.c_str());

    // go back to the beginning of the file to read all the data
    skelInput.seek(0);

    // write the skeleton data into the data buffer
    skelInput.getReader()->read(pDataBuf + fileNameDataSize, mdlhdr.length);
    skelInput.close();

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
    char* phyBuf = nullptr;
    size_t phyFileSize = 0;

    if (mapEntry.HasMember("usePhysics") && mapEntry["usePhysics"].GetBool())
    {
        BinaryIO phyInput;
        phyInput.open(phyFilePath, BinaryIOMode::Read);

        phyInput.seek(0, std::ios::end);

        phyFileSize = phyInput.tell();

        phyBuf = new char[phyFileSize];

        phyInput.seek(0);
        phyInput.getReader()->read(phyBuf, phyFileSize);
        phyInput.close();
    }

    //
    // Anim Rigs
    //
    char* pAnimRigBuf = nullptr;

    if (mapEntry.HasMember("animrigs") && mapEntry["animrigs"].IsArray())
    {
        pHdr->animRigCount = mapEntry["animrigs"].Size();

        pAnimRigBuf = new char[mapEntry["animrigs"].Size() * sizeof(uint64_t)];

        rmem arigBuf(pAnimRigBuf);

        for (auto& it : mapEntry["animrigs"].GetArray())
        {
            if (it.IsString())
            {
                std::string rig = it.GetStdString();

                arigBuf.write<uint64_t>(RTech::StringToGuid(rig.c_str()));
            }
            else {
                Error("invalid animrig entry for model '%s'\n", assetPath);
            }
        }
    }

    //
    // Starpak
    //
    std::string starpakPath = pak->primaryStarpakPath;

    if (mapEntry.HasMember("starpakPath") && mapEntry["starpakPath"].IsString())
    {
        starpakPath = mapEntry["starpakPath"].GetStdString();

        pak->AddStarpakReference(starpakPath);
    }

    if (starpakPath.length() == 0)
        Error("attempted to add asset '%s' as a streaming asset, but no starpak files were available.\n-- to fix: add 'starpakPath' as an rpak-wide variable\n-- or: add 'starpakPath' as an asset specific variable\n", assetPath);


    SRPkDataEntry de{ 0, vgFileSize, (uint8_t*)pVGBuf};
    de = pak->AddStarpakDataEntry(de);

    pHdr->alignedStreamingSize = de.m_nDataSize;

    // Segments
    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(ModelHeader), SF_HEAD, 16);

    // data segment
    _vseginfo_t dataseginfo = pak->CreateNewSegment(mdlhdr.length + fileNameDataSize, SF_CPU, 64);

    _vseginfo_t physeginfo;
    if (phyBuf)
        physeginfo = pak->CreateNewSegment(phyFileSize, SF_CPU, 64);

    _vseginfo_t arigseginfo;
    if (pAnimRigBuf)
        arigseginfo = pak->CreateNewSegment(pHdr->animRigCount * 8, SF_CPU, 64);

    pHdr->pName = { dataseginfo.index, 0 };

    pHdr->pRMDL = { dataseginfo.index, fileNameDataSize };

    pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pRMDL));
    pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pName));

    if (phyBuf)
    {
        pHdr->pPhyData = { physeginfo.index, 0 };
        pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pPhyData));
    }

    std::vector<RPakGuidDescriptor> guids{};

    // anim rigs are "includemodels" from source, containing a very stripped version of the main .rmdl data without any meshes
    // they are used for sharing anim data between models
    // file extension is ".rrig", rpak asset id is "arig"
    if (pAnimRigBuf)
    {
        pHdr->pAnimRigs = { arigseginfo.index, 0 };
        pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pAnimRigs));

        for (int i = 0; i < pHdr->animRigCount; ++i)
        {
            pak->AddGuidDescriptor(&guids, arigseginfo.index, sizeof(uint64_t) * i);
        }
    }

    rmem dataBuf(pDataBuf);
    dataBuf.seek(fileNameDataSize + mdlhdr.textureindex, rseekdir::beg);

    bool hasMaterialOverrides = mapEntry.HasMember("materials");

    // handle material overrides register all material guids
    for (int i = 0; i < mdlhdr.numtextures; ++i)
    {
        dataBuf.seek(fileNameDataSize + mdlhdr.textureindex + (i * sizeof(materialref_t)), rseekdir::beg);

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

        if(material->guid != 0)
            pak->AddGuidDescriptor(&guids, dataseginfo.index, dataBuf.getPosition()+ offsetof(materialref_t, guid));
    }

    RPakRawDataBlock shdb{ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr };
    pak->AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf };
    pak->AddRawDataBlock(rdb);

    uint32_t lastPageIdx = dataseginfo.index;

    if (phyBuf)
    {
        RPakRawDataBlock phydb{ physeginfo.index, physeginfo.size, (uint8_t*)phyBuf };
        pak->AddRawDataBlock(phydb);
        lastPageIdx = physeginfo.index;
    }

    if (pAnimRigBuf)
    {
        RPakRawDataBlock arigdb{ arigseginfo.index, arigseginfo.size, (uint8_t*)pAnimRigBuf };
        pak->AddRawDataBlock(arigdb);
        lastPageIdx = arigseginfo.index;
    }

    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), subhdrinfo.index, 0, subhdrinfo.size, -1, 0, de.m_nOffset, -1, (std::uint32_t)AssetType::RMDL);
    asset.version = RMDL_VERSION;
    // i have literally no idea what these are
    asset.pageEnd = lastPageIdx + 1;
    asset.unk1 = 2;

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);
}