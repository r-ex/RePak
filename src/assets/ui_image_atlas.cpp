#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

extern bool Texture_AutoAddTexture(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const bool forceDisableStreaming);

// todo:
// - keepDevOnly names

// page lump structure and order:
// - header        HEAD        (align=8)
// - image offsets CPU_CLIENT  (align=32)
// - information   CPU_CLIENT  (align=4?16) unknown, dimensions, then hashes, aligned to 16 if we have unknown (which needs to be reserved still).
// - uv data       TEMP_CLIENT (align=4)
void Assets::AddUIImageAsset_v10(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    // get the info for the ui atlas image
    const char* const atlasPath = JSON_GetValueRequired<const char*>(mapEntry, "atlas");
    const PakGuid_t atlasGuid = RTech::StringToGuid(atlasPath);

    // note: we error here as we can't check if it was added as a streamed texture, and uimg doesn't support texture streaming.
    if (!Texture_AutoAddTexture(pak, atlasGuid, atlasPath, true/*streaming disabled as uimg can not be streamed*/))
        Error("Atlas asset \"%s\" with GUID 0x%llX was already added as 'txtr' asset; it can only be added through an 'uimg' asset.\n", atlasPath, atlasGuid);

    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
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

    rapidjson::Value::ConstMemberIterator imagesIt;
    JSON_GetRequired(mapEntry, "images", JSONFieldType_e::kArray, imagesIt);

    const rapidjson::Value::ConstArray& imageArray = imagesIt->value.GetArray();
    const uint16_t imageCount = static_cast<uint16_t>(imageArray.Size());

    // needs to be reversed still, not all uimg's use this! this might be
    // necessary to reverse at some point since some uimg's (especially in
    // world rui's) seem to flicker or glitch at certain view angles and the
    // only data we currently do not set is this.
    const uint16_t unkCount = 0;

    PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(UIImageAtlasHeader_t), SF_HEAD | SF_CLIENT, 8);

    UIImageAtlasHeader_t* const pHdr = reinterpret_cast<UIImageAtlasHeader_t*>(hdrLump.data);
    const TextureAssetHeader_t* const atlasHdr = reinterpret_cast<const TextureAssetHeader_t*>(atlasAsset->header);

    pHdr->width = atlasHdr->width;
    pHdr->height = atlasHdr->height;

    pHdr->widthRatio = 1.f / pHdr->width;
    pHdr->heightRatio = 1.f / pHdr->height;

    // legion uses this to get the image count, so its probably set correctly
    pHdr->imageCount = imageCount;
    pHdr->unkCount = unkCount;
    pHdr->atlasGUID = atlasGuid;

    Pak_RegisterGuidRefAtOffset(atlasGuid, offsetof(UIImageAtlasHeader_t, atlasGUID), hdrLump, asset);

    const size_t imageOffsetsDataSize = sizeof(UIImageOffset) * imageCount;

    // ui image offset info
    PakPageLump_s offsetLump = pak->CreatePageLump(imageOffsetsDataSize, SF_CPU | SF_CLIENT, 32);
    rmem ofBuf(offsetLump.data);

    // set image offset page index and offset
    pak->AddPointer(hdrLump, offsetof(UIImageAtlasHeader_t, pImageOffsets), offsetLump, 0);

    ////////////////////
    // IMAGE OFFSETS
    for (const rapidjson::Value& it : imageArray)
    {
        UIImageOffset uiio;
        uiio.f0 = uiio.f1 = 0.0f; // These don't do anything.

        uiio.endAnchorX = JSON_GetValueOrDefault(it, "endAnchorX", 1.0f);
        uiio.endAnchorY = JSON_GetValueOrDefault(it, "endAnchorY", 1.0f);

        uiio.startAnchorX = JSON_GetValueOrDefault(it, "startAnchorX", 0.0f);
        uiio.startAnchorY = JSON_GetValueOrDefault(it, "startAnchorY", 0.0f);

        // Lower means more zoomed in.
        uiio.scaleRatioX = JSON_GetValueOrDefault(it, "scaleRatioX", 1.0f);
        uiio.scaleRatioY = JSON_GetValueOrDefault(it, "scaleRatioY", 1.0f);

        // Original code -- incorrect? Just let the user specify these
        // as external tools can calculate these, and it probably also
        // is responsibility of user/external tools to calculate it.
        /*
        float startX = it["posX"].GetFloat() / pHdr->width;
        float endX = (it["posX"].GetFloat() + it["width"].GetFloat()) / pHdr->width;

        float startY = it["posY"].GetFloat() / pHdr->height;
        float endY = (it["posY"].GetFloat() + it["height"].GetFloat()) / pHdr->height;

        // this doesn't affect legion but does affect game?
        uiio.InitUIImageOffset(startX, startY, endX, endY);
        */

        ofBuf.write(uiio);
    }

    const size_t imageDimensionsDataSize = sizeof(uint16_t) * 2 * imageCount;
    const size_t imageHashesDataSize = (sizeof(uint32_t) + sizeof(uint32_t)) * imageCount;

    // note: aligned to 4 if we do not have UIImageAtlasHeader_t::unkCount
    // (which needs to be reversed still). Else this lump must reside in a
    // page that is aligned to 16.
    PakPageLump_s infoLump = pak->CreatePageLump(imageDimensionsDataSize + imageHashesDataSize, SF_CPU | SF_CLIENT, unkCount > 0 ? 16 : 4);
    rmem ifBuf(infoLump.data);

    ///////////////////////
    // IMAGE DIMENSIONS
    // set image dimensions page index and offset
    pak->AddPointer(hdrLump, offsetof(UIImageAtlasHeader_t, pImageDimensions), infoLump, 0);

    for (const rapidjson::Value& it : imageArray)
    {
        const uint16_t width = (uint16_t)JSON_GetNumberRequired<int>(it, "width");
        const uint16_t height = (uint16_t)JSON_GetNumberRequired<int>(it, "height");

        ifBuf.write<uint16_t>(width);
        ifBuf.write<uint16_t>(height);
    }

    // set image hashes page index and offset
    pak->AddPointer(hdrLump, offsetof(UIImageAtlasHeader_t, pImageHashes), infoLump, imageDimensionsDataSize);

    // TODO: is this used?
    //uint32_t nextStringTableOffset = 0;

    /////////////////////////
    // IMAGE HASHES/NAMES
    for (const rapidjson::Value& it : imageArray)
    {
        rapidjson::Value::ConstMemberIterator pathIt;
        JSON_GetRequired(it, "path", JSONFieldType_e::kString, pathIt);

        const char* const imagePath = pathIt->value.GetString();
        const uint32_t pathHash = RTech::StringToUIMGHash(imagePath);

        ifBuf.write(pathHash);

        // offset into the path table for this image - not really needed since we don't write the image names
        ifBuf.write(0ul);

        //nextStringTableOffset += textIt->value.GetStringLength();
    }

    // cpu data
    PakPageLump_s uvLump = pak->CreatePageLump(imageCount * sizeof(UIImageUV), SF_CPU | SF_TEMP | SF_CLIENT, 4);
    rmem uvBuf(uvLump.data);

    //////////////
    // IMAGE UVS
    for (const rapidjson::Value& it : imageArray)
    {
        UIImageUV uiiu;

        const float uv0x = JSON_GetNumberRequired<float>(it, "posX") / pHdr->width;
        const float uv1x = JSON_GetNumberRequired<float>(it, "width") / pHdr->width;

        Debug("X: %f -> %f\n", uv0x, uv0x + uv1x);

        const float uv0y = JSON_GetNumberRequired<float>(it, "posY") / pHdr->height;
        const float uv1y = JSON_GetNumberRequired<float>(it, "height") / pHdr->height;

        Debug("Y: %f -> %f\n", uv0y, uv0y + uv1y);

        uiiu.InitUIImageUV(uv0x, uv0y, uv1x, uv1y);
        uvBuf.write(uiiu);
    }

    asset.InitAsset(hdrLump.GetPointer(), sizeof(UIImageAtlasHeader_t), uvLump.GetPointer(), UIMG_VERSION, AssetType::UIMG);
    asset.SetHeaderPointer(hdrLump.data);

    pak->FinishAsset();
}
