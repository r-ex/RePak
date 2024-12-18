#include "pch.h"
#include "assets.h"
#include "public/material.h"

#undef GetObject

extern bool Texture_AutoAddTexture(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const bool forceDisableStreaming);

// returns true if a texture was added to the pak, false if we just use a guid ref
static bool Material_CheckAndAddTexture(CPakFile* const pak, const rapidjson::Value& texture, const int index, const bool disableStreaming)
{
    // check if texture string is an asset guid (e.g., "0x5DCAT")
    // if it is then we don't add it here as its a reference.
    PakGuid_t textureGuid;
    if (JSON_ParseNumber(texture, textureGuid))
        return false;

    if (texture.GetStringLength() == 0)
        Error("Texture defined in slot #%i was empty.\n", index);

    const char* const texturePath = texture.GetString();
    return Texture_AutoAddTexture(pak, RTech::StringToGuid(texturePath), texturePath, disableStreaming);
}

static short Material_CreateTextures(CPakFile* const pak, const rapidjson::Value& textures, const bool disableStreaming)
{
    short numTexturesAdded = 0;
    int i = 0;

    for (const auto& texture : textures.GetObject())
    {
        if (Material_CheckAndAddTexture(pak, texture.value, i++, disableStreaming))
            numTexturesAdded++;
    }

    return numTexturesAdded;
}

