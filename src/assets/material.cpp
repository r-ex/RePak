#include "pch.h"
#include "assets.h"
#include "public/material.h"

#undef GetObject
// we need to take better account of textures once asset caching becomes a thing
void Material_CreateTextures(CPakFile* pak, rapidjson::Value& mapEntry)
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

            Assets::AddTextureAsset(pak, 0, it.GetString(), JSON_GET_BOOL(mapEntry, "disableStreaming"), true);
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

            Assets::AddTextureAsset(pak, 0, it.value.GetString(), JSON_GET_BOOL(mapEntry, "disableStreaming"), true);
        }
    }

}

#define DEFAULT_UNK_FLAGS 0x4
#define DEFAULT_DEPTH_STENCIL_FLAGS 0x17
#define DEFAULT_RASTERIZER_FLAGS 0x6

static bool ParseDXStateFlags(const rapidjson::Value& mapEntry, int& unkFlags, int& depthStencilFlags, int& rasterizerFlags)
{
    // dx flags
    // !!!temp!!! - these should be replaced by proper flag string parsing when possible
    unkFlags = DEFAULT_UNK_FLAGS;
    depthStencilFlags = DEFAULT_DEPTH_STENCIL_FLAGS;
    rasterizerFlags = DEFAULT_RASTERIZER_FLAGS; // CULL_BACK

    JSON_GetValue(mapEntry, "unkFlags", JSONFieldType_e::kNumber, unkFlags);
    JSON_GetValue(mapEntry, "depthStencilFlags", JSONFieldType_e::kNumber, depthStencilFlags);
    JSON_GetValue(mapEntry, "rasterizerFlags", JSONFieldType_e::kNumber, rasterizerFlags);

    return true;
}

template <size_t BLEND_STATE_COUNT>
static bool ParseBlendStateFlags(const rapidjson::Value& mapEntry, unsigned int blendStates[BLEND_STATE_COUNT])
{
    rapidjson::Document::ConstMemberIterator blendStatesIt;
    const bool hasField = JSON_GetIterator(mapEntry, "blendStates", blendStatesIt);

    if (!hasField)
        return false;

    const rapidjson::Value& blendStateJson = blendStatesIt->value;

    if (blendStateJson.IsArray())
    {
        const rapidjson::Value::ConstArray blendStateElems = blendStateJson.GetArray();
        const int numBlendStates = static_cast<int>(blendStateElems.Size());

        if (numBlendStates != BLEND_STATE_COUNT)
        {
            Error("Expected %i blend state flags, found %i\n", BLEND_STATE_COUNT, numBlendStates);
            return false;
        }

        for (int i = 0; i < numBlendStates; i++)
        {
            const rapidjson::Value& obj = blendStateElems[i];
            const rapidjson::Type type = obj.GetType();

            if (type == rapidjson::kNumberType)
                blendStates[i] = obj.GetUint();
            else if (type == rapidjson::kStringType)
            {
                const char* const startPtr = obj.GetString();
                char* endPtr;

                const unsigned int result = strtoul(startPtr, &endPtr, 16);

                if (endPtr != &startPtr[obj.GetStringLength()])
                {
                    Error("Failed parsing blend state flag #%i (%s)\n", (int)i, startPtr);
                    return false;
                }

                blendStates[i] = result;
            }
            else
            {
                Error("Unsupported type for blend state flag #i (got %s, expected %s)\n", (int)i,
                    JSON_TypeToString(type), JSON_TypeToString(rapidjson::kStringType));

                return false;
            }
        }
    }
    else if (blendStateJson.IsString()) // Split the flags from provided string.
    {
        const std::vector<std::string> blendStateElems = Utils::StringSplit(blendStateJson.GetString(), ' ');
        const int numBlendStates = static_cast<int>(blendStateElems.size());

        if (numBlendStates != BLEND_STATE_COUNT)
        {
            Error("Expected %i blend state flags, found %i\n", BLEND_STATE_COUNT, numBlendStates);
            return false;
        }

        for (int i = 0; i < BLEND_STATE_COUNT; i++)
        {
            const std::string& val = blendStateElems[i];

            const char* const startPtr = val.c_str();
            char* endPtr;

            const unsigned int result = strtoul(startPtr, &endPtr, 16);

            if (endPtr != &startPtr[val.length()])
            {
                Error("Failed parsing blend state flag #%i (%s)\n", i, startPtr);
                return false;
            }
            else
                blendStates[i] = result;
        }
    }
    else
    {
        Error("Unsupported type for blend state flags (got %s, expected %s or %s)\n",
            JSON_TypeToString(blendStateJson.GetType()), JSON_TypeToString(rapidjson::kArrayType),
            JSON_TypeToString(rapidjson::kStringType));

        return false;
    }

    return true;
}

