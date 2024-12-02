#include "pch.h"
#include "assets.h"
#include "public/material.h"

#undef GetObject

static void Material_CheckAndAddTexture(CPakFile* const pak, const rapidjson::Value& texture, const bool disableStreaming)
{
    if (!texture.IsString())
        return;

    if (texture.GetStringLength() == 0)
        return;

    // check if texture string is an asset guid (e.g., "0x5DCAT")
    // 
    // todo(amos): we probably need to get rid of this check and allow
    // the user to specify textures by guid, because otherwise its impossible
    // to deduplicate data that is either present in the pak we currently
    // are building, or in one from which we are planning to share a streaming
    // set with. streaming set sharing is planned so this needs to be reworked
    // to eliminate duplicate assets that are in any other paks.
    //
    // ideally we just provide the guid into AddTextureAsset from here, and 
    // slightly modify AddTextureAsset to use the provided guid for lookup
    // rather than generating one from provided asset path.
    if (RTech::ParseGUIDFromString(texture.GetString()))
        return;

    Assets::AddTextureAsset(pak, 0, texture.GetString(), disableStreaming, true);
}

// we need to take better account of textures once asset caching becomes a thing
static void Material_CreateTextures(CPakFile* const pak, const rapidjson::Value& textures, const bool disableStreaming)
{
    for (const auto& texture : textures.GetObject())
        Material_CheckAndAddTexture(pak, texture.value, disableStreaming);
}

static size_t Material_GetHighestTextureBindPoint(const rapidjson::Value& textures)
{
    uint32_t max = 0;

    for (const auto& it : textures.GetObject())
    {
        char* end;
        const uint32_t index = strtoul(it.name.GetString(), &end, 0);

        if (index > max)
            max = index;
    }

    return max;
}

static size_t Material_AddTextures(CPakFile* const pak, const rapidjson::Value& mapEntry, const rapidjson::Value& textures)
{
    const bool disableStreaming = JSON_GetValueOrDefault(mapEntry, "disableStreaming", false);
    Material_CreateTextures(pak, textures, disableStreaming);

    // textureSlotCount determines the total number of texture slots in the assigned shaderset.
    // shaderset has a texture input count variable that is used when looping over the texture array
    // and since we can't modify that from here, we have to rely on the user to set this properly!
    const size_t textureCount = JSON_GetValueOrDefault(mapEntry, "textureSlotCount", 0ull);

    return max(textureCount, Material_GetHighestTextureBindPoint(textures) + 1);
}

static short Material_AddTextureRefs(CPakFile* const pak, CPakDataChunk& dataChunk, char* const dataBuf, std::vector<PakGuidRefHdr_t>& guids,
                                     const rapidjson::Value& textures, const size_t alignedPathSize)
{
    short externalDependencyCount = 0;
    size_t curIndex = 0;

    for (auto it = textures.MemberBegin(); it != textures.MemberEnd(); it++, curIndex++)
    {
        const char* const start = it->name.GetString();
        char* end;
        const uint32_t bindPoint = strtoul(start, &end, 0);

        const rapidjson::Value& val = it->value;

        if (end != &start[it->name.GetStringLength()])
            Error("Unable to determine bind point for texture #%zu\n", bindPoint);

        bool success;
        const PakGuid_t textureGuid = Pak_ParseGuid(val, &success);

        if (!success)
            Error("Unable to parse texture #%zu\n", bindPoint);

        reinterpret_cast<PakGuid_t*>(dataBuf)[bindPoint] = textureGuid;

        if (textureGuid != 0) // only deal with dependencies if the guid is not 0
        {
            pak->AddGuidDescriptor(&guids, dataChunk.GetPointer(alignedPathSize + (bindPoint * sizeof(PakGuid_t)))); // register guid for this texture reference

            PakAsset_t* txtrAsset = pak->GetAssetByGuid(textureGuid, nullptr);

            if (txtrAsset)
                pak->SetCurrentAssetAsDependentForAsset(txtrAsset);
            else
            {
                externalDependencyCount++; // if the asset doesn't exist in the pak it has to be external, or missing
                Warning("Unable to find texture #%zu within the local assets\n", bindPoint);
            }
        }
    }

    return externalDependencyCount;
}

