#pragma once

#include <pch.h>

#pragma pack(push, 1)
// identifies the data type for each column in a datatable asset
enum class dtblcoltype_t : uint32_t
{
	Bool,
	Int,
	Float,
	Vector,
	StringT,
	Asset,
	AssetNoPrecache
};

struct DataTableHeader
{
	uint32_t ColumnCount;
	uint32_t RowCount;

	RPakPtr ColumnHeaderPtr;
	RPakPtr RowHeaderPtr;
	uint32_t UnkHash;

	uint16_t Un1 = 0;
	uint16_t Un2 = 0;

	uint32_t RowStride;	// Number of bytes per row
	uint32_t Padding;
};

struct DataTableColumn
{
	RPakPtr NamePtr;
	dtblcoltype_t Type;
	uint32_t RowOffset;
};

struct DataTableColumnData
{
	dtblcoltype_t Type;
	bool bValue = 0;
	int iValue = -1;
	float fValue = -1;
	Vector3 vValue;
	std::string stringValue;
	std::string assetValue;
	std::string assetNPValue;
};
#pragma pack(pop)