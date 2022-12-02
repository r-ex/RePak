#include "pch.h"
#include "Assets.h"
#include <dxutils.h>
#include "assets/rui.h"

void Assets::AddUIImageAsset_v10(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("\n==============================\n");
    Log("Asset uimg -> '%s'\n", assetPath);

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
    std::string sAtlasFilePath = g_sAssetsDir + mapEntry["atlas"].GetStdString() + ".dds";
    std::string sAtlasAssetName = mapEntry["atlas"].GetStdString() + ".rpak";
    uint64_t atlasGuid = RTech::StringToGuid(sAtlasAssetName.c_str());

    // get the txtr asset that this asset is using
    RPakAssetEntry* atlasAsset = pak->GetAssetByGuid(atlasGuid, nullptr);

    if (!atlasAsset)
        Error("Atlas asset was not found when trying to add uimg asset '%s'. Make sure that the txtr is above the uimg in your map file. Exiting...\n", assetPath);

    uint32_t nTexturesCount = mapEntry["textures"].GetArray().Size();

    // grab the dimensions of the atlas
    BinaryIO atlas;
    atlas.open(sAtlasFilePath, BinaryIOMode::Read);
    atlas.seek(4, std::ios::beg);
    DDS_HEADER ddsh = atlas.read<DDS_HEADER>();

    atlas.close();

    UIImageHeader* pHdr = new UIImageHeader();
    pHdr->Width = ddsh.dwWidth;
    pHdr->Height = ddsh.dwHeight;

    const float Widthf = static_cast<float>(pHdr->Width);
    const float Heightf = static_cast<float>(pHdr->Height);

    pHdr->WidthRatio = 1.0 / Widthf;
    pHdr->HeightRatio = 1.0 / Heightf;

    // legion uses this to get the texture count, so its probably set correctly
    pHdr->TextureOffsetsCount = nTexturesCount;
    // unused by legion? - might not be correct
    //pHdr->textureCount = nTexturesCount <= 1 ? 0 : nTexturesCount - 1; // don't even ask
    pHdr->TextureCount = 0;
    pHdr->TextureGuid = atlasGuid;

    // calculate data sizes so we can allocate a page and segment
    uint32_t textureOffsetsDataSize = sizeof(UIImageOffset) * nTexturesCount;
    uint32_t textureDimensionsDataSize = sizeof(uint16_t) * 2 * nTexturesCount;
    uint32_t textureHashesDataSize = (sizeof(uint32_t) + sizeof(uint32_t)) * nTexturesCount;

    // get total size
    uint32_t textureInfoPageSize = textureOffsetsDataSize + textureDimensionsDataSize + textureHashesDataSize /*+ (4 * nTexturesCount)*/;

    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(UIImageHeader), SF_HEAD | SF_CLIENT, 8, 8);

    // ui image/texture info
    _vseginfo_t tiseginfo = pak->CreateNewSegment(textureInfoPageSize, SF_CPU | SF_CLIENT, 32, 32);

    // cpu data
    _vseginfo_t dataseginfo = pak->CreateNewSegment(nTexturesCount * 0x10, SF_CPU | SF_TEMP | SF_CLIENT, 4, 4);
    
    // register our descriptors so they get converted properly
    pak->AddPointer(subhdrinfo.index, offsetof(UIImageHeader, pTextureOffsets));
    pak->AddPointer(subhdrinfo.index, offsetof(UIImageHeader, pTextureDimensions));
    pak->AddPointer(subhdrinfo.index, offsetof(UIImageHeader, pTextureHashes));

    // buffer for texture info data
    char* pTextureInfoBuf = new char[textureInfoPageSize] {};
    rmem tiBuf(pTextureInfoBuf);

    // set texture offset page index and offset
    pHdr->pTextureOffsets = { tiseginfo.index, 0 };

    ////////////////////
    // IMAGE OFFSETS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        const Vector2& pos{ it["posX"].GetFloat() , it["posY"].GetFloat() };
        const Vector2& res{ it["width"].GetFloat() , it["height"].GetFloat() };

        UIImageOffset uiio{};

        Vector2 start = { 
            (pos.x / Widthf),
            (pos.y / Heightf)
        };

        Vector2 end = { 
            (pos.x + Widthf) / Widthf,
            (pos.y + Heightf) / Heightf
        };
        
        // this doesnt affect legion but does affect game?
        //uiio.InitUIImageOffset(start, end);
        tiBuf.write(uiio);
    }

    ///////////////////////
    // IMAGE DIMENSIONS
    // set texture dimensions page index and offset
    pHdr->pTextureDimensions = { tiseginfo.index, textureOffsetsDataSize };

    for (auto& it : mapEntry["textures"].GetArray())
    {
        tiBuf.write<uint16_t>(it["width"].GetInt());
        tiBuf.write<uint16_t>(it["height"].GetInt());
    }

    // set texture hashes page index and offset
    pHdr->pTextureHashes = { tiseginfo.index, textureOffsetsDataSize + textureDimensionsDataSize };

    uint32_t nextStringTableOffset = 0;

    /////////////////////////
    // IMAGE HASHES/NAMES
    for (auto& it : mapEntry["textures"].GetArray())
    {
        uint32_t pathHash = RTech::StringToUIMGHash(it["path"].GetString());
        tiBuf.write(pathHash);

        // offset into the path table for this texture
        // NOTE: this is set regardless of whether the path table exists in original rpaks
        tiBuf.write(nextStringTableOffset);

        // was + 1 before but that guranteed a broken third texture, if any issues like this: https://imgur.com/a/BIYMZwV occour again look into this futher.
        nextStringTableOffset += 0;
    }

    // add the file relation from this uimg asset to the atlas txtr
    if (atlasAsset)
        atlasAsset->AddRelation(assetEntries->size());

    char* pUVBuf = new char[nTexturesCount * sizeof(UIImageUV)];
    rmem uvBuf(pUVBuf);

    //////////////
    // IMAGE UVS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        const Vector2& pos{ it["posX"].GetFloat() , it["posY"].GetFloat() };
        const Vector2& res{ it["width"].GetFloat() , it["height"].GetFloat() };

        UIImageUV uiiu{};

        Vector2 uv0 = { pos.x / Widthf , pos.y / Heightf };
        Vector2 uv1 = { res.x / Widthf , res.y / Heightf };

        Log("X: %f -> %f\n", uv0.x, uv0.x + uv1.x);
        Log("Y: %f -> %f\n", uv0.y, uv0.y + uv1.y);

        uiiu.InitUIImageUV( uv0, uv1);
        uvBuf.write(uiiu);
    }

    pak->AddRawDataBlock( { subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr } );
    pak->AddRawDataBlock( { tiseginfo.index, tiseginfo.size, (uint8_t*)pTextureInfoBuf } );
    pak->AddRawDataBlock( { dataseginfo.index, dataseginfo.size, (uint8_t*)pUVBuf } );


    // create and init the asset entry
    RPakAssetEntry asset;
    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::UIMG);
    asset.version = UIMG_VERSION;

    asset.pageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 2;

    // this asset only has one guid reference so im just gonna do it here
    asset.AddGuid({ subhdrinfo.index, offsetof(UIImageHeader, TextureGuid) });

    // add the asset entry
    assetEntries->push_back(asset);
}
