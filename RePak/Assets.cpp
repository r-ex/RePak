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
    RPakVirtualSegment SubHeaderSegment{};
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(DataTableHeader), 0, 8, SubHeaderSegment);

    // page for DataTableColumn entries
    RPakVirtualSegment ColumnHeaderSegment{};
    uint32_t chsIdx = RePak::CreateNewSegment(sizeof(DataTableColumn)*columnCount, 1, 8, ColumnHeaderSegment, 64);

    hdr->ColumnCount = columnCount;
    hdr->RowCount = rowCount-1;
    hdr->ColumnHeaderPtr.Index = chsIdx;
    hdr->ColumnHeaderPtr.Offset = 0;

    RePak::RegisterDescriptor(shsIdx, offsetof(DataTableHeader, ColumnHeaderPtr));

    ///-----------------------------------------
    // make a segment/page for the column names
    //
    RPakVirtualSegment ColumnNamesSegment{};
    uint32_t nameSegIdx = RePak::CreateNewSegment(ColumnNameBufSize, 1, 8, ColumnNamesSegment, 64);

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
                // this can be std::string since we only really need to deal with the string types here
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
    RPakVirtualSegment RowDataSegment{};
    uint32_t rdsIdx = RePak::CreateNewSegment(rowDataPageSize, 1, 8, RowDataSegment, 64);

    // page for string entries
    RPakVirtualSegment StringEntrySegment{};
    uint32_t sesIdx = RePak::CreateNewSegment(stringEntriesSize, 1, 8, StringEntrySegment, 64);

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
            {
                // parse from format <x,y,z>
                // put into float[3]
                // then write it
                Log("NOT IMPLEMENTED: dtbl vector type");
                break;
            }

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

    RPakRawDataBlock shDataBlock{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shDataBlock);

    RPakRawDataBlock colDataBlock{ chsIdx, ColumnHeaderSegment.DataSize, (uint8_t*)columnHeaderBuf };
    RePak::AddRawDataBlock(colDataBlock);

    RPakRawDataBlock colNameDataBlock{ nameSegIdx, ColumnNamesSegment.DataSize, (uint8_t*)namebuf };
    RePak::AddRawDataBlock(colNameDataBlock);

    RPakRawDataBlock rowDataBlock{ rdsIdx, RowDataSegment.DataSize, (uint8_t*)rowDataBuf };
    RePak::AddRawDataBlock(rowDataBlock);

    RPakRawDataBlock stringEntryDataBlock{ rdsIdx, StringEntrySegment.DataSize, (uint8_t*)stringEntryBuf };
    RePak::AddRawDataBlock(stringEntryDataBlock);

    RPakAssetEntryV8 asset;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), shsIdx, 0, SubHeaderSegment.DataSize, rdsIdx, 0, -1, -1, (std::uint32_t)AssetType::DTBL);
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
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(UIImageHeader), 0x40, 8, SubHeaderSegment);

    RPakVirtualSegment TextureInfoSegment;
    uint32_t tisIdx = RePak::CreateNewSegment(textureInfoPageSize, 0x41, 32, TextureInfoSegment);

    RPakVirtualSegment RawDataSegment;
    uint32_t rdsIdx = RePak::CreateNewSegment(nTexturesCount * 0x10, 0x43, 4, RawDataSegment);

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

        hdr->DataSize = ddsh.pitchOrLinearSize;
        hdr->Width = ddsh.width;
        hdr->Height = ddsh.height;
        
        DXGI_FORMAT dxgiFormat;

        switch (ddsh.pixelfmt.fourCC)
        {
        case '1TXD':
            Log("-> fmt: DXT1\n");
            dxgiFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
            break;
        case 'U4CB':
            Log("-> fmt: BC4U\n");
            dxgiFormat = DXGI_FORMAT_BC4_UNORM;
            break;
        case 'U5CB':
            Log("-> fmt: BC5U\n");
            dxgiFormat = DXGI_FORMAT_BC5_UNORM;
            break;
        case '01XD':
            Log("-> fmt: DX10\n");
            dxgiFormat = DXGI_FORMAT_BC7_UNORM;
            break;
        default:
            Warning("Attempted to add txtr asset '%s' that was not using a supported DDS type. Skipping asset...\n", assetPath);
            return;
        }

        hdr->Format = (uint16_t)TxtrFormatMap[dxgiFormat];

        ///
        // ddsh.size is the size of the primary rpakHeader after the "DDS "
        ///
        input.seek(ddsh.size + 4);

        if (dxgiFormat == DXGI_FORMAT_BC7_UNORM || dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB)
            input.seek(20, std::ios::cur);
    }

    hdr->NameHash = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    // unfortunately i'm not a respawn engineer so 1 (unstreamed) mip level will have to do
    hdr->MipLevels = 1;

    bool bSaveDebugName = mapEntry.HasMember("saveDebugName") && mapEntry["saveDebugName"].GetBool();

    // give us a segment to use for the subheader
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(TextureHeader), 0, 8, SubHeaderSegment);

    RPakVirtualSegment DebugNameSegment;
    char* namebuf = new char[sAssetName.size() + 1];
    uint32_t nsIdx = -1;

    if (bSaveDebugName)
    {
        sprintf_s(namebuf, sAssetName.length() + 1, "%s", sAssetName.c_str());
        nsIdx = RePak::CreateNewSegment(sAssetName.size() + 1, 129, 1, DebugNameSegment);
    }
    else {
        delete[] namebuf;
    }

    // woo more segments
    RPakVirtualSegment RawDataSegment;
    uint32_t rdsIdx = RePak::CreateNewSegment(hdr->DataSize, 3, 16, RawDataSegment);

    char* databuf = new char[hdr->DataSize];

    input.getReader()->read(databuf, hdr->DataSize);

    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    if (bSaveDebugName)
    {
        RPakRawDataBlock ndb{ nsIdx, DebugNameSegment.DataSize, (uint8_t*)namebuf };
        RePak::AddRawDataBlock(ndb);
        hdr->NameIndex = nsIdx;
        hdr->NameOffset = 0;
    }

    RPakRawDataBlock rdb{ rdsIdx, RawDataSegment.DataSize, (uint8_t*)databuf };
    RePak::AddRawDataBlock(rdb);

    // now time to add the higher level asset entry
    RPakAssetEntryV8 asset;
    
    uint64_t StarpakOffset = -1;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), shsIdx, 0, SubHeaderSegment.DataSize, rdsIdx, 0, StarpakOffset, -1, (std::uint32_t)AssetType::TEXTURE);
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
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(PtchHeader), 0, 8, SubHeaderPage);

    RPakVirtualSegment DataPage;
    uint32_t rdsIdx = RePak::CreateNewSegment(nDataPageSize, 1, 8, DataPage);

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
    // guid is hardcoded here because this is the only guid that is ever used for the shipped patch_master rpaks
    asset.InitAsset(0x6fc6fa5ad8f8bc9c, shsIdx, 0, SubHeaderPage.DataSize, -1, 0, -1, -1, (std::uint32_t)AssetType::PTCH);
    asset.Version = 1;

    asset.HighestPageNum = rdsIdx+1;
    asset.Un2 = 1;

    assetEntries->push_back(asset);
}