static bool Material_ParseDXStateFlags(const rapidjson::Value& mapEntry, int& blendStateMask, int& depthStencilFlags, int& rasterizerFlags)
{
    JSON_ParseNumberRequired(mapEntry, "blendStateMask", blendStateMask);
    JSON_ParseNumberRequired(mapEntry, "depthStencilFlags", depthStencilFlags);
    JSON_ParseNumberRequired(mapEntry, "rasterizerFlags", rasterizerFlags);

    return true;
}

template <size_t BLEND_STATE_COUNT>
static void Material_ParseBlendStateFlags(const rapidjson::Value& mapEntry, unsigned int blendStates[BLEND_STATE_COUNT])
{
    rapidjson::Document::ConstMemberIterator blendStatesIt;
    JSON_GetRequired(mapEntry, "blendStates", blendStatesIt);

    const rapidjson::Value& blendStateJson = blendStatesIt->value;

    if (!blendStateJson.IsArray())
    {
        Error("Unsupported type for blend state flags (got %s, expected %s)\n",
            JSON_InternalTypeToString(blendStateJson.GetType()), JSON_InternalTypeToString(rapidjson::kArrayType));
    }

    const rapidjson::Value::ConstArray blendStateElems = blendStateJson.GetArray();
    const size_t numBlendStates = blendStateElems.Size();

    if (numBlendStates != BLEND_STATE_COUNT)
    {
        Error("Expected %zu blend state flags, found %zu\n", BLEND_STATE_COUNT, numBlendStates);
    }

    for (size_t i = 0; i < numBlendStates; i++)
    {
        const rapidjson::Value& obj = blendStateElems[i];
        uint32_t blendState;

        if (!JSON_ParseNumber(obj, blendState))
            Error("Failed parsing blend state flag #%zu\n", i);

        blendStates[i] = blendState;
    }
}

template <typename MaterialDXState_t>
static void Material_SetDXStates(const rapidjson::Value& mapEntry, MaterialDXState_t dxStates[MAT_DX_STATE_COUNT])
{
    int blendStateMask, depthStencilFlags, rasterizerFlags;
    Material_ParseDXStateFlags(mapEntry, blendStateMask, depthStencilFlags, rasterizerFlags);

    static const int totalBlendStateCount = ARRAYSIZE(dxStates[0].blendStates);

    unsigned int blendStateMap[totalBlendStateCount];
    Material_ParseBlendStateFlags<totalBlendStateCount>(mapEntry, blendStateMap);

    for (int i = 0; i < MAT_DX_STATE_COUNT; i++)
    {
        MaterialDXState_t& dxState = dxStates[i];

        dxState.blendStateMask = blendStateMask;
        dxState.depthStencilFlags = (uint16_t)depthStencilFlags;
        dxState.rasterizerFlags = (uint16_t)rasterizerFlags;

        for (int j = 0; j < ARRAYSIZE(dxState.blendStates); j++)
        {
            MaterialBlendState_t& blendState = dxState.blendStates[j];
            blendState = MaterialBlendState_t(blendStateMap[j]);
        }
    }
}

enum MaterialDepthType_e
{
    Shadow,
    Prepass,
    VSM,
    ShadowTight,
};

static PakGuid_t Material_DetermineDefaultDepthMaterial(const MaterialDepthType_e depthType, const MaterialShaderType_e shaderType, const int rasterizerFlags)
{
    switch (depthType)
    {
    case Shadow:
        return RTech::StringToGuid(Utils::VFormat("material/code_private/depth_shadow%s_%s.rpak", 
            ((rasterizerFlags & 0x4) && !(rasterizerFlags & 0x2)) ? "_frontculled" : "", s_materialShaderTypeNames[shaderType]).c_str());
    case Prepass:
        return RTech::StringToGuid(Utils::VFormat("material/code_private/depth_prepass%s_%s.rpak", 
            ((rasterizerFlags & 0x2) && !(rasterizerFlags & 0x4)) ? "_twosided" : "", s_materialShaderTypeNames[shaderType]).c_str());
    case VSM:
        return RTech::StringToGuid(Utils::VFormat("material/code_private/depth_vsm_%s.rpak", s_materialShaderTypeNames[shaderType]).c_str());
    case ShadowTight:
        return RTech::StringToGuid(Utils::VFormat("material/code_private/depth_shadow_tight_%s.rpak", s_materialShaderTypeNames[shaderType]).c_str());
    default:
        // code bug
        assert(0);
        return 0;
    }
}

