#include "pch.h"
#include "assets.h"
#include "public/settings_layout.h"

uint32_t SettingsLayout_GetFieldSizeForType(const SettingsFieldType_e type)
{
    switch (type)
    {
    case SettingsFieldType_e::ST_Bool:
        return sizeof(bool);
    case SettingsFieldType_e::ST_Int:
        return sizeof(int);
    case SettingsFieldType_e::ST_Float:
        return sizeof(float);
    case SettingsFieldType_e::ST_Float2:
        return sizeof(float) * 2;
    case SettingsFieldType_e::ST_Float3:
        return sizeof(float) * 3;
    case SettingsFieldType_e::ST_String:
    case SettingsFieldType_e::ST_Asset:
    case SettingsFieldType_e::ST_Asset_2:
    case SettingsFieldType_e::ST_StaticArray:
    case SettingsFieldType_e::ST_DynamicArray:
        return sizeof(PagePtr_t);

    default: return 0;
    }
}

SettingsFieldType_e SettingsLayout_GetFieldTypeForString(const char* const typeName)
{
    for (unsigned short i = 0; i < ARRAYSIZE(s_settingsFieldTypeNames); i++)
    {
        if (strcmp(typeName, s_settingsFieldTypeNames[i]) == 0)
            return (SettingsFieldType_e)i;
    }

    return SettingsFieldType_e::ST_Invalid;
}

void SettingsLayout_ParseLayout(CPakFileBuilder* const pak, const char* const assetPath, rapidcsv::Document& document, SettingsLayoutParseResult_s& result)
{
    const std::string settingsLayoutFile = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".csv");
    std::ifstream datatableStream(settingsLayoutFile);

    if (!datatableStream.is_open())
        Error("Failed to open settings layout \"%s\".\n", settingsLayoutFile.c_str());

    document.ReadCsv(datatableStream);

    result.fieldNames = document.GetColumn<std::string>(0);
    result.typeNames = document.GetColumn<std::string>(1);
    result.offsetMap = document.GetColumn<uint32_t>(2);

    const size_t numFieldNames = result.fieldNames.size();
    const size_t numTypeNames = result.typeNames.size();
    const size_t numOffsets = result.offsetMap.size();

    if (numFieldNames != numTypeNames)
        Error("Settings layout column count mismatch (%zu != %zu || %zu != %zu).\n", numFieldNames, numTypeNames, numTypeNames, numOffsets);

    result.typeMap.resize(numTypeNames);

    for(size_t i = 0; i < numTypeNames; i++)
    {
        const std::string& typeName = result.typeNames[i];
        const SettingsFieldType_e typeToUse = SettingsLayout_GetFieldTypeForString(typeName.c_str());

        if (typeToUse == SettingsFieldType_e::ST_Invalid)
        {
            const std::string& fieldName = result.fieldNames[i];
            Error("Settings layout field \"%s\" uses unknown type \"%s\".\n", fieldName.c_str(), typeName.c_str());
        }

        result.typeMap[i] = typeToUse;
    }

    // Get the total layout value buffer size, and make sure we don't have any
    // overlapping fields.
    int nextOffset = -1;

    for (size_t i = 0; i < numOffsets; i++)
    {
        const int curOffset = result.offsetMap[i];

        if (curOffset < nextOffset)
        {
            const std::string& fieldName = result.fieldNames[i];
            Error("Settings layout field \"%s\" has an offset that overlaps (%u < %u).\n", fieldName.c_str(), curOffset, nextOffset);
        }

        nextOffset = curOffset + SettingsLayout_GetFieldSizeForType(result.typeMap[i]);
    }

    // The last offset + its type size is the total value buffer size.
    result.totalValueBufferSize = nextOffset;
}
