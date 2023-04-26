#include "pch.h"
#include "assets.h"
#include "public/material.h"

// VERSION 7
void Assets::AddMaterialAsset_v12(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding matl asset '%s'\n", assetPath);

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

    mtlHdr->guid = RTech::StringToGuid(sFullAssetRpakPath.c_str()); // Convert full rpak asset path to textureGUID and set it in the material header.

    // this was for 'UnknownSignature' but isn't valid anymore I think.
    // Game ignores this field when parsing, retail rpaks also have this as 0. But In-Game its being set to either 0x4, 0x5, 0x9.
    // Based on resolution.
    // 512x512 = 0x5
    // 1024x1024 = 0x4
    // 2048x2048 = 0x9

    // Game ignores this field when parsing, retail rpaks also have this as 0. But In-Game its being set to the number of textures with streamed mip levels.
    if (mapEntry.HasMember("streamedtexturecount"))
        mtlHdr->numStreamingTextureHandles = mapEntry["streamedtexturecount"].GetInt();

    if (mapEntry.HasMember("width")) // Set material width.
        mtlHdr->width = mapEntry["width"].GetInt();

    if (mapEntry.HasMember("height")) // Set material width.
        mtlHdr->height = mapEntry["height"].GetInt();

    if (mapEntry.HasMember("imageflags")) // Set flags properly. Responsible for texture stretching, tiling etc.
        mtlHdr->flags = mapEntry["imageflags"].GetUint();

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

        mtlHdr->unkSections[0].VisibilityFlags = visFlag;
        mtlHdr->unkSections[1].VisibilityFlags = visFlag;

    }

    if (mapEntry.HasMember("faceflags")) {
        mtlHdr->unkSections[0].FaceDrawingFlags = mapEntry["faceflags"].GetInt();
        mtlHdr->unkSections[1].FaceDrawingFlags = mapEntry["faceflags"].GetInt();
        Log("Using faceflags, only touch this if you know what you're doing! \n");
    }
    else {
        mtlHdr->unkSections[0].FaceDrawingFlags = 0x0006;
        mtlHdr->unkSections[1].FaceDrawingFlags = 0x0006;
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

    std::vector<PakGuidRefHdr_t> guids{};

    int textureIdx = 0;
    int fileRelationIdx = -1;
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the first TextureGUID Map.
    {
        if (it.GetStdString() != "")
        {
            uint64_t textureGUID = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str()); // Convert texture path to guid.
            *(uint64_t*)dataBuf = textureGUID;
            pak->AddGuidDescriptor(&guids, dataseginfo.index, guidPageOffset + (textureIdx * sizeof(uint64_t))); // Register GUID descriptor for current texture index.

            PakAsset_t* txtrAsset = pak->GetAssetByGuid(textureGUID, nullptr);

            if (txtrAsset)
                txtrAsset->AddRelation(assetEntries->size());
            else
                Warning("unable to find texture '%s' for material '%s' within the local assets\n", it.GetString(), assetPath);
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++; // Next texture index coming up.
    }
    dataBuf += textureRefSize;

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
    mtlHdr->materialName.index = dataseginfo.index;
    mtlHdr->materialName.offset = 0;

    mtlHdr->surfaceProp.index = dataseginfo.index;
    mtlHdr->surfaceProp.offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2);

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, materialName));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, surfaceProp));

    if (mapEntry.HasMember("surface2")) {

        mtlHdr->surfaceProp2.index = dataseginfo.index;
        mtlHdr->surfaceProp2.offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2) + (surface.length() + 1);

        pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, surfaceProp2));
    }

    // V12 Type Handling
    if (version == 12)
    {
        if (type == "gen")
        {
            if (subtype == "loadscreen")
            {
                mtlHdr->flags2 = 0x10000002;

                mtlHdr->shaderSet = 0xA5B8D4E9A3364655;
            }
            else
            {
                Warning("Invalid type used! Defaulting to subtype 'loadscreen'... \n");

                mtlHdr->flags2 = 0x10000002;

                mtlHdr->shaderSet = 0xA5B8D4E9A3364655;
            }

            mtlHdr->flags = 0x050300;

            mtlHdr->unk3 = 0xFBA63181;
        }
        else if (type == "wld")
        {
            Warning("Type 'wld' is not supported currently!!!");

            if (subtype == "test1")
            {
                mtlHdr->shaderSet = 0x8FB5DB9ADBEB1CBC;

                mtlHdr->flags2 = 0x72000002;
            }
            else
            {
                Warning("Invalid type used! Defaulting to subtype 'viewmodel'... \n");

                // same as 'viewmodel'.
                mtlHdr->shaderSet = 0x8FB5DB9ADBEB1CBC;

                mtlHdr->flags2 = 0x72000002;
            }

            mtlHdr->unkSections[0].UnkRenderFlags = 0x00000005;

            mtlHdr->unkSections[1].UnkRenderFlags = 0x00000005;

            mtlHdr->flags = 0x1D0300;

            mtlHdr->unk3 = 0x40D33E8F;
        }
        else if (type == "fix")
        {
            if (subtype == "worldmodel")
            {
                // supports a set of seven textures.
                // viewmodel shadersets don't seem to allow ilm in third person, this set supports it.
                mtlHdr->shaderSet = 0x586783F71E99553D;

                mtlHdr->flags2 = 0x56000020;
            }
            else if (subtype == "worldmodel_skn31")
            {
                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->shaderSet = 0x5F8181FEFDB0BAD8;

                mtlHdr->flags2 = 0x56040020;
            }
            else if (subtype == "worldmodel_noglow")
            {
                // supports a set of six textures, lacks ilm.
                // there is a different one used for viewmodels, unsure what difference it makes considering the lack of ilm.
                mtlHdr->shaderSet = 0x477A8F31B5963070;

                mtlHdr->flags2 = 0x56000020;
            }
            else if (subtype == "worldmodel_skn31_noglow")
            {
                // supports a set of six textures plus a set of two relating to detail textures (camos), lacks ilm.
                // same as above, why.
                mtlHdr->shaderSet = 0xC9B736D2C8027726;

                mtlHdr->flags2 = 0x56040020;
            }
            else if (subtype == "viewmodel")
            {
                // supports a set of seven textures.
                // worldmodel shadersets don't seem to allow ilm in first person, this set supports it.
                mtlHdr->shaderSet = 0x5259835D8C44A14D;

                mtlHdr->flags2 = 0x56000020;
            }
            else if (subtype == "viewmodel_skn31")
            {
                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->shaderSet = 0x19F840A12774CA4C;

                mtlHdr->flags2 = 0x56040020;
            }
            else if (subtype == "nose_art")
            {
                mtlHdr->shaderSet = 0x3DAD868FA7485BDD;

                mtlHdr->flags2 = 0x56000023;
            }
            else
            {
                Warning("Invalid type used! Defaulting to subtype 'viewmodel'... \n");

                // same as 'viewmodel'.
                mtlHdr->shaderSet = 0x5259835D8C44A14D;

                mtlHdr->flags2 = 0x56000020;
            }

            if (subtype == "nose_art")
            {
                for (int i = 0; i < 2; ++i)
                {
                    mtlHdr->unkSections[i].UnkRenderLighting = 0xF0138286;
                    mtlHdr->unkSections[i].UnkRenderAliasing = 0xF0138286;
                    mtlHdr->unkSections[i].UnkRenderDoF = 0xF0008286;
                    mtlHdr->unkSections[i].UnkRenderUnknown = 0x00138286;

                    mtlHdr->unkSections[i].UnkRenderFlags = 0x00000005;
                }
            }
            else {

                for (int i = 0; i < 2; ++i)
                {
                    mtlHdr->unkSections[i].UnkRenderLighting = 0xF0138004;
                    mtlHdr->unkSections[i].UnkRenderAliasing = 0xF0138004;
                    mtlHdr->unkSections[i].UnkRenderDoF = 0xF0138004;
                    mtlHdr->unkSections[i].UnkRenderUnknown = 0x00138004;

                    mtlHdr->unkSections[i].UnkRenderFlags = 0x00000004;
                }

                mtlHdr->depthShadowMaterial = 0x39C739E9928E555C;
                mtlHdr->depthPrepassMaterial = 0x67D89B36EDCDDF6E;
                mtlHdr->depthVSMMaterial = 0x43A9D8D429698B9F;
            }

            mtlHdr->flags = 0x1D0300;

            mtlHdr->unk3 = 0x40D33E8F;

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
                mtlHdr->shaderSet = 0xC3ACAF7F1DC7F389;

                mtlHdr->flags2 = 0x56000020;
            }
            else if (subtype == "worldmodel_skn31")
            {
                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->shaderSet = 0x4CFB9F15FD2DE909;

                mtlHdr->flags2 = 0x56040020;
            }
            else if (subtype == "worldmodel_noglow")
            {
                // supports a set of six textures, lacks ilm.
                // there is a different one used for viewmodels, unsure what difference it makes considering the lack of ilm.
                mtlHdr->shaderSet = 0x34A7BB3C163A8139;

                mtlHdr->flags2 = 0x56000020;
            }
            else if (subtype == "worldmodel_skn31_noglow")
            {
                // supports a set of six textures plus a set of two relating to detail textures (camos), lacks ilm.
                // same as above, why.
                mtlHdr->shaderSet = 0x98EA4745D8801A9B;

                mtlHdr->flags2 = 0x56040020;
            }
            else if (subtype == "viewmodel")
            {
                // supports a set of seven textures.
                // worldmodel shadersets don't seem to allow ilm in first person, this set supports it.
                mtlHdr->shaderSet = 0xBD04CCCC982F8C15;

                mtlHdr->flags2 = 0x56000020;
            }
            else if (subtype == "viewmodel_skn31")
            {
                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->shaderSet = 0x07BF4EC4B9632A03;

                mtlHdr->flags2 = 0x56040020;
            }
            else if (subtype == "nose_art")
            {
                mtlHdr->shaderSet = 0x6CBEA6FE48218FAA;

                mtlHdr->flags2 = 0x56000023;
            }
            else if (subtype == "test1")
            {
                mtlHdr->shaderSet = 0x942791681799941D;

                mtlHdr->flags2 = 0x56040022;
            }
            else
            {
                Warning("Invalid type used! Defaulting to subtype 'viewmodel'... \n");

                // same as 'viewmodel'.
                mtlHdr->shaderSet = 0xBD04CCCC982F8C15;

                mtlHdr->flags2 = 0x56000020;
            }

            if (subtype == "nose_art")
            {
                for (int i = 0; i < 2; ++i)
                {
                    mtlHdr->unkSections[i].UnkRenderLighting = 0xF0138286;
                    mtlHdr->unkSections[i].UnkRenderAliasing = 0xF0138286;
                    mtlHdr->unkSections[i].UnkRenderDoF = 0xF0008286;
                    mtlHdr->unkSections[i].UnkRenderUnknown = 0x00138286;

                    mtlHdr->unkSections[i].UnkRenderFlags = 0x00000005;
                }
            }
            else
            {
                for (int i = 0; i < 2; ++i)
                {
                    mtlHdr->unkSections[i].UnkRenderLighting = 0xF0138004;
                    mtlHdr->unkSections[i].UnkRenderAliasing = 0xF0138004;
                    mtlHdr->unkSections[i].UnkRenderDoF = 0xF0138004;
                    mtlHdr->unkSections[i].UnkRenderUnknown = 0x00138004;

                    mtlHdr->unkSections[i].UnkRenderFlags = 0x00000004;
                }

                mtlHdr->depthShadowMaterial = 0xA4728358C3B043CA;
                mtlHdr->depthPrepassMaterial = 0x370BABA9D9147F3D;
                mtlHdr->depthVSMMaterial = 0x12DCE94708487F8C;
            }

            mtlHdr->flags = 0x1D0300;

            mtlHdr->unk3 = 0x40D33E8F;

        }
    }

    pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, shaderSet));

    if (mtlHdr->shaderSet != 0)
    {
        PakAsset_t* asset = pak->GetAssetByGuid(mtlHdr->shaderSet);

        if (asset)
            asset->AddRelation(assetEntries->size());
    }

    // Is this a colpass asset?
    bool bColpass = false;
    if (mapEntry.HasMember("colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + "_" + type + ".rpak";
        mtlHdr->colpassMaterial = RTech::StringToGuid(colpassPath.c_str());
    }

    for (int i = 0; i < 4; ++i)
    {
        uint64_t guid = *((uint64_t*)&mtlHdr->depthShadowMaterial + i);

        if (guid != 0)
        {
            pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV12, depthShadowMaterial) + (i*8));

            PakAsset_t* asset = pak->GetAssetByGuid(guid);

            if (asset)
                asset->AddRelation(assetEntries->size());
        }
    }

    mtlHdr->textureHandles.index = dataseginfo.index;
    mtlHdr->textureHandles.offset = guidPageOffset;

    mtlHdr->streamingTextureHandles.index = dataseginfo.index;
    mtlHdr->streamingTextureHandles.offset = guidPageOffset + textureRefSize;

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, textureHandles));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV12, streamingTextureHandles));

    mtlHdr->unk5 = 0x100000;

    std::uint64_t cpuDataSize = sizeof(MaterialCPUDataV12);

    // cpu data
    _vseginfo_t cpuseginfo = pak->CreateNewSegment(sizeof(MaterialCPUHeader) + cpuDataSize, 3, 16);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.dataPtr.index = cpuseginfo.index;
    cpuhdr.dataPtr.offset = sizeof(MaterialCPUHeader);
    cpuhdr.dataSize = cpuDataSize;

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
    PakAsset_t asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), subhdrinfo.index, 0, subhdrinfo.size, cpuseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = version;

    asset.pageEnd = cpuseginfo.index + 1;
    // this isn't even fully true in some apex materials.
    //asset.unk1 = bColpass ? 7 : 8; // what
    // unk1 appears to be maxusecount, although seemingly nothing is affected by changing it unless you exceed 18.
    // In every TF|2 material asset entry I've looked at it's always UsesCount + 1.
    asset.remainingDependencyCount = guids.size() + 1;

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);
}

