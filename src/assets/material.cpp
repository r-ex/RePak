#include "pch.h"
#include "assets.h"
#include "public/material.h"

#undef GetObject
// we need to take better account of textures once asset caching becomes a thing
void Material_CreateTextures(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, rapidjson::Value& mapEntry)
{
    if (JSON_IS_ARRAY(mapEntry, "textures"))
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

            Assets::AddTextureAsset(pak, assetEntries, it.GetString(), JSON_GET_BOOL(mapEntry, "disableStreaming", false), true);
        }
    }
    else if (JSON_IS_OBJECT(mapEntry, "textures"))
    {
        for (auto& it : mapEntry["textures"].GetObject())
        {
            if (!it.value.IsString())
                continue;

            if (it.value.GetStringLength() == 0)
                continue;

            // check if texture string is an asset guid (e.g., "0x5DCAT")
            if (RTech::ParseGUIDFromString(it.value.GetString()))
                continue;

            Assets::AddTextureAsset(pak, assetEntries, it.value.GetString(), JSON_GET_BOOL(mapEntry, "disableStreaming", false), true);
        }
    }

}

// ideally replace these with material file funcs
void MaterialAsset_t::FromJSON(rapidjson::Value& mapEntry)
{
    // default type on v12 assets is "skn"
    // default type on v15 assets is "sknp"
    std::string defaultMaterialType = this->assetVersion <= 12 ? "skn" : "sknp";
    this->materialTypeStr = JSON_GET_STR(mapEntry, "type", defaultMaterialType);
    
    this->materialType = Material_ShaderTypeFromString(this->materialTypeStr);

    // material max dimensions
    this->width = JSON_GET_INT(mapEntry, "width", 0); // Set material width.
    this->height = JSON_GET_INT(mapEntry, "height", 0); // Set material height.

    // temp samplers !!!
    // this will be done a bit better when material files become real
    // for now this is the only real way of doing it so meh
    uint32_t samplers = 0x1D0300;

    if (JSON_IS_STR(mapEntry, "samplers")) // Set samplers properly. Responsible for texture stretching, tiling etc.
        samplers = strtoul(("0x" + mapEntry["samplers"].GetStdString()).c_str(), NULL, 0);

    memcpy(this->samplers, &samplers, sizeof(samplers));

    // more tradition vmt like flags
    if (JSON_IS_STR(mapEntry, "flags2")) // This does a lot of very important stuff.
        this->flags2 = strtoull(("0x" + mapEntry["flags2"].GetStdString()).c_str(), NULL, 0);
    else
        this->flags2 = (0x56000020 | 0x10000000000000); // beeg flag is used on most things

    // dx flags
    // !!!temp!!! - these should be replaced by proper flag string parsing when possible
    int unkFlags = JSON_GET_INT(mapEntry, "unkFlags", 0x4);
    short depthStencilFlags = JSON_GET_INT(mapEntry, "depthStencilFlags", 0x17);
    short rasterizerFlags = JSON_GET_INT(mapEntry, "rasterizerFlags", 0x6); // CULL_BACK

    for (int i = 0; i < 2; ++i)
    {
        // set default as apex values, set r2 later if needed
        for (int j = 0; j < 8; ++j)
            this->dxStates[i].blendStates[j] = MaterialBlendState_t(false, 0xf);

        this->dxStates[i].unk = unkFlags;
        this->dxStates[i].depthStencilFlags = depthStencilFlags;
        this->dxStates[i].rasterizerFlags = rasterizerFlags;
    }

    // surfaces are defined in scripts/surfaceproperties.txt or scripts/surfaceproperties.rson
    this->surface = JSON_GET_STR(mapEntry, "surface", "default");

    // used for blend materials and the like
    this->surface2 = JSON_GET_STR(mapEntry, "surface2", "");

    // optional depth material overrides
    // probably should add types and prefixes
    std::string depthPath{};

    // defaults
    //depthPath = "material/code_private/depth_shadow_" + ((rasterizerFlags & 0x4 && !(rasterizerFlags & 0x2)) ? "frontculled_" : "") + this->materialTypeStr + ".rpak";
    depthPath = "material/code_private/depth_shadow_" + this->materialTypeStr + ".rpak";
    this->depthShadowMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());

    //depthPath = "material/code_private/depth_prepass_" + ((rasterizerFlags & 0x2 && !(rasterizerFlags & 0x4)) ? "twosided_" : "") + this->materialTypeStr + ".rpak";
    depthPath = "material/code_private/depth_prepass_" + this->materialTypeStr + ".rpak";
    this->depthPrepassMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());

    depthPath = "material/code_private/depth_vsm_" + this->materialTypeStr + ".rpak";
    this->depthVSMMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());

    depthPath = "material/code_private/depth_shadow_tight_" + this->materialTypeStr + ".rpak";
    this->depthShadowTightMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());

    // check for overrides
    if (JSON_IS_STR(mapEntry, "depthShadowMaterial"))
    {
        depthPath = "material/" + mapEntry["depthShadowMaterial"].GetStdString() + "_" + this->materialTypeStr + ".rpak";
        this->depthShadowMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());
    }

    if (JSON_IS_STR(mapEntry, "depthPrepassMaterial"))
    {
        depthPath = "material/" + mapEntry["depthPrepassMaterial"].GetStdString() + "_" + this->materialTypeStr + ".rpak";
        this->depthPrepassMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());
    }

    if (JSON_IS_STR(mapEntry, "depthVSMMaterial"))
    {
        depthPath = "material/" + mapEntry["depthVSMMaterial"].GetStdString() + "_" + this->materialTypeStr + ".rpak";
        this->depthVSMMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());
    }

    if (JSON_IS_STR(mapEntry, "depthShadowTightMaterial"))
    {
        depthPath = "material/" + mapEntry["depthShadowTightMaterial"].GetStdString() + "_" + this->materialTypeStr + ".rpak";
        this->depthShadowTightMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());
    }


    // get referenced colpass material if exists
    if (JSON_IS_STR(mapEntry, "colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString()+ "_" + this->materialTypeStr + ".rpak"; // auto add type? remove if disagree. materials never have their types in names so I don't think this should be expected?
        this->colpassMaterial = RTech::StringToGuid(colpassPath.c_str());
    }


    // optional shaderset override
    if (JSON_IS_STR(mapEntry, "shaderset"))
        this->shaderSet = RTech::GetAssetGUIDFromString(mapEntry["shaderset"].GetString());

    // this is more desirable but would break guid input
    /*if (JSON_IS_STR(mapEntry, "shaderset"))
    {
        std::string shadersetPath = "shaderset/" + mapEntry["shaderset"].GetStdString() + ".rpak";
        this->shaderSet = RTech::GetAssetGUIDFromString(shadersetPath.c_str());
    }*/

    // texan
    if (JSON_IS_STR(mapEntry, "textureAnimation"))
        this->textureAnimation = RTech::GetAssetGUIDFromString(mapEntry["textureAnimation"].GetString());
}

