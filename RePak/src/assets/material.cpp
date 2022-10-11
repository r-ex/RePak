#include "pch.h"
#include "Assets.h"

// VERSION 7
void Assets::AddMaterialAsset_v12(RPakFileBase* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding matl asset '%s'\n", assetPath);

    int assetUsesCount = 1; // track how many other assets are used by this asset, set to 1 because a material should always have a shaderset, and it if doesn't you have other problems.
    int ExternalAssetUsesCount = 0; // number of assets not within this rpak

    MaterialHeaderV12* mtlHdr = new MaterialHeaderV12();
    std::string AssetPath = std::string(assetPath);

    // this should be replaced with getting the resolution from the color texture
    if (mapEntry.HasMember("width")) // Set material width.
        mtlHdr->width = mapEntry["width"].GetInt();

    if (mapEntry.HasMember("height")) // Set material width.
        mtlHdr->height = mapEntry["height"].GetInt();

    //===============
    // Type Handling
    //===============
    std::string type = "skn";
    std::string subtype = ""; // mostly phased out thankfully

    if (mapEntry.HasMember("type")) // Sets the type of the material, skn, fix, etc.
        type = mapEntry["type"].GetStdString();
    else
        Warning("Adding material without an explicitly defined type. Assuming 'skn'...\n");

    if (mapEntry.HasMember("subtype")) // Set subtype, mostly redundant now, only used for "nose_art".
        subtype = mapEntry["subtype"].GetStdString();

    // type handling checks
    if (type == "gen")
        mtlHdr->unknown3 = 0xFBA63181;
    else
        mtlHdr->unknown3 = 0x40D33E8F;

    if (type == "gen" || type == "wld" || subtype == "nose_art")
    {
        for (int i = 0; i < 2; ++i)
        {
            mtlHdr->unkSections[i].unkRenderLighting = 0xF0138286;
            mtlHdr->unkSections[i].unkRenderAliasing = 0xF0138286;
            mtlHdr->unkSections[i].unkRenderDoF = 0xF0008286;
            mtlHdr->unkSections[i].unkRenderUnknown = 0x00138286;

            mtlHdr->unkSections[i].unkFlags = 0x00000005;
        }
    }
    else if ((type == "fix" || type == "skn") && subtype != "nose_art")
    {
        for (int i = 0; i < 2; ++i)
        {
            mtlHdr->unkSections[i].unkRenderLighting = 0xF0138004;
            mtlHdr->unkSections[i].unkRenderAliasing = 0xF0138004;
            mtlHdr->unkSections[i].unkRenderDoF = 0xF0138004;
            mtlHdr->unkSections[i].unkRenderUnknown = 0x00138004;

            mtlHdr->unkSections[i].unkFlags = 0x00000004;
        }
    }
    else
    {
        // exit because of unsupported type
        Warning("Type '%s' is not valid in Titanfall 2!!", type.c_str());
        exit(EXIT_FAILURE);
        return;
    }


    // do material guid after types so we can get the type for hashing
    // get material path and guid
    std::string FullAssetPath = "material/" + AssetPath + "_" + type + ".rpak"; // Make full rpak asset path.

    mtlHdr->guid = RTech::StringToGuid(FullAssetPath.c_str()); // Convert full rpak asset path to textureGUID and set it in the material header.


    //===============
    // Set ShaderSet
    //===============
    uint64_t ShaderSetGUID = 0x0;

    if (mapEntry.HasMember("shaderset") && mapEntry["shaderset"].GetStdString() != "")
    {
        ShaderSetGUID = RTech::StringToGuid(("shaderset/" + mapEntry["shaderset"].GetStdString() + ".rpak").c_str());
    }
    else if (mapEntry.HasMember("shadersetGUID") && mapEntry["shadersetGUID"].GetStdString() != "")
    {
        ShaderSetGUID = strtoul((mapEntry["shadersetGUID"].GetStdString()).c_str(), NULL, 0);
    }
    else
    {
        ShaderSetGUID = RTech::StringToGuid("shaderset/uberAoCavEmitEntcolmeSamp2222222_skn.rpak");
        Warning("Adding material without an explicitly defined shaderset, Assuming 'uberAoCavEmitEntcolmeSamp2222222_skn'... \n");
    }

    mtlHdr->shadersetGuid = ShaderSetGUID;

    // check if shaderset is local.
    RPakAssetEntry* shdsAsset = pak->GetAssetByGuid(mtlHdr->shadersetGuid, nullptr);

    if (!shdsAsset)
        ExternalAssetUsesCount++;


    //===========
    // Set Flags
    //===========
    if (mapEntry.HasMember("flags") && mapEntry["flags"].GetStdString() != "") // Set flags properly. Responsible for texture stretching, tiling etc.
        mtlHdr->flags = strtoul(("0x" + mapEntry["flags"].GetStdString()).c_str(), NULL, 0);
    else
        mtlHdr->flags = 0x1D0300;

    if (mapEntry.HasMember("flags2") && mapEntry["flags2"].GetStdString() != "") // This does a lot of very important stuff.
        mtlHdr->flags2 = strtoul(("0x" + mapEntry["flags2"].GetStdString()).c_str(), NULL, 0);
    else
        mtlHdr->flags2 = 0x56000020;

    // not always 0x100000, however if the material you're trying to do doesn't have this seek help
    mtlHdr->flags2 += 0x10000000000000;

    // Visibility related flags, opacity and the like.
    std::string visibility = "opaque";

    if (mapEntry.HasMember("visibilityflags"))
    {
        visibility = mapEntry["visibilityflags"].GetString();
        short visFlag = 0x0017;

        if (visibility == "opaque") 
        {
            visFlag = 0x17;
        }
        else if (visibility == "transparent")
        {
            // this will not work properly unless some flags are set in Flags2
            visFlag = 0x7;
        }
        else if (visibility == "colpass")
        {
            visFlag = 0x5;
        }
        else if (visibility == "none")
        {
            // for loadscreens
            visFlag = 0x0;
        }
        else
        {
            //Log("No valid visibility specified, defaulting to opaque... \n");

            visFlag = 0x0017;
        }

        mtlHdr->unkSections[0].visibilityFlags = visFlag;
        mtlHdr->unkSections[1].visibilityFlags = visFlag;
    }


    // Sets how the faces draw.
    uint16_t faceFlag = 0x0006;

    if (mapEntry.HasMember("faceflags")) {

        faceFlag = strtoul(("0x" + mapEntry["faceflags"].GetStdString()).c_str(), NULL, 0);

    }

    mtlHdr->unkSections[0].faceFlags = faceFlag;
    mtlHdr->unkSections[1].faceFlags = faceFlag;


    //================
    // Do Data Buffer
    //================
    std::string surface = "default";
    std::string surface2 = "default";

    // titanfall surfaces are defined in scripts/surfaceproperties.txt
    if (mapEntry.HasMember("surface"))
        surface = mapEntry["surface"].GetStdString();

    // rarely used edge case but it's good to have, especially for doing world materials.
    if (mapEntry.HasMember("surface2"))
        surface2 = mapEntry["surface2"].GetStdString();

    // Get the size of the texture guid section.
    size_t textureRefSize = 0;

    if (mapEntry.HasMember("textures"))
    {
        textureRefSize = mapEntry["textures"].GetArray().Size() * 8;
    }
    else
    {
        Warning("Trying to add material with no textures. Skipping asset...\n");
        return;
    }

    int surfaceDataBuffLength = surface.length() + 1;

    // add surface2's length to buffer if needed.
    if (mapEntry.HasMember("surface2"))
        surfaceDataBuffLength += surface2.length() + 1;


    uint32_t assetPathSize = (AssetPath.length() + 1);
    uint32_t dataBufSize = (assetPathSize + (assetPathSize % 4)) + (textureRefSize * 2) + surfaceDataBuffLength;

    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(MaterialHeaderV12), SF_HEAD, 8);

    // asset data
    _vseginfo_t dataseginfo = pak->CreateNewSegment(dataBufSize, SF_CPU, 64);

    char* dataBuf = new char[dataBufSize] {};
    char* tmp = dataBuf;

    // ===============================
    // write the material path into the buffer
    snprintf(dataBuf, AssetPath.length() + 1, "%s", assetPath);
    uint8_t assetPathAlignment = (assetPathSize % 4);
    dataBuf += AssetPath.length() + 1 + assetPathAlignment;

    // ===============================
    // add the texture guids to the buffer
    size_t guidPageOffset = AssetPath.length() + 1 + assetPathAlignment;

    std::vector<RPakGuidDescriptor> guids{};
    
    // write texture slots into buffer
    int textureIdx = 0;
    int fileRelationIdx = -1;
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the first TextureGUID Map.
    {
        if (it.GetStdString() != "")
        {
            uint64_t textureGUID = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str()); // Convert texture path to guid.
            *(uint64_t*)dataBuf = textureGUID;
            pak->AddGuidDescriptor(&guids, dataseginfo.index, guidPageOffset + (textureIdx * sizeof(uint64_t))); // Register GUID descriptor for current texture index.

            RPakAssetEntry* txtrAsset = pak->GetAssetByGuid(textureGUID, nullptr);

            if (txtrAsset)
                txtrAsset->AddRelation(assetEntries->size());
            else
            {
                Warning("unable to find texture '%s' for material '%s' within the local assets\n", it.GetString(), assetPath);
                ExternalAssetUsesCount++;
            }

            assetUsesCount++;
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++; // Next texture index coming up.
    }

    textureIdx = 0; // reset index for streamed texture slots
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the streamed TextureGUID Map.
    {
        *(uint64_t*)dataBuf = 0;

        dataBuf += sizeof(uint64_t);
        textureIdx++;
    }

    // ===============================
    // write surface names into the buffer.
    snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());

    if (mapEntry.HasMember("surface2"))
        snprintf(dataBuf + (surface.length() + 1), surface2.length() + 1, "%s", surface2.c_str());

    // get the original pointer back so it can be used later for writing the buffer
    dataBuf = tmp;


    // ===============================
    // fill out the rest of the header
    mtlHdr->pName.m_nIndex = dataseginfo.index;
    mtlHdr->pName.m_nOffset = 0;

    mtlHdr->pSurfaceProp.m_nIndex = dataseginfo.index;
    mtlHdr->pSurfaceProp.m_nOffset = (AssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2);

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, pName));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, pSurfaceProp));

    if (mapEntry.HasMember("surface2")) 
    {
        mtlHdr->pSurfaceProp2.m_nIndex = dataseginfo.index;
        mtlHdr->pSurfaceProp2.m_nOffset = (AssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2) + (surface.length() + 1);

        pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, pSurfaceProp2));
    }

    //==========================
    // Material Reference Setup
    //==========================

    // make this sizeof guidrefs that way it could easily be used for apex
    for (int i = 0; i < 3; i++)
    {
        auto& MaterialReference = mapEntry["materialrefs"].GetArray()[i];

        if (MaterialReference.GetStdString() != "")
        {
            mtlHdr->guidRefs[i] = RTech::StringToGuid(("material/" + MaterialReference.GetStdString() + "_" + type + ".rpak").c_str());

            pak->AddGuidDescriptor(&guids, subhdrinfo.index, (offsetof(MaterialHeaderV12, guidRefs) + (i * 8)));

            assetUsesCount++;
        }        
    }

    // Is this a colpass asset?
    bool bColpass = false;
    if (mapEntry.HasMember("colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + "_" + type + ".rpak";
        mtlHdr->guidRefs[3] = RTech::StringToGuid(colpassPath.c_str());

        // todo, the relations count is not being set properly on the colpass for whatever reason.
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, guidRefs) + 24);
        assetUsesCount++;

        bColpass = false;
    }

    // add uses and relations for material refs
    for (int i = 0; i < 4; ++i)
    {
        uint64_t guid = mtlHdr->guidRefs[i];

        if (guid != 0)
        {
            RPakAssetEntry* asset = pak->GetAssetByGuid(guid, nullptr);

            if (asset)
                asset->AddRelation(assetEntries->size());
            else
                ExternalAssetUsesCount++;
        }
    }

    // add uses and relations for shaderset
    if (mtlHdr->shadersetGuid != 0)
    {
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, shadersetGuid));

        assetUsesCount++;

        RPakAssetEntry* asset = pak->GetAssetByGuid(mtlHdr->shadersetGuid);

        if (asset)
            asset->AddRelation(assetEntries->size());
        else
            ExternalAssetUsesCount++;
    }
    
    mtlHdr->pTextureHandles.m_nIndex = dataseginfo.index;
    mtlHdr->pTextureHandles.m_nOffset = guidPageOffset;

    mtlHdr->pStreamingTextureHandles.m_nIndex = dataseginfo.index;
    mtlHdr->pStreamingTextureHandles.m_nOffset = guidPageOffset + textureRefSize;

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, pTextureHandles));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, pStreamingTextureHandles));


    //////////////////////////////////////////
    /// cpu
    std::uint64_t cpuDataSize = sizeof(MaterialCPUDataV12);

    // cpu data
    _vseginfo_t cpuseginfo = pak->CreateNewSegment(sizeof(MaterialCPUHeader) + cpuDataSize, 3, 16);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.pData.m_nIndex = cpuseginfo.index;
    cpuhdr.pData.m_nOffset = sizeof(MaterialCPUHeader);
    cpuhdr.size = cpuDataSize;

    // for debuging in the meantime
    if (mapEntry.HasMember("ForceCPUVersion"))
        cpuhdr.version = mapEntry["ForceCPUVersion"].GetInt();
    else
        cpuhdr.version = 3; // hardcode this until more is known.

    pak->AddPointer(cpuseginfo.index, 0);

    char* cpuData = new char[sizeof(MaterialCPUHeader) + cpuDataSize];

    // copy header into the start
    memcpy_s(cpuData, 16, &cpuhdr, 16);

    // copy the rest of the data after the header
    MaterialCPUDataV12 cpudata{};

    // fill in cpu data

    // ideally we can find better solutions for all of these but for now I cleaned it up
    // emissivetint
    std::float_t emissivetint[3] = { 0.0, 0.0, 0.0 };

    if (mapEntry.HasMember("emissivetint"))
    {
        for (int i = 0; i < 3; i++)
        {
            auto& EmissiveFloat = mapEntry["emissivetint"].GetArray()[i];
            
            emissivetint[i] = EmissiveFloat.GetFloat();
        }
    }
    else 
        Log("No 'emissivetint' specified, assuming there is no emissive texture! \n");

    cpudata.c_emissiveTint.x = emissivetint[0];
    cpudata.c_emissiveTint.y = emissivetint[1];
    cpudata.c_emissiveTint.z = emissivetint[2];

    // albedotint
    std::float_t albedotint[3] = { 1.0, 1.0, 1.0 };

    if (mapEntry.HasMember("albedotint")) 
    {
        for (int i = 0; i < 3; i++)
        {
            auto& AlbedoFloat = mapEntry["albedotint"].GetArray()[i];

            albedotint[i] = AlbedoFloat.GetFloat();
        }
    }

    cpudata.c_albedoTint.x = albedotint[0];
    cpudata.c_albedoTint.y = albedotint[1];
    cpudata.c_albedoTint.z = albedotint[2];

    // uv transforms
    std::float_t uv1Transform[6] = { 1.0, 0, -0, 1.0, 0.0, 0.0 };
    std::float_t uv2Transform[6] = { 1.0, 0, -0, 1.0, 0.0, 0.0 };
    std::float_t uv3Transform[6] = { 1.0, 0, -0, 1.0, 0.0, 0.0 };

    if (mapEntry.HasMember("uv1transform"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv1transform"].GetArray()[i];

            uv1Transform[i] = UVFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("uv2transform"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv2transform"].GetArray()[i];

            uv2Transform[i] = UVFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("uv3transform"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv3transform"].GetArray()[i];

            uv3Transform[i] = UVFloat.GetFloat();
        }
    }

    cpudata.c_uv1.uvScaleX = uv1Transform[0];
    cpudata.c_uv1.uvRotationX = uv1Transform[1];
    cpudata.c_uv1.uvRotationY = uv1Transform[2];
    cpudata.c_uv1.uvScaleY = uv1Transform[3];
    cpudata.c_uv1.uvTranslateX = uv1Transform[4];
    cpudata.c_uv1.uvTranslateY = uv1Transform[5];

    cpudata.c_uv2.uvScaleX = uv2Transform[0];
    cpudata.c_uv2.uvRotationX = uv2Transform[1];
    cpudata.c_uv2.uvRotationY = uv2Transform[2];
    cpudata.c_uv2.uvScaleY = uv2Transform[3];
    cpudata.c_uv2.uvTranslateX = uv2Transform[4];
    cpudata.c_uv2.uvTranslateY = uv2Transform[5];

    cpudata.c_uv3.uvScaleX = uv3Transform[0];
    cpudata.c_uv3.uvRotationX = uv3Transform[1];
    cpudata.c_uv3.uvRotationY = uv3Transform[2];
    cpudata.c_uv3.uvScaleY = uv3Transform[3];
    cpudata.c_uv3.uvTranslateX = uv3Transform[4];
    cpudata.c_uv3.uvTranslateY = uv3Transform[5];

    memcpy_s(cpuData + sizeof(MaterialCPUHeader), cpuDataSize, &cpudata, cpuDataSize);
    //////////////////////////////////////////

    pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)mtlHdr });
    pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)dataBuf });
    pak->AddRawDataBlock({ cpuseginfo.index, cpuseginfo.size, (uint8_t*)cpuData });

    //////////////////////////////////////////

    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid(FullAssetPath.c_str()), subhdrinfo.index, 0, subhdrinfo.size, cpuseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = 12;

    asset.pageEnd = cpuseginfo.index + 1;
    // this isn't even fully true in some apex materials.
    //asset.unk1 = bColpass ? 7 : 8; // what
    // unk1 appears to be maxusecount, although seemingly nothing is affected by changing it unless you exceed 18.
    // In every TF|2 material asset entry I've looked at it's always UsesCount + 1.
    asset.unk1 = (assetUsesCount - ExternalAssetUsesCount) + 1;

    asset.AddGuids(&guids);

    Debug("External assets: %i\n", ExternalAssetUsesCount);

    assetEntries->push_back(asset);
}

