#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

extern bool Texture_AutoAddTexture(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const bool forceDisableStreaming);

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
        Error("Atlas texture \"%s\" with GUID 0x%llX was already added as Texture asset; it can only be added through an UI image atlas asset.\n", atlasPath, atlasGuid);

    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
    PakAsset_t* const atlasAsset = pak->GetAssetByGuid(atlasGuid, nullptr);

    // this really shouldn't be triggered, since the texture is either automatically added above, or a fatal error is thrown
    // there is no code path in AddTextureAsset in which the texture does not exist after the call and still continues execution
    if (!atlasAsset) [[ unlikely ]]
    {
        assert(0);
        Error("Internal failure while adding atlas texture \"%s\" with GUID 0x%llX.\n", atlasPath, assetGuid);
    }

    // make sure referenced asset is a texture for sanity
    atlasAsset->EnsureType(TYPE_TXTR);

    rapidjson::Value::ConstMemberIterator imagesIt;
    JSON_GetRequired(mapEntry, "images", JSONFieldType_e::kArray, imagesIt);

    const rapidjson::Value::ConstArray& imageArray = imagesIt->value.GetArray();
    const size_t imageArraySize = imageArray.Size();

    if (imageArraySize > MAX_UI_ATLAS_IMAGES)
        Error("UI image atlas contains too many images (max %zu, got %zu).\n", (size_t)MAX_UI_ATLAS_IMAGES, imageArraySize);

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
    pHdr->imageCount = static_cast<uint16_t>(imageArraySize);
    pHdr->unkCount = unkCount;
    pHdr->atlasGUID = atlasGuid;

    Pak_RegisterGuidRefAtOffset(atlasGuid, offsetof(UIImageAtlasHeader_t, atlasGUID), hdrLump, asset);

    const size_t imageOffsetsDataSize = sizeof(UIImageOffset) * imageArraySize;

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
        uiio.cropInsetLeft = JSON_GetValueOrDefault(it, "cropInsetLeft", 0.0f);
        uiio.cropInsetTop = JSON_GetValueOrDefault(it, "cropInsetTop", 0.0f);

        uiio.endAnchorX = JSON_GetValueOrDefault(it, "endAnchorX", 1.0f);
        uiio.endAnchorY = JSON_GetValueOrDefault(it, "endAnchorY", 1.0f);

        uiio.startAnchorX = JSON_GetValueOrDefault(it, "startAnchorX", 0.0f);
        uiio.startAnchorY = JSON_GetValueOrDefault(it, "startAnchorY", 0.0f);

        // Lower means more zoomed in.
        uiio.scaleRatioX = JSON_GetValueOrDefault(it, "scaleRatioX", 1.0f);
        uiio.scaleRatioY = JSON_GetValueOrDefault(it, "scaleRatioY", 1.0f);

        // [amos]: tools like TexturePacker can automatically create entire ui image atlases.
        // TexturePacker can also trim out transparent area's (exactly like how the original
        // Respawn UI image atlas textures have their transparency clipped out). The purpose
        // of this `UIImageAtlasOffset` structure is to account for this; the scaleRatioX
        // and scaleRatioY can zoom the image back out again to reconstruct the trimmed out
        // transparency in the runtime. This is very ideal for keeping the image atlas size
        // as low as possible.
        // 
        // TexturePacker exports the following data alongside the generated atlas texture:
        // {
        // 	"filename": "rui/hud/tactical_icons/pilot_tactical_particle_wall",
        // 	"frame": {"x":1921,"y":1501,"w":62,"h":80},
        // 	"rotated": false,
        // 	"trimmed": true,
        // 	"spriteSourceSize": {"x":33,"y":24,"w":62,"h":80},
        // 	"sourceSize": {"w":128,"h":128}
        // },
        // {
        // 	"filename": "rui/menu/buttons/weapon_categories/marksman",
        // 	"frame": {"x":1226,"y":1,"w":526,"h":329},
        // 	"rotated": false,
        // 	"trimmed": true,
        // 	"spriteSourceSize": {"x":138,"y":55,"w":526,"h":329},
        // 	"sourceSize": {"w":802,"h":440}
        // },
        // 
        // It keeps the source sprite size and positions and provides the actual (new) size
        // and positions of the sprite within the atlas, the idea is to figure out how to
        // compute the scale ratios and anchors based on these values.
        // 
        // For scaleRatio, this seems to get very close:
        // uiio.scaleRatioX = 1.0f + ((float)(sourceWidth - croppedWidth) / (float)croppedWidth);
        // uiio.scaleRatioY = 1.0f + ((float)(sourceHeight - croppedHeight) / (float)croppedHeight);
        // 
        // I'm not sure how to calculate the anchors correctly yet, time ran out when I
        // started to poke around with those. These need to be figured out so that the
        // images are correctly placed again on the RUI mesh within the runtime as scaling
        // does offset the image slightly (which the anchors need to correct).
        // 
        // 
        // Also, on Respawn UI image atlases, the cropInsetLeft and cropInsetTop variables
        // are sometimes set as well, these aren't very important, but probably need more
        // research as well...
        /*
        uint32_t sourcePosX;
        uint32_t sourcePosY;
        uint32_t sourceWidth;
        uint32_t sourceHeight;

        if (JSON_GetValue(it, "sourcePosX", JSONFieldType_e::kUint32, sourcePosX) &&
            JSON_GetValue(it, "sourcePosY", JSONFieldType_e::kUint32, sourcePosY) &&
            JSON_GetValue(it, "sourceWidth", JSONFieldType_e::kUint32, sourceWidth) &&
            JSON_GetValue(it, "sourceHeight", JSONFieldType_e::kUint32, sourceHeight)
            )
        {
            const uint16_t croppedWidth = (uint16_t)JSON_GetNumberRequired<uint32_t>(it, "width");
            const uint16_t croppedHeight = (uint16_t)JSON_GetNumberRequired<uint32_t>(it, "height");

            uiio.scaleRatioX = (float)sourceWidth / (float)croppedWidth;
            uiio.scaleRatioY = (float)sourceHeight / (float)croppedHeight;

            uiio.startAnchorX = (float)sourcePosX / (float)sourceWidth;
            uiio.startAnchorY = (float)sourcePosY / (float)sourceHeight;
            uiio.endAnchorX = ((float)sourcePosX + (float)croppedWidth) / (float)sourceWidth;
            uiio.endAnchorY = ((float)sourcePosY + (float)croppedHeight) / (float)sourceHeight;
        }
        else // Default scale.
        {
            uiio.endAnchorX = 1.0f;
            uiio.endAnchorY = 1.0f;

            uiio.startAnchorX = 0.0f;
            uiio.startAnchorY = 0.0f;

            uiio.scaleRatioX = 1.0f;
            uiio.scaleRatioY = 1.0f;
        }
        */
        ofBuf.write(uiio);
    }

    const size_t imageDimensionsDataSize = sizeof(uint16_t) * 2 * imageArraySize;
    const size_t imageHashesDataSize = (sizeof(uint32_t) + sizeof(uint32_t)) * imageArraySize;

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
    size_t stringBufSize = 0;

    if (pak->IsFlagSet(PF_KEEP_DEV))
    {
        int index = -1;

        for (const rapidjson::Value& it : imageArray)
        {
            index++;

            rapidjson::Value::ConstMemberIterator pathIt;
            JSON_GetRequired(it, "path", JSONFieldType_e::kString, pathIt);

            const size_t pathLen = pathIt->value.GetStringLength();

            if (pathLen == 0)
                Error("Image #%i has an empty name!\n", index);

            stringBufSize += pathLen + 1; // +1 for null terminator.
        }
    }

    PakPageLump_s devLump{};

    if (stringBufSize > 0)
    {
        devLump = pak->CreatePageLump(stringBufSize, SF_CPU | SF_DEV, 1);
        pak->AddPointer(hdrLump, offsetof(UIImageAtlasHeader_t, pImagesNames), devLump, 0);
    }

    uint32_t nextStringTableOffset = 0;
    int index = -1;

    /////////////////////////
    // IMAGE HASHES/NAMES
    for (const rapidjson::Value& it : imageArray)
    {
        index++;

        rapidjson::Value::ConstMemberIterator pathIt;
        JSON_GetRequired(it, "path", JSONFieldType_e::kString, pathIt);

        const size_t pathLen = pathIt->value.GetStringLength();

        if (pathLen == 0)
            Error("Image #%i has an empty name!\n", index);

        const char* const imagePath = pathIt->value.GetString();
        const uint32_t pathHash = RTech::StringToUIMGHash(imagePath);

        ifBuf.write(pathHash);

        if (devLump.data)
        {
            const size_t pathBufSize = pathLen + 1; // +1 for null terminator.
            memcpy(&devLump.data[nextStringTableOffset], imagePath, pathBufSize);

            ifBuf.write(nextStringTableOffset);
            nextStringTableOffset += (uint32_t)pathBufSize;
        }
        else
        {
            // No dev data, don't write the image path.
            ifBuf.write(0ul);
        }
    }

    // cpu data
    PakPageLump_s uvLump = pak->CreatePageLump(imageArraySize * sizeof(UIImageUV), SF_CPU | SF_TEMP | SF_CLIENT, 4);
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