static size_t Material_GetHighestTextureBindPoint(const rapidjson::Value& textures)
{
    size_t max = 0;

    for (const auto& it : textures.GetObject())
    {
        char* end;
        const size_t index = strtoull(it.name.GetString(), &end, 0);

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
    short internalDependencyCount = 0;
    size_t curIndex = 0;

    for (auto it = textures.MemberBegin(); it != textures.MemberEnd(); it++, curIndex++)
    {
        const char* const start = it->name.GetString();
        char* end;
        const size_t bindPoint = strtoull(start, &end, 0);

        const rapidjson::Value& val = it->value;

        if (end != &start[it->name.GetStringLength()])
            Error("Unable to determine bind point for texture #%zu.\n", bindPoint);

        bool success;
        const PakGuid_t textureGuid = Pak_ParseGuid(val, &success);

        if (!success)
            Error("Unable to parse texture #%zu.\n", bindPoint);

        reinterpret_cast<PakGuid_t*>(dataBuf)[bindPoint] = textureGuid;
        const size_t offset = alignedPathSize + (bindPoint * sizeof(PakGuid_t));

        if (!Pak_RegisterGuidRefAtOffset(pak, textureGuid, offset, dataChunk, guids, internalDependencyCount))
            Warning("Unable to find texture #%zu within the local assets.\n", bindPoint);
    }

    return internalDependencyCount;
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
        Error("Unsupported type for blend state flags (expected %s, got %s).\n",
            JSON_InternalTypeToString(blendStateJson.GetType()), JSON_InternalTypeToString(rapidjson::kArrayType));
    }

    const rapidjson::Value::ConstArray blendStateElems = blendStateJson.GetArray();
    const size_t numBlendStates = blendStateElems.Size();

    if (numBlendStates != BLEND_STATE_COUNT)
    {
        Error("Expected %zu blend state flags, found %zu.\n", BLEND_STATE_COUNT, numBlendStates);
    }

    for (size_t i = 0; i < numBlendStates; i++)
    {
        const rapidjson::Value& obj = blendStateElems[i];
        uint32_t blendState;

        if (!JSON_ParseNumber(obj, blendState))
            Error("Failed parsing blend state flag #%zu.\n", i);

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

static const char* const Material_GetPassMaterialKeyForType(const RenderPassMaterial_e type)
{
    switch (type)
    {
    case DEPTH_SHADOW:       return "$depthShadowMaterial";
    case DEPTH_PREPASS:      return "$depthPrepassMaterial";
    case DEPTH_VSM:          return "$depthVSMMaterial";
    case DEPTH_SHADOW_TIGHT: return "$depthShadowTightMaterial";
    case COL_PASS:           return "$colpassMaterial";
    default: assert(0); return nullptr;
    }
}

static PakGuid_t Material_DetermineDefaultDepthMaterial(const RenderPassMaterial_e depthType, const MaterialShaderType_e shaderType, const int rasterizerFlags)
{
    switch (depthType)
    {
    case DEPTH_SHADOW:
        return RTech::StringToGuid(Utils::VFormat("material/code_private/depth_shadow%s_%s.rpak", 
            ((rasterizerFlags & 0x4) && !(rasterizerFlags & 0x2)) ? "_frontculled" : "", s_materialShaderTypeNames[shaderType]).c_str());
    case DEPTH_PREPASS:
        return RTech::StringToGuid(Utils::VFormat("material/code_private/depth_prepass%s_%s.rpak", 
            ((rasterizerFlags & 0x2) && !(rasterizerFlags & 0x4)) ? "_twosided" : "", s_materialShaderTypeNames[shaderType]).c_str());
    case DEPTH_VSM:
        return RTech::StringToGuid(Utils::VFormat("material/code_private/depth_vsm_%s.rpak", s_materialShaderTypeNames[shaderType]).c_str());
    case DEPTH_SHADOW_TIGHT:
        return RTech::StringToGuid(Utils::VFormat("material/code_private/depth_shadow_tight_%s.rpak", s_materialShaderTypeNames[shaderType]).c_str());
    case COL_PASS:
        return 0; // the default for colpass is 0.
    default:
        // code bug
        assert(0);
        return 0;
    }
}

bool Material_AutoAddMaterial(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const int assetVersion);

void MaterialAsset_t::SetupDepthMaterials(CPakFile* const pak, const rapidjson::Value& mapEntry)
{
    if (this->materialTypeStr == "wld")
        Warning("WLD materials do not have generic depth materials, make sure that you have set them to 0 or have created your own.\n");

    // titanfall 2 (v12) doesn't have depth_tight, so it has 1 less depth material.
    const int depthMatCount = this->assetVersion > 12 ? RENDER_PASS_MAT_COUNT : (RENDER_PASS_MAT_COUNT - 1);

    for (int i = 0; i < depthMatCount; i++)
    {
        const RenderPassMaterial_e depthMatType = (RenderPassMaterial_e)i;
        PakGuid_t& passMaterial = passMaterials[i];

        const char* const fieldName = Material_GetPassMaterialKeyForType(depthMatType);
        rapidjson::Value::ConstMemberIterator it;

        if (!JSON_GetIterator(mapEntry, fieldName, it))
        {
            // Use code_private depth materials if user didn't explicitly defined or nulled the field.
            passMaterial = Material_DetermineDefaultDepthMaterial(depthMatType, materialType, dxStates[0].rasterizerFlags);
            continue;
        }

        const rapidjson::Value& val = it->value;

        if (JSON_ParseNumber(val, passMaterial))
            continue;

        if (!val.IsString())
            Error("%s #%i is of unsupported type; expected %s or %s, found %s.\n", fieldName, i,
                JSON_TypeToString(JSONFieldType_e::kUint64), JSON_TypeToString(JSONFieldType_e::kString),
                JSON_TypeToString(JSON_ExtractType(val)));

        if (val.GetStringLength() == 0)
            Error("%s #%i was defined as an invalid empty string.\n", fieldName, i);

        const char* const materialPath = val.GetString();
        passMaterial = RTech::StringToGuid(materialPath);

        Material_AutoAddMaterial(pak, passMaterial, materialPath, this->assetVersion);
    }
}

static inline void CheckCountOrError(const rapidjson::Value::ConstArray& elements, const size_t expectedCount, const char* const fieldName)
{
    const size_t elemCount = elements.Size();

    if (elemCount != expectedCount)
        Error("Expected %zu elements for field \"%s\", found %zu.\n", expectedCount, fieldName, elemCount);
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

    return Utils::VFormat("%s%s.cpu_raw", pak->GetAssetPath().c_str(), hasPath ? path : matlAsset->path.c_str());
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
        Error("Failed to open cpu asset \"%s\".\n", cpuPath.c_str());

        // todo(amos): do we want this? without the cpu, the material will always look incorrect/dark
        // when we fall back here. disabled the code for now in favor of an error as this prevents a
        // ton of confusion and questions from users when the material ends up looking incorrect.
        // 
        // Warning("Failed to open cpu asset \"%s\"; using generic buffer\n", cpuPath.c_str());
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

    // this only seems to be in apex? heavily affects how the buffers are setup
    // and which one is selected at Pak_LoadMaterialAsset(). Func sub_1403B4680
    // also checks if this flag is '4' and materialType is 'PTCS'. The material
    // "particle/smoke/smoke_charge02_close" (PTCU) for instance, has this set
    // to 4, and 4 uses a different global buffer if this (flag & 253) != 0
    if (this->assetVersion == 15)
        this->uberBufferFlags = (uint8_t)JSON_GetNumberRequired<int>(mapEntry, "uberBufferFlags");

    // material max dimensions
    this->width = (short)JSON_GetNumberRequired<int>(mapEntry, "width"); // Set material width.
    this->height = (short)JSON_GetNumberRequired<int>(mapEntry, "height"); // Set material height.
    this->depth = (short)JSON_GetNumberRequired<int>(mapEntry, "depth");

    // base material glue flags. defaults are flags used on most materials.
    this->flags = JSON_GetNumberRequired<uint32_t>(mapEntry, "glueFlags");
    this->flags2 = JSON_GetNumberRequired<uint32_t>(mapEntry, "glueFlags2");

    // the partial name of the material, e.g. "debug/debugempty" (without material/ and _<shaderType>.rpak)
    this->name = JSON_GetValueRequired<const char*>(mapEntry, "name");

    // surfaces are defined in scripts/surfaceproperties.txt or scripts/surfaceproperties.rson
    this->surface = JSON_GetValueRequired<const char*>(mapEntry, "surfaceProp");

    // used for blend materials and the like
    this->surface2 = JSON_GetValueRequired<const char*>(mapEntry, "surfaceProp2");

    // This seems to be set on all materials, i haven't it being used yet in
    // the engine by hardware breakpointing it. but given that it was
    // preinitialized in the pak file, and the possible fact that various
    // other flags might dictate when and if this gets used, it must be
    // provided by the user.
    this->features = JSON_GetNumberRequired<uint32_t>(mapEntry, "features");

    // Set samplers properly. Responsible for texture stretching, tiling etc.
    *(uint32_t*)this->samplers = JSON_GetNumberRequired<uint32_t>(mapEntry, "samplers");

    Material_SetDXStates(mapEntry, dxStates);

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
        Error("Unexpected preset name \"%s\".\n", presetName.c_str());
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

static bool Material_OpenFile(CPakFile* const pak, const char* const assetPath, rapidjson::Document& document)
{
    const string fileName = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".json");
    BinaryIO materialStream;

    // note(amos): once finished this must error on failure
    if (!materialStream.Open(fileName, BinaryIO::Mode_e::Read))
    {
        //Error("Failed to open material asset \"%s\".\n", fileName.c_str());
        return false;
    }

    const ssize_t fileSize = materialStream.GetSize();

    if (!fileSize)
        Error("Material asset was empty.\n");

    std::unique_ptr<char[]> uniquebuf(new char[fileSize+1]);
    char* const bufptr = uniquebuf.get();

    materialStream.Read(bufptr, fileSize);
    bufptr[fileSize] = '\0';

    if (document.Parse(bufptr).HasParseError())
    {
        Error("Material asset parse error at position %zu: [%s].\n",
            document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError()));
    }

    if (!document.IsObject())
    {
        Error("Material asset root was not an object.\n");
    }

    return true;
}

static void Material_InternalAddMaterialV12(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& matEntry)
{
    short internalDependencyCount = 0; // number of dependencies inside this pak

    rapidjson::Value::ConstMemberIterator texturesIt;
    const bool hasTextures = JSON_GetIterator(matEntry, "$textures", JSONFieldType_e::kObject, texturesIt);

    const size_t textureCount = hasTextures
        ? Material_AddTextures(pak, matEntry, texturesIt->value)
        : 0; // note: no error as materials without textures do exist.
    // typically, these are prepass/vsm/etc materials.

    MaterialAsset_t matlAsset{};
    matlAsset.assetVersion = 12; // set asset as a titanfall 2 material
    matlAsset.path = Utils::ChangeExtension(assetPath, "");

    // header data chunk and generic struct
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(MaterialAssetHeader_v12_t), SF_HEAD, 16);

    // some var declaration
    size_t textureRefSize = textureCount * sizeof(PakGuid_t); // size of the texture guid section.

    // parse json inputs for matl header
    matlAsset.FromJSON(matEntry);
    matlAsset.guid = assetGuid;

    // !!!R2 SPECIFIC!!!
    {
        const size_t nameBufLen = matlAsset.name.length() + 1;
        CPakDataChunk nameChunk = pak->CreateDataChunk(nameBufLen, SF_CPU | SF_DEV, 1);

        sprintf_s(nameChunk.Data(), nameBufLen, "%s", matlAsset.name.c_str());

        matlAsset.materialName = nameChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, materialName)));
    }

    if (matlAsset.materialType != _TYPE_LEGACY)
        Error("Material type '%s' is not supported on version 12 (Titanfall 2) assets.\n", matlAsset.materialTypeStr.c_str());

    if ((matlAsset.materialTypeStr == "fix" || matlAsset.materialTypeStr == "skn"))
    {
        for (int i = 0; i < 2; ++i)
        {
            MaterialDXState_v15_t& dxState = matlAsset.dxStates[i];

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
            MaterialDXState_v15_t& dxState = matlAsset.dxStates[i];

            dxState.blendStates[0] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[1] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[2] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, 0xF);
            dxState.blendStates[3] = MaterialBlendState_t(false, true, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, 0x0);
        }
    }

    const size_t surfaceProp1Size = !matlAsset.surface.empty() ? (matlAsset.surface.length() + 1) : 0;
    const size_t surfaceProp2Size = !matlAsset.surface2.empty() ? (matlAsset.surface2.length() + 1) : 0;

    const size_t dataBufSize = (textureRefSize * 2) + surfaceProp1Size + surfaceProp2Size;

    const char* presetValue;

    if (JSON_GetValue(matEntry, "$preset", presetValue))
    {
        // get presets for dxstate, derived from existing r2 materials
        Material_SetTitanfall2Preset(&matlAsset, presetValue);
    }

    // asset data
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataBufSize, SF_CPU, 8);

    char* dataBuf = dataChunk.Data();

    std::vector<PakGuidRefHdr_t> guids;

    if (hasTextures)
    {
        internalDependencyCount += Material_AddTextureRefs(pak, dataChunk, dataBuf, guids, texturesIt->value, 0);
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
        snprintf(dataBuf, surfaceProp1Size, "%s", matlAsset.surface.c_str());
        dataBuf += surfaceProp1Size;
    }

    if (surfaceProp2Size)
    {
        snprintf(dataBuf, surfaceProp2Size, "%s", matlAsset.surface2.c_str());
        dataBuf += surfaceProp2Size;
    }

    // ===============================
    // fill out the rest of the header

    size_t currentDataBufOffset = 0;

    matlAsset.textureHandles = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, textureHandles)));
    currentDataBufOffset += textureRefSize;

    matlAsset.streamingTextureHandles = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, streamingTextureHandles)));
    currentDataBufOffset += textureRefSize;

    if (surfaceProp1Size)
    {
        matlAsset.surfaceProp = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, surfaceProp)));
        currentDataBufOffset += surfaceProp1Size;
    }

    if (surfaceProp2Size)
    {
        matlAsset.surfaceProp2 = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v12_t, surfaceProp2)));
        currentDataBufOffset += surfaceProp2Size;
    }

    // register referenced assets (depth materials, colpass material, shader sets)

    Pak_RegisterGuidRefAtOffset(pak, matlAsset.passMaterials[0], offsetof(MaterialAssetHeader_v12_t, passMaterials[0]), hdrChunk, guids, internalDependencyCount);
    Pak_RegisterGuidRefAtOffset(pak, matlAsset.passMaterials[1], offsetof(MaterialAssetHeader_v12_t, passMaterials[1]), hdrChunk, guids, internalDependencyCount);
    Pak_RegisterGuidRefAtOffset(pak, matlAsset.passMaterials[2], offsetof(MaterialAssetHeader_v12_t, passMaterials[2]), hdrChunk, guids, internalDependencyCount);
    Pak_RegisterGuidRefAtOffset(pak, matlAsset.passMaterials[3], offsetof(MaterialAssetHeader_v12_t, passMaterials[3]), hdrChunk, guids, internalDependencyCount);

    Pak_RegisterGuidRefAtOffset(pak, matlAsset.shaderSet, offsetof(MaterialAssetHeader_v12_t, shaderSet), hdrChunk, guids, internalDependencyCount);

    // write header now that we are done setting it up
    matlAsset.WriteToBuffer(hdrChunk.Data());

    //////////////////////////////////////////
    /// cpu
    size_t dxStaticBufSize = 0;
    CPakDataChunk uberBufChunk;

    Material_AddCpuData<MaterialShaderBufferV12>(pak, &matlAsset, matEntry, uberBufChunk, dxStaticBufSize);

    MaterialCPUHeader* cpuhdr = reinterpret_cast<MaterialCPUHeader*>(uberBufChunk.Data());
    cpuhdr->dataPtr = uberBufChunk.GetPointer(sizeof(MaterialCPUHeader));
    cpuhdr->dataSize = (uint32_t)dxStaticBufSize;
    cpuhdr->version = 3; // unsure what this value actually is but some cpu headers have
    // different values. the engine doesn't seem to use it however.

    pak->AddPointer(uberBufChunk.GetPointer(offsetof(MaterialCPUHeader, dataPtr)));

    //////////////////////////////////////////

    PakAsset_t asset;

    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), uberBufChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::MATL);
    asset.version = 12;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = internalDependencyCount + 1; // plus one for the asset itself

    asset.AddGuids(&guids);
    pak->PushAsset(asset);
}

