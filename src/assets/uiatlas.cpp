#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

void Assets::AddUIImageAsset_v10(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding uimg asset '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    ///////////////////////
    // JSON VALIDATION
    {
        // atlas checks
        if (!mapEntry.HasMember("atlas"))
            Error("Required field 'atlas' not found for uimg asset '%s'. Exiting...\n", assetPath);
        else if (!mapEntry["atlas"].IsString())
            Error("'atlas' field is not of required type 'string' for uimg asset '%s'. Exiting...\n", assetPath);

        // textures checks
        if (!mapEntry.HasMember("textures"))
            Error("Required field 'textures' not found for uimg asset '%s'. Exiting...\n", assetPath);
        else if (!mapEntry["textures"].IsArray())
            Error("'textures' field is not of required type 'array' for uimg asset '%s'. Exiting...\n", assetPath);

        // validate fields for each texture
        for (auto& it : mapEntry["textures"].GetArray())
        {
            if (!it.HasMember("path"))
                Error("Required field 'path' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
            else if (!it["path"].IsString())
                Error("'path' field is not of required type 'string' for a texture in uimg asset '%s'. Exiting...\n", assetPath);

            if (!it.HasMember("width"))
                Error("Required field 'width' not found for texture '%s' in uimg asset '%s'. Exiting...\n", it["path"].GetString(), assetPath);
            else if (!it["width"].IsNumber())
                Error("'width' field is not of required type 'number' for texture '%s' in uimg asset '%s'. Exiting...\n", it["path"].GetString(), assetPath);

            if (!it.HasMember("height"))
                Error("Required field 'height' not found for texture '%s' in uimg asset '%s'. Exiting...\n", it["path"].GetString(), assetPath);
            else if (!it["height"].IsNumber())
                Error("'height' field is not of required type 'number' for texture '%s' in uimg asset '%s'. Exiting...\n", it["path"].GetString(), assetPath);

            if (!it.HasMember("posX"))
                Error("Required field 'posX' not found for texture '%s' in uimg asset '%s'. Exiting...\n", it["path"].GetString(), assetPath);
            else if (!it["posX"].IsNumber())
                Error("'posX' field is not of required type 'number' for texture '%s' in uimg asset '%s'. Exiting...\n", it["path"].GetString(), assetPath);

            if (!it.HasMember("posY"))
                Error("Required field 'posY' not found for texture '%s' in uimg asset '%s'. Exiting...\n", it["path"].GetString(), assetPath);
            else if (!it["posY"].IsNumber())
                Error("'posY' field is not of required type 'number' for texture '%s' in uimg asset '%s'. Exiting...\n", it["path"].GetString(), assetPath);
        }
    }

    // get the info for the ui atlas image
    std::string sAtlasFilePath = pak->GetAssetPath() + mapEntry["atlas"].GetStdString() + ".dds";
    std::string sAtlasAssetName = mapEntry["atlas"].GetStdString() + ".rpak";

    AddTextureAsset(pak, 0, mapEntry["atlas"].GetString(), mapEntry.HasMember("disableStreaming") && mapEntry["disableStreaming"].GetBool(), true);

    uint64_t atlasGuid = RTech::StringToGuid(sAtlasAssetName.c_str());

    PakAsset_t* atlasAsset = pak->GetAssetByGuid(atlasGuid, nullptr);

    // this really shouldn't happen, since the texture is either automatically added above, or a fatal error is thrown
    // there is no code path in AddTextureAsset in which the texture does not exist after the call and still continues execution
    if (!atlasAsset) [[ unlikely ]]
        Error("Atlas asset was not found when trying to add uimg asset '%s'. Make sure that the txtr is above the uimg in your map file. Exiting...\n", assetPath);

    uint16_t textureCount = static_cast<uint16_t>(mapEntry["textures"].GetArray().Size());


    // grab the dimensions of the atlas
    BinaryIO atlas;
    atlas.open(sAtlasFilePath, BinaryIOMode::Read);
    atlas.seek(4, std::ios::beg);
    DDS_HEADER ddsh = atlas.read<DDS_HEADER>();

    atlas.close();


    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(UIImageAtlasHeader_t), SF_HEAD | SF_CLIENT, 8);

    UIImageAtlasHeader_t* pHdr = reinterpret_cast<UIImageAtlasHeader_t*>(hdrChunk.Data());

    assert(ddsh.dwWidth <= UINT16_MAX);
    assert(ddsh.dwHeight <= UINT16_MAX);
    pHdr->width = static_cast<uint16_t>(ddsh.dwWidth);
    pHdr->height = static_cast<uint16_t>(ddsh.dwHeight);

    pHdr->widthRatio = 1.f / pHdr->width;
    pHdr->heightRatio = 1.f / pHdr->height;

    // legion uses this to get the texture count, so its probably set correctly
    pHdr->textureCount = textureCount;
    // unused by legion? - might not be correct
    //pHdr->textureCount = nTexturesCount <= 1 ? 0 : nTexturesCount - 1; // don't even ask
    pHdr->unkCount = 0;
    pHdr->atlasGUID = atlasGuid;

    // calculate data sizes so we can allocate a page and segment
    int textureOffsetsDataSize = sizeof(UIImageOffset) * textureCount;
    int textureDimensionsDataSize = sizeof(uint16_t) * 2 * textureCount;
    int textureHashesDataSize = (sizeof(uint32_t) + sizeof(uint32_t)) * textureCount;

    // get total size
    int textureInfoPageSize = textureOffsetsDataSize + textureDimensionsDataSize + textureHashesDataSize /*+ (4 * nTexturesCount)*/;

    // ui image/texture info
    CPakDataChunk textureInfoChunk = pak->CreateDataChunk(textureInfoPageSize, SF_CPU | SF_CLIENT, 32);

    // cpu data
    CPakDataChunk dataChunk = pak->CreateDataChunk(textureCount * sizeof(UIImageUV), SF_CPU | SF_TEMP | SF_CLIENT, 4);
    
    // register our descriptors so they get converted properly
    pak->AddPointer(hdrChunk.GetPointer(offsetof(UIImageAtlasHeader_t, pTextureOffsets)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(UIImageAtlasHeader_t, pTextureDimensions)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(UIImageAtlasHeader_t, pTextureHashes)));

    rmem tiBuf(textureInfoChunk.Data());

    // set texture offset page index and offset
    pHdr->pTextureOffsets = textureInfoChunk.GetPointer();

    ////////////////////
    // IMAGE OFFSETS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageOffset uiio{};

        // TODO: commented as these were causing code warnings.
        // this should be revisited at some point.
        UNUSED(it);

        //float startX = it["posX"].GetFloat() / pHdr->width;
        //float endX = (it["posX"].GetFloat() + it["width"].GetFloat()) / pHdr->width;

        //float startY = it["posY"].GetFloat() / pHdr->height;
        //float endY = (it["posY"].GetFloat() + it["height"].GetFloat()) / pHdr->height;

        // this doesn't affect legion but does affect game?
        //uiio.InitUIImageOffset(startX, startY, endX, endY);
        tiBuf.write(uiio);
    }

    ///////////////////////
    // IMAGE DIMENSIONS
    // set texture dimensions page index and offset
    pHdr->pTextureDimensions = textureInfoChunk.GetPointer(textureOffsetsDataSize);

    for (auto& it : mapEntry["textures"].GetArray())
    {
        tiBuf.write<uint16_t>((uint16_t)it["width"].GetInt());
        tiBuf.write<uint16_t>((uint16_t)it["height"].GetInt());
    }

    // set texture hashes page index and offset
    pHdr->pTextureHashes = textureInfoChunk.GetPointer(textureOffsetsDataSize + textureDimensionsDataSize);

    // TODO: is this used?
    //uint32_t nextStringTableOffset = 0;

    /////////////////////////
    // IMAGE HASHES/NAMES
    for (auto& it : mapEntry["textures"].GetArray())
    {
        uint32_t pathHash = RTech::StringToUIMGHash(it["path"].GetString());
        tiBuf.write(pathHash);

        // offset into the path table for this texture - not really needed since we don't write the image names
        tiBuf.write(0i32);

        //nextStringTableOffset += it["path"].GetStringLength();
    }

    // add the file relation from this uimg asset to the atlas txtr
    if (atlasAsset)
        pak->SetCurrentAssetAsDependentForAsset(atlasAsset);
    else
        Warning("unable to find texture asset locally for uimg asset. assuming it is external...\n");

    rmem uvBuf(dataChunk.Data());

    //////////////
    // IMAGE UVS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageUV uiiu{};
        float uv0x = it["posX"].GetFloat() / pHdr->width;
        float uv1x = it["width"].GetFloat() / pHdr->width;
        Log("X: %f -> %f\n", uv0x, uv0x + uv1x);
        float uv0y = it["posY"].GetFloat() / pHdr->height;
        float uv1y = it["height"].GetFloat() / pHdr->height;
        Log("Y: %f -> %f\n", uv0y, uv0y + uv1y);
        uiiu.InitUIImageUV(uv0x, uv0y, uv1x, uv1y);
        uvBuf.write(uiiu);
    }

    // create and init the asset entry
    PakAsset_t asset;

    asset.InitAsset(sAssetName + ".rpak", hdrChunk.GetPointer(), hdrChunk.GetSize(), dataChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::UIMG);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = UIMG_VERSION;

    asset.pageEnd = pak->GetNumPages(); // number of the highest page that the asset references pageidx + 1
    asset.remainingDependencyCount = 2;

    // this asset only has one guid reference so im just gonna do it here
    asset.AddGuid(hdrChunk.GetPointer(offsetof(UIImageAtlasHeader_t, atlasGUID)));

    // add the asset entry
    pak->PushAsset(asset);
}
