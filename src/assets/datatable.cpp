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

static void DataTable_SetupRows(const rapidcsv::Document& doc, datatable_asset_t* const pHdrTemp, std::vector<std::string>& outTypeRow)
{
    // cache it so we don't have to make another deep copy.
    outTypeRow = doc.GetRow<std::string>(pHdrTemp->numRows);
    const uint32_t numTypeNames = static_cast<uint32_t>(outTypeRow.size());

    // typically happens when there's an empty line in the csv file.
    if (numTypeNames != pHdrTemp->numColumns)
        Error("Expected %u columns for type name row, found %u.\n", pHdrTemp->numRows, numTypeNames);

    for (uint32_t i = 0; i < pHdrTemp->numColumns; ++i)
    {
        const dtblcoltype_t type = DataTable_GetTypeFromString(outTypeRow[i]);

        if (DataTable_IsStringType(type))
        {
            for (uint32_t j = 0; j < pHdrTemp->numRows; ++j)
            {
                // this can be std::string since we only deal with the string types here
                std::vector<std::string> row = doc.GetRow<std::string>(j);
                pHdrTemp->rowStringValueBufSize += row[i].length() + 1;
            }
        }

        pHdrTemp->rowPodValueBufSize += static_cast<size_t>(DataTable_GetValueSize(type)) * pHdrTemp->numRows; // size of type * row count (excluding the type row)
    }
}

// fills a CPakDataChunk with column data from a provided csv
static void DataTable_SetupColumns(CPakFile* const pak, CPakDataChunk& dataChunk, const size_t columnNameBase, datatable_asset_t* const pHdrTemp,
    const rapidcsv::Document& doc, const std::vector<std::string>& typeRow)
{
    char* const colNameBufBase = &dataChunk.Data()[columnNameBase];
    char* colNameBuf = colNameBufBase;

    for (uint32_t i = 0; i < pHdrTemp->numColumns; ++i)
    {
        const std::string name = doc.GetColumnName(i);
        const size_t nameBufLen = name.length() + 1;

        // copy the column name into the namebuf
        memcpy(colNameBuf, name.c_str(), nameBufLen);

        datacolumn_t& col = pHdrTemp->pDataColums[i];
        col.pName = dataChunk.GetPointer(columnNameBase + (colNameBuf - colNameBufBase));

        // register name pointer
        pak->AddPointer(dataChunk.GetPointer(((sizeof(datacolumn_t) * i) + offsetof(datacolumn_t, pName))));
        colNameBuf += nameBufLen;

        const dtblcoltype_t type = DataTable_GetTypeFromString(typeRow[i]);

        col.rowOffset = pHdrTemp->rowStride;
        col.type = type;

        pHdrTemp->rowStride += DataTable_GetValueSize(type);
    }
}

static void DataTable_ReportInvalidValueError(const dtblcoltype_t type, const uint32_t colIdx, const uint32_t rowIdx)
{
    Error("Invalid %s value at cell (%u, %u).\n", DataTable_GetStringFromType(type), colIdx, rowIdx);
}

