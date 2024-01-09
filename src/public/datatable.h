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

static char char_tolower(const char ch) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}
dtblcoltype_t DataTable_GetTypeFromString(std::string& sType)
{
	std::transform(sType.begin(), sType.end(), sType.begin(), char_tolower);

	for (const auto& [key, value] : s_dataTableColumnTypeMap) // get each element in the type map
	{
		if (sType.compare(key) == 0) // are they equal?
			return value;
	}

	return dtblcoltype_t::String;
}

inline bool DataTable_IsStringType(dtblcoltype_t type)
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

inline uint8_t DataTable_GetValueSize(dtblcoltype_t type)
{
	switch (type)
	{
	case dtblcoltype_t::Bool:
	case dtblcoltype_t::Int:
	case dtblcoltype_t::Float:
		return sizeof(int32_t);
	case dtblcoltype_t::Vector:
		return 3 * sizeof(float); // sizeof(Vector3)
	case dtblcoltype_t::String:
	case dtblcoltype_t::Asset:
	case dtblcoltype_t::AssetNoPrecache:
		return 8; // sizeof(PagePtr_t)
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
};

static_assert(sizeof(datacolumn_t) == 16);
static_assert(sizeof(datatable_v0_t) == 32);
static_assert(sizeof(datatable_v1_t) == 40);