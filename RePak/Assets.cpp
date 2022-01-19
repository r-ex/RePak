#include "pch.h"
#include "Assets.h"

namespace Assets
{
    std::string g_sAssetsDir;
}

DataTableColumnDataType GetDataTableTypeFromString(std::string sType)
{
    std::transform(sType.begin(), sType.end(), sType.begin(), ::tolower);

    if (sType == "bool") return DataTableColumnDataType::Bool;
    if (sType == "int") return DataTableColumnDataType::Int;
    if (sType == "float") return DataTableColumnDataType::Float;
    if (sType == "vector") return DataTableColumnDataType::Vector;
    if (sType == "string") return DataTableColumnDataType::StringT;
    if (sType == "asset") return DataTableColumnDataType::Asset;
    if (sType == "assetnoprecache") return DataTableColumnDataType::AssetNoPrecache;

    return DataTableColumnDataType::StringT;
}

void Assets::AddDataTableAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    rapidcsv::Document doc(g_sAssetsDir + assetPath + ".csv");

    DataTableHeader* hdr = new DataTableHeader();

    const size_t columnCount = doc.GetColumnCount();
    const size_t rowCount = doc.GetRowCount();

    if (rowCount < 2)
    {
        printf("WARNING - Attempted to add dtbl asset with an invalid row count. Skipping asset...\n");
        printf("DTBL    - CSV must have a row of column types at the end of the table\n");
        return;
    }

    std::vector<std::string> typeRow = doc.GetRow<std::string>(rowCount - 1);

    std::vector<DataTableColumn> vcColumns{};
    std::vector<std::string> vsColumnNames{};
    size_t columnNamesSize = 0;

    for (int i = 0; i < columnCount; ++i)
    {
        DataTableColumn col{};

        col.NameOffset = columnNamesSize; // only set the offset here. we have to set the index later when the segment is created
        col.Type = GetDataTableTypeFromString(typeRow[i]);

        std::string sColumnName = doc.GetColumnName(i);
        vsColumnNames.emplace_back(sColumnName);

        columnNamesSize += sColumnName.length() + 1;

        vcColumns.emplace_back(col);
    }
    // i dont wanna finish this
}

void Assets::AddUIImageAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    UIImageHeader* hdr = new UIImageHeader();

    std::string sAssetName = assetPath;

    // get the info for the ui atlas image
    std::string sAtlasFilePath = g_sAssetsDir + mapEntry["atlas"].GetStdString() + ".dds";
    std::string sAtlasAssetName = mapEntry["atlas"].GetStdString() + ".rpak";

    uint32_t nTexturesCount = mapEntry["textures"].GetArray().Size();

    // open the atlas and read its header to find out
    // what its dimensions are so they can be set in the header
    BinaryIO atlas;
    atlas.open(sAtlasFilePath, BinaryIOMode::BinaryIOMode_Read);

    DDS_HEADER ddsh = atlas.read<DDS_HEADER>();

    atlas.close();

    hdr->Width = ddsh.width;
    hdr->Height = ddsh.height;

    hdr->TextureOffsetsCount = nTexturesCount;
    hdr->TextureCount = nTexturesCount;
    
    //
    // calculate data sizes so we can allocate a page and segment
    uint32_t textureOffsetsDataSize = sizeof(float) * 8 * nTexturesCount;
    uint32_t textureDimensionsDataSize = sizeof(uint16_t) * 2 * nTexturesCount;
    uint32_t textureHashesDataSize = sizeof(uint32_t) * nTexturesCount;

    // get total size
    uint32_t textureInfoPageSize = textureOffsetsDataSize + textureDimensionsDataSize + textureHashesDataSize;

    // allocate the page and segment
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(UIImageHeader), SegmentType::AssetSubHeader, SubHeaderSegment);

    RPakVirtualSegment TextureInfoSegment;
    uint32_t tisIdx = RePak::CreateNewSegment(textureInfoPageSize, SegmentType::Unknown1, TextureInfoSegment);

    // register our descriptors so they get converted properly
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, TextureOffsetsIndex));
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, TextureDimsIndex));
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, TextureHashesIndex));

    // buffer for texture info data
    char* buf = new char[textureInfoPageSize];
    char* yea = buf;

    // set texture offset page index and offset
    hdr->TextureOffsetsIndex = tisIdx;
    hdr->TextureOffsetsOffset = 0; // start of the page

    for (auto& it : mapEntry["textures"].GetArray())
    {
        // todo: deal with this later
        memset(buf, 0, sizeof(float) * 8);
        buf += sizeof(float) * 8;
    }

    // set texture dimensions page index and offset
    hdr->TextureDimsIndex = tisIdx;
    hdr->TextureDimsOffset = textureOffsetsDataSize;

    for (auto& it : mapEntry["textures"].GetArray())
    {
        *(uint16_t*)buf = it["width"].GetInt();
        buf += 2;
        *(uint16_t*)buf = it["height"].GetInt();
        buf += 2;
    }

    // set texture hashes page index and offset
    hdr->TextureHashesIndex = tisIdx;
    hdr->TextureHashesOffset = textureOffsetsDataSize + textureHashesDataSize;

    for (auto& it : mapEntry["textures"].GetArray())
    {
        *(uint32_t*)buf = RTech::StringToUIMGHash(it["path"].GetString());
        buf += sizeof(uint32_t);
    }

    // move our pointer back in
    buf = yea;

    hdr->TextureGuid = RTech::StringToGuid(sAtlasAssetName.c_str());

    // add the data blocks so they can be written properly
    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock tib{ tisIdx, TextureInfoSegment.DataSize, (uint8_t*)buf };
    RePak::AddRawDataBlock(tib);

    // create and init the asset entry
    RPakAssetEntryV8 asset;
    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), shsIdx, 0, SubHeaderSegment.DataSize, 0 /* todo: rawdatablock */, 0, -1, -1, (std::uint32_t)AssetType::UIMG);
    asset.Version = UIMG_VERSION;
    
    // \_('_')_/
    asset.Un1 = 2;
    asset.Un2 = 1;

    // add the asset entry
    assetEntries->push_back(asset);
}