// fills a CPakDataChunk with row data from a provided csv
static void DataTable_SetupValues(CPakFile* const pak, CPakDataChunk& dataChunk, const size_t podValueBase, const size_t stringValueBase, datatable_asset_t* const pHdrTemp, rapidcsv::Document& doc)
{
    char* const pStringBufBase = &dataChunk.Data()[stringValueBase];
    char* pStringBuf = pStringBufBase;

    for (uint32_t rowIdx = 0; rowIdx < pHdrTemp->numRows; ++rowIdx)
    {
        for (uint32_t colIdx = 0; colIdx < pHdrTemp->numColumns; ++colIdx)
        {
            const datacolumn_t& col = pHdrTemp->pDataColums[colIdx];
            const size_t valueOffset = (pHdrTemp->rowStride * rowIdx) + col.rowOffset;

            void* const valueBufBase = &dataChunk.Data()[podValueBase + valueOffset];

            // get rmem instance for this cell's value buffer
            rmem valbuf(valueBufBase);

            switch (col.type)
            {
            case dtblcoltype_t::Bool:
            {
                const std::string val = doc.GetCell<std::string>(colIdx, rowIdx);

                if (!_stricmp(val.c_str(), "true") || val == "1")
                    valbuf.write<uint32_t>(true);
                else if (!_stricmp(val.c_str(), "false") || val == "0")
                    valbuf.write<uint32_t>(false);
                else
                    DataTable_ReportInvalidValueError(col.type, colIdx, rowIdx);

                break;
            }
            case dtblcoltype_t::Int:
            {
                const uint32_t val = doc.GetCell<uint32_t>(colIdx, rowIdx);
                valbuf.write(val);
                break;
            }
            case dtblcoltype_t::Float:
            {
                const float val = doc.GetCell<float>(colIdx, rowIdx);
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
                    const Vector3 vec(
                        static_cast<float>(atof(sm[1].str().c_str())),
                        static_cast<float>(atof(sm[2].str().c_str())),
                        static_cast<float>(atof(sm[3].str().c_str())));

                    valbuf.write(vec);
                }
                else
                    DataTable_ReportInvalidValueError(col.type, colIdx, rowIdx);
                break;
            }
            case dtblcoltype_t::String:
            case dtblcoltype_t::Asset:
            case dtblcoltype_t::AssetNoPrecache:
            {
                const std::string val = doc.GetCell<std::string>(colIdx, rowIdx);
                const size_t valBufLen = val.length() + 1;

                memcpy(pStringBuf, val.c_str(), valBufLen);

                valbuf.write(dataChunk.GetPointer(stringValueBase + (pStringBuf - pStringBufBase)));
                pak->AddPointer(dataChunk.GetPointer(podValueBase + valueOffset));

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
void Assets::AddDataTableAsset(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
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

    CPakDataChunk hdrChunk;
    if (pak->GetVersion() <= 7)
        hdrChunk = pak->CreateDataChunk(sizeof(datatable_v0_t), SF_HEAD, 8);
    else
        hdrChunk = pak->CreateDataChunk(sizeof(datatable_v1_t), SF_HEAD, 8);

    datatable_asset_t dtblHdr{}; // temp header that we store values in, this is for sharing funcs across versions

    dtblHdr.numColumns = static_cast<uint32_t>(doc.GetColumnCount());
    dtblHdr.numRows = static_cast<uint32_t>(doc.GetRowCount()-1); // -1 because last row isn't added (used for type info)
    dtblHdr.assetPath = assetPath;

    std::vector<std::string> typeRow;
    DataTable_SetupRows(doc, &dtblHdr, typeRow);

    const size_t dataColumnsBufSize = dtblHdr.numColumns * sizeof(datacolumn_t);
    const size_t columnNamesBufSize = DataTable_CalcColumnNameBufSize(doc);

    const size_t totalChunkSize = dataColumnsBufSize + columnNamesBufSize + dtblHdr.rowPodValueBufSize + dtblHdr.rowStringValueBufSize;

    // create whole datatable chunk
    CPakDataChunk dataChunk = pak->CreateDataChunk(totalChunkSize, SF_CPU, 8);
    
    // colums from data chunk
    dtblHdr.pColumns = dataChunk.GetPointer();
    dtblHdr.pDataColums = reinterpret_cast<datacolumn_t*>(dataChunk.Data());

    pak->AddPointer(hdrChunk.GetPointer(offsetof(datatable_asset_t, pColumns)));

    // setup data in column data chunk
    DataTable_SetupColumns(pak, dataChunk, dataColumnsBufSize, &dtblHdr, doc, typeRow);

    // Plain-old-data and string values use different buffers!
    const size_t rowPodValuesBase = dataColumnsBufSize + columnNamesBufSize;
    const size_t rowStringValuesBase = rowPodValuesBase + dtblHdr.rowPodValueBufSize;

    // setup row data chunks
    DataTable_SetupValues(pak, dataChunk, rowPodValuesBase, rowStringValuesBase, &dtblHdr, doc);

    // setup row page ptr
    dtblHdr.pRows = dataChunk.GetPointer(rowPodValuesBase);
    pak->AddPointer(hdrChunk.GetPointer(offsetof(datatable_asset_t, pRows)));

    dtblHdr.WriteToBuffer(hdrChunk.Data(), pak->GetVersion());

    PakAsset_t asset;

    asset.InitAsset(
        assetPath,
        assetGuid,
        hdrChunk.GetPointer(), hdrChunk.GetSize(),
        dtblHdr.pRows,
        UINT64_MAX, UINT64_MAX, AssetType::DTBL);

    asset.SetHeaderPointer(hdrChunk.Data());

    // rpak v7: v0
    // rpak v8: v1
    asset.version = pak->GetVersion() <= 7 ? 0 : 1;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 1; // asset only depends on itself

    pak->PushAsset(asset);
}