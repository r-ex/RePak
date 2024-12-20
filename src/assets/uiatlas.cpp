#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

extern bool Texture_AutoAddTexture(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const bool forceDisableStreaming);

void Assets::AddUIImageAsset_v10(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    PakAsset_t asset;

    // get the info for the ui atlas image
    const char* const atlasPath = JSON_GetValueRequired<const char*>(mapEntry, "atlas");
    const PakGuid_t atlasGuid = RTech::StringToGuid(atlasPath);

    // note: we error here as we can't check if it was added as a streamed texture, and uimg doesn't support texture streaming.
    if (!Texture_AutoAddTexture(pak, atlasGuid, atlasPath, true/*streaming disabled as uimg can not be streamed*/))
        Error("Atlas asset \"%s\" with GUID 0x%llX was already added as 'txtr' asset; it can only be added through an 'uimg' asset.\n", atlasPath, atlasGuid);

    PakAsset_t* const atlasAsset = pak->GetAssetByGuid(atlasGuid, nullptr);

    // this really shouldn't be triggered, since the texture is either automatically added above, or a fatal error is thrown
    // there is no code path in AddTextureAsset in which the texture does not exist after the call and still continues execution
    if (!atlasAsset) [[ unlikely ]]
    {
        assert(0);
        Error("Atlas asset was not found when trying to add 'uimg' asset \"%s\".\n", assetPath);
    }

    // make sure referenced asset is a texture for sanity
    atlasAsset->EnsureType(TYPE_TXTR);

    rapidjson::Value::ConstMemberIterator texturesIt;
    JSON_GetRequired(mapEntry, "textures", JSONFieldType_e::kArray, texturesIt);

    const rapidjson::Value::ConstArray& textureArray = texturesIt->value.GetArray();
    const uint16_t textureCount = static_cast<uint16_t>(textureArray.Size());

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(UIImageAtlasHeader_t), SF_HEAD | SF_CLIENT, 8);

    UIImageAtlasHeader_t* const  pHdr = reinterpret_cast<UIImageAtlasHeader_t*>(hdrChunk.Data());
    const TextureAssetHeader_t* const atlasHdr = reinterpret_cast<const TextureAssetHeader_t*>(atlasAsset->header);

    pHdr->width = atlasHdr->width;
    pHdr->height = atlasHdr->height;

    pHdr->widthRatio = 1.f / pHdr->width;
    pHdr->heightRatio = 1.f / pHdr->height;

    // legion uses this to get the texture count, so its probably set correctly
    pHdr->textureCount = textureCount;
    pHdr->unkCount = 0;
    pHdr->atlasGUID = atlasGuid;

    Pak_RegisterGuidRefAtOffset(pak, atlasGuid, offsetof(UIImageAtlasHeader_t, atlasGUID), hdrChunk, asset, atlasAsset);

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

    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), dataChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::UIMG);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = UIMG_VERSION;
    asset.pageEnd = pak->GetNumPages(); // number of the highest page that the asset references pageidx + 1

    // add the asset entry
    pak->PushAsset(asset);
}
