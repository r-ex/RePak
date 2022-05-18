#include "pch.h"
#include "Assets.h"

void Assets::AddUIImageAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding uimg asset '%s'\n", assetPath);

    std::string sAssetName = assetPath;

    // get the info for the ui atlas image
    std::string sAtlasFilePath = g_sAssetsDir + mapEntry["atlas"].GetStdString() + ".dds";
    std::string sAtlasAssetName = mapEntry["atlas"].GetStdString() + ".rpak";
    uint64_t atlasGuid = RTech::StringToGuid(sAtlasAssetName.c_str());

    RPakAssetEntryV8* atlasAsset = RePak::GetAssetByGuid(assetEntries, atlasGuid, nullptr);

    if (atlasAsset == nullptr)
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
    uint32_t textureHashesDataSize = (sizeof(uint32_t) + sizeof(uint64_t)) * nTexturesCount;

    // get total size
    uint32_t textureInfoPageSize = textureOffsetsDataSize + textureDimensionsDataSize + textureHashesDataSize;

    // allocate the page and segment
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(UIImageHeader), 0x40, 8, SubHeaderSegment);

    RPakVirtualSegment TextureInfoSegment;
    uint32_t tisIdx = RePak::CreateNewSegment(textureInfoPageSize, 0x41, 32, TextureInfoSegment);

    RPakVirtualSegment RawDataSegment;
    uint32_t rdsIdx = RePak::CreateNewSegment(nTexturesCount * 0x10, 0x43, 4, RawDataSegment);

    // register our descriptors so they get converted properly
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, pTextureOffsets));
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, pTextureDims));
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, pTextureHashes));

    // textureGUID descriptors
    RePak::RegisterGuidDescriptor(shsIdx, offsetof(UIImageHeader, atlasGuid));

    // buffer for texture info data
    char* pTextureInfoBuf = new char[textureInfoPageSize]{};
    rmem tiBuf(pTextureInfoBuf);

    // set texture offset page index and offset
    pHdr->pTextureOffsets = { tisIdx, 0 };

    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageOffset uiio{};
        tiBuf.write(uiio);
    }

    // set texture dimensions page index and offset
    pHdr->pTextureDims = { tisIdx, textureOffsetsDataSize };

    for (auto& it : mapEntry["textures"].GetArray())
    {
        tiBuf.write<uint16_t>(it["width"].GetInt());
        tiBuf.write<uint16_t>(it["height"].GetInt());
    }

    // set texture hashes page index and offset
    pHdr->pTextureHashes = { tisIdx, textureOffsetsDataSize + textureDimensionsDataSize };

    uint64_t nextStringTableOffset = 0;

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

    atlasAsset->RelationsStartIndex = fileRelationIdx;
    atlasAsset->RelationsCount++;

    char* pUVBuf = new char[nTexturesCount * sizeof(UIImageUV)];
    rmem uvBuf(pUVBuf);

    for (uint32_t i = 0; i < nTexturesCount; ++i)
    {
        UIImageUV uiiu{};
        uvBuf.write(uiiu);
    }

    //
    // add the data blocks so they can be written properly
    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock tib{ tisIdx, TextureInfoSegment.DataSize, (uint8_t*)pTextureInfoBuf };
    RePak::AddRawDataBlock(tib);

    RPakRawDataBlock rdb{ rdsIdx, RawDataSegment.DataSize, (uint8_t*)pUVBuf };
    RePak::AddRawDataBlock(rdb);
    //
    // create and init the asset entry
    RPakAssetEntryV8 asset;
    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), shsIdx, 0, SubHeaderSegment.DataSize, rdsIdx, 0, -1, -1, (std::uint32_t)AssetType::UIMG);
    asset.Version = UIMG_VERSION;

    asset.HighestPageNum = rdsIdx + 1; // number of the highest page that the asset references pageidx + 1
    asset.Un2 = 2;

    asset.UsesStartIndex = fileRelationIdx;
    asset.UsesCount = 1; // the asset should only use 1 other asset for the atlas

    // add the asset entry
    assetEntries->push_back(asset);
}
