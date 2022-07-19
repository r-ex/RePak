#include "pch.h"
#include "Assets.h"

void Assets::AddMaterialAsset(std::vector<RPakAssetEntryV7>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding matl asset '%s'\n", assetPath);

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
    _vseginfo_t subhdrinfo = RePak::CreateNewSegment(sizeof(MaterialHeaderV12), 0, 8);

    // asset data
    _vseginfo_t dataseginfo = RePak::CreateNewSegment(dataBufSize, 1, 64);

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

    int textureIdx = 0;
    int fileRelationIdx = -1;
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the first TextureGUID Map.
    {
        if (it.GetStdString() != "")
        {
            uint64_t textureGUID = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str()); // Convert texture path to guid.
            *(uint64_t*)dataBuf = textureGUID;
            RePak::RegisterGuidDescriptor(dataseginfo.index, guidPageOffset + (textureIdx * sizeof(uint64_t))); // Register GUID descriptor for current texture index.

            if (fileRelationIdx == -1)
                fileRelationIdx = RePak::AddFileRelation(assetEntries->size());
            else
                RePak::AddFileRelation(assetEntries->size());

            RPakAssetEntryV7* txtrAsset = RePak::GetAssetByGuid(assetEntries, textureGUID, nullptr);

            txtrAsset->m_nRelationsStartIdx = fileRelationIdx;
            txtrAsset->m_nRelationsCounts++;

            assetUsesCount++;
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++; // Next texture index coming up.
    }

    textureIdx = 0; // reset index for next TextureGUID Section.
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the second TextureGUID Map.
    {
        if (it.GetStdString() != "")
        {
            //uint64_t guid = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str());

            // there should not be anything here so it shall remain 0
            uint64_t guid = 0x0000000000000000;

            *(uint64_t*)dataBuf = guid;
            //RePak::RegisterGuidDescriptor(dataseginfo.index, guidPageOffset + textureRefSize + (textureIdx * sizeof(uint64_t)));

            //RePak::AddFileRelation(assetEntries->size());

            if (guid != 0)
            {
                RPakAssetEntryV7* txtrAsset = RePak::GetAssetByGuid(assetEntries, guid, nullptr);

                txtrAsset->m_nRelationsStartIdx = fileRelationIdx;
                txtrAsset->m_nRelationsCounts++;


                assetUsesCount++; // Next texture index coming up.
            }
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++;
    }

    // ===============================
    // write the surface names into the buffer.
    // this is an extremely janky way to do this but I don't know better, basically it writes surface2 first so then the first can overwrite it.
    // please someone do this better I beg you.
    if (mapEntry.HasMember("surface2")) {

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
    mtlHdr->Name.m_nIndex = dataseginfo.index;
    mtlHdr->Name.m_nOffset = 0;

    mtlHdr->SurfaceName.m_nIndex = dataseginfo.index;
    mtlHdr->SurfaceName.m_nOffset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2);

    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, Name));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, SurfaceName));

    if (mapEntry.HasMember("surface2")) {

        mtlHdr->SurfaceName2.m_nIndex = dataseginfo.index;
        mtlHdr->SurfaceName2.m_nOffset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize * 2) + (surface.length() + 1);

        RePak::RegisterDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, SurfaceName2));

    }

    // V12 Type Handling
    if (version == 12)
    {

        if (type == "gen")
        {

            if (subtype == "loadscreen") {

                mtlHdr->Flags2 = 0x10000002;

                mtlHdr->ShaderSetGUID = 0xA5B8D4E9A3364655;

            }
            else {

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

            if (subtype == "test1") {

                mtlHdr->ShaderSetGUID = 0x8FB5DB9ADBEB1CBC;

                mtlHdr->Flags2 = 0x72000002;

            }
            else {

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

            if (subtype == "worldmodel") {

                // supports a set of seven textures.
                // viewmodel shadersets don't seem to allow ilm in third person, this set supports it.
                mtlHdr->ShaderSetGUID = 0x586783F71E99553D;

                mtlHdr->Flags2 = 0x56000020;

            }
            else if (subtype == "worldmodel_skn31") {

                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->ShaderSetGUID = 0x5F8181FEFDB0BAD8;

                mtlHdr->Flags2 = 0x56040020;

            }
            else if (subtype == "worldmodel_noglow") {

                // supports a set of six textures, lacks ilm.
                // there is a different one used for viewmodels, unsure what difference it makes considering the lack of ilm.
                mtlHdr->ShaderSetGUID = 0x477A8F31B5963070;

                mtlHdr->Flags2 = 0x56000020;

            }
            else if (subtype == "worldmodel_skn31_noglow") {

                // supports a set of six textures plus a set of two relating to detail textures (camos), lacks ilm.
                // same as above, why.
                mtlHdr->ShaderSetGUID = 0xC9B736D2C8027726;

                mtlHdr->Flags2 = 0x56040020;

            }
            else if (subtype == "viewmodel") {

                // supports a set of seven textures.
                // worldmodel shadersets don't seem to allow ilm in first person, this set supports it.
                mtlHdr->ShaderSetGUID = 0x5259835D8C44A14D;

                mtlHdr->Flags2 = 0x56000020;

            }
            else if (subtype == "viewmodel_skn31") {

                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->ShaderSetGUID = 0x19F840A12774CA4C;

                mtlHdr->Flags2 = 0x56040020;

            }
            else if (subtype == "nose_art") {

                mtlHdr->ShaderSetGUID = 0x3DAD868FA7485BDD;

                mtlHdr->Flags2 = 0x56000023;

            }
            else {

                Warning("Invalid type used! Defaulting to subtype 'viewmodel'... \n");

                // same as 'viewmodel'.
                mtlHdr->ShaderSetGUID = 0x5259835D8C44A14D;

                mtlHdr->Flags2 = 0x56000020;

            }

            if (subtype == "nose_art") {

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

                RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs));
                RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 8);
                RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 16);

                RePak::AddFileRelation(assetEntries->size(), 3);
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

            if (subtype == "worldmodel") {

                // supports a set of seven textures.
                // viewmodel shadersets don't seem to allow ilm in third person, this set supports it.
                mtlHdr->ShaderSetGUID = 0xC3ACAF7F1DC7F389;

                mtlHdr->Flags2 = 0x56000020;

            }
            else if (subtype == "worldmodel_skn31") {

                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->ShaderSetGUID = 0x4CFB9F15FD2DE909;

                mtlHdr->Flags2 = 0x56040020;

            }
            else if (subtype == "worldmodel_noglow") {

                // supports a set of six textures, lacks ilm.
                // there is a different one used for viewmodels, unsure what difference it makes considering the lack of ilm.
                mtlHdr->ShaderSetGUID = 0x34A7BB3C163A8139;

                mtlHdr->Flags2 = 0x56000020;

            }
            else if (subtype == "worldmodel_skn31_noglow") {

                // supports a set of six textures plus a set of two relating to detail textures (camos), lacks ilm.
                // same as above, why.
                mtlHdr->ShaderSetGUID = 0x98EA4745D8801A9B;

                mtlHdr->Flags2 = 0x56040020;

            }
            else if (subtype == "viewmodel") {

                // supports a set of seven textures.
                // worldmodel shadersets don't seem to allow ilm in first person, this set supports it.
                mtlHdr->ShaderSetGUID = 0xBD04CCCC982F8C15;

                mtlHdr->Flags2 = 0x56000020;

            }
            else if (subtype == "viewmodel_skn31") {

                // supports a set of seven textures plus a set of two relating to detail textures (camos).
                mtlHdr->ShaderSetGUID = 0x07BF4EC4B9632A03;

                mtlHdr->Flags2 = 0x56040020;

            }
            else if (subtype == "nose_art") {

                mtlHdr->ShaderSetGUID = 0x6CBEA6FE48218FAA;

                mtlHdr->Flags2 = 0x56000023;

            }
            else if (subtype == "test1") {

                mtlHdr->ShaderSetGUID = 0x942791681799941D;

                mtlHdr->Flags2 = 0x56040022;

            }
            else {

                Warning("Invalid type used! Defaulting to subtype 'viewmodel'... \n");

                // same as 'viewmodel'.
                mtlHdr->ShaderSetGUID = 0xBD04CCCC982F8C15;

                mtlHdr->Flags2 = 0x56000020;

            }

            if (subtype == "nose_art") {

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

                mtlHdr->GUIDRefs[0] = 0xA4728358C3B043CA;
                mtlHdr->GUIDRefs[1] = 0x370BABA9D9147F3D;
                mtlHdr->GUIDRefs[2] = 0x12DCE94708487F8C;

                RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs));
                RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 8);
                RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 16);

                RePak::AddFileRelation(assetEntries->size(), 3);
                assetUsesCount += 3;

            }
            

            mtlHdr->ImageFlags = 0x1D0300;

            mtlHdr->Unknown2 = 0x40D33E8F;

        }

    }
    
    // unsure how to swap the struct depending on version but this should work otherwise.
    /*
    // some funny apex people can test the subtypes if they care enough.
    // V16 Type Handling
    else if (version == 16)
    {

        if (type == "sknp")
        {
            // unsure if apex still has the issue of needing viewmodel and worldmodel shadersets
            if (subtype == "default") {

                mtlHdr->ShaderSetGUID = 0x1D9FFF314E152725;

                mtlHdr->Flags2 = 0x72000000;

            }
            else if (subtype == "skn31") {

                mtlHdr->ShaderSetGUID = 0xDFDD369CFE3A4817;

                mtlHdr->Flags2 = 0x56040020;

            }
            else {

                Warning("Invalid type used! Defaulting to subtype 'default'... \n");

                mtlHdr->ShaderSetGUID = 0x1D9FFF314E152725;

                mtlHdr->Flags2 = 0x72000000;

            }

            // These should always be constant (per each material type)
            // There's different versions of these for each material type
            // GUIDRefs[4] is Colpass entry.
            mtlHdr->GUIDRefs[0] = 0x2B93C99C67CC8B51;
            mtlHdr->GUIDRefs[1] = 0x1EBD063EA03180C7;
            mtlHdr->GUIDRefs[2] = 0xF95A7FA9E8DE1A0E;
            mtlHdr->GUIDRefs[3] = 0x227C27B608B3646B;

            RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeader, GUIDRefs));
            RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeader, GUIDRefs) + 8);
            RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeader, GUIDRefs) + 16);
            RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeader, GUIDRefs) + 24);

            RePak::AddFileRelation(assetEntries->size(), 4);
            assetUsesCount += 4;

            for (int i = 0; i < 2; ++i)
            {
                for (int j = 0; j < 8; ++j)
                    mtlHdr->UnkSections[i].Unknown5[j] = 0xf0000000;

                mtlHdr->UnkSections[i].Unknown6 = 4;
            }

            mtlHdr->ImageFlags = 0x1D0300;

            mtlHdr->Unknown2 = 0x1F5A92BD;

        }
        else if (type == "wldc")
        {
            //this has been updated to reflect the new structure but nothing other than that.
            if (subtype == "default") {

                mtlHdr->ShaderSetGUID = 0x4B0F3B4CBD009096;

                mtlHdr->Flags2 = 0x72000000;

            }
            else {

                Warning("Invalid type used! Defaulting to subtype 'default'... \n");

                mtlHdr->ShaderSetGUID = 0x4B0F3B4CBD009096;

                mtlHdr->Flags2 = 0x72000000;

            }

            // GUIDRefs[4] is Colpass entry which is optional for wldc.
            mtlHdr->GUIDRefs[0] = 0x435FA77E363BEA48; // DepthShadow
            mtlHdr->GUIDRefs[1] = 0xF734F96BE92E0E71; // DepthPrepass
            mtlHdr->GUIDRefs[2] = 0xD306370918620EC0; // DepthVSM
            mtlHdr->GUIDRefs[3] = 0xDAB17AEAD2D3387A; // DepthShadowTight

            RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeader, GUIDRefs));
            RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeader, GUIDRefs) + 8);
            RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeader, GUIDRefs) + 16);
            RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeader, GUIDRefs) + 24);

            RePak::AddFileRelation(assetEntries->size(), 4);
            assetUsesCount += 4;

            for (int i = 0; i < 2; ++i)
            {
                for (int j = 0; j < 8; ++j)
                    mtlHdr->UnkSections[i].Unknown5[j] = 0xf0000000;

                mtlHdr->UnkSections[i].Unknown6 = 4;
            }

            mtlHdr->ImageFlags = 0x1D0300;

            mtlHdr->Unknown2 = 0x1F5A92BD;

        }
        else if (type == "gen")
        {
            // hehe haha I'm not doing the research for this.
            Warning("Type 'gen' is not supported currently!!!");
            return;

        }

    }*/
    
    RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, ShaderSetGUID));
    RePak::AddFileRelation(assetEntries->size());
    assetUsesCount++;

    // Is this a colpass asset?
    bool bColpass = false;
    if (mapEntry.HasMember("colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + "_" + type + ".rpak";
        mtlHdr->GUIDRefs[3] = RTech::StringToGuid(colpassPath.c_str());

        // todo, the relations count is not being set properly on the colpass for whatever reason.
        RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, GUIDRefs) + 24);
        RePak::AddFileRelation(assetEntries->size());
        assetUsesCount++;

        bColpass = false;
    }

    mtlHdr->TextureGUIDs.m_nIndex = dataseginfo.index;
    mtlHdr->TextureGUIDs.m_nOffset = guidPageOffset;

    mtlHdr->TextureGUIDs2.m_nIndex = dataseginfo.index;
    mtlHdr->TextureGUIDs2.m_nOffset = guidPageOffset + textureRefSize;

    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, TextureGUIDs));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(MaterialHeaderV12, TextureGUIDs2));

    mtlHdr->something2 = 0x100000;

    std::uint64_t cpuDataSize = sizeof(MaterialCPUDataV12);

    // cpu data
    _vseginfo_t cpuseginfo = RePak::CreateNewSegment(sizeof(MaterialCPUHeader) + cpuDataSize, 3, 16);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.m_nUnknownRPtr.m_nIndex = cpuseginfo.index;
    cpuhdr.m_nUnknownRPtr.m_nOffset = sizeof(MaterialCPUHeader);
    cpuhdr.m_nDataSize = cpuDataSize;

    RePak::RegisterDescriptor(cpuseginfo.index, 0);

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

    if (mapEntry.HasMember("color2")) {

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

    if (mapEntry.HasMember("detailtransform")) {

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

    RePak::AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)mtlHdr });
    RePak::AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)dataBuf });
    RePak::AddRawDataBlock({ cpuseginfo.index, cpuseginfo.size, (uint8_t*)cpuData });

    //////////////////////////////////////////

    //  todo make thise swap depending on version, probably a global rpak version.
    RPakAssetEntryV7 asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), subhdrinfo.index, 0, subhdrinfo.size, cpuseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.m_nVersion = version;

    asset.m_nPageEnd = cpuseginfo.index + 1;
    // this isn't even fully true in some apex materials.
    //asset.unk1 = bColpass ? 7 : 8; // what
    // unk1 appears to be maxusecount, although seemingly nothing is affected by changing it unless you exceed 18.
    // In every TF|2 material asset entry I've looked at it's always UsesCount + 1.
    asset.unk1 = assetUsesCount + 1;

    asset.m_nUsesStartIdx = fileRelationIdx;
    asset.m_nUsesCount = assetUsesCount;

    assetEntries->push_back(asset);
}