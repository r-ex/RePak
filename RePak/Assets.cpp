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
    Debug("Adding dtbl asset '%s'\n", assetPath);

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
    Debug("Adding uimg asset '%s'\n", assetPath);

    UIImageHeader* hdr = new UIImageHeader();

    std::string sAssetName = assetPath;

    // get the info for the ui atlas image
    std::string sAtlasFilePath = g_sAssetsDir + mapEntry["atlas"].GetStdString() + ".dds";
    std::string sAtlasAssetName = mapEntry["atlas"].GetStdString() + ".rpak";
    uint64_t atlasGuid = RTech::StringToGuid(sAtlasAssetName.c_str());

    RPakAssetEntryV8* atlasAsset = RePak::GetAssetByGuid(assetEntries, atlasGuid, nullptr);

    if (atlasAsset == nullptr)
    {
        Warning("Atlas asset was not found when trying to add uimg asset '%s'. Make sure that the txtr is above the uimg in your map file. Skipping asset...\n", assetPath);
        return;
    }

    uint32_t nTexturesCount = mapEntry["textures"].GetArray().Size();

    // open the atlas and read its header to find out
    // what its dimensions are so they can be set in the header
    BinaryIO atlas;
    atlas.open(sAtlasFilePath, BinaryIOMode::BinaryIOMode_Read);
    atlas.seek(4, std::ios::beg);
    DDS_HEADER ddsh = atlas.read<DDS_HEADER>();

    atlas.close();

    hdr->Width = ddsh.width;
    hdr->Height = ddsh.height;

    hdr->TextureOffsetsCount = nTexturesCount;
    hdr->TextureCount = nTexturesCount == 1 ? 0 : nTexturesCount; // don't even ask

    hdr->TextureGuid = atlasGuid;
    
    //
    // calculate data sizes so we can allocate a page and segment
    uint32_t textureOffsetsDataSize = sizeof(float) * 8 * nTexturesCount;
    uint32_t textureDimensionsDataSize = sizeof(uint16_t) * 2 * nTexturesCount;
    uint32_t textureHashesDataSize = sizeof(uint32_t) * nTexturesCount;

    // get total size
    uint32_t textureInfoPageSize = textureOffsetsDataSize + textureDimensionsDataSize + textureHashesDataSize + (4*nTexturesCount); // man idk what this +4 is

    // allocate the page and segment
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(UIImageHeader), 0x40, SegmentType::AssetSubHeader, SubHeaderSegment);

    RPakVirtualSegment TextureInfoSegment;
    uint32_t tisIdx = RePak::CreateNewSegment(textureInfoPageSize, 0x41, SegmentType::Unknown2, TextureInfoSegment);

    RPakVirtualSegment RawDataSegment;
    uint32_t rdsIdx = RePak::CreateNewSegment(nTexturesCount * 0x10, 0x43, SegmentType::Unknown1, RawDataSegment);

    // register our descriptors so they get converted properly
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, TextureOffsetsIndex));
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, TextureDimsIndex));
    RePak::RegisterDescriptor(shsIdx, offsetof(UIImageHeader, TextureHashesIndex));

    // guid descriptors
    RePak::RegisterGuidDescriptor(shsIdx, offsetof(UIImageHeader, TextureGuid));

    // buffer for texture info data
    char* buf = new char[textureInfoPageSize];
    char* yea = buf;

    // set texture offset page index and offset
    hdr->TextureOffsetsIndex = tisIdx;
    hdr->TextureOffsetsOffset = 0; // start of the page

    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageOffset uiio{};
        *(UIImageOffset*)buf = uiio;
        buf += sizeof(UIImageOffset);
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

    //
    // add the file relation from this uimg asset to the atlas txtr
    size_t fileRelationIdx = RePak::AddFileRelation(assetEntries->size());

    atlasAsset->RelationsStartIndex = fileRelationIdx;
    atlasAsset->RelationsCount++;

    char* rdbuf = new char[nTexturesCount * 0x10];
    yea = rdbuf; // this is just our temporary buf ptr var now
    
    for (int i = 0; i < nTexturesCount; ++i)
    {
        UIImageUV uiiu{};
        *(UIImageUV*)rdbuf = uiiu;
        rdbuf += sizeof(UIImageUV);
    }
    rdbuf = yea;

    //
    // add the data blocks so they can be written properly
    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock tib{ tisIdx, TextureInfoSegment.DataSize, (uint8_t*)buf };
    RePak::AddRawDataBlock(tib);

    RPakRawDataBlock rdb{ rdsIdx, RawDataSegment.DataSize, (uint8_t*)rdbuf };
    RePak::AddRawDataBlock(rdb);
    //
    // create and init the asset entry
    RPakAssetEntryV8 asset;
    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), shsIdx, 0, SubHeaderSegment.DataSize, rdsIdx, 0, -1, -1, (std::uint32_t)AssetType::UIMG);
    asset.Version = UIMG_VERSION;
    
    // \_('_')_/
    asset.Un1 = 5;
    asset.Un2 = 2;

    asset.UsesStartIndex = fileRelationIdx;
    asset.UsesCount = 1; // the asset should only use 1 other asset for the atlas

    // add the asset entry
    assetEntries->push_back(asset);
}

