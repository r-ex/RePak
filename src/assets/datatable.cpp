#include "pch.h"
#include "assets.h"
#include "public/datatable.h"

static inline size_t DataTable_CalcColumnNameBufSize(const rapidcsv::Document& doc)
{
    size_t colNameBufSize = 0;

    // get required size to store all of the column names in a single buffer
    for (const std::string& it : doc.GetColumnNames())
    {
        colNameBufSize += it.length() + 1;
    }

    return colNameBufSize;
}

static void DataTable_ReportInvalidDataTypeError(const char* const type, const uint32_t rowIdx, const uint32_t colIdx)
{
    Error("Invalid data type \"%s\" at cell [%u,%u].\n", type, rowIdx, colIdx);
}

template <typename datatable_t>
static void DataTable_SetupRows(const rapidcsv::Document& doc, datatable_t* const dtblHdr, datatable_asset_t& tmp, std::vector<std::string>& outTypeRow)
{
    // cache it so we don't have to make another deep copy.
    outTypeRow = doc.GetRow<std::string>(dtblHdr->numRows);
    const uint32_t numTypeNames = static_cast<uint32_t>(outTypeRow.size());

    // typically happens when there's an empty line in the csv file.
    if (numTypeNames != dtblHdr->numColumns)
        Error("Expected %u columns for type name row, found %u.\n", dtblHdr->numRows, numTypeNames);

    // Make sure every row (including rows we don't end up storing in the pak),
    // have the same number of columns as the type row. The column count in the
    // datatable header is set to the count in the type row and therefore all
    // other rows must match this count.
    for (uint32_t i = 0; i < doc.GetRowCount(); ++i)
    {
        const uint32_t columnCount = static_cast<uint32_t>(doc.GetColumnCount(i));

        if (columnCount != dtblHdr->numColumns)
            Error("Expected %u columns for data row #%u, found %u.\n", dtblHdr->numColumns, i, columnCount);
    }

    for (uint32_t i = 0; i < dtblHdr->numColumns; ++i)
    {
        const std::string& typeString = outTypeRow[i];
        const dtblcoltype_t type = DataTable_GetTypeFromString(typeString);

        if (type == dtblcoltype_t::INVALID)
            DataTable_ReportInvalidDataTypeError(typeString.c_str(), dtblHdr->numRows, i);

        if (DataTable_IsStringType(type))
        {
            for (uint32_t j = 0; j < dtblHdr->numRows; ++j)
            {
                // this can be std::string since we only deal with the string types here
                std::vector<std::string> row = doc.GetRow<std::string>(j);
                tmp.rowStringValueBufSize += row[i].length() + 1;
            }
        }

        tmp.rowPodValueBufSize += static_cast<size_t>(DataTable_GetValueSize(type)) * dtblHdr->numRows; // size of type * row count (excluding the type row)
    }
}

// fills a PakPageDataChunk_s with column data from a provided csv
template <typename datatable_t>
static void DataTable_SetupColumns(CPakFileBuilder* const pak, PakPageLump_s& dataChunk, const size_t columnNameBase, datatable_t* const dtblHdr,
    datatable_asset_t& tmp, const rapidcsv::Document& doc, const std::vector<std::string>& typeRow)
{
    char* const colNameBufBase = &dataChunk.data[columnNameBase];
    char* colNameBuf = colNameBufBase;

    for (uint32_t i = 0; i < dtblHdr->numColumns; ++i)
    {
        const std::string name = doc.GetColumnName(i);
        const size_t nameBufLen = name.length() + 1;

        // copy the column name into the namebuf
        memcpy(colNameBuf, name.c_str(), nameBufLen);

        datacolumn_t& col = tmp.pDataColums[i];

        // register name pointer
        pak->AddPointer(dataChunk, ((sizeof(datacolumn_t) * i) + offsetof(datacolumn_t, pName)), dataChunk, columnNameBase + (colNameBuf - colNameBufBase));
        colNameBuf += nameBufLen;

        const std::string& typeString = typeRow[i];
        const dtblcoltype_t type = DataTable_GetTypeFromString(typeString);

        if (type == dtblcoltype_t::INVALID)
            DataTable_ReportInvalidDataTypeError(typeString.c_str(), dtblHdr->numRows, i);

        col.rowOffset = dtblHdr->rowStride;
        col.type = type;

        dtblHdr->rowStride += DataTable_GetValueSize(type);
    }
}

