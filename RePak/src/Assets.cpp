#include "pch.h"
#include "Assets.h"
#include <regex>
#include "rmem.h"

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

static std::regex s_VectorStringRegex("<(.*),(.*),(.*)>");

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
        // strings get placed at a separate place and have a pointer in place of the actual value
        return sizeof(RPakPtr);
    }

    return 0; // should be unreachable
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
            
            rmem valbuf(EntryPtr);

            switch (col.Type)
            {
            case DataTableColumnDataType::Bool:
            {
                std::string val = doc.GetCell<std::string>(columnIdx, rowIdx);

                transform(val.begin(), val.end(), val.begin(), ::tolower);

                if (val == "true")
                    valbuf.write<uint32_t>(true);
                else
                    valbuf.write<uint32_t>(false);
                break;
            }
            case DataTableColumnDataType::Int:
            {
                uint32_t val = doc.GetCell<uint32_t>(columnIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case DataTableColumnDataType::Float:
            {
                float val = doc.GetCell<float>(columnIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case DataTableColumnDataType::Vector:
            {
                std::string val = doc.GetCell<std::string>(columnIdx, rowIdx);

                std::smatch sm;

                std::regex_search(val, sm, s_VectorStringRegex);

                if (sm.size() == 4)
                {
                    float x = atof(sm[1].str().c_str());
                    float y = atof(sm[2].str().c_str());
                    float z = atof(sm[3].str().c_str());
                    Vector3 vec(x, y, z);

                    valbuf.write(vec);
                }
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

                valbuf.write(stringPtr);
                RePak::RegisterDescriptor(rdsIdx, (hdr->RowStride * rowIdx) + col.RowOffset);

                nextStringEntryOffset += val.length() + 1;
                break;
            }
            }
        }
    }

    hdr->RowHeaderPtr = { rdsIdx, 0 };

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

    UIImageHeader* pHdr = new UIImageHeader();

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

    // grab the dimensions of the atlas
    BinaryIO atlas;
    atlas.open(sAtlasFilePath, BinaryIOMode::BinaryIOMode_Read);
    atlas.seek(4, std::ios::beg);
    DDS_HEADER ddsh = atlas.read<DDS_HEADER>();

    atlas.close();

    pHdr->Width = ddsh.width;
    pHdr->Height = ddsh.height;

    pHdr->TextureOffsetsCount = nTexturesCount;
    pHdr->TextureCount = nTexturesCount == 1 ? 0 : nTexturesCount; // don't even ask

    pHdr->TextureGuid = atlasGuid;
    
    // calculate data sizes so we can allocate a page and segment
    uint32_t textureOffsetsDataSize = sizeof(UIImageOffset) * nTexturesCount;
    uint32_t textureDimensionsDataSize = sizeof(uint16_t) * 2 * nTexturesCount;
    uint32_t textureHashesDataSize = (sizeof(uint32_t) + sizeof(uint64_t)) * nTexturesCount;

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

    // textureGUID descriptors
    RePak::RegisterGuidDescriptor(shsIdx, offsetof(UIImageHeader, TextureGuid));

    // buffer for texture info data
    char* pTextureInfoBuf = new char[textureInfoPageSize];
    rmem tiBuf(pTextureInfoBuf);

    // set texture offset page index and offset
    pHdr->TextureOffsetsIndex = tisIdx;
    pHdr->TextureOffsetsOffset = 0; // start of the page

    for (auto& it : mapEntry["textures"].GetArray())
    {
        UIImageOffset uiio{};
        tiBuf.write(uiio);
    }

    // set texture dimensions page index and offset
    pHdr->TextureDimsIndex = tisIdx;
    pHdr->TextureDimsOffset = textureOffsetsDataSize;

    for (auto& it : mapEntry["textures"].GetArray())
    {
        tiBuf.write<uint16_t>(it["width"].GetInt());
        tiBuf.write<uint16_t>(it["height"].GetInt());
    }

    // set texture hashes page index and offset
    pHdr->TextureHashesIndex = tisIdx;
    pHdr->TextureHashesOffset = textureOffsetsDataSize + textureDimensionsDataSize;

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
    
    for (int i = 0; i < nTexturesCount; ++i)
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

        hdr->dataLength = ddsh.pitchOrLinearSize;
        hdr->width = ddsh.width;
        hdr->height = ddsh.height;
        
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

        hdr->format = (uint16_t)TxtrFormatMap[dxgiFormat];

        // go to the end of the main header
        input.seek(ddsh.size + 4);

        if (dxgiFormat == DXGI_FORMAT_BC7_UNORM || dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB)
            input.seek(20, std::ios::cur);
    }

    hdr->assetGuid = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    hdr->permanentMipLevels = 1;

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
    uint32_t rdsIdx = RePak::CreateNewSegment(hdr->dataLength, 3, 16, RawDataSegment);

    char* databuf = new char[hdr->dataLength];

    input.getReader()->read(databuf, hdr->dataLength);

    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    if (bSaveDebugName)
    {
        RPakRawDataBlock ndb{ nsIdx, DebugNameSegment.DataSize, (uint8_t*)namebuf };
        RePak::AddRawDataBlock(ndb);
        hdr->pDebugName = { nsIdx, 0 };
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

    PtchHeader* pHdr = new PtchHeader();

    pHdr->patchedPakCount = mapEntry["entries"].GetArray().Size();

    std::vector<PtchEntry> patchEntries{};
    uint32_t entryNamesSectionSize = 0;

    for (auto& it : mapEntry["entries"].GetArray())
    {
        std::string name = it["name"].GetStdString();
        uint8_t patchNum = it["patchnum"].GetInt();
        
        patchEntries.push_back({ name, patchNum, entryNamesSectionSize });

        entryNamesSectionSize += name.length() + 1;
    }

    size_t dataPageSize = (sizeof(RPakPtr) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + entryNamesSectionSize;

    RPakVirtualSegment SubHeaderPage;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(PtchHeader), 0, 8, SubHeaderPage);

    RPakVirtualSegment DataPage;
    uint32_t rdsIdx = RePak::CreateNewSegment(dataPageSize, 1, 8, DataPage);

    pHdr->pPakNames = { rdsIdx, 0 };
    pHdr->pPakPatchNums = { rdsIdx, (int)sizeof(RPakPtr) * pHdr->patchedPakCount };

    RePak::RegisterDescriptor(shsIdx, offsetof(PtchHeader, pPakNames));
    RePak::RegisterDescriptor(shsIdx, offsetof(PtchHeader, pPakPatchNums));

    char* pDataBuf = new char[dataPageSize];
    rmem dataBuf(pDataBuf);

    uint32_t i = 0;
    for (auto& it : patchEntries)
    {
        uint32_t fileNameOffset = (sizeof(RPakPtr) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + it.FileNamePageOffset;

        // write the ptr to the file name into the buffer
        dataBuf.write<RPakPtr>({ rdsIdx, fileNameOffset }, sizeof(RPakPtr) * i);
        // write the patch number for this entry into the buffer
        dataBuf.write<uint8_t>(it.PatchNum, pHdr->pPakPatchNums.Offset + i);

        snprintf(pDataBuf + fileNameOffset, it.FileName.length() + 1, "%s", it.FileName.c_str());

        RePak::RegisterDescriptor(rdsIdx, sizeof(RPakPtr) * i);
        i++;
    }

    RPakRawDataBlock shdb{ shsIdx, SubHeaderPage.DataSize, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ rdsIdx, dataPageSize, (uint8_t*)pDataBuf };
    RePak::AddRawDataBlock(rdb);

    // create and init the asset entry
    RPakAssetEntryV8 asset;

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

    ModelHeader* pHdr = new ModelHeader();

    std::string rmdlFilePath = g_sAssetsDir + sAssetName;
    std::string vgFilePath = g_sAssetsDir + std::string(assetPath) + ".vg";

    ///-------------------
    // Begin skeleton(.rmdl) input
    BinaryIO skelInput;
    skelInput.open(rmdlFilePath, BinaryIOMode::BinaryIOMode_Read);

    studiohdrshort_t mdlhdr = skelInput.read<studiohdrshort_t>();

    if (mdlhdr.id != 0x54534449)
    {
        Warning("invalid file magic for model asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x54534449, mdlhdr.id);
        return;
    }

    if (mdlhdr.version != 54)
    {
        Warning("invalid version for model asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 0x36, mdlhdr.version);
        return;
    }

    // go back to the beginning of the file to read all the data
    skelInput.seek(0);

    uint32_t fileNameDataSize = sAssetName.length() + 1;

    char* pDataBuf = new char[fileNameDataSize + mdlhdr.dataLength];

    // write the model file path into the data buffer
    snprintf(pDataBuf, fileNameDataSize, "%s", sAssetName.c_str());
    // write the skeleton data into the data buffer
    skelInput.getReader()->read(pDataBuf + fileNameDataSize, mdlhdr.dataLength);
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
        return;
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

    pHdr->StreamedDataSize = vgFileSize;

    RPakVirtualSegment SubHeaderSegment{};
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(ModelHeader), 0, 8, SubHeaderSegment);

    RPakVirtualSegment DataSegment{};
    uint32_t dataSegIdx = RePak::CreateNewSegment(mdlhdr.dataLength + fileNameDataSize, 1, 64, DataSegment);

    //RPakVirtualSegment VGSegment{};
    //uint32_t vgIdx = RePak::CreateNewSegment(vgFileSize, 67, SegmentType::Unknown, DataSegment);

    pHdr->SkeletonPtr.Index = dataSegIdx;
    pHdr->SkeletonPtr.Offset = fileNameDataSize;


    pHdr->NamePtr.Index = dataSegIdx;
    pHdr->NamePtr.Offset = 0;

    //hdr->VGPtr.Index = vgIdx;
    //hdr->VGPtr.Offset = 0;

    RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, SkeletonPtr));
    RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, NamePtr));
    //RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, VGPtr));


    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ dataSegIdx, DataSegment.DataSize, (uint8_t*)pDataBuf };
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

    uint32_t assetUsesCount = 0; // Track how often the asset is used.
    MaterialHeader* mtlHdr = new MaterialHeader();
    std::string sAssetPath = std::string(assetPath);

    std::string type = "sknp";

    if (mapEntry.HasMember("type"))
        type = mapEntry["type"].GetStdString();
    else
        Warning("Adding material without an explicitly defined type. Assuming 'sknp'...\n");

    std::string sFullAssetRpakPath = "material/" + sAssetPath + "_" + type + ".rpak"; // Make full rpak asset path.

    mtlHdr->AssetGUID = RTech::StringToGuid(sFullAssetRpakPath.c_str()); // Convert full rpak asset path to textureGUID and set it in the material header.

    // Game ignores this field when parsing, retail rpaks also have this as 0. But In-Game its being set to either 0x4, 0x5, 0x9.
    // Based on resolution.
    // 512x512 = 0x5
    // 1024x1024 = 0x4
    // 2048x2048 = 0x9
    if (mapEntry.HasMember("signature"))
        mtlHdr->UnknownSignature = mapEntry["signature"].GetInt();

    if (mapEntry.HasMember("width")) // Set material width.
        mtlHdr->Width = mapEntry["width"].GetInt();

    if (mapEntry.HasMember("height")) // Set material width.
        mtlHdr->Height = mapEntry["height"].GetInt();

    if (mapEntry.HasMember("flags")) // Set flags properly. Responsible for texture stretching, tiling etc.
        mtlHdr->ImageFlags = mapEntry["flags"].GetUint();

    std::string surface = "default";

    // surfaces are defined in scripts/surfaceproperties.rson
    if (mapEntry.HasMember("surface"))
        surface = mapEntry["surface"].GetStdString();

    // Get the size of the texture guid section.
    size_t textureRefSize = 0;

    if (mapEntry.HasMember("textures"))
    {
        textureRefSize = mapEntry["textures"].GetArray().Size() * 8;
    }
    else
    {
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
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the first TextureGUID Map.
    {
        if (it.GetStdString() != "")
        {
            uint64_t textureGUID = RTech::StringToGuid((it.GetStdString() + ".rpak").c_str()); // Convert texture path to guid.
            *(uint64_t*)dataBuf = textureGUID;
            RePak::RegisterGuidDescriptor(dsIdx, guidPageOffset + (textureIdx * sizeof(uint64_t))); // Register GUID descriptor for current texture index.
            
            if (fileRelationIdx == -1)
                fileRelationIdx = RePak::AddFileRelation(assetEntries->size());
            else
                RePak::AddFileRelation(assetEntries->size());

            auto txtrAsset = RePak::GetAssetByGuid(assetEntries, textureGUID, nullptr);

            txtrAsset->RelationsStartIndex = fileRelationIdx;
            txtrAsset->RelationsCount++;

            assetUsesCount++;
        }
        dataBuf += sizeof(uint64_t);
        textureIdx++; // Next texture index coming up.
    }

    textureIdx = 0; // reset index for next TextureGUID Section.
    for (auto& it : mapEntry["textures"].GetArray()) // Now we setup the second TextureGUID Map.
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

            assetUsesCount++; // Next texture index coming up.
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
    mtlHdr->Name.Index  = dsIdx;
    mtlHdr->Name.Offset = 0;

    mtlHdr->SurfaceName.Index = dsIdx;
    mtlHdr->SurfaceName.Offset = (sAssetPath.length() + 1) + assetPathAlignment + (textureRefSize*2);

    RePak::RegisterDescriptor(shsIdx, offsetof(MaterialHeader, Name));
    RePak::RegisterDescriptor(shsIdx, offsetof(MaterialHeader, SurfaceName));

    // Type Handling
    if (type == "sknp")
    {
        // These should always be constant (per each material type)
        // There's different versions of these for each material type
        // GUIDRefs[4] is Colpass entry.
        mtlHdr->GUIDRefs[0] = 0x2B93C99C67CC8B51;
        mtlHdr->GUIDRefs[1] = 0x1EBD063EA03180C7;
        mtlHdr->GUIDRefs[2] = 0xF95A7FA9E8DE1A0E;
        mtlHdr->GUIDRefs[3] = 0x227C27B608B3646B;

        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs));
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 8);
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 16);
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 24);
        
        RePak::AddFileRelation(assetEntries->size(), 4);
        assetUsesCount += 4;

        mtlHdr->ShaderSetGUID = 0x1D9FFF314E152725;
    }
    else if (type == "wldc")
    {
        // GUIDRefs[4] is Colpass entry which is optional for wldc.
        mtlHdr->GUIDRefs[0] = 0x435FA77E363BEA48; // DepthShadow
        mtlHdr->GUIDRefs[1] = 0xF734F96BE92E0E71; // DepthPrepass
        mtlHdr->GUIDRefs[2] = 0xD306370918620EC0; // DepthVSM
        mtlHdr->GUIDRefs[3] = 0xDAB17AEAD2D3387A; // DepthShadowTight

        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs));
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 8);
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 16);
        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 24);

        RePak::AddFileRelation(assetEntries->size(), 4);
        assetUsesCount += 4;

        mtlHdr->ShaderSetGUID = 0x4B0F3B4CBD009096;
    }

    RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, ShaderSetGUID));
    RePak::AddFileRelation(assetEntries->size());
    assetUsesCount++;

    // Is this a colpass asset?
    bool bColpass = false;
    if (mapEntry.HasMember("colpass"))
    {
        std::string colpassPath = "material/" + mapEntry["colpass"].GetStdString() + ".rpak";
        mtlHdr->GUIDRefs[4] = RTech::StringToGuid(colpassPath.c_str());

        RePak::RegisterGuidDescriptor(shsIdx, offsetof(MaterialHeader, GUIDRefs) + 32);
        RePak::AddFileRelation(assetEntries->size());
        assetUsesCount++;

        bColpass = false;
    }
    mtlHdr->TextureGUIDs.Index = dsIdx;
    mtlHdr->TextureGUIDs.Offset = guidPageOffset;

    mtlHdr->TextureGUIDs2.Index = dsIdx;
    mtlHdr->TextureGUIDs2.Offset = guidPageOffset + textureRefSize;

    RePak::RegisterDescriptor(shsIdx, offsetof(MaterialHeader, TextureGUIDs));
    RePak::RegisterDescriptor(shsIdx, offsetof(MaterialHeader, TextureGUIDs2));

    mtlHdr->something = 0x72000000;
    mtlHdr->something2 = 0x100000;

    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8; ++j)
            mtlHdr->UnkSections[i].Unknown5[j] = 0xf0000000;

        uint32_t f1 = bColpass ? 0x5 : 0x17;

        mtlHdr->UnkSections[i].Unknown6 = 4;
        mtlHdr->UnkSections[i].Flags1 = f1;
        mtlHdr->UnkSections[i].Flags2 = 6;
    }

    //////////////////////////////////////////
    /// cpu

    // required for accurate colour
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

    std::uint64_t cpuDataSize = sizeof(testData) / sizeof(unsigned char);

    RPakVirtualSegment CPUSegment;
    uint32_t cpuIdx = RePak::CreateNewSegment(sizeof(MaterialCPUHeader) + cpuDataSize, 3, 16, CPUSegment);

    MaterialCPUHeader cpuhdr{};
    cpuhdr.Unknown.Index = cpuIdx;
    cpuhdr.Unknown.Offset = sizeof(MaterialCPUHeader);
    cpuhdr.DataSize = cpuDataSize;

    RePak::RegisterDescriptor(cpuIdx, 0);

    char* cpuData = new char[sizeof(MaterialCPUHeader) + cpuDataSize];

    memcpy_s(cpuData, 16, &cpuhdr, 16);

    memcpy_s(cpuData + sizeof(MaterialCPUHeader), cpuDataSize, testData, cpuDataSize);
    //////////////////////////////////////////

    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)mtlHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock dsdb{ dsIdx, DataSegment.DataSize, (uint8_t*)dataBuf };
    RePak::AddRawDataBlock(dsdb);

    RPakRawDataBlock cdb{ cpuIdx, CPUSegment.DataSize, (uint8_t*)cpuData };
    RePak::AddRawDataBlock(cdb);

    //////////////////////////////////////////

    RPakAssetEntryV8 asset;

    asset.InitAsset(RTech::StringToGuid(sFullAssetRpakPath.c_str()), shsIdx, 0, SubHeaderSegment.DataSize, cpuIdx, 0, -1, -1, (std::uint32_t)AssetType::MATL);
    asset.Version = MATL_VERSION;

    asset.HighestPageNum = cpuIdx + 1;
    asset.Un2 = bColpass ? 7 : 8; // what

    asset.UsesStartIndex = fileRelationIdx;
    asset.UsesCount = assetUsesCount;

    assetEntries->push_back(asset);
}