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
	AssetNoPrecache,

	INVALID = 0xffffffff
};

static inline const std::unordered_map<std::string, dtblcoltype_t> s_dataTableColumnTypeMap =
{
	{ "bool",   dtblcoltype_t::Bool },
	{ "int",    dtblcoltype_t::Int },
	{ "float",  dtblcoltype_t::Float },
	{ "vector", dtblcoltype_t::Vector },
	{ "string", dtblcoltype_t::String },
	{ "asset",  dtblcoltype_t::Asset },
	{ "asset_noprecache", dtblcoltype_t::AssetNoPrecache }
};

// gets enum value from type string
// e.g. "string" to dtblcoltype::StringT

dtblcoltype_t DataTable_GetTypeFromString(const std::string& sType)
{
	for (const auto& [key, value] : s_dataTableColumnTypeMap) // get each element in the type map
	{
		if (_stricmp(sType.c_str(), key.c_str()) == 0) // are they equal?
			return value;
	}

	return dtblcoltype_t::INVALID;
}

inline const char* DataTable_GetStringFromType(const dtblcoltype_t type)
{
	switch (type)
	{
	case dtblcoltype_t::Bool: return "bool";
	case dtblcoltype_t::Int: return "int";
	case dtblcoltype_t::Float: return "float";
	case dtblcoltype_t::Vector: return "vector";
	case dtblcoltype_t::String: return "string";
	case dtblcoltype_t::Asset: return "asset";
	case dtblcoltype_t::AssetNoPrecache: return "asset_noprecache";
	default: return "type_invalid";
	}
}

inline bool DataTable_IsStringType(const dtblcoltype_t type)
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

inline uint8_t DataTable_GetValueSize(const dtblcoltype_t type)
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
		return sizeof(PagePtr_t);
	default:
		return 0;
	}
}

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
	// previously func vars
	size_t rowPodValueBufSize;
	size_t rowStringValueBufSize;

	datacolumn_t* pDataColums; // pointer to column data from data chunk
};

static_assert(sizeof(datacolumn_t) == 16);
static_assert(sizeof(datatable_v0_t) == 32);
static_assert(sizeof(datatable_v1_t) == 40);