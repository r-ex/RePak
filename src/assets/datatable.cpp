#include "pch.h"
#include "assets.h"
#include "public/table.h"

static const std::unordered_map<std::string, dtblcoltype_t> s_dataTableColumnTypeMap =
{
    { "bool",   dtblcoltype_t::Bool },
    { "int",    dtblcoltype_t::Int },
    { "float",  dtblcoltype_t::Float },
    { "vector", dtblcoltype_t::Vector },
    { "string", dtblcoltype_t::String },
    { "asset",  dtblcoltype_t::Asset },
    { "assetnoprecache", dtblcoltype_t::AssetNoPrecache }
};

// gets enum value from type string
// e.g. "string" to dtblcoltype::StringT
dtblcoltype_t DataTable_GetTypeFromString(std::string sType)
{
    std::transform(sType.begin(), sType.end(), sType.begin(), ::tolower);

    for (const auto& [key, value] : s_dataTableColumnTypeMap) // get each element in the type map
    {
        if (sType.compare(key) == 0) // are they equal?
            return value;
    }

    return dtblcoltype_t::String;
}

// get required data size to store the specified data type
uint8_t DataTable_GetEntrySize(dtblcoltype_t type)
{
    switch (type)
    {
    case dtblcoltype_t::Bool:
    case dtblcoltype_t::Int:
    case dtblcoltype_t::Float:
        return sizeof(int32_t);
    case dtblcoltype_t::Vector:
        return sizeof(Vector3);
    case dtblcoltype_t::String:
    case dtblcoltype_t::Asset:
    case dtblcoltype_t::AssetNoPrecache:
        // string types get placed elsewhere and are referenced with a pointer
        return sizeof(PagePtr_t);
    }

    Error("tried to get entry size for an unknown dtbl column type. asserting...\n");
    assert(0);
    return 0; // should be unreachable
}

bool DataTable_IsStringType(dtblcoltype_t type)
{
    switch (type)
    {
    case dtblcoltype_t::String:
    case dtblcoltype_t::Asset:
    case dtblcoltype_t::AssetNoPrecache:
        return true;
    default:
        return false;
    }
}

// fills a CPakDataChunk with column data from a provided csv
void DataTable_SetupColumns(CPakFile* pak, CPakDataChunk& colChunk, datatable_asset_t* pHdrTemp, rapidcsv::Document& doc)
{
    size_t colNameBufSize = 0;
    // get required size to store all of the column names in a single buffer
    for (auto& it : doc.GetColumnNames())
    {
        colNameBufSize += it.length() + 1;
    }

    // column names
    CPakDataChunk colNameChunk = pak->CreateDataChunk(colNameBufSize, SF_CPU, 8);

    // get a copy of the pointer because then we can shift it for each name
    char* colNameBuf = colNameChunk.Data();

    // vectors
    std::vector<std::string> typeRow = doc.GetRow<std::string>(pHdrTemp->numRows);

    for (uint32_t i = 0; i < pHdrTemp->numColumns; ++i)
    {
        std::string name = doc.GetColumnName(i);

        // copy the column name into the namebuf
        snprintf(colNameBuf, name.length() + 1, "%s", name.c_str());

        dtblcoltype_t type = DataTable_GetTypeFromString(typeRow[i]);

        datacolumn_t& col = pHdrTemp->pDataColums[i];

        // get number of bytes that we've added in the name buf so far
        col.pName = colNameChunk.GetPointer(colNameBuf - colNameChunk.Data());
        col.rowOffset = pHdrTemp->rowStride;
        col.type = type;

        // register name pointer
        pak->AddPointer(colChunk.GetPointer((sizeof(datacolumn_t) * i) + offsetof(datacolumn_t, pName)));

        if (DataTable_IsStringType(type))
        {
            for (uint32_t j = 0; j < pHdrTemp->numRows; ++j)
            {
                // this can be std::string since we only deal with the string types here
                std::vector<std::string> row = doc.GetRow<std::string>(j);

                pHdrTemp->stringEntriesSize += row[i].length() + 1;
            }
        }

        pHdrTemp->rowStride += DataTable_GetEntrySize(type);
        pHdrTemp->rowDataPageSize += DataTable_GetEntrySize(type) * pHdrTemp->numRows; // size of type * row count (excluding the type row)

        colNameBuf += name.length() + 1;
    }
}

