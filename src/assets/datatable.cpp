#include "pch.h"
#include "assets.h"
#include "public/datatable.h"

// fills a CPakDataChunk with column data from a provided csv
static void DataTable_SetupColumns(CPakFile* const pak, CPakDataChunk& colChunk, datatable_asset_t* const pHdrTemp, rapidcsv::Document& doc)
{
    size_t colNameBufSize = 0;
    // get required size to store all of the column names in a single buffer
    for (const std::string& it : doc.GetColumnNames())
    {
        colNameBufSize += it.length() + 1;
    }

    // column names
    CPakDataChunk colNameChunk = pak->CreateDataChunk(colNameBufSize, SF_CPU, 8);

    // get a copy of the pointer because then we can shift it for each name
    char* colNameBuf = colNameChunk.Data();

    // vectors
    std::vector<std::string> typeRow = doc.GetRow<std::string>(pHdrTemp->numRows);
    const uint32_t numTypeNames = static_cast<uint32_t>(typeRow.size());

    // typically happens when there's an empty line in the csv file.
    if (numTypeNames != pHdrTemp->numColumns)
        Error("Expected %u columns for type name row, found %u.\n", pHdrTemp->numRows, numTypeNames);

    for (uint32_t i = 0; i < pHdrTemp->numColumns; ++i)
    {
        const std::string name = doc.GetColumnName(i);

        // copy the column name into the namebuf
        snprintf(colNameBuf, name.length() + 1, "%s", name.c_str());

        const dtblcoltype_t type = DataTable_GetTypeFromString(typeRow[i]);
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

        pHdrTemp->rowStride += DataTable_GetValueSize(type);
        pHdrTemp->rowDataPageSize += static_cast<size_t>(DataTable_GetValueSize(type)) * pHdrTemp->numRows; // size of type * row count (excluding the type row)

        colNameBuf += name.length() + 1;
    }
}

// fills a CPakDataChunk with row data from a provided csv
static void DataTable_SetupRows(CPakFile* const pak, CPakDataChunk& rowDataChunk, CPakDataChunk& stringChunk, datatable_asset_t* const pHdrTemp, rapidcsv::Document& doc)
{
    char* pStringBuf = stringChunk.Data();

    for (uint32_t rowIdx = 0; rowIdx < pHdrTemp->numRows; ++rowIdx)
    {
        for (uint32_t colIdx = 0; colIdx < pHdrTemp->numColumns; ++colIdx)
        {
            const datacolumn_t& col = pHdrTemp->pDataColums[colIdx];

            // get rmem instance for this cell's value buffer
            rmem valbuf(rowDataChunk.Data() + (pHdrTemp->rowStride * rowIdx) + col.rowOffset);

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
                    Error("Invalid bool value at cell (%u, %u).\n", colIdx, rowIdx);

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
                break;
            }
            case dtblcoltype_t::String:
            case dtblcoltype_t::Asset:
            case dtblcoltype_t::AssetNoPrecache:
            {
                const std::string val = doc.GetCell<std::string>(colIdx, rowIdx);
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

// VERSION 8
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
        hdrChunk = pak->CreateDataChunk(sizeof(datatable_v0_t), SF_HEAD, 16);
    else
        hdrChunk = pak->CreateDataChunk(sizeof(datatable_v1_t), SF_HEAD, 16);

    // todo: allocate on stack?
    datatable_asset_t* pHdr = new datatable_asset_t{}; // temp header that we store values in, this is for sharing funcs across versions

    pHdr->numColumns = static_cast<uint32_t>(doc.GetColumnCount());
    pHdr->numRows = static_cast<uint32_t>(doc.GetRowCount()-1); // -1 because last row isnt added (used for type info)
    pHdr->assetPath = assetPath;

    // create column chunk
    CPakDataChunk colChunk = pak->CreateDataChunk(sizeof(datacolumn_t) * pHdr->numColumns, SF_CPU, 8);
    
    // colums from data chunk
    pHdr->pDataColums = reinterpret_cast<datacolumn_t*>(colChunk.Data());

    // setup data in column data chunk
    DataTable_SetupColumns(pak, colChunk, pHdr, doc);

    // setup column page ptr
    pHdr->pColumns = colChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(datatable_asset_t, pColumns)));

    // page for Row Data
    CPakDataChunk rowDataChunk = pak->CreateDataChunk(pHdr->rowDataPageSize, SF_CPU, 8);

    // page for string entries
    CPakDataChunk stringChunk = pak->CreateDataChunk(pHdr->stringEntriesSize, SF_CPU, 8);

    // setup row data chunks
    DataTable_SetupRows(pak, rowDataChunk, stringChunk, pHdr, doc);

    // setup row page ptr
    pHdr->pRows = rowDataChunk.GetPointer();

    pak->AddPointer(hdrChunk.GetPointer(offsetof(datatable_asset_t, pRows)));

    pHdr->WriteToBuffer(hdrChunk.Data(), pak->GetVersion());

    PakAsset_t asset;

    asset.InitAsset(
        assetPath,
        assetGuid,
        hdrChunk.GetPointer(), hdrChunk.GetSize(),
        rowDataChunk.GetPointer(),
        UINT64_MAX, UINT64_MAX, AssetType::DTBL);

    asset.SetHeaderPointer(hdrChunk.Data());

    // rpak v7: v0
    // rpak v8: v1
    asset.version = pak->GetVersion() <= 7 ? 0 : 1;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 1; // asset only depends on itself

    pak->PushAsset(asset);

    delete pHdr;
}