void MaterialAsset_t::SetupDepthMaterials(const rapidjson::Value& mapEntry)
{
    if (this->materialTypeStr == "wld")
        Warning("WLD materials do not have generic depth materials, make sure that you have set them to 0 or have created your own.\n");

    bool success;

    depthShadowMaterial = Pak_ParseGuid(mapEntry, "$depthShadowMaterial", &success);
    if (!success)
        depthShadowMaterial = Material_DetermineDefaultDepthMaterial(MaterialDepthType_e::Shadow, materialType, dxStates[0].rasterizerFlags);

    depthPrepassMaterial = Pak_ParseGuid(mapEntry, "$depthPrepassMaterial", &success);
    if (!success)
        depthPrepassMaterial = Material_DetermineDefaultDepthMaterial(MaterialDepthType_e::Prepass, materialType, dxStates[0].rasterizerFlags);

    depthVSMMaterial = Pak_ParseGuid(mapEntry, "$depthVSMMaterial", &success);
    if (!success)
        depthVSMMaterial = Material_DetermineDefaultDepthMaterial(MaterialDepthType_e::VSM, materialType, dxStates[0].rasterizerFlags);

    depthShadowTightMaterial = Pak_ParseGuid(mapEntry, "$depthShadowTightMaterial", &success);
    if (!success)
        depthShadowTightMaterial = Material_DetermineDefaultDepthMaterial(MaterialDepthType_e::ShadowTight, materialType, dxStates[0].rasterizerFlags);
}

static inline void CheckCountOrError(const rapidjson::Value::ConstArray& elements, const size_t expectedCount, const char* const fieldName)
{
    const size_t elemCount = elements.Size();

    if (elemCount != expectedCount)
        Error("Expected %zu element for field '%s', found %zu\n", expectedCount, fieldName, elemCount);
}

/*
* note(amos): commented for now until we can parse shaders and set all fields
*             correctly. currently, when we fall back to this when a cpu asset
*             hasn't been found the material will always end up looking incorrect.
static inline void Material_SetTintOverrides(const rapidjson::Value& mapEntry, const char* const fieldName, float tintVars[3])
{
    rapidjson::Value::ConstMemberIterator it;

    if (JSON_GetIterator(mapEntry, fieldName, JSONFieldType_e::kArray, it))
    {
        const rapidjson::Value::ConstArray elements = it->value.GetArray();
        CheckCountOrError(elements, 3, fieldName);

        for (int i = 0; i < 3; i++)
            tintVars[i] = elements[i].GetFloat();
    }
}

static inline void Material_SetUVOverrides(const rapidjson::Value& mapEntry, const char* const fieldName, uvTransform_t& transform)
{
    rapidjson::Value::ConstMemberIterator it;

    if (JSON_GetIterator(mapEntry, fieldName, JSONFieldType_e::kArray, it))
    {
        const rapidjson::Value::ConstArray elements = it->value.GetArray();
        CheckCountOrError(elements, 6, fieldName);

        for (int i = 0; i < 6; i++)
            *transform.pFloat(i) = elements[i].GetFloat();
    }
}

// shader parsing eventually
static void Material_SetupDXBufferFromJson(GenericShaderBuffer* shaderBuf, const rapidjson::Value& mapEntry)
{
    float layerBlendRamp;

    if (JSON_GetValue(mapEntry, "layerBlendRamp", layerBlendRamp))
        shaderBuf->c_layerBlendRamp = layerBlendRamp;

    float opacity;

    if (JSON_GetValue(mapEntry, "opacity", opacity))
        shaderBuf->c_opacity = opacity;

    Material_SetTintOverrides(mapEntry, "emissiveTint", shaderBuf->c_L0_emissiveTint);
    Material_SetTintOverrides(mapEntry, "albedoTint", shaderBuf->c_L0_albedoTint);
    Material_SetTintOverrides(mapEntry, "perfSpecColor", shaderBuf->c_L0_perfSpecColor);

    Material_SetUVOverrides(mapEntry, "uv1", shaderBuf->c_uv1);
    Material_SetUVOverrides(mapEntry, "uv2", shaderBuf->c_uv2);
    Material_SetUVOverrides(mapEntry, "uv3", shaderBuf->c_uv3);
    Material_SetUVOverrides(mapEntry, "uv4", shaderBuf->c_uv4);
    Material_SetUVOverrides(mapEntry, "uv5", shaderBuf->c_uv5);
}
*/