template <typename T>
static T DataTable_ParseCellFromDocument(rapidcsv::Document& doc, const uint32_t colIdx, const uint32_t rowIdx)
{
    try {
        return doc.GetCell<T>(colIdx, rowIdx);
    }
    catch (const std::exception& ex) {
        Error("Exception while parsing cell [%u,%u]: %s.\n", rowIdx, colIdx, ex.what());
        return T{};
    }
}

static void DataTable_ReportInvalidValueError(const dtblcoltype_t type, const uint32_t rowIdx, const uint32_t colIdx)
{
    Error("Invalid %s value at cell [%u,%u].\n", DataTable_GetStringFromType(type), rowIdx, colIdx);
}

// fills a PakPageDataChunk_s with row data from a provided csv
template <typename datatable_t>
static void DataTable_SetupValues(CPakFileBuilder* const pak, PakPageLump_s& dataChunk, const size_t podValueBase, const size_t stringValueBase,
    datatable_t* const dtblHdr, datatable_asset_t& tmp, rapidcsv::Document& doc)
{
    char* const pStringBufBase = &dataChunk.data[stringValueBase];
    char* pStringBuf = pStringBufBase;

    for (uint32_t rowIdx = 0; rowIdx < dtblHdr->numRows; ++rowIdx)
    {
        for (uint32_t colIdx = 0; colIdx < dtblHdr->numColumns; ++colIdx)
        {
            const datacolumn_t& col = tmp.pDataColums[colIdx];
            const size_t valueOffset = (dtblHdr->rowStride * rowIdx) + col.rowOffset;

            void* const valueBufBase = &dataChunk.data[podValueBase + valueOffset];

            // get rmem instance for this cell's value buffer
            rmem valbuf(valueBufBase);

            switch (col.type)
            {
            case dtblcoltype_t::Bool:
            {
                const std::string val = DataTable_ParseCellFromDocument<std::string>(doc, colIdx, rowIdx);

                if (!_stricmp(val.c_str(), "true") || val == "1")
                    valbuf.write<uint32_t>(true);
                else if (!_stricmp(val.c_str(), "false") || val == "0")
                    valbuf.write<uint32_t>(false);
                else
                    DataTable_ReportInvalidValueError(col.type, rowIdx, colIdx);

                break;
            }
            case dtblcoltype_t::Int:
            {
                const uint32_t val = DataTable_ParseCellFromDocument<uint32_t>(doc, colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case dtblcoltype_t::Float:
            {
                const float val = DataTable_ParseCellFromDocument<float>(doc, colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case dtblcoltype_t::Vector:
            {
                std::string val = DataTable_ParseCellFromDocument<std::string>(doc, colIdx, rowIdx);
                std::smatch sm;

                // get values from format "<x,y,z>"
                std::regex_search(val, sm, std::regex("<(.*),(.*),(.*)>"));

                // 0 - all
                // 1 - x
                // 2 - y
                // 3 - z
                if (sm.size() == 4)
                {
                    const Vector3 vec(
                        static_cast<float>(atof(sm[1].str().c_str())),
                        static_cast<float>(atof(sm[2].str().c_str())),
                        static_cast<float>(atof(sm[3].str().c_str())));

                    valbuf.write(vec);
                }
                else
                    DataTable_ReportInvalidValueError(col.type, rowIdx, colIdx);
                break;
            }
            case dtblcoltype_t::String:
            case dtblcoltype_t::Asset:
            case dtblcoltype_t::AssetNoPrecache:
            {
                const std::string val = DataTable_ParseCellFromDocument<std::string>(doc, colIdx, rowIdx);
                const size_t valBufLen = val.length() + 1;

                memcpy(pStringBuf, val.c_str(), valBufLen);

                valbuf.write(dataChunk.GetPointer(stringValueBase + (pStringBuf - pStringBufBase)));

                pak->AddPointer(dataChunk, podValueBase + valueOffset);

                pStringBuf += valBufLen;
                break;
            }
            }
        }
    }
}

// page chunk structure and order:
// - header        HEAD        (align=8)
// - data          CPU         (align=8) data columns, column names, pod row values then string row values. only data columns is aligned to 8, the rest is 1.
template <typename datatable_t>
static void DataTable_AddDataTable(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);

    const std::string datatableFile = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".csv");
    std::ifstream datatableStream(datatableFile);

    if (!datatableStream.is_open())
        Error("Failed to open datatable asset \"%s\".\n", datatableFile.c_str());

    rapidcsv::Document doc(datatableStream);
    const size_t columnCount = doc.GetColumnCount();

    if (columnCount == 0)
    {
        Error("Attempted to add datatable with no columns.\n");
        return;
    }

    const size_t rowCount = doc.GetRowCount();

    if (rowCount < 2)
    {
        Error("Attempted to add datatable with invalid row count %zu.\nDTBL    - CSV must have a row of column types at the end of the table.\n", rowCount);
        return;
    }

    const uint32_t pakVersion = pak->GetVersion();

    PakPageLump_s hdrChunk;
    if (pakVersion <= 7)
        hdrChunk = pak->CreatePageLump(sizeof(datatable_v0_t), SF_HEAD, 8);
    else
        hdrChunk = pak->CreatePageLump(sizeof(datatable_v1_t), SF_HEAD, 8);

    datatable_t* const dtblHdr = reinterpret_cast<datatable_t*>(hdrChunk.data);
    datatable_asset_t dtblAsset{}; // temp header that we store values in.

    dtblHdr->numColumns = static_cast<uint32_t>(doc.GetColumnCount());
    dtblHdr->numRows = static_cast<uint32_t>(doc.GetRowCount() - 1); // -1 because last row isn't added (used for type info)

    std::vector<std::string> typeRow;
    DataTable_SetupRows(doc, dtblHdr, dtblAsset, typeRow);

    const size_t dataColumnsBufSize = dtblHdr->numColumns * sizeof(datacolumn_t);
    const size_t columnNamesBufSize = DataTable_CalcColumnNameBufSize(doc);

    const size_t totalChunkSize = dataColumnsBufSize + columnNamesBufSize + dtblAsset.rowPodValueBufSize + dtblAsset.rowStringValueBufSize;

    // create whole datatable chunk
    PakPageLump_s dataChunk = pak->CreatePageLump(totalChunkSize, SF_CPU, 8);

    // colums from data chunk
    dtblAsset.pDataColums = reinterpret_cast<datacolumn_t*>(dataChunk.data);

    // datatable v0 and v1 use the same struct offset for pColumns.
    pak->AddPointer(hdrChunk, offsetof(datatable_v1_t, pColumns), dataChunk, 0);

    // setup data in column data chunk
    DataTable_SetupColumns(pak, dataChunk, dataColumnsBufSize, dtblHdr, dtblAsset, doc, typeRow);

    // Plain-old-data and string values use different buffers!
    const size_t rowPodValuesBase = dataColumnsBufSize + columnNamesBufSize;
    const size_t rowStringValuesBase = rowPodValuesBase + dtblAsset.rowPodValueBufSize;

    // setup row data chunks
    DataTable_SetupValues(pak, dataChunk, rowPodValuesBase, rowStringValuesBase, dtblHdr, dtblAsset, doc);

    // datatable v0 and v1 use the same struct offset for pRows.
    pak->AddPointer(hdrChunk, offsetof(datatable_v0_t, pRows), dataChunk, rowPodValuesBase);

    asset.InitAsset(
        hdrChunk.GetPointer(), pakVersion <= 7 ? sizeof(datatable_v0_t) : sizeof(datatable_v1_t),
        dataChunk.GetPointer(rowPodValuesBase), // points to datatable_asset_t::pRow
        -1, -1, 
        // rpak v7: v0
        // rpak v8: v1
        pakVersion <= 7 ? 0 : 1,
        AssetType::DTBL);

    asset.SetHeaderPointer(hdrChunk.data);

    pak->FinishAsset();
}

void Assets::AddDataTableAsset(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    if (pak->GetVersion() <= 7)
        DataTable_AddDataTable<datatable_v0_t>(pak, assetGuid, assetPath, mapEntry);
    else
        DataTable_AddDataTable<datatable_v1_t>(pak, assetGuid, assetPath, mapEntry);
}