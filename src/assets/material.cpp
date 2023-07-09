#include "pch.h"
#include "assets.h"
#include "public/material.h"

// VERSION 7
void Assets::AddMaterialAsset_v12(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding matl asset '%s'\n", assetPath);

    // we need to take better account of textures once asset caching becomes a thing
    short externalDependencyCount = 0;

    if (mapEntry.HasMember("textures") && mapEntry["textures"].IsArray())
    {
        for (auto& it : mapEntry["textures"].GetArray())
        {
            if (!it.IsString())
                continue;

            if (it.GetStringLength() == 0)
                continue;

            // check if texture string is an asset guid (e.g., "0x5DCAT")
            if (RTech::ParseGUIDFromString(it.GetString()))
            {
                externalDependencyCount++; // we are not adding an asset so it is external
                continue;
            }

            AddTextureAsset(pak, assetEntries, it.GetString(), mapEntry.HasMember("disableStreaming") && mapEntry["disableStreaming"].GetBool());
        }
    }


    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(MaterialHeaderV12), SF_HEAD, 16);
    MaterialHeaderV12* mtlHdr = reinterpret_cast<MaterialHeaderV12*>(hdrChunk.Data());
    std::string sAssetPath = std::string(assetPath);

    std::string type = "skn";

    if (mapEntry.HasMember("type"))
        type = mapEntry["type"].GetStdString();
    else
        Warning("Adding material without an explicitly defined type. Assuming 'skn'...\n");

    std::string sFullAssetRpakPath = "material/" + sAssetPath + "_" + type + ".rpak"; // Make full rpak asset path.

    mtlHdr->guid = RTech::StringToGuid(sFullAssetRpakPath.c_str()); // Convert full rpak asset path to guid and set it in the material header.

    if (mapEntry.HasMember("width")) // Set material width.
        mtlHdr->width = mapEntry["width"].GetInt();

    if (mapEntry.HasMember("height")) // Set material width.
        mtlHdr->height = mapEntry["height"].GetInt();

    //if (mapEntry.HasMember("flags")) // Set flags properly. Responsible for texture stretching, tiling etc.
    //    mtlHdr->flags = mapEntry["flags"].GetUint();

    if (mapEntry.HasMember("flags") && mapEntry["flags"].GetStdString() != "") // Set flags properly. Responsible for texture stretching, tiling etc.
        mtlHdr->flags = strtoul(("0x" + mapEntry["flags"].GetStdString()).c_str(), NULL, 0);
    else
        mtlHdr->flags = 0x1D0300;

    if (mapEntry.HasMember("flags2") && mapEntry["flags2"].GetStdString() != "") // This does a lot of very important stuff.
        mtlHdr->flags2 = strtoul(("0x" + mapEntry["flags2"].GetStdString()).c_str(), NULL, 0);
    else
        mtlHdr->flags2 = 0x56000020;

    // not always 0x100000, however if the material you're trying to do doesn't have this seek help
    mtlHdr->flags2 |= 0x10000000000000;

    // some janky setup you love to see
    mtlHdr->unk3 = type == "gen" ? 0xFBA63181 : 0x40D33E8F;

    if (type == "rgd")
    {
        Warning("Type 'rgd' is not supported in Titanfall 2!!!", type.c_str());
        exit(EXIT_FAILURE);
    }

    if ((type == "fix" || type == "skn"))
    {
        for (int i = 0; i < 2; ++i)
        {
            mtlHdr->unkSections[i].UnkRenderLighting = 0xF0138004;
            mtlHdr->unkSections[i].UnkRenderAliasing = 0xF0138004;
            mtlHdr->unkSections[i].UnkRenderDoF = 0xF0138004;
            mtlHdr->unkSections[i].UnkRenderUnknown = 0x00138004;

            mtlHdr->unkSections[i].unk = 0x00000004;
        }
    }

    std::string surface = "default";
    std::string surface2 = "default";

    // surfaces are defined in scripts/surfaceproperties.txt
    if (mapEntry.HasMember("surface"))
        surface = mapEntry["surface"].GetStdString();

    // rarely used edge case but it's good to have.
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

    size_t alignedPathSize = IALIGN4(sAssetPath.length() + 1);
    uint32_t dataBufSize = alignedPathSize + (textureRefSize * 2) + (surface.length() + 1);

    // asset data
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataBufSize, SF_CPU /*| SF_CLIENT*/, 8);

    char* dataBuf = dataChunk.Data();
    char* tmp = dataBuf;

    // ===============================
    // write the material path into the buffer
    snprintf(dataBuf, sAssetPath.length() + 1, "%s", assetPath);
    dataBuf += alignedPathSize;

    // ===============================
    // add the texture guids to the buffer
    size_t guidPageOffset = alignedPathSize;

    std::vector<PakGuidRefHdr_t> guids{};

    int textureIdx = 0;
    for (auto& it : mapEntry["textures"].GetArray())
    {
        uint64_t textureGuid = RTech::GetAssetGUIDFromString(it.GetString(), true); // get texture guid

        *(uint64_t*)dataBuf = textureGuid;

        if (textureGuid != 0) // only deal with dependencies if the guid is not 0
        {
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(guidPageOffset + (textureIdx * sizeof(uint64_t)))); // register guid for this texture reference

            PakAsset_t* txtrAsset = pak->GetAssetByGuid(textureGuid, nullptr);

            if (txtrAsset)
                txtrAsset->AddRelation(assetEntries->size());
            else
            {
                externalDependencyCount++; // if the asset doesn't exist in the pak it has to be external, or missing
                Warning("unable to find texture '%s' for material '%s' within the local assets\n", it.GetString(), assetPath);
            }
        }

        dataBuf += sizeof(uint64_t);
        textureIdx++;
    }

    dataBuf += sizeof(uint64_t) * mapEntry["textures"].Size();

    // write the surface name into the buffer
    snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());

    // write surface2 name into the buffer if used
    if (mapEntry.HasMember("surface2"))
    {
        dataBuf += (surface.length() + 1);
        snprintf(dataBuf, surface2.length() + 1, "%s", surface2.c_str());
    }

    // get the original pointer back so it can be used later for writing the buffer
    dataBuf = tmp;

    // ===============================
    // fill out the rest of the header
    mtlHdr->materialName = dataChunk.GetPointer();

    mtlHdr->surfaceProp = dataChunk.GetPointer(alignedPathSize + (textureRefSize * 2));

    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialHeaderV12, materialName)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialHeaderV12, surfaceProp)));

    // there is no excuss for not inputting these in titanfall 2 as debug names are saved in /all/ rpaks.
    {
        // optional depth material overrides
        if (mapEntry.HasMember("depthShadowMaterial") && mapEntry["depthShadowMaterial"].IsString())
        {
            mtlHdr->depthShadowMaterial = RTech::GetAssetGUIDFromString(mapEntry["depthShadowMaterial"].GetString());
        }

        if (mapEntry.HasMember("depthPrepassMaterial") && mapEntry["depthPrepassMaterial"].IsString())
        {
            mtlHdr->depthPrepassMaterial = RTech::GetAssetGUIDFromString(mapEntry["depthPrepassMaterial"].GetString());
        }

        if (mapEntry.HasMember("depthVSMMaterial") && mapEntry["depthVSMMaterial"].IsString())
        {
            mtlHdr->depthVSMMaterial = RTech::GetAssetGUIDFromString(mapEntry["depthVSMMaterial"].GetString());
        }

        if (mapEntry.HasMember("shaderset") && mapEntry["shaderset"].IsString())
        {
            mtlHdr->shaderSet = RTech::GetAssetGUIDFromString(mapEntry["shaderset"].GetString());
        }
    }

    bool bColpass = false; // is this colpass material?

    // get referenced colpass material if exists
    if (mapEntry.HasMember("colpass") && mapEntry["colpass"].IsString())
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + ".rpak";
        mtlHdr->colpassMaterial = RTech::StringToGuid(colpassPath.c_str());
    }

    // loop thru referenced assets (depth materials, colpass material) note: shaderset isn't inline with these vars in r2, so we set it after
    for (int i = 0; i < 4; ++i)
    {
        uint64_t guid = *((uint64_t*)&mtlHdr->depthShadowMaterial + i);

        if (guid != 0)
        {
            pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialHeaderV12, depthShadowMaterial) + (i * 8)));

            PakAsset_t* asset = pak->GetAssetByGuid(guid, nullptr, true);

            if (asset)
                asset->AddRelation(assetEntries->size());
            else
                externalDependencyCount++;
        }
    }

    // shaderset, see note above
    if (mtlHdr->shaderSet != 0)
    {
        pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialHeaderV12, shaderSet)));

        PakAsset_t* asset = pak->GetAssetByGuid(mtlHdr->shaderSet, nullptr, true);

        if (asset)
            asset->AddRelation(assetEntries->size());
        else
            externalDependencyCount++;
    }

    mtlHdr->textureHandles = dataChunk.GetPointer(guidPageOffset);

    mtlHdr->streamingTextureHandles = dataChunk.GetPointer(guidPageOffset + textureRefSize);

    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialHeaderV12, textureHandles)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialHeaderV12, streamingTextureHandles)));

    int unkFlags = 4;
    short depthStencilFlags = bColpass ? 0x5 : 0x17;
    short rasterizerFlags = 0x6; // CULL_BACK

    // !!!temp!!! - these should be replaced by proper flag string parsing when possible
    if (mapEntry.HasMember("unkFlags") && mapEntry["unkFlags"].IsInt())
        unkFlags = mapEntry["unkFlags"].GetInt();

    if (mapEntry.HasMember("depthStencilFlags") && mapEntry["depthStencilFlags"].IsInt())
        depthStencilFlags = mapEntry["depthStencilFlags"].GetInt();

    if (mapEntry.HasMember("rasterizerFlags") && mapEntry["rasterizerFlags"].IsInt())
        rasterizerFlags = mapEntry["rasterizerFlags"].GetInt();

    for (int i = 0; i < 2; ++i)
    {
        // set automatically for r2 as they have different values per, still no clue what they do
        //for (int j = 0; j < 8; ++j)
        //    mtlHdr->unkSections[i].unk_0[j] = 0xf0000000;

        mtlHdr->unkSections[i].unk = unkFlags;
        mtlHdr->unkSections[i].depthStencilFlags = depthStencilFlags;
        mtlHdr->unkSections[i].rasterizerFlags = rasterizerFlags;
    }

    //////////////////////////////////////////
    /// cpu
    uint64_t dxStaticBufSize = 0;

    std::string cpuPath = pak->GetAssetPath() + sAssetPath + "_" + type + ".cpu";

    MaterialShaderBufferV12 shaderBuf{};

    /*SHADERBUF SETUP START*/
    if (mapEntry.HasMember("emissiveTint"))
    {
        for (int i = 0; i < 3; i++)
        {
            auto& EmissiveFloat = mapEntry["emissiveTint"].GetArray()[i];

            shaderBuf.c_emissiveTint[i] = EmissiveFloat.GetFloat();
        }
    }
    else
        Log("No 'emissiveTint' specified, assuming there is no emissive texture! \n");

    if (mapEntry.HasMember("albedoTint"))
    {
        for (int i = 0; i < 3; i++)
        {
            auto& EmissiveFloat = mapEntry["albedoTint"].GetArray()[i];

            shaderBuf.c_albedoTint[i] = EmissiveFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("opacity"))
        shaderBuf.c_opacity = mapEntry["opacity"].GetFloat();

    // spec tint is not really needed currently

    if (mapEntry.HasMember("uv1"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv1"].GetArray()[i];

            *shaderBuf.c_uv1.pFloat(i) = UVFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("uv2"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv2"].GetArray()[i];

            *shaderBuf.c_uv1.pFloat(i) = UVFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("uv3"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv3"].GetArray()[i];

            *shaderBuf.c_uv1.pFloat(i) = UVFloat.GetFloat();
        }
    }
    /*SHADERBUF SETUP END*/

    CPakDataChunk uberBufChunk;
    if (FILE_EXISTS(cpuPath))
    {
        dxStaticBufSize = Utils::GetFileSize(cpuPath);

        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 16);

        std::ifstream cpuIn(cpuPath, std::ios::in | std::ios::binary);
        cpuIn.read(uberBufChunk.Data() + sizeof(MaterialCPUHeader), dxStaticBufSize);
        cpuIn.close();
    }
    else {
        dxStaticBufSize = sizeof(MaterialShaderBufferV12);
        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 16);

        memcpy(uberBufChunk.Data() + sizeof(MaterialCPUHeader), shaderBuf.AsCharPtr(), dxStaticBufSize);
    }

    MaterialCPUHeader* cpuhdr = reinterpret_cast<MaterialCPUHeader*>(uberBufChunk.Data());
    cpuhdr->dataPtr = uberBufChunk.GetPointer(sizeof(MaterialCPUHeader));
    cpuhdr->dataSize = dxStaticBufSize;

    pak->AddPointer(uberBufChunk.GetPointer(offsetof(MaterialCPUHeader, dataPtr)));

    //////////////////////////////////////////

    PakAsset_t asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), hdrChunk.GetPointer(), hdrChunk.GetSize(), uberBufChunk.GetPointer(), -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = 12; // might be good to define this somewhere

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = (asset.dependenciesCount - externalDependencyCount) + 1; // plus one for material asset (I think)

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);

    Log("\n");   
}