void Assets::AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding txtr asset '%s'\n", assetPath);

    TextureHeader* hdr = new TextureHeader();

    std::string filePath = g_sAssetsDir + assetPath + ".dds";

    BinaryIO input;
    input.open(filePath, BinaryIOMode::BinaryIOMode_Read);

    uint64_t nInputFileSize = Utils::GetFileSize(filePath);

    std::string sAssetName = assetPath; // todo: this needs to be changed to the actual name

    // parse input image file
    {
        int magic;
        input.read(magic);

        if (magic != 0x20534444) // b'DDS '
        {
            Warning("Attempted to add txtr asset '%s' that was not a valid DDS file (invalid magic). Skipping asset...\n", assetPath);
            return;
        }

        DDS_HEADER ddsh = input.read<DDS_HEADER>();

        if (ddsh.pixelfmt.fourCC != '1TXD')
        {
            Warning("Attempted to add txtr asset '%s' that was not using a supported DDS type (currently only DXT1). Skipping asset...\n", assetPath);
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

    // unfortunately i'm not a respawn engineer so 1 (unstreamed) mip level will have to do
    hdr->MipLevels = 1;

    // give us a segment to use for the subheader
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(TextureHeader), 0, SegmentType::AssetSubHeader, SubHeaderSegment);

    // woo more segments
    RPakVirtualSegment RawDataSegment;
    uint32_t rdsIdx = RePak::CreateNewSegment(hdr->DataSize, 3, SegmentType::AssetRawData, RawDataSegment);

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

void Assets::AddPatchAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding Ptch asset '%s'\n", assetPath);

    PtchHeader* hdr = new PtchHeader();

    hdr->EntryCount = mapEntry["entries"].GetArray().Size();

    std::vector<PtchEntry> patchEntries{};
    uint32_t entryNamesSectionSize = 0;

    for (auto& it : mapEntry["entries"].GetArray())
    {
        std::string name = it["name"].GetStdString();
        uint8_t patchNum = it["patchnum"].GetInt();
        patchEntries.push_back({ name, patchNum, entryNamesSectionSize });
        entryNamesSectionSize += name.length() + 1;
    }

    size_t nDataPageSize = (sizeof(RPakPtr) * hdr->EntryCount) + (sizeof(uint8_t) * hdr->EntryCount) + entryNamesSectionSize;

    RPakVirtualSegment SubHeaderPage;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(PtchHeader), 0, SegmentType::AssetSubHeader, SubHeaderPage);

    RPakVirtualSegment DataPage;
    uint32_t rdsIdx = RePak::CreateNewSegment(nDataPageSize, 1, SegmentType::AssetSubHeader, DataPage);

    hdr->EntryNames.Index  = rdsIdx;
    hdr->EntryNames.Offset = 0;

    hdr->EntryPatchNums.Index  = rdsIdx;
    hdr->EntryPatchNums.Offset = sizeof(RPakPtr) * hdr->EntryCount;

    RePak::RegisterDescriptor(shsIdx, offsetof(PtchHeader, EntryNames));
    RePak::RegisterDescriptor(shsIdx, offsetof(PtchHeader, EntryPatchNums));


    char* buf = new char[nDataPageSize];
    char* temp = buf;

    uint32_t i = 0;
    for (auto& it : patchEntries)
    {
        uint32_t fileNameOffset = (sizeof(RPakPtr) * hdr->EntryCount) + (sizeof(uint8_t) * hdr->EntryCount) + it.FileNamePageOffset;

        // write the ptr to the file name into the buffer
        *(RPakPtr*)(buf + sizeof(RPakPtr) * i) = { rdsIdx, fileNameOffset };
        // write the patch number for this entry into the buffer
        *(uint8_t*)(buf + hdr->EntryPatchNums.Offset + i) = it.PatchNum;

        snprintf(buf + fileNameOffset, it.FileName.length() + 1, "%s", it.FileName.c_str());

        RePak::RegisterDescriptor(rdsIdx, sizeof(RPakPtr) * i);
        i++;
    }

    RPakRawDataBlock shdb{ shsIdx, SubHeaderPage.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ rdsIdx, nDataPageSize, (uint8_t*)buf };
    RePak::AddRawDataBlock(rdb);

    // create and init the asset entry
    RPakAssetEntryV8 asset;
    asset.InitAsset(0x6fc6fa5ad8f8bc9c, shsIdx, 0, SubHeaderPage.DataSize, -1, 0, -1, -1, (std::uint32_t)AssetType::PTCH);
    asset.Version = 1;

    asset.Un1 = 2;
    asset.Un2 = 1;

    assetEntries->push_back(asset);
}