void DataTable_SetupRows(CPakFile* pak, CPakDataChunk& rowDataChunk, CPakDataChunk& stringChunk, datatable_asset_t* pHdrTemp, rapidcsv::Document& doc)
{
    char* pStringBuf = stringChunk.Data();

    for (size_t rowIdx = 0; rowIdx < pHdrTemp->numRows; ++rowIdx)
    {
        for (size_t colIdx = 0; colIdx < pHdrTemp->numColumns; ++colIdx)
        {
            datacolumn_t& col = pHdrTemp->pDataColums[colIdx];

            // get rmem instance for this cell's value buffer
            rmem valbuf(rowDataChunk.Data() + (pHdrTemp->rowStride * rowIdx) + col.rowOffset);

            switch (col.type)
            {
            case dtblcoltype_t::Bool:
            {
                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);

                if (!_stricmp(val.c_str(), "true") || val == "1")
                    valbuf.write<uint32_t>(true);
                else if (!_stricmp(val.c_str(), "false") || val == "0")
                    valbuf.write<uint32_t>(false);
                else
                    Error("Invalid bool value at cell (%i, %i) in datatable %s\n", colIdx, rowIdx, pHdrTemp->assetPath);

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
                std::regex_search(val, sm, std::regex("<(.*),(.*),(.*)>"));

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
            case dtblcoltype_t::String:
            case dtblcoltype_t::Asset:
            case dtblcoltype_t::AssetNoPrecache:
            {
                std::string val = doc.GetCell<std::string>(colIdx, rowIdx);
                snprintf(pStringBuf, val.length() + 1, "%s", val.c_str());

                valbuf.write(stringChunk.GetPointer(pStringBuf - stringChunk.Data()));
                pak->AddPointer(rowDataChunk.GetPointer((pHdrTemp->rowStride * rowIdx) + col.rowOffset));

                pStringBuf += val.length() + 1;
                break;
            }
            }
        }
    }
}

void Assets::AddDataTableAsset_v0(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding dtbl asset '%s'\n", assetPath);

    REQUIRE_FILE(pak->GetAssetPath() + assetPath + ".csv");

    rapidcsv::Document doc(pak->GetAssetPath() + assetPath + ".csv");

    std::string sAssetName = assetPath;

    if (doc.GetColumnCount() < 0)
    {
        Warning("Attempted to add dtbl asset '%s' with no columns. Skipping asset...\n", assetPath);
        return;
    }

    if (doc.GetRowCount() < 2)
    {
        Warning("Attempted to add dtbl asset '%s' with invalid row count. Skipping asset...\nDTBL    - CSV must have a row of column types at the end of the table\n", assetPath);
        return;
    }

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(datatable_v0_t), SF_HEAD, 16);
    datatable_v0_t* pHdr = reinterpret_cast<datatable_v0_t*>(hdrChunk.Data());
    datatable_asset_t* pHdrTemp = new datatable_asset_t{}; // temp header that we store values in, this is for sharing funcs across versions

    pHdrTemp->numColumns = doc.GetColumnCount();
    pHdrTemp->numRows = doc.GetRowCount() - 1; // -1 because last row isnt added (used for type info)
    pHdrTemp->assetPath = assetPath;

    // create column chunk
    CPakDataChunk colChunk = pak->CreateDataChunk(sizeof(datacolumn_t) * pHdrTemp->numColumns, SF_CPU, 8);

    // colums from data chunk
    pHdrTemp->pDataColums = reinterpret_cast<datacolumn_t*>(colChunk.Data());

    // setup data in column data chunk
    DataTable_SetupColumns(pak, colChunk, pHdrTemp, doc);

    // setup column page ptr
    pHdrTemp->pColumns = colChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(datatable_asset_t, pColumns)));

    // page for Row Data
    CPakDataChunk rowDataChunk = pak->CreateDataChunk(pHdrTemp->rowDataPageSize, SF_CPU, 8);

    // page for string entries
    CPakDataChunk stringChunk = pak->CreateDataChunk(pHdrTemp->stringEntriesSize, SF_CPU, 8);

    // setup row data chunks
    DataTable_SetupRows(pak, rowDataChunk, stringChunk, pHdrTemp, doc);

    // setup row page ptr
    pHdrTemp->pRows = rowDataChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(datatable_asset_t, pRows)));

    pHdrTemp->SetDTBL_V0(pHdr);

    PakAsset_t asset;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()),
        hdrChunk.GetPointer(), hdrChunk.GetSize(),
        rowDataChunk.GetPointer(),
        -1, -1, (std::uint32_t)AssetType::DTBL);

    asset.version = 0; // really need defines for R2 assets

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 1; // asset only depends on itself

    assetEntries->push_back(asset);

    delete pHdrTemp;
}

