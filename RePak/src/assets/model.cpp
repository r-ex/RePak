#include "pch.h"
#include "Assets.h"
#include "assets/model.h"

void Assets::AddModelAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("\n==============================\n");
    Error("RPak version 7 (Titanfall 2) cannot contain models");
}

void Assets::AddModelAsset_v9(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("\n==============================\n");
    Log("Asset mdl_ -> '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    if (pak->GetAssetByGuid(RTech::StringToGuid(sAssetName.c_str())) != nullptr)
    {
        Warning("Asset mdl_ -> '%s' already exists skipping\n", assetPath);
        return;
    }

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
    BinaryIO rmdlInput;
    rmdlInput.open(rmdlFilePath, BinaryIOMode::Read);

    studiohdr_t mdlhdr = rmdlInput.read<studiohdr_t>();

    if (mdlhdr.id != 0x54534449) // "IDST"
        Error("invalid file magic for model asset '%s'. expected %x, found %x\n", sAssetName.c_str(), 0x54534449, mdlhdr.id);

    if (mdlhdr.version != 54)
        Error("invalid version for model asset '%s'. expected %i, found %i\n", sAssetName.c_str(), 54, mdlhdr.version);

    uint32_t fileNameDataSize = sAssetName.length() + 1;

    char* pDataBuf = new char[fileNameDataSize + mdlhdr.length];

    // write the model file path into the data buffer
    snprintf(pDataBuf, fileNameDataSize, "%s", sAssetName.c_str());

    // go back to the beginning of the file to read all the data
    rmdlInput.seek(0);

    // write the skeleton data into the data buffer
    rmdlInput.getReader()->read(pDataBuf + fileNameDataSize, mdlhdr.length);
    rmdlInput.close();

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
    
    if (mapEntry.HasMember("solid") && mapEntry["solid"].IsBool() && !mapEntry["solid"].GetBool())
    {
        mdlhdr.contents = 0; // CONTENTS_EMPTY

        // remove static model flag
        if (mdlhdr.HasFlag(STUDIOHDR_FLAGS_STATIC_PROP))
            mdlhdr.flags = mdlhdr.RemoveFlag(STUDIOHDR_FLAGS_STATIC_PROP);
    }

    char* pStaticVGBuf = nullptr;
    if (mapEntry.HasMember("static") && mapEntry["static"].IsBool() && mapEntry["static"].GetBool())
    {
        pStaticVGBuf = new char[vgFileSize];
        memcpy_s(pStaticVGBuf, vgFileSize, pVGBuf, vgFileSize);
    }
    vgInput.close();


    char* phyBuf = nullptr;
    size_t phyFileSize = 0;

    if (mapEntry.HasMember("usePhysics") && mapEntry["usePhysics"].IsBool() && mapEntry["usePhysics"].GetBool())
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

    char* pAnimRigBuf = nullptr;

    if (mapEntry.HasMember("animrigs"))
    {
        if (!mapEntry["animrigs"].IsArray())
            Error("found field 'animrigs' on model asset '%s' with invalid type. expected 'array'\n", assetPath);

        pHdr->animRigCount = mapEntry["animrigs"].Size();

        pAnimRigBuf = new char[mapEntry["animrigs"].Size() * sizeof(uint64_t)];

        rmem arigBuf(pAnimRigBuf);

        int i = 0;
        for (auto& it : mapEntry["animrigs"].GetArray())
        {
            if (!it.IsString())
                Error("invalid animrig entry for model '%s'\n", assetPath);

            if (it.GetStringLength() == 0)
                Error("anim rig #%i for model '%s' was defined as an invalid empty string\n", i, assetPath);

            uint64_t guid = RTech::StringToGuid(it.GetString());

            arigBuf.write<uint64_t>(guid);

            // check if anim rig is a local asset so that the relation can be added
            auto Asset = pak->GetAssetByGuid(guid);
            if (Asset)
                Asset->AddRelation(assetEntries->size());
            i++;
        }
    }

    char* pAnimSeqBuf = nullptr;

    if (mapEntry.HasMember("animseqs"))
    {
        if (!mapEntry["animseqs"].IsArray())
            Error("found field 'animrigs' on model asset '%s' with invalid type. expected 'array'\n", assetPath);

        pHdr->animSeqCount = mapEntry["animseqs"].Size();

        pAnimSeqBuf = new char[mapEntry["animseqs"].Size() * sizeof(uint64_t)];

        rmem aseqBuf(pAnimSeqBuf);

        int i = 0;
        for (auto& it : mapEntry["animseqs"].GetArray())
        {
            if (!it.IsString())
                Error("invalid animseq entry for model '%s'\n", assetPath);

            if (it.GetStringLength() == 0)
                Error("anim seq #%i for model '%s' was defined as an invalid empty string\n", i, assetPath);

            uint64_t guid = RTech::StringToGuid(it.GetString());

            aseqBuf.write<uint64_t>(guid);

            // check if anim seq is a local asset so that the relation can be added
            auto Asset = pak->GetAssetByGuid(guid);
            if (Asset)
                Asset->AddRelation(assetEntries->size());
            i++;
        }
    }

    std::string starpakPath = pak->primaryStarpakPath;

    if (mapEntry.HasMember("starpakPath") && mapEntry["starpakPath"].IsString())
        starpakPath = mapEntry["starpakPath"].GetStdString();

    if (starpakPath.length() == 0)
        Error("attempted to add asset '%s' as a streaming asset, but no starpak files were available.\n-- to fix: add 'starpakPath' as an rpak-wide variable\n-- or: add 'starpakPath' as an asset specific variable\n", assetPath);
    else
        pak->AddStarpakReference(starpakPath);

 
    auto de = pak->AddStarpakDataEntry({ 0, vgFileSize, (uint8_t*)pVGBuf });
    pHdr->alignedStreamingSize = de.m_nDataSize;
    

    // Segments
    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(ModelHeader), SF_HEAD, 16);

    // data segment
    size_t DataSize = mdlhdr.length + fileNameDataSize;

    _vseginfo_t dataseginfo = pak->CreateNewSegment(DataSize, SF_CPU, 64);

    _vseginfo_t staticvgseginfo;
    if(pStaticVGBuf)
       staticvgseginfo = pak->CreateNewSegment(vgFileSize, SF_CPU, 64);

    // .phy
    _vseginfo_t physeginfo;
    if (phyBuf)
        physeginfo = pak->CreateNewSegment(phyFileSize, SF_CPU, 64);

    // animation rigs
    _vseginfo_t arigseginfo;
    if (pAnimRigBuf)
        arigseginfo = pak->CreateNewSegment(pHdr->animRigCount * 8, SF_CPU, 64);

    // animation seqs
    _vseginfo_t aseqseginfo;
    if (pAnimSeqBuf)
        aseqseginfo = pak->CreateNewSegment(pHdr->animSeqCount * 8, SF_CPU, 64);

    pHdr->pName = { dataseginfo.index, 0 };
    pHdr->pRMDL = { dataseginfo.index, fileNameDataSize };

    pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pRMDL));
    pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pName));

    std::vector<RPakGuidDescriptor> guids{};

    if (phyBuf)
    {
        pHdr->pPhyData = { physeginfo.index, 0 };
        pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pPhyData));

    }

    if (pAnimRigBuf)
    {
        pHdr->pAnimRigs = { arigseginfo.index, 0 };
        pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pAnimRigs));

        for (int i = 0; i < pHdr->animRigCount; ++i)
            pak->AddGuidDescriptor(&guids, arigseginfo.index, sizeof(uint64_t) * i);
    }

    if (pAnimSeqBuf)
    {
        pHdr->pAnimSeqs = { aseqseginfo.index, 0 };
        pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pAnimSeqs));

        for (int i = 0; i < pHdr->animSeqCount; ++i)
            pak->AddGuidDescriptor(&guids, aseqseginfo.index, sizeof(uint64_t)* i);
            
    }

    if (pStaticVGBuf)
    {
        pHdr->pStaticPropVtxCache = { staticvgseginfo.index , 0 };
        pak->AddPointer(subhdrinfo.index, offsetof(ModelHeader, pStaticPropVtxCache));
    }

    rmem dataBuf(pDataBuf);

    // handle material overrides register all material guids
    Log("Materials -> %d\n", mdlhdr.numtextures);
    for (int i = 0; i < mdlhdr.numtextures; ++i)
    {
        dataBuf.seek(fileNameDataSize + mdlhdr.textureindex + (i * sizeof(materialref_t)), rseekdir::beg);

        materialref_t* material = dataBuf.get<materialref_t>();

        // if material overrides are possible and this material has an entry in the array
        if (material->guid != 0 && mapEntry.HasMember("materials") && mapEntry["materials"].GetArray().Size() > i)
        {
            auto& matlEntry = mapEntry["materials"].GetArray()[i];
            
            // if string, calculate the guid
            if (matlEntry.IsString() && matlEntry.GetStringLength() != 0)
            {
               // if no material path, use the original model material
               if (matlEntry.GetStdString() == "none") // use wingman elite mat as placeholder
                   material->guid = RTech::StringToGuid("material/models/characters/ShadowSquad/ShadowSquad_3p_sknp.rpak");
               else
                    material->guid = RTech::StringToGuid(("material/" + matlEntry.GetStdString() + ".rpak").c_str()); // use user provided path
            }
            // if uint64, treat the value as the guid
            else if (matlEntry.IsUint64() && matlEntry.GetUint64() != 0x0)
                material->guid = matlEntry.GetUint64();
        }

        if(material->guid != 0)
            pak->AddGuidDescriptor(&guids, dataseginfo.index, dataBuf.getPosition() + offsetof(materialref_t, guid));

        auto Asset = pak->GetAssetByGuid(material->guid);
        if (Asset)
            Asset->AddRelation(assetEntries->size());

        Log("Material Guid -> 0x%llX\n", material->guid);
    }

    // write modified header
    dataBuf.write<studiohdr_t>(mdlhdr, fileNameDataSize);

	pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr });
	pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf });
   
	uint32_t lastPageIdx = dataseginfo.index;

    if (pStaticVGBuf)
    {
        pak->AddRawDataBlock({ staticvgseginfo.index, staticvgseginfo.size, (uint8_t*)pStaticVGBuf });
        lastPageIdx = staticvgseginfo.index;
    }

	if (phyBuf)
	{
		pak->AddRawDataBlock({ physeginfo.index, physeginfo.size, (uint8_t*)phyBuf });
		lastPageIdx = physeginfo.index;
	}

	if (pAnimRigBuf)
	{
		pak->AddRawDataBlock({ arigseginfo.index, arigseginfo.size, (uint8_t*)pAnimRigBuf });
		lastPageIdx = arigseginfo.index;
	}

	if (pAnimSeqBuf)
	{
		pak->AddRawDataBlock({ aseqseginfo.index, aseqseginfo.size, (uint8_t*)pAnimSeqBuf });
		lastPageIdx = aseqseginfo.index;
	}

    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), subhdrinfo.index, 0, subhdrinfo.size, -1, 0, de.m_nOffset, -1, (std::uint32_t)AssetType::RMDL);
    asset.version = RMDL_VERSION;

    // i have literally no idea what these are
    asset.pageEnd = lastPageIdx + 1;
    asset.unk1 = guids.size() + 1;

    asset.AddGuids(&guids);
    assetEntries->push_back(asset);
}