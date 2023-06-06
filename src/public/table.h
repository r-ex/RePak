#pragma once

// identifies the data type for each column in a datatable asset
enum class dtblcoltype_t : uint32_t
{
	Bool,
	Int,
	Float,
	Vector,
	String,
	Asset,
	AssetNoPrecache
};

struct datacolumn_t
{
	PagePtr_t pName; // column name/heading
	dtblcoltype_t type; // column value data type
	uint32_t rowOffset; // offset in row data to this column's value
};

struct datatable_v0_t
{
	uint32_t numColumns;
	uint32_t numRows;

	PagePtr_t pColumns;
	PagePtr_t pRows;

	uint32_t rowStride;	// Number of bytes per row
	uint32_t pad; // alignment
};

struct datatable_v1_t
{
	uint32_t numColumns;
	uint32_t numRows;

	PagePtr_t pColumns;
	PagePtr_t pRows;

	uint32_t unk1c;

	uint16_t unk20 = 0;
	uint16_t unk22 = 0;

	uint32_t rowStride;	// Number of bytes per row
	uint32_t pad;
};

struct datatable_asset_t
{
	uint32_t numColumns;
	uint32_t numRows;

	// !!!! DO NOT CHANGE THE POSITION OF THESE !!!!
	// !!!!			IT WILL CAUSE ISSSUES		!!!!
	// we are lucky and can cheat!!
	PagePtr_t pColumns;
	PagePtr_t pRows;

	uint32_t rowStride;	// Number of bytes per row

	const char* assetPath; // assets path on disk

	// previously func vars
	size_t stringEntriesSize;
	size_t rowDataPageSize;

	datacolumn_t* pDataColums; // pointer to column data from data chunk

	void SetDTBL_V0(datatable_v0_t* pDTBL)
	{
		pDTBL->numColumns = numColumns;
		pDTBL->numRows = numRows;
		pDTBL->pColumns = pColumns;
		pDTBL->pRows = pRows;
		pDTBL->rowStride = rowStride;
	}

	void WriteToBuffer(char* buf, int pakVersion)
	{
		if (pakVersion <= 7)
		{
			datatable_v0_t* dtbl = reinterpret_cast<datatable_v0_t*>(buf);
			dtbl->numColumns = numColumns;
			dtbl->numRows = numRows;
			dtbl->pColumns = pColumns;
			dtbl->pRows = pRows;
			dtbl->rowStride = rowStride;
		}
		else
		{
			datatable_v1_t* dtbl = reinterpret_cast<datatable_v1_t*>(buf);
			dtbl->numColumns = numColumns;
			dtbl->numRows = numRows;
			dtbl->pColumns = pColumns;
			dtbl->pRows = pRows;
			dtbl->rowStride = rowStride;
		}
	}

	void SetDTBL_V1(datatable_v1_t* pDTBL)
	{
		pDTBL->numColumns = numColumns;
		pDTBL->numRows = numRows;
		pDTBL->pColumns = pColumns;
		pDTBL->pRows = pRows;
		pDTBL->rowStride = rowStride;
	}
};

static_assert(sizeof(datacolumn_t) == 16);
static_assert(sizeof(datatable_v0_t) == 32);
static_assert(sizeof(datatable_v1_t) == 40);