template <typename MaterialDXState_t>
static void SetDXStates(const rapidjson::Value& mapEntry, MaterialDXState_t dxStates[MAT_DX_STATE_COUNT])
{
    int unkFlags, depthStencilFlags, rasterizerFlags;
    ParseDXStateFlags(mapEntry, unkFlags, depthStencilFlags, rasterizerFlags);

    static const int totalBlendStateCount = ARRAYSIZE(dxStates[0].blendStates);

    unsigned int blendStateMap[totalBlendStateCount];
    const bool hasBlendStates = ParseBlendStateFlags<totalBlendStateCount>(mapEntry, blendStateMap);

    for (int i = 0; i < MAT_DX_STATE_COUNT; i++)
    {
        MaterialDXState_t& dxState = dxStates[i];

        dxState.unk = unkFlags;
        dxState.depthStencilFlags = (uint16_t)depthStencilFlags;
        dxState.rasterizerFlags = (uint16_t)rasterizerFlags;

        for (int j = 0; j < ARRAYSIZE(dxState.blendStates); j++)
        {
            MaterialBlendState_t& blendState = dxState.blendStates[j];

            if (!hasBlendStates) // Default it off.
            {
                // todo(amos): is there a reason we default to setting all
                // bits for renderTargetWriteMask ?
                blendState = MaterialBlendState_t(0xF0000000);
                continue;
            }

            blendState = MaterialBlendState_t(blendStateMap[j]);
        }
    }
}