static std::string Material_GetCpuPath(CPakFile* const pak, MaterialAsset_t* const matlAsset, const rapidjson::Value& mapEntry)
{
    const char* path; // is user hasn't specified a cpu path, load the one from the material path.
    const bool hasPath = JSON_GetValue(mapEntry, "$cpu", path);

    return Utils::VFormat("%s%s.cpu_raw", pak->GetAssetPath().c_str(), hasPath ? path : matlAsset->materialAssetPath);
}

template <typename MaterialShaderBuffer_t>
static void Material_AddCpuData(CPakFile* const pak, MaterialAsset_t* const matlAsset, const rapidjson::Value& mapEntry,
    CPakDataChunk& uberBufChunk, size_t& staticBufSize)
{
    const std::string cpuPath = Material_GetCpuPath(pak, matlAsset, mapEntry);
    BinaryIO cpuFile;

    if (cpuFile.Open(cpuPath, BinaryIO::Mode_e::Read))
    {
        staticBufSize = cpuFile.GetSize();
        uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + staticBufSize, SF_CPU | SF_TEMP, 8);

        cpuFile.Read(uberBufChunk.Data() + sizeof(MaterialCPUHeader), staticBufSize);
    }
    else
    {
        Error("Failed to open cpu asset '%s'\n", cpuPath.c_str());

        // todo(amos): do we want this? without the cpu, the material will always look incorrect/dark
        // when we fall back here. disabled the code for now in favor of an error as this prevents a
        // ton of confusion and questions from users when the material ends up looking incorrect.
        // 
        // Warning("Failed to open cpu asset '%s'; using generic buffer\n", cpuPath.c_str());
        // 
        //staticBufSize = sizeof(MaterialShaderBuffer_t);
        //uberBufChunk = pak->CreateDataChunk(sizeof(MaterialCPUHeader) + staticBufSize, SF_CPU | SF_TEMP, 8);

        ///* SETUP DX SHADER BUF */
        //GenericShaderBuffer genericShaderBuf{};
        //Material_SetupDXBufferFromJson(&genericShaderBuf, mapEntry);

        //MaterialShaderBuffer_t shaderBuf;
        //genericShaderBuf.Generic(shaderBuf);
        ///* SETUP DX SHADER BUF */

        //memcpy(uberBufChunk.Data() + sizeof(MaterialCPUHeader), shaderBuf.AsCharPtr(), staticBufSize);
    }
}

void MaterialAsset_t::FromJSON(const rapidjson::Value& mapEntry)
{
    this->materialTypeStr = JSON_GetValueRequired<const char*>(mapEntry, "shaderType");
    this->materialType = Material_ShaderTypeFromString(this->materialTypeStr);

    // material max dimensions
    this->width = (short)JSON_GetNumberRequired<int>(mapEntry, "width"); // Set material width.
    this->height = (short)JSON_GetNumberRequired<int>(mapEntry, "height"); // Set material height.

    // base material glue flags. defaults are flags used on most materials.
    this->flags = JSON_GetNumberRequired<uint32_t>(mapEntry, "glueFlags");
    this->flags2 = JSON_GetNumberRequired<uint32_t>(mapEntry, "glueFlags2");

    // surfaces are defined in scripts/surfaceproperties.txt or scripts/surfaceproperties.rson
    this->surface = JSON_GetValueRequired<const char*>(mapEntry, "surfaceProp");

    // used for blend materials and the like
    this->surface2 = JSON_GetValueRequired<const char*>(mapEntry, "surfaceProp2");

    // Set samplers properly. Responsible for texture stretching, tiling etc.
    const uint32_t nSamplers = JSON_GetNumberRequired<uint32_t>(mapEntry, "samplers");
    memcpy(this->samplers, &nSamplers, sizeof(nSamplers));

    Material_SetDXStates(mapEntry, dxStates);
    this->SetupDepthMaterials(mapEntry);

    // get referenced colpass material, can be 0 as many materials don't have them
    this->colpassMaterial = Pak_ParseGuid(mapEntry, "$colpassMaterial");

    this->shaderSet = Pak_ParseGuidRequired(mapEntry, "shaderSet");
    this->textureAnimation = Pak_ParseGuid(mapEntry, "$textureAnimation");
}

