#include "pch.h"
#include "Assets.h"
#include <regex>

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

    DataTableHeader* pHdr = new DataTableHeader();

    const size_t columnCount = doc.GetColumnCount();
    const size_t rowCount = doc.GetRowCount();

    if (columnCount < 0)
    {
        Warning("Attempted to add dtbl asset with no columns. Skipping asset...\n");
        return;
    }

    if (rowCount < 2)
    {
        Warning("Attempted to add dtbl asset with invalid row count. Skipping asset...\nDTBL    - CSV must have a row of column types at the end of the table\n");
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

    ///-----------------------------------------
    // make a page for the sub header
    //
    RPakVirtualSegment SubHeaderSegment{};
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(DataTableHeader), 0, 8, SubHeaderSegment);

    // DataTableColumn entries
    RPakVirtualSegment ColumnHeaderSegment{};
    uint32_t chsIdx = RePak::CreateNewSegment(sizeof(DataTableColumn) * columnCount, 1, 8, ColumnHeaderSegment, 64);

    // column names
    RPakVirtualSegment ColumnNamesSegment{};
    uint32_t nameSegIdx = RePak::CreateNewSegment(ColumnNameBufSize, 1, 8, ColumnNamesSegment, 64);


    pHdr->ColumnCount = columnCount;
    pHdr->RowCount = rowCount - 1;
    pHdr->ColumnHeaderPtr = { chsIdx, 0 };

    RePak::RegisterDescriptor(shsIdx, offsetof(DataTableHeader, ColumnHeaderPtr));

    // allocate buffers for the loop
    char* namebuf = new char[ColumnNameBufSize];
    char* columnHeaderBuf = new char[sizeof(DataTableColumn) * columnCount];

    uint32_t nextNameOffset = 0;
    uint32_t colIdx = 0;
    // temp var used for storing the row offset for the next column in the loop below
    uint32_t tempColumnRowOffset = 0;
    uint32_t stringEntriesSize = 0;
    size_t rowDataPageSize = 0;

    for (auto& it : doc.GetColumnNames())
    {
        DataTableColumn col{};
        // copy the column name into the namebuf
        snprintf(namebuf + nextNameOffset, it.length() + 1, "%s", it.c_str());

        DataTableColumnDataType type = GetDataTableTypeFromString(typeRow[colIdx]);

        // set the page index and offset
        col.NamePtr = { nameSegIdx, nextNameOffset };
        col.RowOffset = tempColumnRowOffset;
        col.Type = type;

        // register name pointer
        RePak::RegisterDescriptor(chsIdx, (sizeof(DataTableColumn) * colIdx) + offsetof(DataTableColumn, NamePtr));

        uint32_t columnEntrySize = 0;

        if (type == DataTableColumnDataType::StringT || type == DataTableColumnDataType::Asset || type == DataTableColumnDataType::AssetNoPrecache)
        {
            for (size_t i = 0; i < rowCount - 1; ++i)
            {
                // this can be std::string since we only really need to deal with the string types here
                auto row = doc.GetRow<std::string>(i);

                stringEntriesSize += row[colIdx].length() + 1;
            }
        }

        columnEntrySize = DataTable_GetEntrySize(type);

        columns.emplace_back(col);

        *(DataTableColumn*)(columnHeaderBuf + (sizeof(DataTableColumn) * colIdx)) = col;

        tempColumnRowOffset += columnEntrySize;
        rowDataPageSize += columnEntrySize * (rowCount - 1);
        nextNameOffset += it.length() + 1;
        colIdx++;

        // if this is the final column, set the total row bytes to the column's row offset + the column's row size
        // (effectively the full length of the row)
        if (colIdx == columnCount)
            pHdr->RowStride = tempColumnRowOffset;
    }

    // page for Row Data
    RPakVirtualSegment RowDataSegment{};
    uint32_t rdsIdx = RePak::CreateNewSegment(rowDataPageSize, 1, 8, RowDataSegment, 64);

    // page for string entries
    RPakVirtualSegment StringEntrySegment{};
    uint32_t sesIdx = RePak::CreateNewSegment(stringEntriesSize, 1, 8, StringEntrySegment, 64);

    char* rowDataBuf = new char[rowDataPageSize];

    char* stringEntryBuf = new char[stringEntriesSize];

    for (size_t rowIdx = 0; rowIdx < rowCount - 1; ++rowIdx)
    {
        for (size_t colIdx = 0; colIdx < columnCount; ++colIdx)
        {
            DataTableColumn col = columns[colIdx];

            char* EntryPtr = (rowDataBuf + (pHdr->RowStride * rowIdx) + col.RowOffset);

            rmem valbuf(EntryPtr);

            switch (col.Type)
            {
            case DataTableColumnDataType::Bool:
            {
                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);

                transform(val.begin(), val.end(), val.begin(), ::tolower);

                if (val == "true")
                    valbuf.write<uint32_t>(true);
                else
                    valbuf.write<uint32_t>(false);
                break;
            }
            case DataTableColumnDataType::Int:
            {
                uint32_t val = doc.GetCell<uint32_t>(colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case DataTableColumnDataType::Float:
            {
                float val = doc.GetCell<float>(colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case DataTableColumnDataType::Vector:
            {
                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);

                std::smatch sm;

                // get values from format "<x,y,z>"
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

                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);
                snprintf(stringEntryBuf + nextStringEntryOffset, val.length() + 1, "%s", val.c_str());

                valbuf.write(stringPtr);
                RePak::RegisterDescriptor(rdsIdx, (pHdr->RowStride * rowIdx) + col.RowOffset);

                nextStringEntryOffset += val.length() + 1;
                break;
            }
            }
        }
    }

    pHdr->RowHeaderPtr = { rdsIdx, 0 };

    RePak::RegisterDescriptor(shsIdx, offsetof(DataTableHeader, RowHeaderPtr));

    RPakRawDataBlock shDataBlock{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)pHdr };
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

    asset.HighestPageNum = sesIdx + 1; // number of the highest page that the asset references pageidx + 1
    asset.Un2 = 1;

    assetEntries->push_back(asset);
}