void MaterialAsset_t::SetupDepthMaterialOverrides(const rapidjson::Value& mapEntry)
{
    std::string depthPath;

    if (JSON_IS_NULL(mapEntry, "depthShadowMaterial"))
    {
        this->depthShadowMaterial = NULL;
    }
    else if (JSON_IS_STR(mapEntry, "depthShadowMaterial"))
    {
        depthPath = "material/" + mapEntry["depthShadowMaterial"].GetStdString() + "_" + this->materialTypeStr + ".rpak";
        this->depthShadowMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());
    }

    if (JSON_IS_NULL(mapEntry, "depthPrepassMaterial"))
    {
        this->depthPrepassMaterial = NULL;
    }
    else if (JSON_IS_STR(mapEntry, "depthPrepassMaterial"))
    {
        depthPath = "material/" + mapEntry["depthPrepassMaterial"].GetStdString() + "_" + this->materialTypeStr + ".rpak";
        this->depthPrepassMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());
    }

    if (JSON_IS_NULL(mapEntry, "depthVSMMaterial"))
    {
        this->depthVSMMaterial = NULL;
    }
    else if (JSON_IS_STR(mapEntry, "depthVSMMaterial"))
    {
        depthPath = "material/" + mapEntry["depthVSMMaterial"].GetStdString() + "_" + this->materialTypeStr + ".rpak";
        this->depthVSMMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());
    }

    if (JSON_IS_NULL(mapEntry, "depthShadowTightMaterial"))
    {
        this->depthShadowTightMaterial = NULL;
    }
    else if (JSON_IS_STR(mapEntry, "depthShadowTightMaterial"))
    {
        depthPath = "material/" + mapEntry["depthShadowTightMaterial"].GetStdString() + "_" + this->materialTypeStr + ".rpak";
        this->depthShadowTightMaterial = RTech::GetAssetGUIDFromString(depthPath.c_str());
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
    this->width = (short)JSON_GET_INT(mapEntry, "width", 0); // Set material width.
    this->height = (short)JSON_GET_INT(mapEntry, "height", 0); // Set material height.

    // temp samplers !!!
    // this will be done a bit better when material files become real
    // for now this is the only real way of doing it so meh
    uint32_t nSamplers = 0x1D0300;

    if (JSON_IS_STR(mapEntry, "samplers")) // Set samplers properly. Responsible for texture stretching, tiling etc.
        nSamplers = strtoul(("0x" + mapEntry["samplers"].GetStdString()).c_str(), NULL, 0);

    memcpy(this->samplers, &nSamplers, sizeof(nSamplers));

    // more tradition vmt like flags
    if (JSON_IS_STR(mapEntry, "flags2")) // This does a lot of very important stuff.
        this->flags2 = strtoull(("0x" + mapEntry["flags2"].GetStdString()).c_str(), NULL, 0);
    else
        this->flags2 = (0x56000020 | 0x10000000000000); // beeg flag is used on most things

    SetDXStates(mapEntry, dxStates);

    // surfaces are defined in scripts/surfaceproperties.txt or scripts/surfaceproperties.rson
    this->surface = JSON_GET_STR(mapEntry, "surface", "default");

    // used for blend materials and the like
    this->surface2 = JSON_GET_STR(mapEntry, "surface2", "");

    if (this->materialTypeStr == "wld")
        Warning("WLD materials do not have generic depth materials. Make sure that you have set them to null or have created your own.\n");

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

    this->SetupDepthMaterialOverrides(mapEntry);


    // get referenced colpass material if exists
    if (JSON_IS_STR(mapEntry, "colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString()+ "_" + this->materialTypeStr + ".rpak"; // auto add type? remove if disagree. materials never have their types in names so I don't think this should be expected?
        this->colpassMaterial = RTech::StringToGuid(colpassPath.c_str());
    }


    // optional shaderset override
    if (JSON_IS_STR(mapEntry, "shaderset"))
        this->shaderSet = RTech::GetAssetGUIDFromString(mapEntry["shaderset"].GetString(), true);

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

size_t Material_AddTextures(CPakFile* pak, rapidjson::Value& mapEntry)
{
    Material_CreateTextures(pak, mapEntry);

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

    MaterialDXState_v15_t& dxState = material->dxStates[0];

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

        dxState.blendStates[0] = MaterialBlendState_t(false, false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        dxState.blendStates[1] = MaterialBlendState_t(false, false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        dxState.blendStates[2] = MaterialBlendState_t(false, false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        dxState.blendStates[3] = MaterialBlendState_t(false, false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0x0);

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
        dxState.blendStates[0] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        // 0xF0138286
        dxState.blendStates[1] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
        // 0xF0008286
        dxState.blendStates[2] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, 0xF);
        // 0x00138286
        dxState.blendStates[3] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0);

        dxState.unk = 5;
    }

    // copy all settings to the second dx state
    material->dxStates[1] = material->dxStates[0];
}

// VERSION 7
void Assets::AddMaterialAsset_v12(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding matl asset '%s'\n", assetPath);

    std::string sAssetPath = std::string(assetPath); // hate this var name, love that it is different for every asset

    size_t textureCount = Material_AddTextures(pak, mapEntry);

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
            MaterialDXState_v15_t& dxState = matlAsset->dxStates[i];

            dxState.blendStates[0] = MaterialBlendState_t(false, false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[1] = MaterialBlendState_t(false, false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[2] = MaterialBlendState_t(false, false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[3] = MaterialBlendState_t(false, false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0x0);
        }
    }
    else
    {
        for (int i = 0; i < 2; ++i)
        {
            MaterialDXState_v15_t& dxState = matlAsset->dxStates[i];

            dxState.blendStates[0] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[1] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[2] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[3] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0x0);
        }
    }

    const size_t dataBufSize = (textureRefSize * 2) + (matlAsset->surface.length() + 1) + (mapEntry.HasMember("surface2") ? matlAsset->surface2.length() + 1 : 0);

    if (JSON_IS_STR(mapEntry, "preset"))
    {
        // get presets for dxstate, derived from existing r2 materials
        Material_SetTitanfall2Preset(matlAsset, JSON_GET_STR(mapEntry, "preset", "none"));
    }

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
                    pak->SetCurrentAssetAsDependentForAsset(txtrAsset);
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
                    pak->SetCurrentAssetAsDependentForAsset(txtrAsset);
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
                pak->SetCurrentAssetAsDependentForAsset(asset);
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
            pak->SetCurrentAssetAsDependentForAsset(asset);
        else
            externalDependencyCount++;
    }

    // shaderset, see note above
    if (matlAsset->shaderSet != 0)
    {
        pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, shaderSet)));

        PakAsset_t* asset = pak->GetAssetByGuid(matlAsset->shaderSet, nullptr, true);

        if (asset)
            pak->SetCurrentAssetAsDependentForAsset(asset);
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
    cpuhdr->dataSize = (uint32_t)dxStaticBufSize;
    cpuhdr->unk_C = 3; // unsure what this value actually is but it changes

    pak->AddPointer(uberBufChunk.GetPointer(offsetof(MaterialCPUHeader, dataPtr)));

    //////////////////////////////////////////

    PakAsset_t asset;

    asset.InitAsset(matlAsset->guid, hdrChunk.GetPointer(), hdrChunk.GetSize(), uberBufChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::MATL);
    asset.version = 12;

    asset.pageEnd = pak->GetNumPages();

    asset.remainingDependencyCount = static_cast<short>((guids.size() - externalDependencyCount) + 1); // plus one for the asset itself (I think)
  
    asset.AddGuids(&guids);

    pak->PushAsset(asset);

    Log("\n");

    delete matlAsset;
}

