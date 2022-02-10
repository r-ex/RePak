#include "pch.h"
#include "Assets.h"

namespace Assets
{
    std::string g_sAssetsDir;
}

std::unordered_map<std::string, DataTableColumnDataType> DataTableColumnMap =
{
    { "bool",   DataTableColumnDataType::Bool },
    { "int",    DataTableColumnDataType::Int },
    { "float",  DataTableColumnDataType::Float },
    { "vector", DataTableColumnDataType::Vector },
    { "string", DataTableColumnDataType::StringT },
    { "asset",  DataTableColumnDataType::Asset },
    { "assetnoprecache", DataTableColumnDataType::AssetNoPrecache }
};

DataTableColumnDataType GetDataTableTypeFromString(std::string sType)
{
    std::transform(sType.begin(), sType.end(), sType.begin(), ::tolower);

    for (const auto& [key, value] : DataTableColumnMap) // Iterate through unordered_map.
    {
        if (sType.compare(key) == 0) // Do they equal?
            return value;
    }

    return DataTableColumnDataType::StringT;
}

uint32_t DataTable_GetEntrySize(DataTableColumnDataType type)
{
    switch (type)
    {
    case DataTableColumnDataType::Bool:
    case DataTableColumnDataType::Int:
    case DataTableColumnDataType::Float:
        return 4;
    case DataTableColumnDataType::Vector:
        return sizeof(float) * 3;
    case DataTableColumnDataType::StringT:
    case DataTableColumnDataType::Asset:
    case DataTableColumnDataType::AssetNoPrecache:
        // these assets are variable size so they have to use a ptr to keep the entry as a static size
        // basically the strings are moved elsewhere and then referenced here
        return sizeof(RPakPtr);
    }
}

