#include "pch.h"
#include "Assets.h"

// VERSION 7
void Assets::AddUIImageAsset_r2(std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding uimg asset '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    ///////////////////////
    // JSON VALIDATION
    {
        // atlas checks
        if (!mapEntry.HasMember("atlas"))
        {
            Error("Required field 'atlas' not found for uimg asset '%s'. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
        }
        else if (!mapEntry["atlas"].IsString())
        {
            Error("'atlas' field is not of required type 'string' for uimg asset '%s'. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
        }

        // textures checks
        if (!mapEntry.HasMember("textures"))
        {
            Error("Required field 'textures' not found for uimg asset '%s'. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
        }
        else if (!mapEntry["textures"].IsArray())
        {
            Error("'textures' field is not of required type 'array' for uimg asset '%s'. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
        }

        // validate fields for each texture
        for (auto& it : mapEntry["textures"].GetArray())
        {
            if (!it.HasMember("path"))
            {
                Error("Required field 'path' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            else if (!it["path"].IsString())
            {
                Error("'path' field is not of required type 'string' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }

            if (!it.HasMember("width"))
            {
                Error("Required field 'width' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            // technically this could be a float i think? Going to limit it to ints for now though for simplicity
            else if (!it["width"].IsInt())
            {
                Error("'width' field is not of required type 'int' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }

            if (!it.HasMember("height"))
            {
                Error("Required field 'height' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            // technically this could be a float i think? Going to limit it to ints for now though for simplicity
            else if (!it["height"].IsInt())
            {
                Error("'height' field is not of required type 'int' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }

            if (!it.HasMember("posX"))
            {
                Error("Required field 'posX' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            // technically this could be a float i think? Going to limit it to ints for now though for simplicity
            else if (!it["posX"].IsInt())
            {
                Error("'posX' field is not of required type 'int' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }

            if (!it.HasMember("posY"))
            {
                Error("Required field 'posY' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            // technically this could be a float i think? Going to limit it to ints for now though for simplicity
            else if (!it["posY"].IsInt())
            {
                Error("'posY' field is not of required type 'int' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
        }
    }

    // get the info for the ui atlas image
    std::string sAtlasFilePath = g_sAssetsDir + mapEntry["atlas"].GetStdString() + ".dds";
    std::string sAtlasAssetName = mapEntry["atlas"].GetStdString() + ".rpak";
    uint64_t atlasGuid = RTech::StringToGuid(sAtlasAssetName.c_str());

    // get the txtr asset that this asset is using
    RPakAssetEntry* atlasAsset = RePak::GetAssetByGuid(assetEntries, atlasGuid, nullptr);

    if (!atlasAsset)
    {
        Error("Atlas asset was not found when trying to add uimg asset '%s'. Make sure that the txtr is above the uimg in your map file. Exiting...\n", assetPath);
        exit(EXIT_FAILURE);
    }

    uint32_t nTexturesCount = mapEntry["textures"].GetArray().Size();

    // grab the dimensions of the atlas
    BinaryIO atlas;
    atlas.open(sAtlasFilePath, BinaryIOMode::Read);
    atlas.seek(4, std::ios::beg);
    DDS_HEADER ddsh = atlas.read<DDS_HEADER>();

    atlas.close();

    UIImageHeader* pHdr = new UIImageHeader();
    pHdr->m_nWidth = ddsh.width;
    pHdr->m_nHeight = ddsh.height;

    // legion uses this to get the texture count, so its probably set correctly
    pHdr->m_nTextureOffsetsCount = nTexturesCount;
    // unused by legion? - might not be correct
    //pHdr->textureCount = nTexturesCount <= 1 ? 0 : nTexturesCount - 1; // don't even ask
    pHdr->m_nTextureCount = 0;
    pHdr->m_nAtlasGUID = atlasGuid;

    // calculate data sizes so we can allocate a page and segment
    uint32_t textureOffsetsDataSize = sizeof(UIImageOffset) * nTexturesCount;
    uint32_t textureDimensionsDataSize = sizeof(uint16_t) * 2 * nTexturesCount;
    uint32_t textureHashesDataSize = (sizeof(uint32_t) + sizeof(uint32_t)) * nTexturesCount;

    // get total size
    uint32_t textureInfoPageSize = textureOffsetsDataSize + textureDimensionsDataSize + textureHashesDataSize /*+ (4 * nTexturesCount)*/;

    // asset header
    _vseginfo_t subhdrinfo = RePak::CreateNewSegment(sizeof(UIImageHeader), 0x40, 8);

    // ui image/texture info
    _vseginfo_t tiseginfo = RePak::CreateNewSegment(textureInfoPageSize, 0x41, 32);

    // cpu data
    _vseginfo_t dataseginfo = RePak::CreateNewSegment(nTexturesCount * 0x10, 0x43, 4);

    // register our descriptors so they get converted properly
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, m_pTextureOffsets));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, m_pTextureDims));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, m_pTextureHashes));

    // textureGUID descriptors
    RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(UIImageHeader, m_nAtlasGUID));

    // buffer for texture info data
    char* pTextureInfoBuf = new char[textureInfoPageSize] {};
    rmem tiBuf(pTextureInfoBuf);

    // set texture offset page index and offset
    pHdr->m_pTextureOffsets = { tiseginfo.index, 0 };

    ////////////////////
    // IMAGE OFFSETS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageOffset uiio{};
        float startX = it["posX"].GetFloat() / pHdr->m_nWidth;
        float endX = (it["posX"].GetFloat() + it["width"].GetFloat()) / pHdr->m_nWidth;

        float startY = it["posY"].GetFloat() / pHdr->m_nHeight;
        float endY = (it["posY"].GetFloat() + it["height"].GetFloat()) / pHdr->m_nHeight;

        // this doesnt affect legion but does affect game?
        //uiio.InitUIImageOffset(startX, startY, endX, endY);
        tiBuf.write(uiio);
    }

    ///////////////////////
    // IMAGE DIMENSIONS
    // set texture dimensions page index and offset
    pHdr->m_pTextureDims = { tiseginfo.index, textureOffsetsDataSize };

    for (auto& it : mapEntry["textures"].GetArray())
    {
        tiBuf.write<uint16_t>(it["width"].GetInt());
        tiBuf.write<uint16_t>(it["height"].GetInt());
    }

    // set texture hashes page index and offset
    pHdr->m_pTextureHashes = { tiseginfo.index, textureOffsetsDataSize + textureDimensionsDataSize };
    //pHdr->pTextureNames = { tiseginfo.index, 0 };

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
        nextStringTableOffset += it["path"].GetStringLength() + 1;
    }

    // add the file relation from this uimg asset to the atlas txtr
    size_t fileRelationIdx = RePak::AddFileRelation(assetEntries->size());

    atlasAsset->m_nRelationsStartIdx = fileRelationIdx;
    atlasAsset->m_nRelationsCounts++;

    char* pUVBuf = new char[nTexturesCount * sizeof(UIImageUV)];
    rmem uvBuf(pUVBuf);

    //////////////
    // IMAGE UVS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageUV uiiu{};
        float uv0x = it["posX"].GetFloat() / pHdr->m_nWidth;
        float uv1x = it["width"].GetFloat() / pHdr->m_nWidth;
        Log("X: %f -> %f\n", uv0x, uv0x + uv1x);
        float uv0y = it["posY"].GetFloat() / pHdr->m_nHeight;
        float uv1y = it["height"].GetFloat() / pHdr->m_nHeight;
        Log("Y: %f -> %f\n", uv0y, uv0y + uv1y);
        uiiu.InitUIImageUV(uv0x, uv0y, uv1x, uv1y);
        uvBuf.write(uiiu);
    }

    RPakRawDataBlock shdb{ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock tib{ tiseginfo.index, tiseginfo.size, (uint8_t*)pTextureInfoBuf };
    RePak::AddRawDataBlock(tib);

    RPakRawDataBlock rdb{ dataseginfo.index, dataseginfo.size, (uint8_t*)pUVBuf };
    RePak::AddRawDataBlock(rdb);

    // create and init the asset entry
    RPakAssetEntry asset;
    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::UIMG);
    asset.m_nVersion = UIMG_VERSION;

    asset.m_nPageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 2;

    asset.m_nUsesStartIdx = fileRelationIdx;
    asset.m_nUsesCount = 1; // the asset should only use 1 other asset for the atlas

    // add the asset entry
    assetEntries->push_back(asset);
}

// VERSION 8
void Assets::AddUIImageAsset_v10(std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding uimg asset '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    ///////////////////////
    // JSON VALIDATION
    {
        // atlas checks
        if (!mapEntry.HasMember("atlas"))
        {
            Error("Required field 'atlas' not found for uimg asset '%s'. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
        }
        else if (!mapEntry["atlas"].IsString())
        {
            Error("'atlas' field is not of required type 'string' for uimg asset '%s'. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
        }

        // textures checks
        if (!mapEntry.HasMember("textures"))
        {
            Error("Required field 'textures' not found for uimg asset '%s'. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
        }
        else if (!mapEntry["textures"].IsArray())
        {
            Error("'textures' field is not of required type 'array' for uimg asset '%s'. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
        }

        // validate fields for each texture
        for (auto& it : mapEntry["textures"].GetArray())
        {
            if (!it.HasMember("path"))
            {
                Error("Required field 'path' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            else if (!it["path"].IsString())
            {
                Error("'path' field is not of required type 'string' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }

            if (!it.HasMember("width"))
            {
                Error("Required field 'width' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            // technically this could be a float i think? Going to limit it to ints for now though for simplicity
            else if (!it["width"].IsInt())
            {
                Error("'width' field is not of required type 'int' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }

            if (!it.HasMember("height"))
            {
                Error("Required field 'height' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            // technically this could be a float i think? Going to limit it to ints for now though for simplicity
            else if (!it["height"].IsInt())
            {
                Error("'height' field is not of required type 'int' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }

            if (!it.HasMember("posX"))
            {
                Error("Required field 'posX' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            // technically this could be a float i think? Going to limit it to ints for now though for simplicity
            else if (!it["posX"].IsInt())
            {
                Error("'posX' field is not of required type 'int' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }

            if (!it.HasMember("posY"))
            {
                Error("Required field 'posY' not found for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
            // technically this could be a float i think? Going to limit it to ints for now though for simplicity
            else if (!it["posY"].IsInt())
            {
                Error("'posY' field is not of required type 'int' for a texture in uimg asset '%s'. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
            }
        }
    }

    // get the info for the ui atlas image
    std::string sAtlasFilePath = g_sAssetsDir + mapEntry["atlas"].GetStdString() + ".dds";
    std::string sAtlasAssetName = mapEntry["atlas"].GetStdString() + ".rpak";
    uint64_t atlasGUID = RTech::StringToGuid(sAtlasAssetName.c_str());

    // get the txtr asset that this asset is using
    RPakAssetEntry* atlasAsset = RePak::GetAssetByGuid(assetEntries, atlasGUID, nullptr);

    if (!atlasAsset)
    {
        Error("Atlas asset was not found when trying to add uimg asset '%s'. Make sure that the txtr is above the uimg in your map file. Exiting...\n", assetPath);
        exit(EXIT_FAILURE);
    }

    uint32_t nTexturesCount = mapEntry["textures"].GetArray().Size();

    // grab the dimensions of the atlas
    BinaryIO atlas;
    atlas.open(sAtlasFilePath, BinaryIOMode::Read);
    atlas.seek(4, std::ios::beg);
    DDS_HEADER ddsh = atlas.read<DDS_HEADER>();

    atlas.close();

    UIImageHeader* pHdr = new UIImageHeader();
    pHdr->m_nWidth = ddsh.width;
    pHdr->m_nHeight = ddsh.height;

    pHdr->m_nTextureOffsetsCount = nTexturesCount;
    pHdr->m_nTextureCount = nTexturesCount == 1 ? 0 : nTexturesCount; // don't even ask

    pHdr->m_nAtlasGUID = atlasGUID;

    // calculate data sizes so we can allocate a page and segment
    uint32_t textureOffsetsDataSize = sizeof(UIImageOffset) * nTexturesCount;
    uint32_t textureDimensionsDataSize = sizeof(uint16_t) * 2 * nTexturesCount;
    uint32_t textureHashesDataSize = (sizeof(uint32_t) + sizeof(uint32_t)) * nTexturesCount;

    // get total size
    uint32_t textureInfoPageSize = textureOffsetsDataSize + textureDimensionsDataSize + textureHashesDataSize /*+ (4 * nTexturesCount)*/;

    // asset header
    _vseginfo_t subhdrinfo = RePak::CreateNewSegment(sizeof(UIImageHeader), 0x40, 8);

    // ui image/texture info
    _vseginfo_t tiseginfo = RePak::CreateNewSegment(textureInfoPageSize, 0x41, 32);

    // cpu data
    _vseginfo_t dataseginfo = RePak::CreateNewSegment(nTexturesCount * 0x10, 0x43, 4);

    // register our descriptors so they get converted properly
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, m_pTextureOffsets));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, m_pTextureDims));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, m_pTextureHashes));

    // textureGUID descriptors
    RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(UIImageHeader, m_nAtlasGUID));

    // buffer for texture info data
    char* pTextureInfoBuf = new char[textureInfoPageSize]{};
    rmem tiBuf(pTextureInfoBuf);

    // set texture offset page index and offset
    pHdr->m_pTextureOffsets = { tiseginfo.index, 0 };

    ////////////////////
    // IMAGE OFFSETS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageOffset uiio{};
        float startX = it["posX"].GetFloat() / pHdr->m_nWidth;
        float endX = (it["posX"].GetFloat() + it["width"].GetFloat()) / pHdr->m_nWidth;

        float startY = it["posY"].GetFloat() / pHdr->m_nHeight;
        float endY = (it["posY"].GetFloat() + it["height"].GetFloat()) / pHdr->m_nHeight;

        // this doesnt affect legion but does affect game?
        //uiio.InitUIImageOffset(startX, startY, endX, endY);
        tiBuf.write(uiio);
    }

    ///////////////////////
    // IMAGE DIMENSIONS
    // set texture dimensions page index and offset
    pHdr->m_pTextureDims = { tiseginfo.index, textureOffsetsDataSize };

    for (auto& it : mapEntry["textures"].GetArray())
    {
        tiBuf.write<uint16_t>(it["width"].GetInt());
        tiBuf.write<uint16_t>(it["height"].GetInt());
    }

    // set texture hashes page index and offset
    pHdr->m_pTextureHashes = { tiseginfo.index, textureOffsetsDataSize + textureDimensionsDataSize };

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
        nextStringTableOffset += it["path"].GetStringLength() + 1;
    }

    // add the file relation from this uimg asset to the atlas txtr
    size_t fileRelationIdx = RePak::AddFileRelation(assetEntries->size());

    atlasAsset->m_nRelationsStartIdx = fileRelationIdx;
    atlasAsset->m_nRelationsCounts++;

    char* pUVBuf = new char[nTexturesCount * sizeof(UIImageUV)];
    rmem uvBuf(pUVBuf);

    //////////////
    // IMAGE UVS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageUV uiiu{};
        float uv0x = it["posX"].GetFloat() / pHdr->m_nWidth;
        float uv1x = it["width"].GetFloat() / pHdr->m_nWidth;
        Log("X: %f -> %f\n", uv0x, uv0x + uv1x);
        float uv0y = it["posY"].GetFloat() / pHdr->m_nHeight;
        float uv1y = it["height"].GetFloat() / pHdr->m_nHeight;
        Log("Y: %f -> %f\n", uv0y, uv0y + uv1y);
        uiiu.InitUIImageUV(uv0x, uv0y, uv1x, uv1y);
        uvBuf.write(uiiu);
    }

    RPakRawDataBlock shdb{ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock tib{ tiseginfo.index, tiseginfo.size, (uint8_t*)pTextureInfoBuf };
    RePak::AddRawDataBlock(tib);

    RPakRawDataBlock rdb{ dataseginfo.index, dataseginfo.size, (uint8_t*)pUVBuf };
    RePak::AddRawDataBlock(rdb);

    // create and init the asset entry
    RPakAssetEntry asset;
    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::UIMG);
    asset.m_nVersion = UIMG_VERSION;

    asset.m_nPageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 2;

    asset.m_nUsesStartIdx = fileRelationIdx;
    asset.m_nUsesCount = 1; // the asset should only use 1 other asset for the atlas

    // add the asset entry
    assetEntries->push_back(asset);
}