// VERSION 8
void Assets::AddMaterialAsset_v15(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding matl asset '%s'\n", assetPath);

    std::string sAssetPath = std::string(assetPath); // hate this var name, love that it is different for every asset

    size_t textureCount = Material_AddTextures(pak, mapEntry);

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

    const size_t alignedPathSize = IALIGN4(sAssetPath.length() + 1);
    const size_t dataBufSize = alignedPathSize + (textureRefSize * 2) + (matlAsset->surface.length() + 1) + (mapEntry.HasMember("surface2") ? matlAsset->surface2.length() + 1 : 0);

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
                    pak->SetCurrentAssetAsDependentForAsset(txtrAsset);
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
                    pak->SetCurrentAssetAsDependentForAsset(txtrAsset);
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
                pak->SetCurrentAssetAsDependentForAsset(asset);
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
            pak->SetCurrentAssetAsDependentForAsset(asset);
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

    // also bad temp
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
    cpuhdr->dataSize = (uint32_t)dxStaticBufSize;

    pak->AddPointer(uberBufChunk.GetPointer(offsetof(MaterialCPUHeader, dataPtr)));

    //////////////////////////////////////////

    PakAsset_t asset;


    asset.InitAsset(matlAsset->guid, hdrChunk.GetPointer(), hdrChunk.GetSize(), uberBufChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::MATL);
    asset.SetHeaderPointer(hdrChunk.Data());
    asset.version = 15;

    asset.pageEnd = pak->GetNumPages();
    //asset.remainingDependencyCount = static_cast<short>((guids.size() - externalDependencyCount) + 1); // plus one for the asset itself (I think)

    // HACKHACK: i don't really understand what the value of this needs to be, so i have set this back to a (very) incorrect value that at least doesn't crash
    asset.remainingDependencyCount = static_cast<short>((0i16 - externalDependencyCount) + 1);

    asset.AddGuids(&guids);

    pak->PushAsset(asset);

    Log("\n");

    delete matlAsset;
}
