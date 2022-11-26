#include "pch.h"
#include "Assets.h"
#include "assets/material.h"

std::unordered_map<std::string, MaterialShaderType_t> ShaderTypeMap =
{
    { "rgdu",   MaterialShaderType_t::RGDU },
    { "rgdp",   MaterialShaderType_t::RGDP },
    { "rgdc",   MaterialShaderType_t::RGDC },
    { "sknu",   MaterialShaderType_t::SKNU },
    { "sknp",   MaterialShaderType_t::SKNP },
    { "wldu",   MaterialShaderType_t::WLDU },
    { "wldc",   MaterialShaderType_t::WLDC },
    { "ptcu",   MaterialShaderType_t::PTCU },
    { "ptcs",   MaterialShaderType_t::PTCS },
};

MaterialShaderType_t GetShaderTypeFromString(std::string sType)
{
    std::transform(sType.begin(), sType.end(), sType.begin(), ::tolower);

    for (const auto& [key, value] : ShaderTypeMap) // Iterate through unordered_map.
    {
        if (sType.compare(key) == 0) // Do they equal?
            return value;
    }

    return MaterialShaderType_t::SKNP;
}

// VERSION 7
void Assets::AddMaterialAsset_v12(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("\n==============================\n");
    Log("Asset matl -> '%s'\n", assetPath);

    uint32_t assetUsesCount = 0; // track how many other assets are used by this asset
    MaterialHeaderV12* mtlHdr = new MaterialHeaderV12();
    std::string sAssetPath = std::string(assetPath);

    std::string type = "skn";
    std::string subtype = "";
    std::string visibility = "opaque";
    uint32_t version = 16;

    if (mapEntry.HasMember("type"))
        type = mapEntry["type"].GetStdString();
    else
        Warning("Adding material without an explicitly defined type. Assuming 'skn'...\n");

    if (mapEntry.HasMember("subtype"))
        subtype = mapEntry["subtype"].GetStdString();
    else
        Warning("No subtype is defined, this may cause issues... \n");

    // version check
    if (mapEntry.HasMember("version"))
        version = mapEntry["version"].GetInt();
    else
        Warning("Adding material without an explicitly defined version. Assuming '16'... \n");


    std::string sFullAssetRpakPath = "material/" + sAssetPath + "_" + type + ".rpak"; // Make full rpak asset path.

    mtlHdr->AssetGUID = RTech::StringToGuid(sFullAssetRpakPath.c_str()); // Convert full rpak asset path to textureGUID and set it in the material header.

    // this was for 'UnknownSignature' but isn't valid anymore I think.
    // Game ignores this field when parsing, retail rpaks also have this as 0. But In-Game its being set to either 0x4, 0x5, 0x9.
    // Based on resolution.
    // 512x512 = 0x5
    // 1024x1024 = 0x4
    // 2048x2048 = 0x9

    // Game ignores this field when parsing, retail rpaks also have this as 0. But In-Game its being set to the number of textures with streamed mip levels.
    if (mapEntry.HasMember("streamedtexturecount"))
        mtlHdr->StreamableTextureCount = mapEntry["streamedtexturecount"].GetInt();

    if (mapEntry.HasMember("width")) // Set material width.
        mtlHdr->Width = mapEntry["width"].GetInt();

    if (mapEntry.HasMember("height")) // Set material width.
        mtlHdr->Height = mapEntry["height"].GetInt();

    if (mapEntry.HasMember("imageflags")) // Set flags properly. Responsible for texture stretching, tiling etc.
        mtlHdr->ImageFlags = mapEntry["imageflags"].GetUint();

    if (mapEntry.HasMember("visibilityflags")) {

        visibility = mapEntry["visibilityflags"].GetString();
        uint16_t visFlag = 0x0017;

        if (visibility == "opaque") {

            visFlag = 0x0017;

        }
        else if (visibility == "transparent") {

            // this will not work properly unless some flags are set in Flags2
            visFlag = 0x0007;

        }
        else if (visibility == "colpass") {

            visFlag = 0x0005;

        }
        else if (visibility == "none") {

            // for loadscreens
            visFlag = 0x0000;

        }
        else {

            Log("No valid visibility specified, defaulting to opaque... \n");

            visFlag = 0x0017;

        }

        mtlHdr->UnkSections[0].VisibilityFlags = visFlag;
        mtlHdr->UnkSections[1].VisibilityFlags = visFlag;

    }

    if (mapEntry.HasMember("faceflags")) {
        mtlHdr->UnkSections[0].FaceDrawingFlags = mapEntry["faceflags"].GetInt();
        mtlHdr->UnkSections[1].FaceDrawingFlags = mapEntry["faceflags"].GetInt();
        Log("Using faceflags, only touch this if you know what you're doing! \n");
    }
    else {
        mtlHdr->UnkSections[0].FaceDrawingFlags = 0x0006;
        mtlHdr->UnkSections[1].FaceDrawingFlags = 0x0006;
    }

    std::string surface = "default";
    std::string surface2 = "default";

    // surfaces are defined in scripts/surfaceproperties.rson
    // titanfall surfaces are defined in scripts/surfaceproperties.txt
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

    int surfaceDataBuffLength = 0;
   // surfaceDataBuffLength = (surface.length() + 1);

    if (mapEntry.HasMember("surface2")) {

        surfaceDataBuffLength = (surface.length() + 1) + (surface2.length() + 1);

    }
    else {

        surfaceDataBuffLength = (surface.length() + 1);

    }

    uint32_t assetPathSize = (sAssetPath.length() + 1);
    uint32_t dataBufSize = (assetPathSize + (assetPathSize % 4)) + (textureRefSize * 2) + surfaceDataBuffLength;

    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(MaterialHeaderV12), SF_HEAD, 8);

    // asset data
    _vseginfo_t dataseginfo = pak->CreateNewSegment(dataBufSize, SF_CPU, 64);

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
                Warning("unable to find texture '%s' for material '%s' within the local assets\n", it.GetString(), assetPath);

            assetUsesCount++;
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++; // Next texture index coming up.
    }

    textureIdx = 0; // reset index for next TextureGUID Section.
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the second TextureGUID Map.
    {
        *(uint64_t*)dataBuf = 0;

        dataBuf += sizeof(uint64_t);
        textureIdx++;
    }

    // ===============================
    // write the surface names into the buffer.
    // this is an extremely janky way to do this but I don't know better, basically it writes surface2 first so then the first can overwrite it.
    // please someone do this better I beg you.
    if (mapEntry.HasMember("surface2"))
    {
        std::string surfaceStrTmp = surface + "." + surface2;

        snprintf(dataBuf, (surface.length() + 1) + (surface2.length() + 1), "%s", surfaceStrTmp.c_str());
        snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());
    }
    else {
        snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());
    }

    // get the original pointer back so it can be used later for writing the buffer
    dataBuf = tmp;

    // ===============================
    // fill out the rest of the header
    mtlHdr->m_pszName.index = dataseginfo.index;
    mtlHdr->m_pszName.offset = 0;

    mtlHdr->m_pszSurfaceProp.index = dataseginfo.index;
    mtlHdr->m_pszSurfaceProp.offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2);

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, m_pszName));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, m_pszSurfaceProp));

    if (mapEntry.HasMember("surface2")) {

        mtlHdr->m_pszSurfaceProp2.index = dataseginfo.index;
        mtlHdr->m_pszSurfaceProp2.offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2) + (surface.length() + 1);

        pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, m_pszSurfaceProp2));
    }

    // V12 Type Handling
    if (version == 12)
    {
        if (type == "gen")
        {
            if (subtype == "loadscreen")
            {
                mtlHdr->Flags2 = 0x10000002;

                mtlHdr->ShaderSetGUID = 0xA5B8D4E9A3364655;
            }
            else
            {
                Warning("Invalid type used! Defaulting to subtype 'loadscreen'... \n");

                mtlHdr->Flags2 = 0x10000002;

                mtlHdr->ShaderSetGUID = 0xA5B8D4E9A3364655;
            }

            // These should always be constant (per each material type)
            // GUIDRefs[3] is Colpass entry, however loadscreens do not have colpass materials.

            mtlHdr->GUIDRefs[0] = 0x0000000000000000;
            mtlHdr->GUIDRefs[1] = 0x0000000000000000;
            mtlHdr->GUIDRefs[2] = 0x0000000000000000;

            mtlHdr->ImageFlags = 0x050300;

            mtlHdr->Unknown2 = 0xFBA63181;
        }
        else if (type == "wld")
        {
            Warning("Type 'wld' is not supported currently!!!");

            if (subtype == "test1")
            {
                mtlHdr->ShaderSetGUID = 0x8FB5DB9ADBEB1CBC;

                mtlHdr->Flags2 = 0x72000002;
            }
            else
            {
                Warning("Invalid type used! Defaulting to subtype 'viewmodel'... \n");

                // same as 'viewmodel'.
                mtlHdr->ShaderSetGUID = 0x8FB5DB9ADBEB1CBC;

                mtlHdr->Flags2 = 0x72000002;
            }

            mtlHdr->GUIDRefs[0] = 0x0000000000000000;
            mtlHdr->GUIDRefs[1] = 0x0000000000000000;
            mtlHdr->GUIDRefs[2] = 0x0000000000000000;

            mtlHdr->UnkSections[0].UnkRenderFlags = 0x00000005;

            mtlHdr->UnkSections[1].UnkRenderFlags = 0x00000005;

            mtlHdr->ImageFlags = 0x1D0300;

            mtlHdr->Unknown2 = 0x40D33E8F;
        }
        else if (type == "fix")
        {
            if (subtype == "worldmodel")
            {
                // supports a set of seven textures.
                // viewmodel shadersets don't seem to allow ilm in third person, this set supports it.
                mtlHdr->ShaderSetGUID = 0x586783F71E99553D;

                mtlHdr->Flags2 = 0x56000020;
            }
            else if (subtype == "worldmodel_skn31")
            {
                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->ShaderSetGUID = 0x5F8181FEFDB0BAD8;

                mtlHdr->Flags2 = 0x56040020;
            }
            else if (subtype == "worldmodel_noglow")
            {
                // supports a set of six textures, lacks ilm.
                // there is a different one used for viewmodels, unsure what difference it makes considering the lack of ilm.
                mtlHdr->ShaderSetGUID = 0x477A8F31B5963070;

                mtlHdr->Flags2 = 0x56000020;
            }
            else if (subtype == "worldmodel_skn31_noglow")
            {
                // supports a set of six textures plus a set of two relating to detail textures (camos), lacks ilm.
                // same as above, why.
                mtlHdr->ShaderSetGUID = 0xC9B736D2C8027726;

                mtlHdr->Flags2 = 0x56040020;
            }
            else if (subtype == "viewmodel")
            {
                // supports a set of seven textures.
                // worldmodel shadersets don't seem to allow ilm in first person, this set supports it.
                mtlHdr->ShaderSetGUID = 0x5259835D8C44A14D;

                mtlHdr->Flags2 = 0x56000020;
            }
            else if (subtype == "viewmodel_skn31")
            {
                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->ShaderSetGUID = 0x19F840A12774CA4C;

                mtlHdr->Flags2 = 0x56040020;
            }
            else if (subtype == "nose_art")
            {
                mtlHdr->ShaderSetGUID = 0x3DAD868FA7485BDD;

                mtlHdr->Flags2 = 0x56000023;
            }
            else
            {
                Warning("Invalid type used! Defaulting to subtype 'viewmodel'... \n");

                // same as 'viewmodel'.
                mtlHdr->ShaderSetGUID = 0x5259835D8C44A14D;

                mtlHdr->Flags2 = 0x56000020;
            }

            if (subtype == "nose_art")
            {

                for (int i = 0; i < 2; ++i)
                {
                    mtlHdr->UnkSections[i].UnkRenderLighting = 0xF0138286;
                    mtlHdr->UnkSections[i].UnkRenderAliasing = 0xF0138286;
                    mtlHdr->UnkSections[i].UnkRenderDoF = 0xF0008286;
                    mtlHdr->UnkSections[i].UnkRenderUnknown = 0x00138286;

                    mtlHdr->UnkSections[i].UnkRenderFlags = 0x00000005;

                }

                mtlHdr->GUIDRefs[0] = 0x0000000000000000;
                mtlHdr->GUIDRefs[1] = 0x0000000000000000;
                mtlHdr->GUIDRefs[2] = 0x0000000000000000;

            }
            else {

                for (int i = 0; i < 2; ++i)
                {



                    mtlHdr->UnkSections[i].UnkRenderLighting = 0xF0138004;
                    mtlHdr->UnkSections[i].UnkRenderAliasing = 0xF0138004;
                    mtlHdr->UnkSections[i].UnkRenderDoF = 0xF0138004;
                    mtlHdr->UnkSections[i].UnkRenderUnknown = 0x00138004;

                    mtlHdr->UnkSections[i].UnkRenderFlags = 0x00000004;

                }

                mtlHdr->GUIDRefs[0] = 0x39C739E9928E555C;
                mtlHdr->GUIDRefs[1] = 0x67D89B36EDCDDF6E;
                mtlHdr->GUIDRefs[2] = 0x43A9D8D429698B9F;

                pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs));
                pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 8);
                pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 16);
                assetUsesCount += 3;

            }

            mtlHdr->ImageFlags = 0x1D0300;

            mtlHdr->Unknown2 = 0x40D33E8F;

        }
        else if (type == "rgd")
        {
            // todo: figure out what rgd is used for.
            Warning("Type 'rgd' is not supported currently!!!");
            return;
        }
        else if (type == "skn")
        {
            if (subtype == "worldmodel")
            {
                // supports a set of seven textures.
                // viewmodel shadersets don't seem to allow ilm in third person, this set supports it.
                mtlHdr->ShaderSetGUID = 0xC3ACAF7F1DC7F389;

                mtlHdr->Flags2 = 0x56000020;
            }
            else if (subtype == "worldmodel_skn31")
            {
                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->ShaderSetGUID = 0x4CFB9F15FD2DE909;

                mtlHdr->Flags2 = 0x56040020;
            }
            else if (subtype == "worldmodel_noglow")
            {
                // supports a set of six textures, lacks ilm.
                // there is a different one used for viewmodels, unsure what difference it makes considering the lack of ilm.
                mtlHdr->ShaderSetGUID = 0x34A7BB3C163A8139;

                mtlHdr->Flags2 = 0x56000020;
            }
            else if (subtype == "worldmodel_skn31_noglow")
            {
                // supports a set of six textures plus a set of two relating to detail textures (camos), lacks ilm.
                // same as above, why.
                mtlHdr->ShaderSetGUID = 0x98EA4745D8801A9B;

                mtlHdr->Flags2 = 0x56040020;
            }
            else if (subtype == "viewmodel")
            {
                // supports a set of seven textures.
                // worldmodel shadersets don't seem to allow ilm in first person, this set supports it.
                mtlHdr->ShaderSetGUID = 0xBD04CCCC982F8C15;

                mtlHdr->Flags2 = 0x56000020;
            }
            else if (subtype == "viewmodel_skn31")
            {
                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->ShaderSetGUID = 0x07BF4EC4B9632A03;

                mtlHdr->Flags2 = 0x56040020;
            }
            else if (subtype == "nose_art")
            {
                mtlHdr->ShaderSetGUID = 0x6CBEA6FE48218FAA;

                mtlHdr->Flags2 = 0x56000023;
            }
            else if (subtype == "test1")
            {
                mtlHdr->ShaderSetGUID = 0x942791681799941D;

                mtlHdr->Flags2 = 0x56040022;
            }
            else
            {
                Warning("Invalid type used! Defaulting to subtype 'viewmodel'... \n");

                // same as 'viewmodel'.
                mtlHdr->ShaderSetGUID = 0xBD04CCCC982F8C15;

                mtlHdr->Flags2 = 0x56000020;
            }

            if (subtype == "nose_art")
            {
                for (int i = 0; i < 2; ++i)
                {
                    mtlHdr->UnkSections[i].UnkRenderLighting = 0xF0138286;
                    mtlHdr->UnkSections[i].UnkRenderAliasing = 0xF0138286;
                    mtlHdr->UnkSections[i].UnkRenderDoF = 0xF0008286;
                    mtlHdr->UnkSections[i].UnkRenderUnknown = 0x00138286;

                    mtlHdr->UnkSections[i].UnkRenderFlags = 0x00000005;
                }

                mtlHdr->GUIDRefs[0] = 0x0000000000000000;
                mtlHdr->GUIDRefs[1] = 0x0000000000000000;
                mtlHdr->GUIDRefs[2] = 0x0000000000000000;
            }
            else
            {
                for (int i = 0; i < 2; ++i)
                {
                    mtlHdr->UnkSections[i].UnkRenderLighting = 0xF0138004;
                    mtlHdr->UnkSections[i].UnkRenderAliasing = 0xF0138004;
                    mtlHdr->UnkSections[i].UnkRenderDoF = 0xF0138004;
                    mtlHdr->UnkSections[i].UnkRenderUnknown = 0x00138004;

                    mtlHdr->UnkSections[i].UnkRenderFlags = 0x00000004;
                }

                mtlHdr->GUIDRefs[0] = 0xA4728358C3B043CA;
                mtlHdr->GUIDRefs[1] = 0x370BABA9D9147F3D;
                mtlHdr->GUIDRefs[2] = 0x12DCE94708487F8C;

                pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs));
                pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 8);
                pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 16);
                assetUsesCount += 3;
            }

            mtlHdr->ImageFlags = 0x1D0300;

            mtlHdr->Unknown2 = 0x40D33E8F;

        }

    }
    pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, ShaderSetGUID));
    assetUsesCount++;

    if (mtlHdr->ShaderSetGUID != 0)
    {
        RPakAssetEntry* asset = pak->GetAssetByGuid(mtlHdr->ShaderSetGUID);

        if (asset)
            asset->AddRelation(assetEntries->size());
    }

    // Is this a colpass asset?
    bool bColpass = false;
    if (mapEntry.HasMember("colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + "_" + type + ".rpak";
        mtlHdr->GUIDRefs[3] = RTech::StringToGuid(colpassPath.c_str());

        // todo, the relations count is not being set properly on the colpass for whatever reason.
        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 24);
        assetUsesCount++;

        bColpass = false;
    }

    for (int i = 0; i < 4; ++i)
    {
        uint64_t guid = mtlHdr->GUIDRefs[i];

        if (guid != 0)
        {
            RPakAssetEntry* asset = pak->GetAssetByGuid(guid);

            if (asset)
                asset->AddRelation(assetEntries->size());
        }
    }

    mtlHdr->TextureGUIDs.index = dataseginfo.index;
    mtlHdr->TextureGUIDs.offset = guidPageOffset;

    mtlHdr->TextureGUIDs2.index = dataseginfo.index;
    mtlHdr->TextureGUIDs2.offset = guidPageOffset + textureRefSize;

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, TextureGUIDs));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, TextureGUIDs2));

    mtlHdr->something2 = 0x100000;

    std::uint64_t cpuDataSize = sizeof(MaterialCPUDataV12);

    // cpu data
    _vseginfo_t cpuseginfo = pak->CreateNewSegment(sizeof(MaterialCPUHeader) + cpuDataSize, 3, 16);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.m_nUnknownRPtr.index = cpuseginfo.index;
    cpuhdr.m_nUnknownRPtr.offset = sizeof(MaterialCPUHeader);
    cpuhdr.m_nDataSize = cpuDataSize;

    pak->AddPointer(cpuseginfo.index, 0);

    char* cpuData = new char[sizeof(MaterialCPUHeader) + cpuDataSize];

    // copy header into the start
    memcpy_s(cpuData, 16, &cpuhdr, 16);

    // copy the rest of the data after the header
    MaterialCPUDataV12 cpudata{};

    std::float_t selfillumtint[4] = { 0.0, 0.0, 0.0, 0.0 };

    if (mapEntry.HasMember("selfillumtint")) {

        int tintId = 0;
        for (auto& sitf : mapEntry["selfillumtint"].GetArray())
        {
            selfillumtint[tintId] = sitf.GetFloat();

            tintId++;
        }
    }
    else {
        Log("No selfillumtint specified, assuming there is no emissive texture! \n");
    }

    cpudata.SelfillumTint->r = selfillumtint[0];
    cpudata.SelfillumTint->g = selfillumtint[1];
    cpudata.SelfillumTint->b = selfillumtint[2];
    cpudata.SelfillumTint->a = selfillumtint[3];

    std::float_t color2[4] = { 1.0, 1.0, 1.0, 1.0 };

    if (mapEntry.HasMember("color2"))
    {
        int color2Id = 0;
        for (auto& c2f : mapEntry["color2"].GetArray())
        {
            color2[color2Id] = c2f.GetFloat();

            color2Id++;
        }
    }

    cpudata.MainTint->r = color2[0];
    cpudata.MainTint->g = color2[1];
    cpudata.MainTint->b = color2[2];
    cpudata.MainTint->a = color2[3];

    std::float_t DetailTransformMatrix[6] = { 1.0, 0, -0, 1.0, 0.0, 0.0 };

    if (mapEntry.HasMember("detailtransform"))
    {
        int detailId = 0;
        for (auto& dtm : mapEntry["detailtransform"].GetArray())
        {
            DetailTransformMatrix[detailId] = dtm.GetFloat();

            detailId++;
        }
    }

    cpudata.DetailTransform->TextureScaleX = DetailTransformMatrix[0];
    cpudata.DetailTransform->TextureUnk = DetailTransformMatrix[1];
    cpudata.DetailTransform->TextureRotation = DetailTransformMatrix[2];
    cpudata.DetailTransform->TextureScaleY = DetailTransformMatrix[3];
    cpudata.DetailTransform->TextureTranslateX = DetailTransformMatrix[4];
    cpudata.DetailTransform->TextureTranslateY = DetailTransformMatrix[5];

    memcpy_s(cpuData + sizeof(MaterialCPUHeader), cpuDataSize, &cpudata, cpuDataSize);
    //////////////////////////////////////////

    pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)mtlHdr });
    pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)dataBuf });
    pak->AddRawDataBlock({ cpuseginfo.index, cpuseginfo.size, (uint8_t*)cpuData });

    //////////////////////////////////////////
    //  todo make thise swap depending on version, probably a global rpak version.
    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), subhdrinfo.index, 0, subhdrinfo.size, cpuseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = version;

    asset.pageEnd = cpuseginfo.index + 1;
    // this isn't even fully true in some apex materials.
    //asset.unk1 = bColpass ? 7 : 8; // what
    // unk1 appears to be maxusecount, although seemingly nothing is affected by changing it unless you exceed 18.
    // In every TF|2 material asset entry I've looked at it's always UsesCount + 1.
    asset.unk1 = assetUsesCount + 1;

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);
}