void Material_SetTitanfall2Preset(MaterialAsset_t* material, const std::string& presetName)
{
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

        dxState.blendStateMask = 4;
    }
    else
    {
        Error("Unexpected preset name '%s' for material '%s'.\n", presetName.c_str(), material->materialAssetPath);
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

        dxState.blendStateMask = 5;
    }

    // copy all settings to the second dx state
    material->dxStates[1] = material->dxStates[0];
}

// VERSION 7
void Assets::AddMaterialAsset_v12(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    Log("Adding matl asset '%s'\n", assetPath);

    std::string sAssetPath = std::string(assetPath); // hate this var name, love that it is different for every asset

    rapidjson::Value::ConstMemberIterator texturesIt;
    const bool hasTextures = JSON_GetIterator(mapEntry, "textures", JSONFieldType_e::kObject, texturesIt);

    const size_t textureCount = hasTextures
        ? Material_AddTextures(pak, mapEntry, texturesIt->value)
        : 0; // note: no error as materials without textures do exist.
             // typically, these are prepass/vsm/etc materials.

    MaterialAsset_t* matlAsset = new MaterialAsset_t{};
    matlAsset->assetVersion = 12; // set asset as a titanfall 2 material
    matlAsset->materialAssetPath = JSON_GetValueRequired<const char*>(mapEntry, "name");

    // header data chunk and generic struct
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(MaterialAssetHeader_v12_t), SF_HEAD, 16);


    // some var declaration
    short externalDependencyCount = 0; // number of dependencies ouside this pak
    size_t textureRefSize = textureCount * 8; // size of the texture guid section.

    // parse json inputs for matl header
    matlAsset->FromJSON(mapEntry);
    matlAsset->guid = Pak_GetGuidOverridable(mapEntry, assetPath);
    
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

    const size_t surfaceProp1Size = !matlAsset->surface.empty() ? (matlAsset->surface.length() + 1) : 0;
    const size_t surfaceProp2Size = !matlAsset->surface2.empty() ? (matlAsset->surface2.length() + 1) : 0;

    const size_t dataBufSize = (textureRefSize * 2) + surfaceProp1Size + surfaceProp2Size;

    const char* presetValue;

    if (JSON_GetValue(mapEntry, "$preset", presetValue))
    {
        // get presets for dxstate, derived from existing r2 materials
        Material_SetTitanfall2Preset(matlAsset, presetValue);
    }

    // asset data
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataBufSize, SF_CPU /*| SF_CLIENT*/, 8);

    char* dataBuf = dataChunk.Data();

    std::vector<PakGuidRefHdr_t> guids;

    if (hasTextures)
    {
        externalDependencyCount += Material_AddTextureRefs(pak, dataChunk, dataBuf, guids, texturesIt->value, 0);
        dataBuf += textureRefSize;
    }

    dataBuf += textureRefSize; // [rika]: already calculated, no need to do it again.
                               // [amos]: this offset is necessary for the streaming
                               //         texture handles. we should look into only
                               //         writing this out if we have streaming
                               //         textures and only up to the count thereof.

    // write the surface names into the buffer
    if (surfaceProp1Size)
    {
        snprintf(dataBuf, surfaceProp1Size, "%s", matlAsset->surface.c_str());
        dataBuf += surfaceProp1Size;
    }

    if (surfaceProp2Size)
    {
        snprintf(dataBuf, surfaceProp2Size, "%s", matlAsset->surface2.c_str());
        dataBuf += surfaceProp2Size;
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

    if (surfaceProp1Size)
    {
        matlAsset->surfaceProp = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, surfaceProp)));
        currentDataBufOffset += surfaceProp1Size;
    }

    if (surfaceProp2Size)
    {
        matlAsset->surfaceProp2 = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, surfaceProp2)));
        currentDataBufOffset += surfaceProp2Size;
    }

    //bool bColpass = false; // is this colpass material? how do you determine this

    // loop thru referenced assets (depth materials, colpass material) note: shaderset isn't inline with these vars in r2, so we set it after
    for (int i = 0; i < 3; ++i)
    {
        const PakGuid_t guid = *((PakGuid_t*)&matlAsset->depthShadowMaterial + i);

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
    size_t dxStaticBufSize = 0;
    CPakDataChunk uberBufChunk;

    Material_AddCpuData<MaterialShaderBufferV12>(pak, matlAsset, mapEntry, uberBufChunk, dxStaticBufSize);

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
void Assets::AddMaterialAsset_v15(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    Log("Adding matl asset '%s'\n", assetPath);

    std::string sAssetPath = std::string(assetPath); // hate this var name, love that it is different for every asset

    rapidjson::Value::ConstMemberIterator texturesIt;
    const bool hasTextures = JSON_GetIterator(mapEntry, "$textures", JSONFieldType_e::kObject, texturesIt);

    const size_t textureCount = hasTextures
        ? Material_AddTextures(pak, mapEntry, texturesIt->value)
        : 0; // note: no error as materials without textures do exist.
             // typically, these are prepass/vsm/etc materials.

    MaterialAsset_t* matlAsset = new MaterialAsset_t{};
    matlAsset->assetVersion = 15;
    matlAsset->materialAssetPath = JSON_GetValueRequired<const char*>(mapEntry, "name");

    // header data chunk and generic struct
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(MaterialAssetHeader_v15_t), SF_HEAD, 16);

    // some var declaration
    short externalDependencyCount = 0; // number of dependencies ouside this pak
    size_t textureRefSize = textureCount * 8; // size of the texture guid section.

    // parse json inputs for matl header
    matlAsset->FromJSON(mapEntry);
    matlAsset->guid = Pak_GetGuidOverridable(mapEntry, assetPath);

    const size_t surfaceProp1Size = !matlAsset->surface.empty() ? (matlAsset->surface.length() + 1) : 0;
    const size_t surfaceProp2Size = !matlAsset->surface2.empty() ? (matlAsset->surface2.length() + 1) : 0;

    const size_t alignedPathSize = IALIGN4(sAssetPath.length() + 1);
    const size_t dataBufSize = alignedPathSize + (textureRefSize * 2) + surfaceProp1Size + surfaceProp2Size;

    // asset data
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataBufSize, SF_CPU /*| SF_CLIENT*/, 8);

    char* dataBuf = dataChunk.Data();

    // write asset name into the start of the buffer
    snprintf(dataBuf, sAssetPath.length() + 1, "%s", assetPath);
    dataBuf += alignedPathSize;

    std::vector<PakGuidRefHdr_t> guids;

    if (hasTextures)
    {
        externalDependencyCount += Material_AddTextureRefs(pak, dataChunk, dataBuf, guids, texturesIt->value, alignedPathSize);
        dataBuf += textureRefSize;
    }

    dataBuf += textureRefSize; // [rika]: already calculated, no need to do it again.
                               // [amos]: this offset is necessary for the streaming
                               //         texture handles. we should look into only
                               //         writing this out if we have streaming
                               //         textures and only up to the count thereof.

    // write the surface names into the buffer
    if (surfaceProp1Size)
    {
        snprintf(dataBuf, surfaceProp1Size, "%s", matlAsset->surface.c_str());
        dataBuf += surfaceProp1Size;
    }

    if (surfaceProp2Size)
    {
        snprintf(dataBuf, surfaceProp2Size, "%s", matlAsset->surface2.c_str());
        dataBuf += surfaceProp2Size;
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

    if (surfaceProp1Size)
    {
        matlAsset->surfaceProp = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, surfaceProp)));
        currentDataBufOffset += surfaceProp1Size;
    }

    if (surfaceProp2Size)
    {
        matlAsset->surfaceProp2 = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, surfaceProp2)));
        currentDataBufOffset += surfaceProp2Size;
    }

    //bool bColpass = false; // is this colpass material? how do you determine this

    // loop thru referenced assets (depth materials, colpass material) note: shaderset isn't inline with these vars in r2, so we set it after
    for (int i = 0; i < 6; ++i)
    {
        const PakGuid_t guid = *((PakGuid_t*)&matlAsset->depthShadowMaterial + i);

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

    // todo(amos): override this too?
    matlAsset->unk = 0x1F5A92BD; // set a quirky little guy

    // write header now that we are done setting it up
    matlAsset->WriteToBuffer(hdrChunk.Data());

    //////////////////////////////////////////
    /// cpu
    size_t dxStaticBufSize = 0;
    CPakDataChunk uberBufChunk;

    Material_AddCpuData<MaterialShaderBufferV15>(pak, matlAsset, mapEntry, uberBufChunk, dxStaticBufSize);

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
