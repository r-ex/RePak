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

struct datatable_t
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

static_assert(sizeof(datacolumn_t) == 16);
static_assert(sizeof(datatable_t) == 40);