// VERSION 8
void Assets::AddMaterialAsset_v15(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("\n==============================\n");
    Log("asset matl -> '%s'\n", assetPath);

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
        mtlHdr->m_ImageFlags = mapEntry["flags"].GetUint();

    std::string surface = "default";
    std::string surface2 = "default";

    // surfaces are defined in scripts/surfaceproperties.rson
    if (mapEntry.HasMember("surface"))
        surface = mapEntry["surface"].GetStdString();

    // rarely used edge case but it's good to have.
    if (mapEntry.HasMember("surface2"))
        surface2 = mapEntry["surface2"].GetStdString();

    int surfaceDataBuffLength = 0;
    // surfaceDataBuffLength = (surface.length() + 1);

    if (mapEntry.HasMember("surface2"))
        surfaceDataBuffLength = (surface.length() + 1) + (surface2.length() + 1);
    else
        surfaceDataBuffLength = (surface.length() + 1);

    // Get the size of the texture guid section.
    size_t textureRefSize = 0;

    if (mapEntry.HasMember("textures"))
        textureRefSize = mapEntry["textures"].GetArray().Size() * 8;
    else
    {
        Warning("Trying to add material with no textures. Skipping asset...\n");
        return;
    }

    uint32_t assetPathSize = (sAssetPath.length() + 1);
    uint32_t dataBufSize = (assetPathSize + (assetPathSize % 4)) + (textureRefSize * 2) + surfaceDataBuffLength;

    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(MaterialHeaderV15), SF_HEAD /*| SF_CLIENT*/, 16, 16);

    // asset data
    _vseginfo_t dataseginfo = pak->CreateNewSegment(dataBufSize, SF_CPU /*| SF_CLIENT*/, 8, 64);

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
        uint64_t textureGUID = 0;

        if (it.IsString() && it.GetStringLength() > 0)
            textureGUID = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str());
        else if(it.IsUint64() && it.GetUint64() > 0)
            textureGUID = it.GetUint64();

        if (textureGUID)
        {
            *(uint64_t*)dataBuf = textureGUID;
            pak->AddGuidDescriptor(&guids, dataseginfo.index, guidPageOffset + (textureIdx * sizeof(uint64_t))); // Register GUID descriptor for current texture index.

            RPakAssetEntry* txtrAsset = pak->GetAssetByGuid(textureGUID, nullptr);

            if (txtrAsset)
                txtrAsset->AddRelation(assetEntries->size());
        }

        dataBuf += sizeof(uint64_t);
        textureIdx++; // Next texture index coming up.
    }

    dataBuf += sizeof(uint64_t) * mapEntry["textures"].Size();


    // ===============================
    // write the surface name into the buffer
    if (mapEntry.HasMember("surface2"))
    {
        std::string surfaceStrTmp = surface + "." + surface2;

        snprintf(dataBuf, (surface.length() + 1) + (surface2.length() + 1), "%s", surfaceStrTmp.c_str());
        snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());
    }
    else {
        snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());
    }

    // get the original pointer back so it can be used later for writing the buffer
    dataBuf = tmp;

    // ===============================
    // fill out the rest of the header
    mtlHdr->m_pszName.index = dataseginfo.index;
    mtlHdr->m_pszName.offset = 0;

    mtlHdr->m_pszSurfaceProp.index = dataseginfo.index;
    mtlHdr->m_pszSurfaceProp.offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2);

    if (mapEntry.HasMember("surface2")) {

        mtlHdr->m_pszSurfaceProp2.index = dataseginfo.index;
        mtlHdr->m_pszSurfaceProp2.offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2) + (surface.length() + 1);

        pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pszSurfaceProp2));
    }

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pszName));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pszSurfaceProp));


    mtlHdr->m_Flags2 = 0x56000020;
    mtlHdr->something = 0x100000;

    mtlHdr->materialType = GetShaderTypeFromString(type);
    switch ( mtlHdr->materialType )
    {
    case RGDU:
    {
        mtlHdr->m_pShaderSet = 0xbdd6a5af5c897e2f;
        break;
    }

    case RGDP:
    {
        mtlHdr->m_GUIDRefs[0] = 0x251FBE09EFFE8AB1;
        mtlHdr->m_GUIDRefs[1] = 0xE2D52641AFC77395;
        mtlHdr->m_GUIDRefs[2] = 0xBDBF90B97E7D9280;
        mtlHdr->m_GUIDRefs[3] = 0x85654E05CF9B40E7;
    
        mtlHdr->m_pShaderSet = 0x2a2db3a47af9b3d5;
        break;
    }

    case RGDC:
    {
        mtlHdr->m_pShaderSet = 0xb703d7a7941109ee;
        break;
    }

    case SKNP:
    {
        mtlHdr->m_GUIDRefs[0] = 0x2B93C99C67CC8B51;
        mtlHdr->m_GUIDRefs[1] = 0x1EBD063EA03180C7;
        mtlHdr->m_GUIDRefs[2] = 0xF95A7FA9E8DE1A0E;
        mtlHdr->m_GUIDRefs[3] = 0x227C27B608B3646B;

        mtlHdr->m_pShaderSet = 0x1D9FFF314E152725;
        break;
    }

    case SKNC:
    {
        mtlHdr->m_GUIDRefs[0] = 0x15e452d19fb86630;
        mtlHdr->m_GUIDRefs[1] = 0xb0435c3828a6f687;
        mtlHdr->m_GUIDRefs[2] = 0x8b2dc6aff75d1572;
        mtlHdr->m_GUIDRefs[3] = 0x22773407baf4f85c;

        mtlHdr->m_pShaderSet = 0x941f952e8193e080;
        break;
    }

    case WLDU:
    {
        mtlHdr->m_pShaderSet = 0x2cbb9f90f1aaac47;
        break;
    }

    case WLDC:
    {
        mtlHdr->m_GUIDRefs[0] = 0x435FA77E363BEA48;
        mtlHdr->m_GUIDRefs[1] = 0xF734F96BE92E0E71;
        mtlHdr->m_GUIDRefs[2] = 0xD306370918620EC0;
        mtlHdr->m_GUIDRefs[3] = 0xDAB17AEAD2D3387A;

        mtlHdr->m_pShaderSet = 0x4B0F3B4CBD009096;
        break;
    }

    }

    for (int i = 0; i < 4; i++)
    {
        if( mtlHdr->m_GUIDRefs[i] )
           pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + (i * 0x8));
    }

    if (mapEntry.HasMember("shaderset"))
    {
        if (mapEntry["shaderset"].IsString() && mapEntry["shaderset"].GetStdString() != "")
            mtlHdr->m_pShaderSet = RTech::StringToGuid(mapEntry["shaderset"].GetString());
        else if (mapEntry["shaderset"].IsUint64() && mapEntry["shaderset"].GetUint64() != 0)
            mtlHdr->m_pShaderSet = mapEntry["shaderset"].GetUint64();
    }

    pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_pShaderSet));

   if (mtlHdr->m_pShaderSet)
   {
       RPakAssetEntry* asset = pak->GetAssetByGuid(mtlHdr->m_pShaderSet);
       if (asset)
           asset->AddRelation(assetEntries->size());
   }

    // Is this a colpass asset?
    bool bColpass = false;
    if (mapEntry.HasMember("colpass") && mapEntry["colpass"].IsString())
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + ".rpak";
        mtlHdr->m_GUIDRefs[4] = RTech::StringToGuid(colpassPath.c_str());

        pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, m_GUIDRefs) + 32);
    }

    if (sAssetPath.find("colpass") != -1)
        bColpass = true;

    for (int i = 0; i < 5; ++i)
    {
        uint64_t guid = mtlHdr->m_GUIDRefs[i];

        if (guid != 0)
        {
            RPakAssetEntry* asset = pak->GetAssetByGuid(guid);

            if (asset)
                asset->AddRelation(assetEntries->size());
        }
    }

    mtlHdr->m_pTextureHandles.index = dataseginfo.index;
    mtlHdr->m_pTextureHandles.offset = guidPageOffset;

    mtlHdr->m_pStreamingTextureHandles.index = dataseginfo.index;
    mtlHdr->m_pStreamingTextureHandles.offset = guidPageOffset + textureRefSize;

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pTextureHandles));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, m_pStreamingTextureHandles));

    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8; ++j)
            mtlHdr->m_UnknownSections[i].m_Unknown1[j] = 0xf0000000;

        uint16_t visFlag = bColpass ? MatVisFlags::Colpass : MatVisFlags::Opaque;
        uint16_t faceFlag = MatRenderFlags::NoCulling;

        if (mapEntry.HasMember("visflags")) 
        {
            auto& value = mapEntry["visflags"];

            if (value.IsString())
            {
                std::string vis = value.GetStdString();

                if (vis == "opaque")  visFlag = MatVisFlags::Opaque;
                else if (vis == "transparent")  visFlag = MatVisFlags::Transparent;
                else if (vis == "colpass")      visFlag = MatVisFlags::Colpass;
                else if (vis == "none")         visFlag = MatVisFlags::None;
            }
            else if (value.IsInt())
                visFlag = value.GetInt();

        }

        if (mapEntry.HasMember("drawflags")) 
        {
            auto& value = mapEntry["drawflags"];

            if (value.IsString())
            {
                std::string draw = value.GetStdString();

                if (draw == "culling")        faceFlag = MatRenderFlags::Culling;
                else if (draw == "noculling")  faceFlag = MatRenderFlags::NoCulling;
                else if (draw == "wireframe") faceFlag = MatRenderFlags::Wireframe;
                else if (draw == "inverted")  faceFlag = MatRenderFlags::Inverted;
                else if (draw == "unknown")   faceFlag = MatRenderFlags::Unknown;
                else if (draw == "default")   faceFlag = MatRenderFlags::Default;
            }
            else if (value.IsInt())
                faceFlag = value.GetInt();
        }

        mtlHdr->m_UnknownSections[i].m_UnkRenderFlags = 4;

        if (mapEntry.HasMember("unkflags") && mapEntry["unkflags"].IsInt())
            mtlHdr->m_UnknownSections[i].m_UnkRenderFlags = mapEntry["unkflags"].GetInt();

        mtlHdr->m_UnknownSections[i].m_VisibilityFlags = visFlag;
        mtlHdr->m_UnknownSections[i].m_FaceDrawingFlags = faceFlag;
    }

    //////////////////////////////////////////
    /// cpu

   // required for accurate colour
    MaterialCPUDataV15 CpuData;

    if (mapEntry.HasMember("cputype") && mapEntry["cputype"].IsString())
    {
        std::string entry = mapEntry["cputype"].GetStdString();

        if (entry == "weapon")
        {
            CpuData.uv1.c_RotScaleX = { -0.000000, 1.000000 };
            CpuData.uv1.c_RotScaleY = { 0.000000, 1.000000 };
            CpuData.uv1.c_Translate = { 0.200000, 0.100000 };
            CpuData.uv2.c_RotScaleX = { -0.000000, 1.000000 };
            CpuData.uv2.c_RotScaleY = { 0.000000, 1.000000 };
            CpuData.uv2.c_Translate = { 0.010000, -0.000000 };
            CpuData.uv3.c_RotScaleX = { 1.000000, -0.000000 };
            CpuData.uv3.c_RotScaleY = { 0.000000, 1.000000 };
            CpuData.uv3.c_Translate = { -0.000000, -0.000000 };
            CpuData.uv4.c_RotScaleX = { 1.000000, -0.000000 };
            CpuData.uv4.c_RotScaleY = { 0.000000, 1.000000 };
            CpuData.uv4.c_Translate = { -0.000000, -0.000000 };
            CpuData.uv5.c_RotScaleX = { 1.000000, -0.000000 };
            CpuData.uv5.c_RotScaleY = { 0.000000, 1.000000 };
            CpuData.uv5.c_Translate = { -0.000000, -0.000000 };

            CpuData.c_uvDistortionIntensity = { 0.300000, 0.300000 };

            CpuData.c_emissiveEdgeFadeExponent = 2.000000;
            CpuData.c_emissiveEdgeFadeOuter = 1.500000;
        }
    }

    if (mapEntry.HasMember("ilmtint"))
    {
        auto& entry = mapEntry["ilmtint"];

        if (entry.IsFloat())
            CpuData.c_L0_emissiveTint = { entry.GetFloat(), entry.GetFloat() , entry.GetFloat() };
        else if (entry.IsArray())
        {
            auto vector = entry.GetArray();

            if (vector.Size() < 3)
                Error("'%s' ilmtint Vector Size : '%d' expected '3' ", assetPath, vector.Size());

            for (auto& rgb : vector)
            {
                if(!rgb.IsFloat())
                    Error("'%s' ilmtint Vector doesn't contain 'float'", assetPath);
            }

            CpuData.c_L0_emissiveTint = { vector[0].GetFloat(), vector[1].GetFloat() ,vector[2].GetFloat() };
        }
    }

    std::uint64_t cpuDataSize = sizeof(MaterialCPUDataV15);

    char* CpuDataBuf = nullptr;
    if (mapEntry.HasMember("cpu") && mapEntry["cpu"].IsString())
    {
        std::string cpudataFilePath = g_sAssetsDir + mapEntry["cpu"].GetStdString() + ".cpu";
        REQUIRE_FILE(cpudataFilePath);

        cpuDataSize = Utils::GetFileSize(cpudataFilePath);
        CpuDataBuf = new char[cpuDataSize];

        BinaryIO cpuInput;
        cpuInput.open(cpudataFilePath, BinaryIOMode::Read);
        cpuInput.getReader()->read(CpuDataBuf, cpuDataSize);
        cpuInput.close();
    }
    else
    {
        CpuDataBuf = new char[cpuDataSize];
        memcpy_s(CpuDataBuf, cpuDataSize, &CpuData, cpuDataSize);
    }

    // cpu data
    _vseginfo_t cpuseginfo = pak->CreateNewSegment(sizeof(MaterialCPUHeader) + cpuDataSize, SF_CPU | SF_TEMP, 8, 16);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.m_nUnknownRPtr.index = cpuseginfo.index;
    cpuhdr.m_nUnknownRPtr.offset = sizeof(MaterialCPUHeader);
    cpuhdr.m_nDataSize = cpuDataSize;

    pak->AddPointer(cpuseginfo.index, 0);

    char* cpuData = new char[sizeof(MaterialCPUHeader) + cpuDataSize];

    // copy header into the start
    memcpy_s(cpuData, sizeof(MaterialCPUHeader), &cpuhdr, sizeof(MaterialCPUHeader));

    // copy the rest of the data after the header
    memcpy_s(cpuData + sizeof(MaterialCPUHeader), cpuDataSize, CpuDataBuf, cpuDataSize);

    //////////////////////////////////////////

    pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)mtlHdr });
    pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)dataBuf });
    pak->AddRawDataBlock({ cpuseginfo.index, cpuseginfo.size, (uint8_t*)cpuData });

    //////////////////////////////////////////

    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), subhdrinfo.index, 0, subhdrinfo.size, cpuseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = MATL_VERSION;

    asset.pageEnd = cpuseginfo.index + 1;
    asset.unk1 = guids.size() + 1;

    asset.AddGuids(&guids);
    assetEntries->push_back(asset);
}