void Assets::AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    TextureHeader* hdr = new TextureHeader();

    std::string filePath = g_sAssetsDir + assetPath + ".dds";

    BinaryIO input;
    input.open(filePath, BinaryIOMode::BinaryIOMode_Read);

    uint64_t nInputFileSize = Utils::GetFileSize(filePath);

    std::string sAssetName = assetPath; // todo: this needs to be changed to the actual name

    {
        int magic;
        input.read(magic);

        if (magic != 0x20534444) // b'DDS '
        {
            printf("WARNING: Attempted to add txtr asset '%s' that was not a valid DDS file (invalid magic). Skipping asset...\n", assetPath);
            return;
        }

        DDS_HEADER ddsh = input.read<DDS_HEADER>();


        if (ddsh.pixelfmt.fourCC != '1TXD')
        {
            printf("WARNING: Attempted to add txtr asset '%s' that was not using a supported DDS type (currently only DXT1). Skipping asset...\n", assetPath);
            return;
        }

        hdr->DataSize = ddsh.pitchOrLinearSize;
        hdr->Width = ddsh.width;
        hdr->Height = ddsh.height;

        // TODO: support other texture formats
        hdr->Format = (uint8_t)TXTRFormat::_DXT1;

        ///
        // ddsh.size is the size of the primary rpakHeader after the "DDS "
        ///
        // NOTE: when adding support for other formats, there may be a "secondary" rpakHeader after this point
        //       this rpakHeader is ONLY used when ddsh.pixelfmt.fourCC is "DX10"
        input.seek(ddsh.size + 4);
    }

    hdr->NameHash = RTech::StringToGuid((sAssetName + ".rpak").c_str());
    // rspn doesn't use named textures so why should we
    hdr->NameIndex = 0;
    hdr->NameOffset = 0;

    // unfortunately i'm not a respawn engineer so 1 (unstreamed) mip level will have to do
    hdr->MipLevels = 1;

    //memset(&hdr.UnknownPad, 0, sizeof(hdr.UnknownPad));

    // give us a segment to use for the subheader
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(TextureHeader), SegmentType::AssetSubHeader, SubHeaderSegment);

    // woo more segments
    RPakVirtualSegment RawDataSegment;
    uint32_t rdsIdx = RePak::CreateNewSegment(hdr->DataSize, SegmentType::AssetRawData, RawDataSegment);

    char* databuf = new char[hdr->DataSize];

    input.getReader()->read(databuf, hdr->DataSize);

    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ rdsIdx, RawDataSegment.DataSize, (uint8_t*)databuf };
    RePak::AddRawDataBlock(rdb);

    // now time to add the higher level asset entry
    RPakAssetEntryV8 asset;
    
    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), shsIdx, 0, SubHeaderSegment.DataSize, rdsIdx, 0, -1, -1, (std::uint32_t)AssetType::TEXTURE);
    asset.Version = TXTR_VERSION;
    // i have literally no idea what these are
    asset.Un1 = 2;
    asset.Un2 = 1;

    assetEntries->push_back(asset);

    input.close();
}