// shader parsing eventually
void Material_SetupDXBufferFromJson(GenericShaderBuffer* shaderBuf, rapidjson::Value& mapEntry)
{
    if (mapEntry.HasMember("emissiveTint"))
    {
        for (int i = 0; i < 3; i++)
        {
            auto& EmissiveFloat = mapEntry["emissiveTint"].GetArray()[i];

            shaderBuf->c_L0_emissiveTint[i] = EmissiveFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("albedoTint"))
    {
        for (int i = 0; i < 3; i++)
        {
            auto& EmissiveFloat = mapEntry["albedoTint"].GetArray()[i];

            shaderBuf->c_L0_albedoTint[i] = EmissiveFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("opacity"))
        shaderBuf->c_opacity = mapEntry["opacity"].GetFloat();

    // spec tint is not really needed currently

    if (mapEntry.HasMember("uv1"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv1"].GetArray()[i];

            *shaderBuf->c_uv1.pFloat(i) = UVFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("uv2"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv2"].GetArray()[i];

            *shaderBuf->c_uv2.pFloat(i) = UVFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("uv3"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv3"].GetArray()[i];

            *shaderBuf->c_uv3.pFloat(i) = UVFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("uv4"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv4"].GetArray()[i];

            *shaderBuf->c_uv4.pFloat(i) = UVFloat.GetFloat();
        }
    }

    if (mapEntry.HasMember("uv5"))
    {
        for (int i = 0; i < 6; i++)
        {
            auto& UVFloat = mapEntry["uv5"].GetArray()[i];

            *shaderBuf->c_uv5.pFloat(i) = UVFloat.GetFloat();
        }
    }
}

size_t Material_GetHighestTextureBindPoint(rapidjson::Value& mapEntry)
{
    uint32_t max = 0;
    for (auto& it : mapEntry["textures"].GetObject())
    {
        uint32_t index = static_cast<uint32_t>(atoi(it.name.GetString()));
        if (index > max)
            max = index;
    }

    return max;
}

size_t Material_AddTextures(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, rapidjson::Value& mapEntry)
{
    Material_CreateTextures(pak, assetEntries, mapEntry);

    size_t textureCount = 0;
    if (JSON_IS_ARRAY(mapEntry, "textures"))
    {
        textureCount = mapEntry["textures"].GetArray().Size();
    }
    else if (JSON_IS_OBJECT(mapEntry, "textures"))
    {
        // uncomment and replace if manually specified texture slot counts are required
        // i don't think it's entirely necessary though, as really the material should only need
        // to have as many slots as the highest non-null texture

        // ok! it is necessary
        // shaderset has a texture input count variable that is used when looping over the texture array
        // and since we can't modify that from here, we have to rely on the user to set this properly!
        textureCount = JSON_GET_UINT(mapEntry, "textureSlotCount", 0);

        textureCount = max(textureCount, Material_GetHighestTextureBindPoint(mapEntry) + 1);
    }
    else
    {
        Warning("Trying to add material with no textures. Skipping asset...\n"); // shouldn't this be possible though??
        return 0;
    }

    return textureCount;
}

void Material_SetTitanfall2Preset(MaterialAsset_t* material, const std::string& presetName)
{
    // default value for presetName is "none"
    if (presetName == "none")
        return;

    MaterialDXState_t& dxState = material->dxStates[0];

    bool useDefaultBlendStates = true;

    if (presetName == "ironsight")
    {
        dxState.depthStencilFlags = DF_DEPTH_WRITE_MASK_ALL | DF_COMPARISON_LESS_EQUAL | DF_DEPTH_ENABLE; // 23
        dxState.rasterizerFlags = RF_CULL_BACK; // 6
    }
    else if (presetName == "epg_mag")
    {
        dxState.depthStencilFlags = DF_COMPARISON_LESS_EQUAL | DF_DEPTH_ENABLE; // 7
        dxState.rasterizerFlags = RF_CULL_BACK; // 6
    }
    else if (presetName == "hair")
    {
        dxState.depthStencilFlags = DF_COMPARISON_LESS_EQUAL | DF_DEPTH_ENABLE; // 7
        dxState.rasterizerFlags = RF_CULL_NONE; // 2
    }
    else if (presetName == "opaque")
    {
        useDefaultBlendStates = false;

        dxState.blendStates[0] = MaterialBlendState_t(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        dxState.blendStates[1] = MaterialBlendState_t(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        dxState.blendStates[2] = MaterialBlendState_t(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        dxState.blendStates[3] = MaterialBlendState_t(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0x0);

        dxState.unk = 4;
    }
    else
    {
        Warning("Unexpected preset name '%s' for material '%s'. Ignoring preset.\n", presetName.c_str(), material->materialAssetPath);
        return;
    }


    // most presets will want to use these blend states, so these are default
    if (useDefaultBlendStates)
    {
        // 0xF0138286
        dxState.blendStates[0] = MaterialBlendState_t(true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        // 0xF0138286
        dxState.blendStates[1] = MaterialBlendState_t(true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        // 0xF0008286
        dxState.blendStates[2] = MaterialBlendState_t(true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, 0xF);
        // 0x00138286
        dxState.blendStates[3] = MaterialBlendState_t(true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0);

        dxState.unk = 5;
    }

    // copy all settings to the second dx state
    material->dxStates[1] = material->dxStates[0];
}

// VERSION 7
void Assets::AddMaterialAsset_v12(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding matl asset '%s'\n", assetPath);

    std::string sAssetPath = std::string(assetPath); // hate this var name, love that it is different for every asset

    size_t textureCount = Material_AddTextures(pak, assetEntries, mapEntry);

    MaterialAsset_t* matlAsset = new MaterialAsset_t{};
    matlAsset->assetVersion = 12; // set asset as a titanfall 2 material
    matlAsset->materialAssetPath = assetPath;

    // header data chunk and generic struct
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(MaterialAssetHeader_v12_t), SF_HEAD, 16);


    // some var declaration
    short externalDependencyCount = 0; // number of dependencies ouside this pak
    size_t textureRefSize = textureCount * 8; // size of the texture guid section.

    // parse json inputs for matl header
    matlAsset->FromJSON(mapEntry);

    // used only for string to guid
    std::string fullAssetPath = "material/" + sAssetPath + "_" + matlAsset->materialTypeStr + ".rpak"; // Make full rpak asset path.
    matlAsset->guid = RTech::StringToGuid(fullAssetPath.c_str()); // Convert full rpak asset path to guid and set it in the material header.
    
    // !!!R2 SPECIFIC!!!
    {
        CPakDataChunk nameChunk = pak->CreateDataChunk(sAssetPath.size() + 1, SF_DEV | SF_CPU, 1);

        sprintf_s(nameChunk.Data(), sAssetPath.length() + 1, "%s", sAssetPath.c_str());

        matlAsset->materialName = nameChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, materialName)));
    }

    if (matlAsset->materialType != _TYPE_LEGACY)
        Error("Material type '%s' is not supported on version 12 (Titanfall 2) assets\n", matlAsset->materialTypeStr.c_str());

    matlAsset->unk = matlAsset->materialTypeStr == "gen" ? 0xFBA63181 : 0x40D33E8F;

    if ((matlAsset->materialTypeStr == "fix" || matlAsset->materialTypeStr == "skn"))
    {
        for (int i = 0; i < 2; ++i)
        {
            MaterialDXState_t& unk = matlAsset->dxStates[i];

            unk.blendStates[0] = MaterialBlendState_t(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            unk.blendStates[1] = MaterialBlendState_t(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            unk.blendStates[2] = MaterialBlendState_t(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            unk.blendStates[3] = MaterialBlendState_t(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0x0);

            matlAsset->dxStates[i].unk = 0x4;
        }
    }
    else
    {
        for (int i = 0; i < 2; ++i)
        {
            MaterialDXState_t& unk = matlAsset->dxStates[i];

            unk.blendStates[0] = MaterialBlendState_t(true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            unk.blendStates[1] = MaterialBlendState_t(true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            unk.blendStates[2] = MaterialBlendState_t(true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, 0xF);
            unk.blendStates[3] = MaterialBlendState_t(true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0x0);

            matlAsset->dxStates[i].unk = 0x5;
        }
    }

    if (JSON_IS_STR(mapEntry, "preset"))
    {
        // get presets for dxstate, derived from existing r2 materials
        Material_SetTitanfall2Preset(matlAsset, JSON_GET_STR(mapEntry, "preset", "none"));
    }

    uint32_t dataBufSize = (textureRefSize * 2) + (matlAsset->surface.length() + 1) + (mapEntry.HasMember("surface2") ? matlAsset->surface2.length() + 1 : 0);

    // asset data
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataBufSize, SF_CPU /*| SF_CLIENT*/, 8);

    char* dataBuf = dataChunk.Data();

    std::vector<PakGuidRefHdr_t> guids{};

    if (mapEntry["textures"].IsArray())
    {
        int textureIdx = 0;
        for (auto& it : mapEntry["textures"].GetArray())
        {
            uint64_t textureGuid = RTech::GetAssetGUIDFromString(it.GetString(), true); // get texture guid

            *(uint64_t*)dataBuf = textureGuid;

            if (textureGuid != 0) // only deal with dependencies if the guid is not 0
            {
                pak->AddGuidDescriptor(&guids, dataChunk.GetPointer((textureIdx * sizeof(uint64_t)))); // register guid for this texture reference

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
    }
    else if (mapEntry["textures"].IsObject())
    {
        for (auto& it : mapEntry["textures"].GetObject())
        {
            uint32_t bindPoint = static_cast<uint32_t>(atoi(it.name.GetString()));

            // this should always be true but might as well check
            assert(bindPoint < textureCount);

            uint64_t textureGuid = RTech::GetAssetGUIDFromString(it.value.GetString(), true); // get texture guid

            reinterpret_cast<uint64_t*>(dataBuf)[bindPoint] = textureGuid;

            if (textureGuid != 0) // only deal with dependencies if the guid is not 0
            {
                pak->AddGuidDescriptor(&guids, dataChunk.GetPointer((bindPoint * sizeof(uint64_t)))); // register guid for this texture reference

                PakAsset_t* txtrAsset = pak->GetAssetByGuid(textureGuid, nullptr);

                if (txtrAsset)
                    txtrAsset->AddRelation(assetEntries->size());
                else
                {
                    externalDependencyCount++; // if the asset doesn't exist in the pak it has to be external, or missing
                    Warning("unable to find texture '%s' (%i) for material '%s' within the local assets\n", it.value.GetString(), bindPoint, assetPath);
                }
            }
        }
        dataBuf += textureRefSize;
    }

    dataBuf += textureRefSize; // [rika]: already calculated, no need to do it again.

    // write the surface name into the buffer
    snprintf(dataBuf, matlAsset->surface.length() + 1, "%s", matlAsset->surface.c_str());

    // write surface2 name into the buffer if used
    if (matlAsset->surface2.length() > 0)
    {
        dataBuf += (matlAsset->surface.length() + 1);
        snprintf(dataBuf, matlAsset->surface2.length() + 1, "%s", matlAsset->surface2.c_str());
    }

    // ===============================
    // fill out the rest of the header
    
    size_t currentDataBufOffset = 0;
    
    matlAsset->textureHandles = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, textureHandles)));
    currentDataBufOffset += textureRefSize;

    matlAsset->streamingTextureHandles = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, streamingTextureHandles)));
    currentDataBufOffset += textureRefSize;


    matlAsset->surfaceProp = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, surfaceProp)));
    currentDataBufOffset += matlAsset->surface.length() + 1;

    if (matlAsset->surface2.length() > 0)
    {
        matlAsset->surfaceProp2 = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, surfaceProp2)));
        currentDataBufOffset += matlAsset->surface2.length() + 1;
    }

    //bool bColpass = false; // is this colpass material? how do you determine this

    // loop thru referenced assets (depth materials, colpass material) note: shaderset isn't inline with these vars in r2, so we set it after
    for (int i = 0; i < 3; ++i)
    {
        uint64_t guid = *((uint64_t*)&matlAsset->depthShadowMaterial + i);

        if (guid != 0)
        {
            pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, depthShadowMaterial) + (i * 8)));

            PakAsset_t* asset = pak->GetAssetByGuid(guid, nullptr, true);

            if (asset)
                asset->AddRelation(assetEntries->size());
            else
                externalDependencyCount++;
        }
    }

    // do colpass here because of how MaterialAsset_t is setup
    if (matlAsset->colpassMaterial != 0)
    {
        pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, colpassMaterial)));

        PakAsset_t* asset = pak->GetAssetByGuid(matlAsset->colpassMaterial, nullptr, true);

        if (asset)
            asset->AddRelation(assetEntries->size());
        else
            externalDependencyCount++;
    }

    // shaderset, see note above
    if (matlAsset->shaderSet != 0)
    {
        pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, shaderSet)));

        PakAsset_t* asset = pak->GetAssetByGuid(matlAsset->shaderSet, nullptr, true);

        if (asset)
            asset->AddRelation(assetEntries->size());
        else
            externalDependencyCount++;
    }

    // write header now that we are done setting it up
    matlAsset->WriteToBuffer(hdrChunk.Data());

    //////////////////////////////////////////
    /// cpu
    std::string cpuPath = pak->GetAssetPath() + JSON_GET_STR(mapEntry, "cpuPath", sAssetPath + "_" + matlAsset->materialTypeStr + ".cpu");

    /* SETUP DX SHADER BUF */
    GenericShaderBuffer genericShaderBuf{};
    
    Material_SetupDXBufferFromJson(&genericShaderBuf, mapEntry);

    MaterialShaderBufferV12 shaderBuf = genericShaderBuf.GenericV12();
    /* SETUP DX SHADER BUF */

    uint64_t dxStaticBufSize = 0;
    CPakDataChunk uberBufChunk;
    if (FILE_EXISTS(cpuPath))
    {
        dxStaticBufSize = Utils::GetFileSize(cpuPath);

        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 8);

        std::ifstream cpuIn(cpuPath, std::ios::in | std::ios::binary);
        cpuIn.read(uberBufChunk.Data() + sizeof(MaterialCPUHeader), dxStaticBufSize);
        cpuIn.close();
    }
    else
    {
        dxStaticBufSize = sizeof(MaterialShaderBufferV12);
        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 8);

        memcpy(uberBufChunk.Data() + sizeof(MaterialCPUHeader), shaderBuf.AsCharPtr(), dxStaticBufSize);
    }

    MaterialCPUHeader* cpuhdr = reinterpret_cast<MaterialCPUHeader*>(uberBufChunk.Data());
    cpuhdr->dataPtr = uberBufChunk.GetPointer(sizeof(MaterialCPUHeader));
    cpuhdr->dataSize = dxStaticBufSize;
    cpuhdr->unk_C = 3; // unsure what this value actually is but it changes

    pak->AddPointer(uberBufChunk.GetPointer(offsetof(MaterialCPUHeader, dataPtr)));

    //////////////////////////////////////////

    PakAsset_t asset;

    asset.InitAsset(matlAsset->guid, hdrChunk.GetPointer(), hdrChunk.GetSize(), uberBufChunk.GetPointer(), -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = 12;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = (asset.dependenciesCount - externalDependencyCount) + 1; // plus one for the asset itself (I think)

    asset.AddGuids(&guids);

    asset.EnsureUnique(assetEntries);
    assetEntries->push_back(asset);

    Log("\n");

    delete matlAsset;
}