// VERSION 8
void Assets::AddMaterialAsset_v15(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding matl asset '%s'\n", assetPath);

    MaterialHeaderV15* mtlHdr = new MaterialHeaderV15();
    std::string sAssetPath = std::string(assetPath);

    std::string type = "sknp";

    if (mapEntry.HasMember("type"))
        type = mapEntry["type"].GetStdString();
    else
        Warning("Adding material without an explicitly defined type. Assuming 'sknp'...\n");

    std::string sFullAssetRpakPath = "material/" + sAssetPath + "_" + type + ".rpak"; // Make full rpak asset path.

    mtlHdr->guid = RTech::StringToGuid(sFullAssetRpakPath.c_str()); // Convert full rpak asset path to guid and set it in the material header.

    // Game ignores this field when parsing, retail rpaks also have this as 0. But In-Game its being set to either 0x4, 0x5, 0x9.
    // Based on resolution.
    // 512x512 = 0x5
    // 1024x1024 = 0x4
    // 2048x2048 = 0x9
    //if (mapEntry.HasMember("signature"))
    //    mtlHdr->StreamableTextureCount = mapEntry["signature"].GetInt();

    if (mapEntry.HasMember("width")) // Set material width.
        mtlHdr->width = mapEntry["width"].GetInt();

    if (mapEntry.HasMember("height")) // Set material width.
        mtlHdr->height = mapEntry["height"].GetInt();

    if (mapEntry.HasMember("flags")) // Set flags properly. Responsible for texture stretching, tiling etc.
        mtlHdr->flags_78 = mapEntry["flags"].GetUint();

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

    std::vector<PakGuidRefHdr_t> guids{};

    int textureIdx = 0;
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the first TextureGUID Map.
    {
        if (it.IsString() && it.GetStdString() != "")
        {
            uint64_t textureGUID = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str()); // Convert texture path to guid.
            *(uint64_t*)dataBuf = textureGUID;
            pak->AddGuidDescriptor(&guids, dataseginfo.index, guidPageOffset + (textureIdx * sizeof(uint64_t))); // Register GUID descriptor for current texture index.

            PakAsset_t* txtrAsset = pak->GetAssetByGuid(textureGUID, nullptr);

            if (txtrAsset)
                txtrAsset->AddRelation(assetEntries->size());
            else
                Warning("unable to find texture '%s' for material '%s' within the local assets\n", it.GetString(), assetPath);
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
    mtlHdr->materialName.index = dataseginfo.index;
    mtlHdr->materialName.offset = 0;

    mtlHdr->surfaceProp.index = dataseginfo.index;
    mtlHdr->surfaceProp.offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2);

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, materialName));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, surfaceProp));

    // Shader Type Handling
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

    bool bColpass = false; // is this colpass material?

    // get referenced colpass material if exists
    if (mapEntry.HasMember("colpass") && mapEntry["colpass"].IsString())
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
            pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(MaterialHeaderV15, depthShadowMaterial) + (i*8));

            PakAsset_t* asset = pak->GetAssetByGuid(guid);

            if (asset)
                asset->AddRelation(assetEntries->size());
        }
    }

    mtlHdr->textureHandles.index = dataseginfo.index;
    mtlHdr->textureHandles.offset = guidPageOffset;

    mtlHdr->streamingTextureHandles.index = dataseginfo.index;
    mtlHdr->streamingTextureHandles.offset = guidPageOffset + textureRefSize;

    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, textureHandles));
    pak->AddPointer(subhdrinfo.index, offsetof(MaterialHeaderV15, streamingTextureHandles));

    mtlHdr->unk_88 = 0x72000000;
    mtlHdr->unk_8C = 0x100000;

    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8; ++j)
            mtlHdr->unkSections[i].unk_0[j] = 0xf0000000;

        uint32_t f1 = bColpass ? 0x5 : 0x17;

        mtlHdr->unkSections[i].m_UnkRenderFlags = 4;
        mtlHdr->unkSections[i].m_VisibilityFlags = f1;
        mtlHdr->unkSections[i].m_FaceDrawingFlags = 6;
    }

    //////////////////////////////////////////
    /// cpu

    char* dxStaticBuf = nullptr;
    uint64_t dxStaticBufSize = 0;

    std::string cpuPath = pak->GetAssetPath() + sAssetPath + "_" + type + ".cpu";
    if (FILE_EXISTS(cpuPath))
    {
        dxStaticBufSize = Utils::GetFileSize(cpuPath);

        dxStaticBuf = new char[dxStaticBufSize];

        std::ifstream cpuIn(cpuPath, std::ios::in | std::ios::binary);
        cpuIn.read(dxStaticBuf, dxStaticBufSize);
        cpuIn.close();
    }
    else {
        dxStaticBufSize = 544;
        dxStaticBuf = new char[dxStaticBufSize];

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

        memcpy(dxStaticBuf, testData, dxStaticBufSize);
    }

    // cpu data
    _vseginfo_t cpuseginfo = pak->CreateNewSegment(sizeof(MaterialCPUHeader) + dxStaticBufSize, SF_CPU | SF_TEMP, 16);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.dataPtr.index = cpuseginfo.index;
    cpuhdr.dataPtr.offset = sizeof(MaterialCPUHeader);
    cpuhdr.dataSize = dxStaticBufSize;

    pak->AddPointer(cpuseginfo.index, 0);

    char* cpuData = new char[sizeof(MaterialCPUHeader) + dxStaticBufSize];

    // copy header into the start
    memcpy_s(cpuData, sizeof(MaterialCPUHeader), &cpuhdr, sizeof(MaterialCPUHeader));

    // copy the rest of the data after the header
    memcpy_s(cpuData + sizeof(MaterialCPUHeader), dxStaticBufSize, dxStaticBuf, dxStaticBufSize);

    //////////////////////////////////////////

    pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)mtlHdr });
    pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)dataBuf });
    pak->AddRawDataBlock({ cpuseginfo.index, cpuseginfo.size, (uint8_t*)cpuData });

    //////////////////////////////////////////

    PakAsset_t asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), subhdrinfo.index, 0, subhdrinfo.size, cpuseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.version = MATL_VERSION;

    asset.pageEnd = cpuseginfo.index + 1;
    asset.remainingDependencyCount = bColpass ? 7 : 8; // what

    asset.AddGuids(&guids);

    assetEntries->push_back(asset);
}