static void Material_InternalAddMaterialV15(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& matEntry)
{
    // deal with dependencies first before creating chunks for this material asset.
    short internalDependencyCount = 0; // number of dependencies inside this pak

    rapidjson::Value::ConstMemberIterator texturesIt;
    const bool hasTextures = JSON_GetIterator(matEntry, "$textures", JSONFieldType_e::kObject, texturesIt);

    const size_t textureCount = hasTextures
        ? Material_AddTextures(pak, matEntry, texturesIt->value)
        : 0; // note: no error as materials without textures do exist.
             // typically, these are prepass/vsm/etc materials.

    MaterialAsset_t matlAsset{};
    matlAsset.FromJSON(matEntry); // parse json inputs for matl header

    matlAsset.assetVersion = 15;
    matlAsset.path = Utils::ChangeExtension(assetPath, ""); // todo: use Pak_ExtractAssetStem
    matlAsset.guid = assetGuid;

    matlAsset.SetupDepthMaterials(pak, matEntry);

    // header data chunk and generic struct
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(MaterialAssetHeader_v15_t), SF_HEAD, 16);

    // some var declaration
    size_t textureRefSize = textureCount * sizeof(PakGuid_t); // size of the texture guid section.

    const size_t nameBufLen = matlAsset.name.length() + 1;

    const size_t surfaceProp1Size = !matlAsset.surface.empty() ? (matlAsset.surface.length() + 1) : 0;
    const size_t surfaceProp2Size = !matlAsset.surface2.empty() ? (matlAsset.surface2.length() + 1) : 0;

    const size_t alignedPathSize = IALIGN4(nameBufLen);
    const size_t dataBufSize = alignedPathSize + (textureRefSize * 2) + surfaceProp1Size + surfaceProp2Size;

    // asset data
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataBufSize, SF_CPU, 8);

    char* dataBuf = dataChunk.Data();

    // write asset name into the start of the buffer
    snprintf(dataBuf, nameBufLen, "%s", matlAsset.name.c_str());
    dataBuf += alignedPathSize;

    std::vector<PakGuidRefHdr_t> guids;

    if (hasTextures)
    {
        internalDependencyCount += Material_AddTextureRefs(pak, dataChunk, dataBuf, guids, texturesIt->value, alignedPathSize);
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
        snprintf(dataBuf, surfaceProp1Size, "%s", matlAsset.surface.c_str());
        dataBuf += surfaceProp1Size;
    }

    if (surfaceProp2Size)
    {
        snprintf(dataBuf, surfaceProp2Size, "%s", matlAsset.surface2.c_str());
        dataBuf += surfaceProp2Size;
    }


    // ===============================
    // fill out the rest of the header

    size_t currentDataBufOffset = 0;

    matlAsset.materialName = dataChunk.GetPointer();
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, materialName)));
    currentDataBufOffset += alignedPathSize;

    matlAsset.textureHandles = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, textureHandles)));
    currentDataBufOffset += textureRefSize;

    matlAsset.streamingTextureHandles = dataChunk.GetPointer(currentDataBufOffset);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, streamingTextureHandles)));
    currentDataBufOffset += textureRefSize;

    if (surfaceProp1Size)
    {
        matlAsset.surfaceProp = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, surfaceProp)));
        currentDataBufOffset += surfaceProp1Size;
    }

    if (surfaceProp2Size)
    {
        matlAsset.surfaceProp2 = dataChunk.GetPointer(currentDataBufOffset);
        pak->AddPointer(hdrChunk.GetPointer(offsetof(MaterialAssetHeader_v15_t, surfaceProp2)));
        currentDataBufOffset += surfaceProp2Size;
    }

    // loop thru referenced assets (depth materials, colpass material)
    for (int i = 0; i < RENDER_PASS_MAT_COUNT; ++i)
    {
        const PakGuid_t guid = matlAsset.passMaterials[i];
        const size_t offset = offsetof(MaterialAssetHeader_v15_t, passMaterials[i]);

        Pak_RegisterGuidRefAtOffset(pak, guid, offset, hdrChunk, guids, internalDependencyCount);
    }

    Pak_RegisterGuidRefAtOffset(pak, matlAsset.shaderSet, offsetof(MaterialAssetHeader_v15_t, shaderSet), hdrChunk, guids, internalDependencyCount);
    Pak_RegisterGuidRefAtOffset(pak, matlAsset.textureAnimation, offsetof(MaterialAssetHeader_v15_t, textureAnimation), hdrChunk, guids, internalDependencyCount);

    // write header now that we are done setting it up
    matlAsset.WriteToBuffer(hdrChunk.Data());

    //////////////////////////////////////////
    /// cpu
    size_t dxStaticBufSize = 0;
    CPakDataChunk uberBufChunk;

    Material_AddCpuData<MaterialShaderBufferV15>(pak, &matlAsset, matEntry, uberBufChunk, dxStaticBufSize);

    MaterialCPUHeader* cpuhdr = reinterpret_cast<MaterialCPUHeader*>(uberBufChunk.Data());
    cpuhdr->dataPtr = uberBufChunk.GetPointer(sizeof(MaterialCPUHeader));
    cpuhdr->dataSize = (uint32_t)dxStaticBufSize;
    cpuhdr->version = 3; // unsure what this value actually is but some cpu headers have
                         // different values. the engine doesn't seem to use it however.

    pak->AddPointer(uberBufChunk.GetPointer(offsetof(MaterialCPUHeader, dataPtr)));

    //////////////////////////////////////////

    PakAsset_t asset;


    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), uberBufChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::MATL);
    asset.SetHeaderPointer(hdrChunk.Data());
    asset.version = 15;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = internalDependencyCount + 1; // plus one for the asset itself

    asset.AddGuids(&guids);
    pak->PushAsset(asset);
}

