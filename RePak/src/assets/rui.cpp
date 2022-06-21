#include "pch.h"
#include "Assets.h"

void Assets::AddUIImageAsset(std::vector<RPakAssetEntryV7>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding uimg asset '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    // get the info for the ui atlas image
    std::string sAtlasFilePath = g_sAssetsDir + mapEntry["atlas"].GetStdString() + ".dds";
    std::string sAtlasAssetName = mapEntry["atlas"].GetStdString() + ".rpak";
    uint64_t atlasGuid = RTech::StringToGuid(sAtlasAssetName.c_str());

    // get the txtr asset that this asset is using
    RPakAssetEntryV7* atlasAsset = RePak::GetAssetByGuid(assetEntries, atlasGuid, nullptr);

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
    pHdr->width = ddsh.width;
    pHdr->height = ddsh.height;

    pHdr->textureOffsetsCount = nTexturesCount;
    pHdr->textureCount = nTexturesCount == 1 ? 0 : nTexturesCount; // don't even ask

    pHdr->atlasGuid = atlasGuid;

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
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, pTextureOffsets));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, pTextureDims));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(UIImageHeader, pTextureHashes));

    // textureGUID descriptors
    RePak::RegisterGuidDescriptor(subhdrinfo.index, offsetof(UIImageHeader, atlasGuid));

    // buffer for texture info data
    char* pTextureInfoBuf = new char[textureInfoPageSize]{};
    rmem tiBuf(pTextureInfoBuf);

    // set texture offset page index and offset
    pHdr->pTextureOffsets = { tiseginfo.index, 0 };

    ////////////////////
    // IMAGE OFFSETS
    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageOffset uiio{};
        float startX = it["posX"].GetFloat() / pHdr->width;
        float endX = ( it["posX"].GetFloat() + it["width"].GetFloat() ) / pHdr->width;

        float startY = it["posY"].GetFloat() / pHdr->height;
        float endY = ( it["posY"].GetFloat() + it["height"].GetFloat() ) / pHdr->height;

        uiio.InitUIImageOffset(startX, startY, endX, endY);
        tiBuf.write(uiio);
    }

    ///////////////////////
    // IMAGE DIMENSIONS
    // set texture dimensions page index and offset
    pHdr->pTextureDims = { tiseginfo.index, textureOffsetsDataSize };

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
        float startX = it["posX"].GetFloat() / pHdr->width;
        float width = it["width"].GetFloat() / pHdr->width;
        Log("X: %f -> %f\n", startX, startX + width);
        float startY = it["posY"].GetFloat() / pHdr->height;
        float height = it["height"].GetFloat() / pHdr->height;
        Log("Y: %f -> %f\n", startY, startY + height);
        uiiu.InitUIImageUV(startX, startY, width, height);
        uvBuf.write(uiiu);
    }

    RPakRawDataBlock shdb{ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock tib{ tiseginfo.index, tiseginfo.size, (uint8_t*)pTextureInfoBuf };
    RePak::AddRawDataBlock(tib);

    RPakRawDataBlock rdb{ dataseginfo.index, dataseginfo.size, (uint8_t*)pUVBuf };
    RePak::AddRawDataBlock(rdb);

    // create and init the asset entry
    RPakAssetEntryV7 asset;
    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::UIMG);
    asset.m_nVersion = UIMG_VERSION;

    asset.m_nPageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 2;

    asset.m_nUsesStartIdx = fileRelationIdx;
    asset.m_nUsesCount = 1; // the asset should only use 1 other asset for the atlas

    // add the asset entry
    assetEntries->push_back(asset);
}