// VERSION 8
void Assets::AddMaterialAsset_v15(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding matl asset '%s'\n", assetPath);

    if (mapEntry.HasMember("textures") && mapEntry["textures"].IsArray())
    {
        for (auto& it : mapEntry["textures"].GetArray())
        {
            if (!it.IsString())
                continue;

            if (it.GetStringLength() == 0)
                continue;

            // check if texture string is an asset guid (e.g., "0x5DCAT")
            if (RTech::ParseGUIDFromString(it.GetString()))
                continue;

            AddTextureAsset(pak, assetEntries, it.GetString(), mapEntry.HasMember("disableStreaming") && mapEntry["disableStreaming"].GetBool());
        }
    }


    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(MaterialHeaderV15), SF_HEAD, 16);
    MaterialHeaderV15* mtlHdr = reinterpret_cast<MaterialHeaderV15*>(hdrChunk.Data());
    std::string sAssetPath = std::string(assetPath);

    std::string type = "sknp";

    if (JSON_IS_STR(mapEntry, "type"))
        type = mapEntry["type"].GetStdString();
    else
        Warning("Adding material without an explicitly defined type. Assuming 'sknp'...\n");

    std::string sFullAssetRpakPath = "material/" + sAssetPath + "_" + type + ".rpak"; // Make full rpak asset path.

    mtlHdr->guid = RTech::StringToGuid(sFullAssetRpakPath.c_str()); // Convert full rpak asset path to guid and set it in the material header.

    mtlHdr->width = JSON_GET_INT(mapEntry, "width", 0);
    mtlHdr->height = JSON_GET_INT(mapEntry, "height", 0);
    mtlHdr->flags_78 = JSON_GET_UINT(mapEntry, "flags", 0);

    // surfaces are defined in scripts/surfaceproperties.rson
    std::string surface = JSON_GET_STR(mapEntry, "surface", "default");

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

    size_t alignedPathSize = IALIGN4(sAssetPath.length() + 1);
    uint32_t dataBufSize = alignedPathSize + (textureRefSize * 2) + (surface.length() + 1);

    // asset data
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataBufSize, SF_CPU /*| SF_CLIENT*/, 8);

    char* dataBuf = dataChunk.Data();
    char* tmp = dataBuf;

    // ===============================
    // write the material path into the buffer
    snprintf(dataBuf, sAssetPath.length() + 1, "%s", assetPath);
    dataBuf += alignedPathSize;

    // ===============================
    // add the texture guids to the buffer
    size_t guidPageOffset = alignedPathSize;

    std::vector<PakGuidRefHdr_t> guids{};

    int textureIdx = 0;
    for (auto& it : mapEntry["textures"].GetArray())
    {
        uint64_t textureGuid = RTech::GetAssetGUIDFromString(it.GetString(), true); // get texture guid

        *(uint64_t*)dataBuf = textureGuid;

        if (textureGuid != 0) // only deal with dependencies if the guid is not 0
        {
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(guidPageOffset + (textureIdx * sizeof(uint64_t)))); // register guid for this texture reference
        
            PakAsset_t* txtrAsset = pak->GetAssetByGuid(textureGuid, nullptr);

            if (txtrAsset)
                txtrAsset->AddRelation(assetEntries->size());
            else
                Warning("unable to find texture '%s' for material '%s' within the local assets\n", it.GetString(), assetPath);
        }

        dataBuf += sizeof(uint64_t);
        textureIdx++;
    }

    dataBuf += sizeof(uint64_t) * mapEntry["textures"].Size();

    // write the surface name into the buffer
    snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());

    // get the original pointer back so it can be used later for writing the buffer
    dataBuf = tmp;

    // ===============================
    // fill out the rest of the header
    mtlHdr->materialName = dataChunk.GetPointer();

    mtlHdr->surfaceProp = dataChunk.GetPointer(alignedPathSize + (textureRefSize * 2));

    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialHeaderV15, materialName)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialHeaderV15, surfaceProp)));

    // default shader type params
    if (type == "sknp")
    {
        mtlHdr->depthShadowMaterial = 0x2B93C99C67CC8B51;
        mtlHdr->depthPrepassMaterial = 0x1EBD063EA03180C7;
        mtlHdr->depthVSMMaterial = 0xF95A7FA9E8DE1A0E;
        mtlHdr->depthShadowTightMaterial = 0x227C27B608B3646B;

        mtlHdr->shaderSet = 0x1D9FFF314E152725;
        mtlHdr->materialType = SKNP;
    }
    else if (type == "wldc")
    {
        mtlHdr->depthShadowMaterial = 0x435FA77E363BEA48;
        mtlHdr->depthPrepassMaterial = 0xF734F96BE92E0E71;
        mtlHdr->depthVSMMaterial = 0xD306370918620EC0;
        mtlHdr->depthShadowTightMaterial = 0xDAB17AEAD2D3387A;

        mtlHdr->shaderSet = 0x4B0F3B4CBD009096;
        mtlHdr->materialType = WLDC;
    }
    else if (type == "rgdp")
    {
        mtlHdr->depthShadowMaterial = 0x251FBE09EFFE8AB1;
        mtlHdr->depthPrepassMaterial = 0xE2D52641AFC77395;
        mtlHdr->depthVSMMaterial = 0xBDBF90B97E7D9280;
        mtlHdr->depthShadowTightMaterial = 0x85654E05CF9B40E7;

        mtlHdr->shaderSet = 0x2a2db3a47af9b3d5;
        mtlHdr->materialType = RGDP;
    }

    {
        // optional depth material overrides
        if (JSON_IS_STR(mapEntry, "depthShadowMaterial"))
        {
            mtlHdr->depthShadowMaterial = RTech::GetAssetGUIDFromString(mapEntry["depthShadowMaterial"].GetString());
        }

        if (JSON_IS_STR(mapEntry, "depthPrepassMaterial"))
        {
            mtlHdr->depthPrepassMaterial = RTech::GetAssetGUIDFromString(mapEntry["depthPrepassMaterial"].GetString());
        }

        if (JSON_IS_STR(mapEntry, "depthVSMMaterial"))
        {
            mtlHdr->depthVSMMaterial = RTech::GetAssetGUIDFromString(mapEntry["depthVSMMaterial"].GetString());
        }

        if (JSON_IS_STR(mapEntry, "depthShadowTightMaterial"))
        {
            mtlHdr->depthShadowTightMaterial = RTech::GetAssetGUIDFromString(mapEntry["depthShadowTightMaterial"].GetString());
        }

        // optional shaderset override
        if (JSON_IS_STR(mapEntry, "shaderset"))
        {
            mtlHdr->shaderSet = RTech::GetAssetGUIDFromString(mapEntry["shaderset"].GetString());
        }
    }

    bool bColpass = false; // is this colpass material?

    // get referenced colpass material if exists
    if (JSON_IS_STR(mapEntry, "colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + ".rpak";
        mtlHdr->colpassMaterial = RTech::StringToGuid(colpassPath.c_str());
    }

    // loop thru referenced assets (depth materials, colpass material, shaderset)
    for (int i = 0; i < 6; ++i)
    {
        uint64_t guid = *((uint64_t*)&mtlHdr->depthShadowMaterial + i);

        if (guid != 0)
        {
            pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialHeaderV15, depthShadowMaterial) + (i*8)));

            PakAsset_t* asset = pak->GetAssetByGuid(guid, nullptr, true);

            if (asset)
                asset->AddRelation(assetEntries->size());
        }
    }

    mtlHdr->textureHandles = dataChunk.GetPointer(guidPageOffset);

    mtlHdr->streamingTextureHandles = dataChunk.GetPointer(guidPageOffset + textureRefSize);

    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialHeaderV15, textureHandles)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialHeaderV15, streamingTextureHandles)));

    mtlHdr->unk_88 = 0x72000000;
    mtlHdr->unk_8C = 0x100000;

    int unkFlags = 4;
    short depthStencilFlags = bColpass ? 0x5 : 0x17;
    short rasterizerFlags = 6;

    // !!!temp!!! - these should be replaced by proper flag string parsing when possible
    if (mapEntry.HasMember("unkFlags") && mapEntry["unkFlags"].IsInt())
        unkFlags = mapEntry["unkFlags"].GetInt();

    if (mapEntry.HasMember("depthStencilFlags") && mapEntry["depthStencilFlags"].IsInt())
        depthStencilFlags = mapEntry["depthStencilFlags"].GetInt();

    if (mapEntry.HasMember("rasterizerFlags") && mapEntry["rasterizerFlags"].IsInt())
        rasterizerFlags = mapEntry["rasterizerFlags"].GetInt();

    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8; ++j)
            mtlHdr->unkSections[i].unk_0[j] = 0xf0000000;

        mtlHdr->unkSections[i].unk = unkFlags;
        mtlHdr->unkSections[i].depthStencilFlags = depthStencilFlags;
        mtlHdr->unkSections[i].rasterizerFlags = rasterizerFlags;
    }

    //////////////////////////////////////////
    /// cpu
    uint64_t dxStaticBufSize = 0;

    std::string cpuPath = pak->GetAssetPath() + sAssetPath + "_" + type + ".cpu";


    CPakDataChunk uberBufChunk;
    if (FILE_EXISTS(cpuPath))
    {
        dxStaticBufSize = Utils::GetFileSize(cpuPath);

        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 16);

        std::ifstream cpuIn(cpuPath, std::ios::in | std::ios::binary);
        cpuIn.read(uberBufChunk.Data() + sizeof(MaterialCPUHeader), dxStaticBufSize);
        cpuIn.close();
    }
    else {
        dxStaticBufSize = 544;
        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 16);

        // i hate this
        unsigned char testData[544] = {
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0xAB, 0xAA, 0x2A, 0x3E, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x1C, 0x46, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
            0x81, 0x95, 0xE3, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F,
            0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x3F, 0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0xDE, 0x88, 0x1B, 0x3D, 0xDE, 0x88, 0x1B, 0x3D, 0xDE, 0x88, 0x1B, 0x3D,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
        };

        memcpy(uberBufChunk.Data() + sizeof(MaterialCPUHeader), testData, dxStaticBufSize);
    }

    MaterialCPUHeader* cpuhdr = reinterpret_cast<MaterialCPUHeader*>(uberBufChunk.Data());
    cpuhdr->dataPtr = uberBufChunk.GetPointer(sizeof(MaterialCPUHeader));
    cpuhdr->dataSize = dxStaticBufSize;

    pak->AddPointer(uberBufChunk.GetPointer(offsetof(MaterialCPUHeader, dataPtr)));

    //////////////////////////////////////////

    PakAsset_t asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), hdrChunk.GetPointer(), hdrChunk.GetSize(), uberBufChunk.GetPointer(), -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = MATL_VERSION;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = bColpass ? 7 : 8; // what

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);

    Log("\n");
}