// VERSION 8
void Assets::AddMaterialAsset_v15(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding matl asset '%s'\n", assetPath);

    std::string sAssetPath = std::string(assetPath); // hate this var name, love that it is different for every asset

    size_t textureCount = Material_AddTextures(pak, assetEntries, mapEntry);

    MaterialAsset_t* matlAsset = new MaterialAsset_t{};
    matlAsset->assetVersion = 15;
    matlAsset->materialAssetPath = assetPath;

    // header data chunk and generic struct
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(MaterialAssetHeader_v15_t), SF_HEAD, 16);

    // some var declaration
    short externalDependencyCount = 0; // number of dependencies ouside this pak
    size_t textureRefSize = textureCount * 8; // size of the texture guid section.

    // parse json inputs for matl header
    matlAsset->FromJSON(mapEntry);

    // used only for string to guid
    std::string fullAssetPath = "material/" + sAssetPath + "_" + matlAsset->materialTypeStr + ".rpak"; // Make full rpak asset path.
    matlAsset->guid = RTech::StringToGuid(fullAssetPath.c_str()); // Convert full rpak asset path to guid and set it in the material header.

    size_t alignedPathSize = IALIGN4(sAssetPath.length() + 1);
    uint32_t dataBufSize = alignedPathSize + (textureRefSize * 2) + (matlAsset->surface.length() + 1) + (mapEntry.HasMember("surface2") ? matlAsset->surface2.length() + 1 : 0);

    // asset data
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataBufSize, SF_CPU /*| SF_CLIENT*/, 8);

    char* dataBuf = dataChunk.Data();

    // write asset name into the start of the buffer
    snprintf(dataBuf, sAssetPath.length() + 1, "%s", assetPath);
    dataBuf += alignedPathSize;

    std::vector<PakGuidRefHdr_t> guids{};

    if (mapEntry["textures"].IsArray())
    {
        int textureIdx = 0;
        for (auto& it : mapEntry["textures"].GetArray())
        {
            uint64_t textureGuid = RTech::GetAssetGUIDFromString(it.GetString(), true); // get texture guid

            *(uint64_t*)dataBuf = textureGuid;

            if (textureGuid != 0) // only deal with dependencies if the guid is not 0
            {
                pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(alignedPathSize + (textureIdx * sizeof(uint64_t)))); // register guid for this texture reference

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
    }
    else if (mapEntry["textures"].IsObject())
    {
        for (auto& it : mapEntry["textures"].GetObject())
        {
            uint32_t bindPoint = static_cast<uint32_t>(atoi(it.name.GetString()));

            uint64_t textureGuid = RTech::GetAssetGUIDFromString(it.value.GetString(), true); // get texture guid

            reinterpret_cast<uint64_t*>(dataBuf)[bindPoint] = textureGuid;

            if (textureGuid != 0) // only deal with dependencies if the guid is not 0
            {
                pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(alignedPathSize + (bindPoint * sizeof(uint64_t)))); // register guid for this texture reference

                PakAsset_t* txtrAsset = pak->GetAssetByGuid(textureGuid, nullptr);

                if (txtrAsset)
                    txtrAsset->AddRelation(assetEntries->size());
                else
                {
                    externalDependencyCount++; // if the asset doesn't exist in the pak it has to be external, or missing
                    Warning("unable to find texture '%s' (%i) for material '%s' within the local assets\n", it.value.GetString(), bindPoint, assetPath);
                }
            }
        }
        dataBuf += textureRefSize;
    }

    dataBuf += textureRefSize; // [rika]: already calculated, no need to do it again.

    // write the surface name into the buffer
    snprintf(dataBuf, matlAsset->surface.length() + 1, "%s", matlAsset->surface.c_str());

    // write surface2 name into the buffer if used
    if (matlAsset->surface2.length() > 0)
    {
        dataBuf += (matlAsset->surface.length() + 1);
        snprintf(dataBuf, matlAsset->surface2.length() + 1, "%s", matlAsset->surface2.c_str());
    }


    // ===============================
    // fill out the rest of the header

    size_t currentDataBufOffset = 0;

    matlAsset->materialName = dataChunk.GetPointer();
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, materialName)));
    currentDataBufOffset += alignedPathSize;

    matlAsset->textureHandles = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, textureHandles)));
    currentDataBufOffset += textureRefSize;

    matlAsset->streamingTextureHandles = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, streamingTextureHandles)));
    currentDataBufOffset += textureRefSize;


    matlAsset->surfaceProp = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, surfaceProp)));
    currentDataBufOffset += matlAsset->surface.length() + 1;

    if (matlAsset->surface2.length() > 0)
    {
        matlAsset->surfaceProp2 = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, surfaceProp2)));
        currentDataBufOffset += matlAsset->surface2.length() + 1;
    }

    //bool bColpass = false; // is this colpass material? how do you determine this

    // loop thru referenced assets (depth materials, colpass material) note: shaderset isn't inline with these vars in r2, so we set it after
    for (int i = 0; i < 6; ++i)
    {
        uint64_t guid = *((uint64_t*)&matlAsset->depthShadowMaterial + i);

        if (guid != 0)
        {
            pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, depthShadowMaterial) + (i * 8)));

            PakAsset_t* asset = pak->GetAssetByGuid(guid, nullptr, true);

            if (asset)
                asset->AddRelation(assetEntries->size());
            else
                externalDependencyCount++;
        }
    }

    // texan
    if (matlAsset->textureAnimation != 0)
    {
        pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, textureAnimation)));

        PakAsset_t* asset = pak->GetAssetByGuid(matlAsset->colpassMaterial, nullptr, true);

        if (asset)
            asset->AddRelation(assetEntries->size());
        else
            externalDependencyCount++;
    }

    matlAsset->unk = 0x1F5A92BD; // set a quirky little guy

    // write header now that we are done setting it up
    matlAsset->WriteToBuffer(hdrChunk.Data());

    //////////////////////////////////////////
    /// cpu
    uint64_t dxStaticBufSize = 0;

    // temp, should be moved to setting things in material files when those exist
    std::string cpuPath = pak->GetAssetPath() + JSON_GET_STR(mapEntry, "cpuPath", sAssetPath + "_" + matlAsset->materialTypeStr + ".cpu");
    if (mapEntry.HasMember("cpu") && mapEntry["cpu"].IsString())
    {
        cpuPath = pak->GetAssetPath() + mapEntry["cpu"].GetStdString() + ".cpu";
    }

    /* SETUP DX SHADER BUF */
    GenericShaderBuffer genericShaderBuf{};

    Material_SetupDXBufferFromJson(&genericShaderBuf, mapEntry);

    MaterialShaderBufferV15 shaderBuf = genericShaderBuf.GenericV15();
    /* SETUP DX SHADER BUF */

    CPakDataChunk uberBufChunk;
    if (FILE_EXISTS(cpuPath))
    {
        dxStaticBufSize = Utils::GetFileSize(cpuPath);

        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 16);

        std::ifstream cpuIn(cpuPath, std::ios::in | std::ios::binary);
        cpuIn.read(uberBufChunk.Data() + sizeof(MaterialCPUHeader), dxStaticBufSize);
        cpuIn.close();
    }
    else
    {
        dxStaticBufSize = sizeof(MaterialShaderBufferV15);
        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 16);

        memcpy(uberBufChunk.Data() + sizeof(MaterialCPUHeader), shaderBuf.AsCharPtr(), dxStaticBufSize);
    }

    MaterialCPUHeader* cpuhdr = reinterpret_cast<MaterialCPUHeader*>(uberBufChunk.Data());
    cpuhdr->dataPtr = uberBufChunk.GetPointer(sizeof(MaterialCPUHeader));
    cpuhdr->dataSize = dxStaticBufSize;

    pak->AddPointer(uberBufChunk.GetPointer(offsetof(MaterialCPUHeader, dataPtr)));

    //////////////////////////////////////////

    PakAsset_t asset;

    asset.InitAsset(matlAsset->guid, hdrChunk.GetPointer(), hdrChunk.GetSize(), uberBufChunk.GetPointer(), -1, -1, (std::uint32_t)AssetType::MATL);
    asset.SetHeaderPointer(hdrChunk.Data());
    asset.version = 15;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = (asset.dependenciesCount - externalDependencyCount) + 1; // plus one for the asset itself (I think)

    asset.AddGuids(&guids);

    asset.EnsureUnique(assetEntries);
    assetEntries->push_back(asset);

    Log("\n");

    delete matlAsset;
}
