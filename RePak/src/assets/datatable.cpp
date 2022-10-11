#include "pch.h"
#include "Assets.h"
#include <regex>

std::unordered_map<std::string, dtblcoltype_t> DataTableColumnMap =
{
    { "bool",   dtblcoltype_t::Bool },
    { "int",    dtblcoltype_t::Int },
    { "float",  dtblcoltype_t::Float },
    { "vector", dtblcoltype_t::Vector },
    { "string", dtblcoltype_t::StringT },
    { "asset",  dtblcoltype_t::Asset },
    { "assetnoprecache", dtblcoltype_t::AssetNoPrecache }
};

static std::regex s_VectorStringRegex("<(.*),(.*),(.*)>");

// gets enum value from type string
// e.g. "string" to dtblcoltype::StringT
dtblcoltype_t GetDataTableTypeFromString(std::string sType)
{
    std::transform(sType.begin(), sType.end(), sType.begin(), ::tolower);

    for (const auto& [key, value] : DataTableColumnMap) // Iterate through unordered_map.
    {
        if (sType.compare(key) == 0) // Do they equal?
            return value;
    }

    return dtblcoltype_t::StringT;
}

// get required data size to store the specified data type
uint8_t DataTable_GetEntrySize(dtblcoltype_t type)
{
    switch (type)
    {
    case dtblcoltype_t::Bool:
    case dtblcoltype_t::Int:
    case dtblcoltype_t::Float:
        return 4;
    case dtblcoltype_t::Vector:
        return sizeof(float) * 3;
    case dtblcoltype_t::StringT:
    case dtblcoltype_t::Asset:
    case dtblcoltype_t::AssetNoPrecache:
        // string types get placed elsewhere and are referenced with a pointer
        return sizeof(RPakPtr);
    }

    Error("tried to get entry size for an unknown dtbl column type. asserting...\n");
    assert(0);
    return 0; // should be unreachable
}