// VERSION 8
void Assets::AddDataTableAsset_v1(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding dtbl asset '%s'\n", assetPath);

    REQUIRE_FILE(pak->GetAssetPath() + assetPath + ".csv");

    rapidcsv::Document doc(pak->GetAssetPath() + assetPath + ".csv");

    std::string sAssetName = assetPath;

    if (doc.GetColumnCount() < 0)
    {
        Warning("Attempted to add dtbl asset '%s' with no columns. Skipping asset...\n", assetPath);
        return;
    }

    if (doc.GetRowCount() < 2)
    {
        Warning("Attempted to add dtbl asset '%s' with invalid row count. Skipping asset...\nDTBL    - CSV must have a row of column types at the end of the table\n", assetPath);
        return;
    }

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(datatable_v1_t), SF_HEAD, 16);
    datatable_v1_t* pHdr = reinterpret_cast<datatable_v1_t*>(hdrChunk.Data());
    datatable_asset_t* pHdrTemp = new datatable_asset_t{}; // temp header that we store values in, this is for sharing funcs across versions

    pHdrTemp->numColumns = doc.GetColumnCount();
    pHdrTemp->numRows = doc.GetRowCount()-1; // -1 because last row isnt added (used for type info)
    pHdrTemp->assetPath = assetPath;

    // create column chunk
    CPakDataChunk colChunk = pak->CreateDataChunk(sizeof(datacolumn_t) * pHdrTemp->numColumns, SF_CPU, 8);
    
    // colums from data chunk
    pHdrTemp->pDataColums = reinterpret_cast<datacolumn_t*>(colChunk.Data());

    // setup data in column data chunk
    DataTable_SetupColumns(pak, colChunk, pHdrTemp, doc);

    // setup column page ptr
    pHdrTemp->pColumns = colChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(datatable_asset_t, pColumns)));

    // page for Row Data
    CPakDataChunk rowDataChunk = pak->CreateDataChunk(pHdrTemp->rowDataPageSize, SF_CPU, 8);

    // page for string entries
    CPakDataChunk stringChunk = pak->CreateDataChunk(pHdrTemp->stringEntriesSize, SF_CPU, 8);

    // setup row data chunks
    DataTable_SetupRows(pak, rowDataChunk, stringChunk, pHdrTemp, doc);

    // setup row page ptr
    pHdrTemp->pRows = rowDataChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(datatable_asset_t, pRows)));

    pHdrTemp->SetDTBL_V1(pHdr);

    PakAsset_t asset;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), 
        hdrChunk.GetPointer(), hdrChunk.GetSize(),
        rowDataChunk.GetPointer(),
        -1, -1, (std::uint32_t)AssetType::DTBL);

    asset.version = DTBL_VERSION;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 1; // asset only depends on itself

    assetEntries->push_back(asset);

    delete pHdrTemp;
}