// VERSION 8
void Assets::AddMaterialAsset_v15(RPakFileBase* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding matl asset '%s'\n", assetPath);

    uint32_t assetUsesCount = 0; // track how many other assets are used by this asset
    MaterialHeaderV15* mtlHdr = new MaterialHeaderV15();
    std::string sAssetPath = std::string(assetPath);

    std::string type = "sknp";

    if (mapEntry.HasMember("type"))
        type = mapEntry["type"].GetStdString();
    else
        Warning("Adding material without an explicitly defined type. Assuming 'sknp'...\n");

    std::string sFullAssetRpakPath = "material/" + sAssetPath + "_" + type + ".rpak"; // Make full rpak asset path.

    mtlHdr->m_nGUID = RTech::StringToGuid(sFullAssetRpakPath.c_str()); // Convert full rpak asset path to textureGUID and set it in the material header.

    // Game ignores this field when parsing, retail rpaks also have this as 0. But In-Game its being set to either 0x4, 0x5, 0x9.
    // Based on resolution.
    // 512x512 = 0x5
    // 1024x1024 = 0x4
    // 2048x2048 = 0x9
    //if (mapEntry.HasMember("signature"))
    //    mtlHdr->StreamableTextureCount = mapEntry["signature"].GetInt();

    if (mapEntry.HasMember("width")) // Set material width.
        mtlHdr->m_nWidth = mapEntry["width"].GetInt();

    if (mapEntry.HasMember("height")) // Set material width.
        mtlHdr->m_nHeight = mapEntry["height"].GetInt();

    if (mapEntry.HasMember("flags")) // Set flags properly. Responsible for texture stretching, tiling etc.
        mtlHdr->m_Flags = mapEntry["flags"].GetUint();

    std::string surface = "default";

    // surfaces are defined in scripts/surfaceproperties.rson
    if (mapEntry.HasMember("surface"))
        surface = mapEntry["surface"].GetStdString();

    // Get the size of the texture guid section.
    size_t textureRefSize = 0;

    if (mapEntry.HasMember("textures"))
    {
        textureRefSize = mapEntry["textures"].GetArray().Size() * 8;
    }
    else
    {
        Warning("Trying to add material with no textures. Skipping asset...\n");
        return;
    }

    uint32_t assetPathSize = (sAssetPath.length() + 1);
    uint32_t dataBufSize = (assetPathSize + (assetPathSize % 4)) + (textureRefSize * 2) + (surface.length() + 1);

    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(MaterialHeaderV15), SF_HEAD /*| SF_CLIENT*/, 8);

    // asset data
    _vseginfo_t dataseginfo = pak->CreateNewSegment(dataBufSize, SF_CPU /*| SF_CLIENT*/, 8);

    char* dataBuf = new char[dataBufSize] {};
    char* tmp = dataBuf;

    // ===============================
    // write the material path into the buffer
    snprintf(dataBuf, sAssetPath.length() + 1, "%s", assetPath);
    uint8_t assetPathAlignment = (assetPathSize % 4);
    dataBuf += sAssetPath.length() + 1 + assetPathAlignment;

    // ===============================
    // add the texture guids to the buffer
    size_t guidPageOffset = sAssetPath.length() + 1 + assetPathAlignment;

    std::vector<RPakGuidDescriptor> guids{};

    int textureIdx = 0;
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the first TextureGUID Map.
    {
        if (it.GetStdString() != "")
        {
            uint64_t textureGUID = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str()); // Convert texture path to guid.
            *(uint64_t*)dataBuf = textureGUID;
            pak->AddGuidDescriptor(&guids, dataseginfo.index, guidPageOffset + (textureIdx * sizeof(uint64_t))); // Register GUID descriptor for current texture index.

            RPakAssetEntry* txtrAsset = pak->GetAssetByGuid(textureGUID, nullptr);

            if (txtrAsset)
                txtrAsset->AddRelation(assetEntries->size());
            else
                Warning("unable to find texture '%s' for material '%s' within the local assets\n", it.GetString(), assetPath);

            assetUsesCount++;
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++; // Next texture index coming up.
    }
    dataBuf += sizeof(uint64_t) * mapEntry["textures"].Size();


    // ===============================
    // write the surface name into the buffer
    snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());

    // get the original pointer back so it can be used later for writing the buffer
    dataBuf = tmp;

    // ===============================
    // fill out the rest of the header
    mtlHdr->m_pszName.m_nIndex = dataseginfo.index;
    mtlHdr->m_pszName.m_nOffset = 0;

    mtlHdr->m_pszSurfaceProp.m_nIndex = dataseginfo.index;
    mtlHdr->m_pszSurfaceProp.m_nOffset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2);

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pszName));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pszSurfaceProp));

    // Shader Type Handling
    if (type == "sknp")
    {
        // GUIDRefs[4] is Colpass entry.
        mtlHdr->m_GUIDRefs[0] = 0x2B93C99C67CC8B51;
        mtlHdr->m_GUIDRefs[1] = 0x1EBD063EA03180C7;
        mtlHdr->m_GUIDRefs[2] = 0xF95A7FA9E8DE1A0E;
        mtlHdr->m_GUIDRefs[3] = 0x227C27B608B3646B;

        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs));
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 8);
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 16);
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 24);
        assetUsesCount += 4;

        mtlHdr->m_pShaderSet = 0x1D9FFF314E152725;
    }
    else if (type == "wldc")
    {
        // GUIDRefs[4] is Colpass entry which is optional for wldc.
        mtlHdr->m_GUIDRefs[0] = 0x435FA77E363BEA48; // DepthShadow
        mtlHdr->m_GUIDRefs[1] = 0xF734F96BE92E0E71; // DepthPrepass
        mtlHdr->m_GUIDRefs[2] = 0xD306370918620EC0; // DepthVSM
        mtlHdr->m_GUIDRefs[3] = 0xDAB17AEAD2D3387A; // DepthShadowTight

        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs));
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 8);
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 16);
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 24);
        assetUsesCount += 4;

        mtlHdr->m_pShaderSet = 0x4B0F3B4CBD009096;
    }
    else if (type == "rgdp")
    {
        // GUIDRefs[4] is Colpass entry which is optional for wldc.
        mtlHdr->m_GUIDRefs[0] = 0x251FBE09EFFE8AB1; // DepthShadow
        mtlHdr->m_GUIDRefs[1] = 0xE2D52641AFC77395; // DepthPrepass
        mtlHdr->m_GUIDRefs[2] = 0xBDBF90B97E7D9280; // DepthVSM
        mtlHdr->m_GUIDRefs[3] = 0x85654E05CF9B40E7; // DepthShadowTight

        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs));
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 8);
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 16);
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 24);
        assetUsesCount += 4;

        mtlHdr->m_pShaderSet = 0x2a2db3a47af9b3d5;
    }

    pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_pShaderSet));
    assetUsesCount++;

    if (mtlHdr->m_pShaderSet != 0)
    {
        RPakAssetEntry* asset = pak->GetAssetByGuid(mtlHdr->m_pShaderSet);

        if (asset)
            asset->AddRelation(assetEntries->size());
    }

    // Is this a colpass asset?
    bool bColpass = false;
    if (mapEntry.HasMember("colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + ".rpak";
        mtlHdr->m_GUIDRefs[4] = RTech::StringToGuid(colpassPath.c_str());

        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 32);
        assetUsesCount++;

        bColpass = false;
    }

    for (int i = 0; i < 5; ++i)
    {
        uint64_t guid = mtlHdr->m_GUIDRefs[i];

        if (guid != 0)
        {
            RPakAssetEntry* asset = pak->GetAssetByGuid(guid, nullptr);

            if (asset)
                asset->AddRelation(assetEntries->size());
        }
    }

    mtlHdr->m_pTextureHandles.m_nIndex = dataseginfo.index;
    mtlHdr->m_pTextureHandles.m_nOffset = guidPageOffset;

    mtlHdr->m_pStreamingTextureHandles.m_nIndex = dataseginfo.index;
    mtlHdr->m_pStreamingTextureHandles.m_nOffset = guidPageOffset + textureRefSize;

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pTextureHandles));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pStreamingTextureHandles));

    mtlHdr->m_Flags2 = 0x72000000;
    mtlHdr->something2 = 0x100000;

    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8; ++j)
            mtlHdr->m_UnknownSections[i].m_Unknown1[j] = 0xf0000000;

        uint32_t f1 = bColpass ? 0x5 : 0x17;

        mtlHdr->m_UnknownSections[i].m_UnkRenderFlags = 4;
        mtlHdr->m_UnknownSections[i].m_VisibilityFlags = f1;
        mtlHdr->m_UnknownSections[i].m_FaceDrawingFlags = 6;
    }

    //////////////////////////////////////////
    /// cpu
    std::uint64_t cpuDataSize = sizeof(MaterialCPUDataV15);

    // cpu data
    _vseginfo_t cpuseginfo = pak->CreateNewSegment(sizeof(MaterialCPUHeader) + cpuDataSize, SF_CPU | SF_TEMP, 16);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.pData.m_nIndex = cpuseginfo.index;
    cpuhdr.pData.m_nOffset = sizeof(MaterialCPUHeader);
    cpuhdr.size = cpuDataSize;
    cpuhdr.version = 3; // hardcode this until more is known, for apex specifically this will vary much more.

    pak->AddPointer(cpuseginfo.index, 0);

    MaterialCPUDataV15 cpudata{};

    char* cpuData = new char[sizeof(MaterialCPUHeader) + cpuDataSize];

    // copy header into the start
    memcpy_s(cpuData, sizeof(MaterialCPUHeader), &cpuhdr, sizeof(MaterialCPUHeader));

    // copy the rest of the data after the header
    memcpy_s(cpuData + sizeof(MaterialCPUHeader), cpuDataSize, &cpudata, cpuDataSize);

    //////////////////////////////////////////

    pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)mtlHdr });
    pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)dataBuf });
    pak->AddRawDataBlock({ cpuseginfo.index, cpuseginfo.size, (uint8_t*)cpuData });

    //////////////////////////////////////////

    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), subhdrinfo.index, 0, subhdrinfo.size, cpuseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = MATL_VERSION;

    asset.pageEnd = cpuseginfo.index + 1;
    asset.unk1 = bColpass ? 7 : 8; // what

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);
}