void Assets::AddModelAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding mdl_ asset '%s'\n", assetPath);

    std::string sAssetName = std::string(assetPath) + ".rmdl";

    ModelHeader* hdr = new ModelHeader();

    std::string rmdlFilePath = g_sAssetsDir + sAssetName;
    std::string vgFilePath = g_sAssetsDir + std::string(assetPath) + ".vg";

    ///-------------------
    // Begin skeleton(.rmdl) input
    BinaryIO skelInput;
    skelInput.open(rmdlFilePath, BinaryIOMode::BinaryIOMode_Read);

    BasicRMDLSkeletonHeader bsh = skelInput.read<BasicRMDLSkeletonHeader>();

    if (bsh.magic != 0x54534449)
    {
        Warning("invalid file magic for model asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x54534449, bsh.magic);
        return;
    }

    if (bsh.version != 0x36)
    {
        Warning("invalid version for model asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 0x36, bsh.version);
    }

    // go back to the beginning of the file to read all the data
    skelInput.seek(0);

    uint32_t fileNameDataSize = sAssetName.length() + 1;

    char* dataBuf = new char[fileNameDataSize + bsh.dataSize];

    // write the model file path into the data buffer
    snprintf(dataBuf, fileNameDataSize, "%s", sAssetName.c_str());
    // write the skeleton data into the data buffer
    skelInput.getReader()->read(dataBuf + fileNameDataSize, bsh.dataSize);
    skelInput.close();

    ///--------------------
    // Add VG data
    BinaryIO vgInput;
    vgInput.open(vgFilePath, BinaryIOMode::BinaryIOMode_Read);

    BasicRMDLVGHeader bvgh = vgInput.read<BasicRMDLVGHeader>();

    if (bvgh.magic != 0x47567430)
    {
        Warning("invalid vg file magic for model asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x47567430, bvgh.magic);
        return;
    }

    if (bvgh.version != 1)
    {
        Warning("invalid vg version for model asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 1, bvgh.version);
    }

    vgInput.seek(0, std::ios::end);

    uint32_t vgFileSize = vgInput.tell();
    char* vgBuf = new char[vgFileSize];

    vgInput.seek(0);
    vgInput.getReader()->read(vgBuf, vgFileSize);
    vgInput.close();

    // static name for now
    RePak::AddStarpakReference("paks/Win64/repak.starpak");

    SRPkDataEntry de{ -1, vgFileSize, (uint8_t*)vgBuf };
    uint64_t starpakOffset = RePak::AddStarpakDataEntry(de);

    hdr->StreamedDataSize = vgFileSize;

    RPakVirtualSegment SubHeaderSegment{};
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(ModelHeader), 0, 8, SubHeaderSegment);

    RPakVirtualSegment DataSegment{};
    uint32_t dataSegIdx = RePak::CreateNewSegment(bsh.dataSize + fileNameDataSize, 1, 64, DataSegment);

    //RPakVirtualSegment VGSegment{};
    //uint32_t vgIdx = RePak::CreateNewSegment(vgFileSize, 67, SegmentType::Unknown, DataSegment);

    hdr->SkeletonPtr.Index = dataSegIdx;
    hdr->SkeletonPtr.Offset = fileNameDataSize;

    hdr->NamePtr.Index = dataSegIdx;
    hdr->NamePtr.Offset = 0;

    //hdr->VGPtr.Index = vgIdx;
    //hdr->VGPtr.Offset = 0;

    RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, SkeletonPtr));
    RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, NamePtr));
    //RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, VGPtr));


    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ dataSegIdx, DataSegment.DataSize, (uint8_t*)dataBuf };
    RePak::AddRawDataBlock(rdb);

    //RPakRawDataBlock vgdb{ vgIdx, vgFileSize, (uint8_t*)vgBuf };
    //RePak::AddRawDataBlock(vgdb);

    // now time to add the higher level asset entry
    RPakAssetEntryV8 asset;

    asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), shsIdx, 0, SubHeaderSegment.DataSize, -1, 0, starpakOffset, -1, (std::uint32_t)AssetType::RMDL);
    asset.Version = RMDL_VERSION;
    // i have literally no idea what these are
    asset.HighestPageNum = dataSegIdx + 1;
    asset.Un2 = 2;

    // note: models use an implicit guid reference to their materials
    // the guids aren't registered but are converted during the loading of the material asset
    // during (what i assume is) regular reference conversion
    // a potential solution to the material guid conversion issue could be just registering the guids?
    // 
    size_t fileRelationIdx = RePak::AddFileRelation(assetEntries->size());
    asset.UsesStartIndex = fileRelationIdx;
    asset.UsesCount = 1;

    assetEntries->push_back(asset);
}