static bool Material_InternalAddMaterial(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value* const mapEntry, const int assetVersion)
{
    rapidjson::Document document;
    const rapidjson::Value* matEntry;

    if (Material_OpenFile(pak, assetPath, document))
        matEntry = &document;
    else
    {
        if (!mapEntry)
        {
            Warning("Unable to open material file \"%s\".\n", assetPath);
            return false;
        }

        matEntry = mapEntry;
    }

    if (assetVersion == 12)
        Material_InternalAddMaterialV12(pak, assetGuid, assetPath, *matEntry);
    else
        Material_InternalAddMaterialV15(pak, assetGuid, assetPath, *matEntry);

    return true;
}

bool Material_AutoAddMaterial(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const int assetVersion)
{
    PakAsset_t* const existingAsset = pak->GetAssetByGuid(assetGuid, nullptr, true);

    if (existingAsset)
        return false; // already present in the pak; not added.

    Log("Auto-adding 'matl' asset \"%s\".\n", assetPath);
    return Material_InternalAddMaterial(pak, assetGuid, assetPath, nullptr, assetVersion);
}

// VERSION 7
void Assets::AddMaterialAsset_v12(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    Material_InternalAddMaterial(pak, assetGuid, assetPath, &mapEntry, 12);
}

// VERSION 8
void Assets::AddMaterialAsset_v15(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    Material_InternalAddMaterial(pak, assetGuid, assetPath, &mapEntry, 15);
}
