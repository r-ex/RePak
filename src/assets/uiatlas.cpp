#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

void Assets::AddUIImageAsset_v10(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    // get the info for the ui atlas image
    const char* const atlasPath = JSON_GetValueRequired<const char*>(mapEntry, "atlas");

    Log("Auto-adding txtr asset \"%s\".\n", atlasPath);
    AddTextureAsset(pak, 0, atlasPath, 
        true/*streaming disabled as uimg can not be streamed*/,
        true/*error if already added because we cannot reliably check if streaming was disabled*/);

    const PakGuid_t atlasGuid = RTech::StringToGuid(atlasPath);
    PakAsset_t* const atlasAsset = pak->GetAssetByGuid(atlasGuid, nullptr);

    // this really shouldn't happen, since the texture is either automatically added above, or a fatal error is thrown
    // there is no code path in AddTextureAsset in which the texture does not exist after the call and still continues execution
    if (!atlasAsset) [[ unlikely ]]
        Error("Atlas asset was not found when trying to add uimg asset '%s'. Make sure that the txtr is above the uimg in your map file. Exiting...\n", assetPath);

    // grab the dimensions of the atlas
    const std::string filePath = Utils::ChangeExtension(pak->GetAssetPath() + atlasPath, ".dds");
    BinaryIO atlas;

    if (!atlas.Open(filePath, BinaryIO::Mode_e::Read))
        Error("Failed to open atlas asset '%s'\n", filePath.c_str());

    atlas.SeekGet(4);
    DDS_HEADER ddsh = atlas.Read<DDS_HEADER>();

    atlas.Close();

    rapidjson::Value::ConstMemberIterator texturesIt;
    JSON_GetRequired(mapEntry, "textures", JSONFieldType_e::kArray, texturesIt);

    const rapidjson::Value::ConstArray& textureArray = texturesIt->value.GetArray();
    const uint16_t textureCount = static_cast<uint16_t>(textureArray.Size());

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
    pHdr->unkCount = 0;
    pHdr->atlasGUID = atlasGuid;

    // calculate data sizes so we can allocate a page and segment
    const size_t textureOffsetsDataSize = sizeof(UIImageOffset) * textureCount;
    const size_t textureDimensionsDataSize = sizeof(uint16_t) * 2 * textureCount;
    const size_t textureHashesDataSize = (sizeof(uint32_t) + sizeof(uint32_t)) * textureCount;

    // get total size
    const size_t textureInfoPageSize = textureOffsetsDataSize + textureDimensionsDataSize + textureHashesDataSize /*+ (4 * nTexturesCount)*/;

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
    for (const rapidjson::Value& it : textureArray)
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

    for (const rapidjson::Value& it : textureArray)
    {
        const uint16_t width = (uint16_t)JSON_GetNumberRequired<int>(it, "width");
        const uint16_t height = (uint16_t)JSON_GetNumberRequired<int>(it, "height");

        tiBuf.write<uint16_t>(width);
        tiBuf.write<uint16_t>(height);
    }

    // set texture hashes page index and offset
    pHdr->pTextureHashes = textureInfoChunk.GetPointer(textureOffsetsDataSize + textureDimensionsDataSize);

    // TODO: is this used?
    //uint32_t nextStringTableOffset = 0;

    /////////////////////////
    // IMAGE HASHES/NAMES
    for (const rapidjson::Value& it : textureArray)
    {
        rapidjson::Value::ConstMemberIterator pathIt;
        JSON_GetRequired(it, "path", pathIt);

        const char* const texturePath = pathIt->value.GetString();
        const uint32_t pathHash = RTech::StringToUIMGHash(texturePath);

        tiBuf.write(pathHash);

        // offset into the path table for this texture - not really needed since we don't write the image names
        tiBuf.write(0ul);

        //nextStringTableOffset += textIt->value.GetStringLength();
    }

    // add the file relation from this uimg asset to the atlas txtr
    if (atlasAsset)
        pak->SetCurrentAssetAsDependentForAsset(atlasAsset);
    else
        Warning("unable to find texture asset locally for uimg asset. assuming it is external...\n");

    rmem uvBuf(dataChunk.Data());

    //////////////
    // IMAGE UVS
    for (const rapidjson::Value& it : textureArray)
    {
        UIImageUV uiiu;

        const float uv0x = JSON_GetNumberRequired<float>(it, "posX") / pHdr->width;
        const float uv1x = JSON_GetNumberRequired<float>(it, "width") / pHdr->width;

        Log("X: %f -> %f\n", uv0x, uv0x + uv1x);

        const float uv0y = JSON_GetNumberRequired<float>(it, "posY") / pHdr->height;
        const float uv1y = JSON_GetNumberRequired<float>(it, "height") / pHdr->height;

        Log("Y: %f -> %f\n", uv0y, uv0y + uv1y);

        uiiu.InitUIImageUV(uv0x, uv0y, uv1x, uv1y);
        uvBuf.write(uiiu);
    }

    // create and init the asset entry
    PakAsset_t asset;

    asset.InitAsset(assetPath, Pak_GetGuidOverridable(mapEntry, assetPath), hdrChunk.GetPointer(), hdrChunk.GetSize(), dataChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::UIMG);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = UIMG_VERSION;

    asset.pageEnd = pak->GetNumPages(); // number of the highest page that the asset references pageidx + 1
    asset.remainingDependencyCount = 2;

    // this asset only has one guid reference so im just gonna do it here
    asset.AddGuid(hdrChunk.GetPointer(offsetof(UIImageAtlasHeader_t, atlasGUID)));

    // add the asset entry
    pak->PushAsset(asset);
}