void Assets::AddMaterialAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding matl asset '%s'\n", assetPath);

    MaterialHeader* hdr = new MaterialHeader();

    std::string sAssetPath = std::string(assetPath);

    std::string type = "sknp";

    if (mapEntry.HasMember("type"))
        type = mapEntry["type"].GetStdString();
    else
        Warning("Adding material without an explicitly defined type. Assuming 'sknp'...\n");


    std::string sAssetName = "material/" + sAssetPath + "_" + type + ".rpak";

    hdr->AssetGUID = RTech::StringToGuid(sAssetName.c_str());

    if (mapEntry.HasMember("resolution")) // Set material resolution.
        hdr->MaterialRes = mapEntry["resolution"].GetInt();

    uint32_t assetUsesCount = 0;

    // get surface name or use "default"
    std::string surface = "default";

    // surface names are defined in surfaceproperties.rson along with other properties
    // i'm not entirely sure if this actually does anything?
    if (mapEntry.HasMember("surface"))
        surface = mapEntry["surface"].GetStdString();

    // get the size of the texture guid section
    size_t textureRefSize = 0;

    if (mapEntry.HasMember("textures"))
        textureRefSize = mapEntry["textures"].GetArray().Size()*8;
    else
    {
        // im pretty sure you *can* have a material with no textures but those are both uncommon and painful to use for anything meaningful
        Warning("Trying to add material with no textures. Skipping asset...\n");
        return;
    }

    uint32_t assetPathSize = (sAssetPath.length() + 1);
    uint32_t dataBufSize = (assetPathSize + (assetPathSize % 4)) + (textureRefSize * 2) + (surface.length() + 1);
    
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(MaterialHeader), 0, 8, SubHeaderSegment);

    RPakVirtualSegment DataSegment;
    uint32_t dsIdx = RePak::CreateNewSegment(dataBufSize, 1, 64, DataSegment);
    
    char* dataBuf = new char[dataBufSize]{};
    char* tmp = dataBuf;

    // ===============================
    // write the material path into the buffer
    snprintf(dataBuf, sAssetPath.length() + 1, "%s", assetPath);
    uint8_t assetPathAlignment = (assetPathSize % 4);
    dataBuf += sAssetPath.length() + 1 + assetPathAlignment;

    // ===============================
    // add the texture guids to the buffer
    size_t guidPageOffset = sAssetPath.length() + 1 + assetPathAlignment;

    int textureIdx = 0;
    int fileRelationIdx = -1;
    for (auto& it : mapEntry["textures"].GetArray())
    {
        if (it.GetStdString() != "")
        {
            uint64_t guid = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str());
            *(uint64_t*)dataBuf = guid;
            RePak::RegisterGuidDescriptor(dsIdx, guidPageOffset + (textureIdx * sizeof(uint64_t)));
            
            if(fileRelationIdx == -1)
                fileRelationIdx = RePak::AddFileRelation(assetEntries->size());
            else
                RePak::AddFileRelation(assetEntries->size());

            auto txtrAsset = RePak::GetAssetByGuid(assetEntries, guid, nullptr);

            txtrAsset->RelationsStartIndex = fileRelationIdx;
            txtrAsset->RelationsCount++;

            assetUsesCount++;
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++;
    }

    textureIdx = 0;
    for (auto& it : mapEntry["textures"].GetArray())
    {
        if (it.GetStdString() != "")
        {
            uint64_t guid = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str());
            *(uint64_t*)dataBuf = guid;
            RePak::RegisterGuidDescriptor(dsIdx, guidPageOffset + textureRefSize + (textureIdx * sizeof(uint64_t)));

            RePak::AddFileRelation(assetEntries->size());

            auto txtrAsset = RePak::GetAssetByGuid(assetEntries, guid, nullptr);

            txtrAsset->RelationsStartIndex = fileRelationIdx;
            txtrAsset->RelationsCount++;

            assetUsesCount++;
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++;
    }

    // ===============================
    // write the surface name into the buffer
    snprintf(dataBuf, surface.length() + 1, "%s", surface.c_str());

    // get the original pointer back so it can be used later for writing the buffer
    dataBuf = tmp;

    // ===============================
    // fill out the rest of the header
    hdr->Name.Index  = dsIdx;
    hdr->Name.Offset = 0;

    hdr->SurfaceName.Index = dsIdx;
    hdr->SurfaceName.Offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize*2);

    RePak::RegisterDescriptor(shsIdx, offsetof(MaterialHeader, Name));
    RePak::RegisterDescriptor(shsIdx, offsetof(MaterialHeader, SurfaceName));

    // Type Handling
    // this isn't great but idk how else to do it :/
    if (type == "sknp")
    {
        // these should always be constant (per each material type)
        // there's different versions of these for each material type
        // 
        hdr->GUIDRefs[0] = 0x2B93C99C67CC8B51;
        hdr->GUIDRefs[1] = 0x1EBD063EA03180C7;
        hdr->GUIDRefs[2] = 0xF95A7FA9E8DE1A0E;
        hdr->GUIDRefs[3] = 0x227C27B608B3646B;
        // GUIDRefs[4] is the guid for the colpass version of this material

        // i hate this format so much
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs));
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 8);
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 16);
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 24);
        
        RePak::AddFileRelation(assetEntries->size(), 4);
        assetUsesCount += 4;

        // oh you wanted custom shaders? shame
        hdr->ShaderSetGUID = 0x1D9FFF314E152725;
    }
    else if (type == "wldc")
    {
        hdr->GUIDRefs[0] = 0x435FA77E363BEA48;
        hdr->GUIDRefs[1] = 0xF734F96BE92E0E71;
        hdr->GUIDRefs[2] = 0xD306370918620EC0;
        hdr->GUIDRefs[3] = 0xDAB17AEAD2D3387A;

        // GUIDRefs[4] is the guid for the colpass version of this material

        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs));
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 8);
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 16);
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 24);

        RePak::AddFileRelation(assetEntries->size(), 4);
        assetUsesCount += 4;

        hdr->ShaderSetGUID = 0x4B0F3B4CBD009096;
    }

    RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, ShaderSetGUID));
    RePak::AddFileRelation(assetEntries->size());
    assetUsesCount++;

    // is this a colpass asset?
    bool bColpass = false;
    if (mapEntry.HasMember("colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + ".rpak";
        hdr->GUIDRefs[4] = RTech::StringToGuid(colpassPath.c_str());

        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 32);
        RePak::AddFileRelation(assetEntries->size());
        assetUsesCount++;

        bColpass = false;
    }
    hdr->TextureGUIDs.Index = dsIdx;
    hdr->TextureGUIDs.Offset = guidPageOffset;

    hdr->TextureGUIDs2.Index = dsIdx;
    hdr->TextureGUIDs2.Offset = guidPageOffset + textureRefSize;

    RePak::RegisterDescriptor(shsIdx, offsetof(MaterialHeader, TextureGUIDs));
    RePak::RegisterDescriptor(shsIdx, offsetof(MaterialHeader, TextureGUIDs2));

    // these are not right
    hdr->something = 1912602624;
    hdr->something2 = 1048576;

    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8; ++j)
            hdr->UnkSections[i].Unknown5[j] = 0xf0000000;

        uint32_t f1 = bColpass ? 5 : 0x17;

        hdr->UnkSections[i].Unknown6 = 4;
        hdr->UnkSections[i].Flags1 = f1;
        hdr->UnkSections[i].Flags2 = 6;
    }

    //////////////////////////////////////////
    /// cpu

    // bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad bad
    // todo: REVERSE THIS?!
    unsigned char testData[544] = {
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0xAB, 0xAA, 0x2A, 0x3E, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x1C, 0x46, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
        0x81, 0x95, 0xE3, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x3F, 0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xDE, 0x88, 0x1B, 0x3D, 0xDE, 0x88, 0x1B, 0x3D, 0xDE, 0x88, 0x1B, 0x3D,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };


    RPakVirtualSegment CPUSegment;
    uint32_t cpuIdx = RePak::CreateNewSegment(sizeof(MaterialCPUHeader) + 544, 3, 16, CPUSegment);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.Unknown.Index = cpuIdx;
    cpuhdr.Unknown.Offset = sizeof(MaterialCPUHeader);
    cpuhdr.DataSize = 544;

    RePak::RegisterDescriptor(cpuIdx, 0);

    char* cpuData = new char[sizeof(MaterialCPUHeader) + 544];

    memcpy_s(cpuData, 16, &cpuhdr, 16);

    memcpy_s(cpuData + sizeof(MaterialCPUHeader), 544, testData, 544);
    //////////////////////////////////////////

    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock dsdb{ dsIdx, DataSegment.DataSize, (uint8_t*)dataBuf };
    RePak::AddRawDataBlock(dsdb);

    RPakRawDataBlock cdb{ cpuIdx, CPUSegment.DataSize, (uint8_t*)cpuData };
    RePak::AddRawDataBlock(cdb);

    // now time to add the higher level asset entry
    RPakAssetEntryV8 asset;

    asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), shsIdx, 0, SubHeaderSegment.DataSize, cpuIdx, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.Version = MATL_VERSION;

    asset.HighestPageNum = cpuIdx + 1;
    asset.Un2 = bColpass ? 7 : 8; // what

    asset.UsesStartIndex = fileRelationIdx;
    asset.UsesCount = assetUsesCount;

    assetEntries->push_back(asset);
}