void Assets::AddDataTableAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding dtbl asset '%s'\n", assetPath);

    rapidcsv::Document doc(g_sAssetsDir + assetPath + ".csv");

    std::string sAssetName = assetPath;

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

    std::vector<DataTableColumn> columns{};

    size_t ColumnNameBufSize = 0;

    ///-------------------------------------
    // figure out the required name buf size
    for (auto& it : doc.GetColumnNames())
    {
        ColumnNameBufSize += it.length() + 1;
    }

    // data buffer used for storing the names
    char* namebuf = new char[ColumnNameBufSize];

    uint32_t nextNameOffset = 0;
    uint32_t columnIdx = 0;
    // temp var used for storing the row offset for the next column in the loop below
    uint32_t tempColumnRowOffset = 0;
    uint32_t stringEntriesSize = 0;
    size_t rowDataPageSize = 0;

    ///-----------------------------------------
    // make a segment/page for the sub header
    //
    RPakVirtualSegment SubHeaderPage{};
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(DataTableHeader), 0, SegmentType::AssetSubHeader, SubHeaderPage);

    // page for DataTableColumn entries
    RPakVirtualSegment ColumnHeaderPage{};
    uint32_t chsIdx = RePak::CreateNewSegment(sizeof(DataTableColumn)*columnCount, 1, SegmentType::AssetSubHeader, ColumnHeaderPage, 64);

    hdr->ColumnCount = columnCount;
    hdr->RowCount = rowCount-1;
    hdr->ColumnHeaderPtr.Index = chsIdx;
    hdr->ColumnHeaderPtr.Offset = 0;

    RePak::RegisterDescriptor(shsIdx, offsetof(DataTableHeader, ColumnHeaderPtr));

    ///-----------------------------------------
    // make a segment/page for the column names
    //
    RPakVirtualSegment ColumnNamesPage{};
    uint32_t nameSegIdx = RePak::CreateNewSegment(ColumnNameBufSize, 1, SegmentType::AssetSubHeader, ColumnNamesPage, 64);

    char* columnHeaderBuf = new char[sizeof(DataTableColumn) * columnCount];

    for (auto& it : doc.GetColumnNames())
    {
        DataTableColumn col{};
        // copy the column name into the namebuf
        snprintf(namebuf + nextNameOffset, it.length() + 1, "%s", it.c_str());

        // set the page index and offset
        col.Name.Index = nameSegIdx;
        col.Name.Offset = nextNameOffset;
        RePak::RegisterDescriptor(chsIdx, (sizeof(DataTableColumn) * columnIdx) + offsetof(DataTableColumn, Name));

        col.RowOffset = tempColumnRowOffset;

        DataTableColumnDataType type = GetDataTableTypeFromString(typeRow[columnIdx]);
        col.Type = type;

        uint32_t columnEntrySize = 0;

        if(type == DataTableColumnDataType::StringT || type == DataTableColumnDataType::Asset || type == DataTableColumnDataType::AssetNoPrecache)
        {
            for (size_t i = 0; i < rowCount-1; ++i)
            {
                // this can be std::string since we only really need to deal with the string types
                auto row = doc.GetRow<std::string>(i);

                stringEntriesSize += row[columnIdx].length() + 1;
            }
        }

        columnEntrySize = DataTable_GetEntrySize(type);

        columns.emplace_back(col);

        *(DataTableColumn*)(columnHeaderBuf + (sizeof(DataTableColumn) * columnIdx)) = col;

        tempColumnRowOffset += columnEntrySize;
        rowDataPageSize += columnEntrySize * (rowCount-1);
        nextNameOffset += it.length() + 1;
        columnIdx++;

        // if this is the final column, set the total row bytes to the column's row offset + the column's row size
        // (effectively the full length of the row)
        if (columnIdx == columnCount)
            hdr->RowStride = tempColumnRowOffset;
    }

    // page for Row Data
    RPakVirtualSegment RowDataPage{};
    uint32_t rdsIdx = RePak::CreateNewSegment(rowDataPageSize, 1, SegmentType::AssetSubHeader, RowDataPage, 64);

    // page for string entries
    RPakVirtualSegment StringEntryPage{};
    uint32_t sesIdx = RePak::CreateNewSegment(stringEntriesSize, 1, SegmentType::AssetSubHeader, StringEntryPage, 64);

    char* rowDataBuf = new char[rowDataPageSize];

    char* stringEntryBuf = new char[stringEntriesSize];

    for (size_t rowIdx = 0; rowIdx < rowCount-1; ++rowIdx)
    {
        for (size_t columnIdx = 0; columnIdx < columnCount; ++columnIdx)
        {
            DataTableColumn col = columns[columnIdx];

            char* EntryPtr = (rowDataBuf + (hdr->RowStride * rowIdx) + col.RowOffset);

            switch (col.Type)
            {
            case DataTableColumnDataType::Bool:
            {
                std::string val = doc.GetCell<std::string>(columnIdx, rowIdx);

                transform(val.begin(), val.end(), val.begin(), ::tolower);

                if (val == "true")
                    *(uint32_t*)EntryPtr = 1;
                else
                    *(uint32_t*)EntryPtr = 0;
                break;
            }
            case DataTableColumnDataType::Int:
            {
                uint32_t val = doc.GetCell<uint32_t>(columnIdx, rowIdx);
                *(uint32_t*)EntryPtr = val;
                break;
            }
            case DataTableColumnDataType::Float:
            {
                float val = doc.GetCell<float>(columnIdx, rowIdx);
                *(float*)EntryPtr = val;
                break;
            }
            case DataTableColumnDataType::Vector:
                Log("NOT IMPLEMENTED: dtbl vector type");
                break;
            case DataTableColumnDataType::StringT:
            case DataTableColumnDataType::Asset:
            case DataTableColumnDataType::AssetNoPrecache:
            {
                static uint32_t nextStringEntryOffset = 0;

                RPakPtr stringPtr{ sesIdx, nextStringEntryOffset };
                
                std::string val = doc.GetCell<std::string>(columnIdx, rowIdx);
                snprintf(stringEntryBuf + nextStringEntryOffset, val.length() + 1, "%s", val.c_str());

                *(RPakPtr*)EntryPtr = stringPtr;
                RePak::RegisterDescriptor(rdsIdx, (hdr->RowStride * rowIdx) + col.RowOffset);

                nextStringEntryOffset += val.length() + 1;
                break;
            }
            }
        }
    }

    hdr->RowHeaderPtr.Index = rdsIdx;
    hdr->RowHeaderPtr.Offset = 0;

    RePak::RegisterDescriptor(shsIdx, offsetof(DataTableHeader, RowHeaderPtr));

    RPakRawDataBlock shDataBlock{ shsIdx, SubHeaderPage.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shDataBlock);

    RPakRawDataBlock colDataBlock{ chsIdx, ColumnHeaderPage.DataSize, (uint8_t*)columnHeaderBuf };
    RePak::AddRawDataBlock(colDataBlock);

    RPakRawDataBlock colNameDataBlock{ nameSegIdx, ColumnNamesPage.DataSize, (uint8_t*)namebuf };
    RePak::AddRawDataBlock(colNameDataBlock);

    RPakRawDataBlock rowDataBlock{ rdsIdx, RowDataPage.DataSize, (uint8_t*)rowDataBuf };
    RePak::AddRawDataBlock(rowDataBlock);

    RPakRawDataBlock stringEntryDataBlock{ rdsIdx, StringEntryPage.DataSize, (uint8_t*)stringEntryBuf };
    RePak::AddRawDataBlock(stringEntryDataBlock);

    RPakAssetEntryV8 asset;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), shsIdx, 0, SubHeaderPage.DataSize, rdsIdx, 0, -1, -1, (std::uint32_t)AssetType::DTBL);
    asset.Version = DTBL_VERSION;

    asset.HighestPageNum = sesIdx+1; // number of the highest page that the asset references pageidx + 1
    asset.Un2 = 1;

    assetEntries->push_back(asset);
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
    
    asset.HighestPageNum = rdsIdx+1; // number of the highest page that the asset references pageidx + 1
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
    
    asset.HighestPageNum = rdsIdx+1; // number of the highest page that the asset references pageidx + 1
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

    asset.HighestPageNum = rdsIdx+1;
    asset.Un2 = 1;

    assetEntries->push_back(asset);
}