void Assets::AddDataTableAsset_v0(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
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
    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(DataTableHeader), SF_HEAD, 8);

    // DataTableColumn entries
    _vseginfo_t colhdrinfo = pak->CreateNewSegment(sizeof(DataTableColumn) * columnCount, SF_CPU, 8, 64);

    // column names
    _vseginfo_t nameseginfo = pak->CreateNewSegment(ColumnNameBufSize, SF_CPU, 8, 64);

    pHdr->ColumnCount = columnCount;
    pHdr->RowCount = rowCount - 1;
    pHdr->ColumnHeaderPtr = { colhdrinfo.index, 0 };

    pak->AddPointer(subhdrinfo.index, offsetof(DataTableHeader, ColumnHeaderPtr));

    // allocate buffers for the loop
    char* namebuf = new char[ColumnNameBufSize];
    char* columnHeaderBuf = new char[sizeof(DataTableColumn) * columnCount];

    // vectors
    std::vector<std::string> typeRow = doc.GetRow<std::string>(rowCount - 1);
    std::vector<DataTableColumn> columns{};

    uint32_t nextNameOffset = 0;
    uint32_t colIdx = 0;
    // temp var used for storing the row offset for the next column in the loop below
    uint32_t tempColumnRowOffset = 0;
    uint32_t stringEntriesSize = 0;
    size_t rowDataPageSize = 0;

    for (auto& it : doc.GetColumnNames())
    {
        // copy the column name into the namebuf
        snprintf(namebuf + nextNameOffset, it.length() + 1, "%s", it.c_str());

        dtblcoltype_t type = GetDataTableTypeFromString(typeRow[colIdx]);

        DataTableColumn col{};

        // set the page index and offset
        col.NamePtr = { nameseginfo.index, nextNameOffset };
        col.RowOffset = tempColumnRowOffset;
        col.Type = type;

        columns.emplace_back(col);

        // register name pointer
        pak->AddPointer(colhdrinfo.index, (sizeof(DataTableColumn) * colIdx) + offsetof(DataTableColumn, NamePtr));

        if (type == dtblcoltype_t::StringT || type == dtblcoltype_t::Asset || type == dtblcoltype_t::AssetNoPrecache)
        {
            for (size_t i = 0; i < rowCount - 1; ++i)
            {
                // this can be std::string since we only really need to deal with the string types here
                std::vector<std::string> row = doc.GetRow<std::string>(i);

                stringEntriesSize += row[colIdx].length() + 1;
            }
        }

        *(DataTableColumn*)(columnHeaderBuf + (sizeof(DataTableColumn) * colIdx)) = col;

        tempColumnRowOffset += DataTable_GetEntrySize(type);
        rowDataPageSize += DataTable_GetEntrySize(type) * (rowCount - 1); // size of type * row count (excluding the type row)
        nextNameOffset += it.length() + 1;
        colIdx++;

        // if this is the final column, set the total row bytes to the column's row offset + the column's row size
        // (effectively the full length of the row)
        if (colIdx == columnCount)
            pHdr->RowStride = tempColumnRowOffset;
    }

    // page for Row Data
    _vseginfo_t rawdatainfo = pak->CreateNewSegment(rowDataPageSize, SF_CPU, 8, 64);

    // page for string entries
    _vseginfo_t stringsinfo = pak->CreateNewSegment(stringEntriesSize, SF_CPU, 8, 64);

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
            case dtblcoltype_t::Bool:
            {
                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);

                transform(val.begin(), val.end(), val.begin(), ::tolower);

                if (val == "true")
                    valbuf.write<uint32_t>(true);
                else
                    valbuf.write<uint32_t>(false);
                break;
            }
            case dtblcoltype_t::Int:
            {
                uint32_t val = doc.GetCell<uint32_t>(colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case dtblcoltype_t::Float:
            {
                float val = doc.GetCell<float>(colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case dtblcoltype_t::Vector:
            {
                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);

                std::smatch sm;

                // get values from format "<x,y,z>"
                std::regex_search(val, sm, s_VectorStringRegex);

                // 0 - all
                // 1 - x
                // 2 - y
                // 3 - z
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
            case dtblcoltype_t::StringT:
            case dtblcoltype_t::Asset:
            case dtblcoltype_t::AssetNoPrecache:
            {
                static uint32_t nextStringEntryOffset = 0;

                RPakPtr stringPtr{ stringsinfo.index, nextStringEntryOffset };

                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);
                snprintf(stringEntryBuf + nextStringEntryOffset, val.length() + 1, "%s", val.c_str());

                valbuf.write(stringPtr);
                pak->AddPointer(rawdatainfo.index, (pHdr->RowStride * rowIdx) + col.RowOffset);

                nextStringEntryOffset += val.length() + 1;
                break;
            }
            }
        }
    }

    pHdr->RowHeaderPtr = { rawdatainfo.index, 0 };

    pak->AddPointer(subhdrinfo.index, offsetof(DataTableHeader, RowHeaderPtr));

    // add raw data blocks
    pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr });
    pak->AddRawDataBlock({ colhdrinfo.index, colhdrinfo.size, (uint8_t*)columnHeaderBuf });
    pak->AddRawDataBlock({ nameseginfo.index, nameseginfo.size, (uint8_t*)namebuf });
    pak->AddRawDataBlock({ rawdatainfo.index, rowDataPageSize, (uint8_t*)rowDataBuf });
    pak->AddRawDataBlock({ stringsinfo.index, stringEntriesSize, (uint8_t*)stringEntryBuf });

    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, rawdatainfo.index, 0, -1, -1, (std::uint32_t)AssetType::DTBL);
    asset.version = DTBL_VERSION;

    asset.pageEnd = stringsinfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 1;

    assetEntries->push_back(asset);
}

// VERSION 8
void Assets::AddDataTableAsset_v1(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
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
    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(DataTableHeader), SF_HEAD, 8);

    // DataTableColumn entries
    _vseginfo_t colhdrinfo = pak->CreateNewSegment(sizeof(DataTableColumn) * columnCount, SF_CPU, 8, 64);

    // column names
    _vseginfo_t nameseginfo = pak->CreateNewSegment(ColumnNameBufSize, SF_CPU, 8, 64);

    pHdr->ColumnCount = columnCount;
    pHdr->RowCount = rowCount - 1;
    pHdr->ColumnHeaderPtr = { colhdrinfo.index, 0 };

    pak->AddPointer(subhdrinfo.index, offsetof(DataTableHeader, ColumnHeaderPtr));

    // allocate buffers for the loop
    char* namebuf = new char[ColumnNameBufSize];
    char* columnHeaderBuf = new char[sizeof(DataTableColumn) * columnCount];

    // vectors
    std::vector<std::string> typeRow = doc.GetRow<std::string>(rowCount - 1);
    std::vector<DataTableColumn> columns{};

    uint32_t nextNameOffset = 0;
    uint32_t colIdx = 0;
    // temp var used for storing the row offset for the next column in the loop below
    uint32_t tempColumnRowOffset = 0;
    uint32_t stringEntriesSize = 0;
    size_t rowDataPageSize = 0;

    for (auto& it : doc.GetColumnNames())
    {
        // copy the column name into the namebuf
        snprintf(namebuf + nextNameOffset, it.length() + 1, "%s", it.c_str());

        dtblcoltype_t type = GetDataTableTypeFromString(typeRow[colIdx]);

        DataTableColumn col{};

        // set the page index and offset
        col.NamePtr = { nameseginfo.index, nextNameOffset };
        col.RowOffset = tempColumnRowOffset;
        col.Type = type;

        columns.emplace_back(col);

        // register name pointer
        pak->AddPointer(colhdrinfo.index, (sizeof(DataTableColumn) * colIdx) + offsetof(DataTableColumn, NamePtr));

        if (type == dtblcoltype_t::StringT || type == dtblcoltype_t::Asset || type == dtblcoltype_t::AssetNoPrecache)
        {
            for (size_t i = 0; i < rowCount - 1; ++i)
            {
                // this can be std::string since we only really need to deal with the string types here
                std::vector<std::string> row = doc.GetRow<std::string>(i);

                stringEntriesSize += row[colIdx].length() + 1;
            }
        }

        *(DataTableColumn*)(columnHeaderBuf + (sizeof(DataTableColumn) * colIdx)) = col;

        tempColumnRowOffset += DataTable_GetEntrySize(type);
        rowDataPageSize += DataTable_GetEntrySize(type) * (rowCount - 1); // size of type * row count (excluding the type row)
        nextNameOffset += it.length() + 1;
        colIdx++;

        // if this is the final column, set the total row bytes to the column's row offset + the column's row size
        // (effectively the full length of the row)
        if (colIdx == columnCount)
            pHdr->RowStride = tempColumnRowOffset;
    }

    // page for Row Data
    _vseginfo_t rawdatainfo = pak->CreateNewSegment(rowDataPageSize, SF_CPU, 8, 64);

    // page for string entries
    _vseginfo_t stringsinfo = pak->CreateNewSegment(stringEntriesSize, SF_CPU, 8, 64);

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
            case dtblcoltype_t::Bool:
            {
                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);

                transform(val.begin(), val.end(), val.begin(), ::tolower);

                if (val == "true")
                    valbuf.write<uint32_t>(true);
                else
                    valbuf.write<uint32_t>(false);
                break;
            }
            case dtblcoltype_t::Int:
            {
                uint32_t val = doc.GetCell<uint32_t>(colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case dtblcoltype_t::Float:
            {
                float val = doc.GetCell<float>(colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case dtblcoltype_t::Vector:
            {
                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);

                std::smatch sm;

                // get values from format "<x,y,z>"
                std::regex_search(val, sm, s_VectorStringRegex);

                // 0 - all
                // 1 - x
                // 2 - y
                // 3 - z
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
            case dtblcoltype_t::StringT:
            case dtblcoltype_t::Asset:
            case dtblcoltype_t::AssetNoPrecache:
            {
                static uint32_t nextStringEntryOffset = 0;

                RPakPtr stringPtr{ stringsinfo.index, nextStringEntryOffset };

                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);
                snprintf(stringEntryBuf + nextStringEntryOffset, val.length() + 1, "%s", val.c_str());

                valbuf.write(stringPtr);
                pak->AddPointer(rawdatainfo.index, (pHdr->RowStride * rowIdx) + col.RowOffset);

                nextStringEntryOffset += val.length() + 1;
                break;
            }
            }
        }
    }

    pHdr->RowHeaderPtr = { rawdatainfo.index, 0 };

    pak->AddPointer(subhdrinfo.index, offsetof(DataTableHeader, RowHeaderPtr));

    // add raw data blocks
    pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr });
    pak->AddRawDataBlock({ colhdrinfo.index, colhdrinfo.size, (uint8_t*)columnHeaderBuf });
    pak->AddRawDataBlock({ nameseginfo.index, nameseginfo.size, (uint8_t*)namebuf });
    pak->AddRawDataBlock({ rawdatainfo.index, rowDataPageSize, (uint8_t*)rowDataBuf });
    pak->AddRawDataBlock({ stringsinfo.index, stringEntriesSize, (uint8_t*)stringEntryBuf });

    RPakAssetEntry asset;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, rawdatainfo.index, 0, -1, -1, (std::uint32_t)AssetType::DTBL);
    asset.version = DTBL_VERSION;

    asset.pageEnd = stringsinfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 1;

    assetEntries->push_back(asset);
}