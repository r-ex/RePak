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

uint32_t SettingsLayout_GetFieldAlignmentForType(const SettingsFieldType_e type)
{
    switch (type)
    {
    case SettingsFieldType_e::ST_Bool:
        return 1;
    case SettingsFieldType_e::ST_Int:
    case SettingsFieldType_e::ST_Float:
    case SettingsFieldType_e::ST_Float2:
    case SettingsFieldType_e::ST_Float3:
        return 4;
    case SettingsFieldType_e::ST_String:
    case SettingsFieldType_e::ST_Asset:
    case SettingsFieldType_e::ST_Asset_2:
    case SettingsFieldType_e::ST_StaticArray:
    case SettingsFieldType_e::ST_DynamicArray:
        return 8;

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

void SettingsLayout_ParseLayout(CPakFileBuilder* const pak, const char* const assetPath, rapidcsv::Document& document, SettingsLayoutParseResult_s& result, const bool isRoot)
{
    const std::string settingsLayoutFile = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".csv");
    std::ifstream datatableStream(settingsLayoutFile);

    if (!datatableStream.is_open())
        Error("Failed to open settings layout table \"%s\".\n", settingsLayoutFile.c_str());

    document.ReadCsv(datatableStream);

    result.fieldNames = document.GetColumn<std::string>(0);
    result.typeNames = document.GetColumn<std::string>(1);
    result.offsetMap = document.GetColumn<uint32_t>(2);

    // Nested arrays aren't supported as preinitialized data in sub layouts.
    if (isRoot)
        result.indexMap = document.GetColumn<uint32_t>(3);

    const size_t numFieldNames = result.fieldNames.size();
    const size_t numTypeNames = result.typeNames.size();
    const size_t numOffsets = result.offsetMap.size();
    const size_t numIndices = result.indexMap.size();

    if (isRoot)
    {
        if (numFieldNames != numTypeNames || numTypeNames != numOffsets || numOffsets != numIndices)
        {
            Error("Root settings layout column count mismatch (%zu != %zu || %zu != %zu || %zu != %zu).\n",
                numFieldNames, numTypeNames, numTypeNames, numOffsets, numOffsets, numIndices);
        }
    }
    else
    {
        if (numFieldNames != numTypeNames || numTypeNames != numOffsets)
        {
            Error("Sub settings layout column count mismatch (%zu != %zu || %zu != %zu).\n",
                numFieldNames, numTypeNames, numTypeNames, numOffsets);
        }
    }

    result.typeMap.resize(numTypeNames);
    uint32_t lastFieldAlign = 0;

    for (size_t i = 0; i < numTypeNames; i++)
    {
        const std::string& typeName = result.typeNames[i];
        const SettingsFieldType_e typeToUse = SettingsLayout_GetFieldTypeForString(typeName.c_str());

        if (typeToUse == SettingsFieldType_e::ST_Invalid)
        {
            const std::string& fieldName = result.fieldNames[i];
            Error("Settings layout field \"%s\" uses unknown type \"%s\".\n", fieldName.c_str(), typeName.c_str());
        }

        const uint32_t curTypeAlign = SettingsLayout_GetFieldAlignmentForType(typeToUse);

        // All fields in the settings layout must be sorted by their alignments.
        // Fields with higher alignments must come first as to avoid padding which
        // the original assets do not support, so we follow the same scheme here.
        if (i > 0 && curTypeAlign > lastFieldAlign)
        {
            Error("Settings layout field \"%s\" is of type %s which has an alignment of %u, but the previous field was aligned to %u; padding is not allowed.\n",
                result.fieldNames[i].c_str(), s_settingsFieldTypeNames[typeToUse], curTypeAlign, lastFieldAlign);
        }

        lastFieldAlign = curTypeAlign;
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

    result.hashTableSize = static_cast<uint32_t>(NextPowerOfTwo(numFieldNames+1));

    // The last offset + its type size is the total value buffer size.
    result.usedValueBufferSize = nextOffset;
}

static void SettingsLayout_ValidateBufferUsage(const SettingsLayoutParseResult_s& parseResult)
{
    if (parseResult.usedValueBufferSize > parseResult.totalValueBufferSize)
    {
        Error("Parsed settings layout's value buffer usage is larger than total room available (%u > %u).\n",
            parseResult.usedValueBufferSize, parseResult.totalValueBufferSize);
    }
}

void SettingsLayout_ParseMap(CPakFileBuilder* const pak, const char* const assetPath, SettingsLayoutAsset_s& asset)
{
    const std::string settingsLayoutFile = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".json");
    rapidjson::Document document;

    if (!JSON_ParseFromFile(settingsLayoutFile.c_str(), "settings layout", document))
        Error("Failed to open settings layout \"%s\".\n", settingsLayoutFile.c_str());

    const uint32_t rootLayoutSize = JSON_GetValueRequired<uint32_t>(document, "size");

    // Parse the root layout and figure out what the highest sub layout index is.
    rapidcsv::Document rootLayoutTable;
    SettingsLayoutParseResult_s& rootParseResult = asset.rootLayout;

    SettingsLayout_ParseLayout(pak, assetPath, rootLayoutTable, rootParseResult, true);
    rootParseResult.totalValueBufferSize = rootLayoutSize;

    SettingsLayout_ValidateBufferUsage(rootParseResult);

    if (rootParseResult.highestSubLayoutIndex > 0)
    {
        rapidjson::Document::ConstMemberIterator subLayoutsIt;
        JSON_GetRequired(document, "subLayouts", JSONFieldType_e::kArray, subLayoutsIt);

        const rapidjson::Value::ConstArray& subLayoutArray = subLayoutsIt->value.GetArray();
        const uint32_t numSubLayouts = static_cast<uint32_t>(subLayoutArray.Size());

        if (rootParseResult.highestSubLayoutIndex+1 != numSubLayouts)
            Error("Root settings layout requested %u sub layouts, got %u.\n", rootParseResult.highestSubLayoutIndex+1, numSubLayouts);

        for (uint32_t i = 0; i < numSubLayouts; i++)
        {
            const rapidjson::Value& obj = subLayoutArray[i];
            const char* const subPath = JSON_GetValueRequired<const char*>(obj, "path");
            const uint32_t subLayoutSize = JSON_GetValueRequired<uint32_t>(obj, "size");

            rapidcsv::Document subLayoutTable;
            SettingsLayoutParseResult_s& subParseResult = asset.subLayouts.emplace_back();

            SettingsLayout_ParseLayout(pak, subPath, subLayoutTable, subParseResult, false);
            subParseResult.totalValueBufferSize = subLayoutSize;

            SettingsLayout_ValidateBufferUsage(subParseResult);
        }
    }
}

static void SettingsLayout_InitializeHeader(SettingsLayoutHeader_s* const header, const SettingsLayoutParseResult_s& parse)
{
    header->hashTableSize = parse.hashTableSize;
    header->fieldCount = static_cast<uint32_t>(parse.fieldNames.size());
    header->extraDataSizeIndex = parse.extraDataSizeIndex;
    header->hashStepScale = parse.hashStepScale;
    header->hashSeed = parse.hashSeed;
}

static void SettingsLayout_CalculateBufferSizes(const SettingsLayoutAsset_s& layoutAsset, size_t& outFieldBufSize, size_t& outSubHeadersBufSize, size_t& outStringBufLen)
{
    const SettingsLayoutParseResult_s& rootParseResult = layoutAsset.rootLayout;

    // Note(amos): the root layout has its header in a separate HEAD page,
    // we therefore shouldn't count its header size in.
    for (const std::string& fieldName : rootParseResult.fieldNames)
    {
        outFieldBufSize += sizeof(SettingsField_s);
        outStringBufLen += fieldName.size() + 1;
    }

    for (const SettingsLayoutParseResult_s& subParseResult : layoutAsset.subLayouts)
    {
        outSubHeadersBufSize += sizeof(SettingsLayoutHeader_s);

        for (const std::string& fieldName : subParseResult.fieldNames)
        {
            outFieldBufSize += sizeof(SettingsField_s);
            outStringBufLen += fieldName.size() + 1;
        }
    }
}

static void SettingsLayout_WriteFieldData(PakPageLump_s& dataLump, SettingsLayoutParseResult_s& parse,
    size_t& curFieldBufIndex, size_t& curStringBufIndex, size_t& numStringBytesWritten)
{
    const size_t numFields = parse.fieldNames.size();
    const bool hasSubLayouts = parse.indexMap.size() > 0;

    for (size_t i = 0; i < numFields; i++)
    {
        SettingsField_s* const field = reinterpret_cast<SettingsField_s*>(&dataLump.data[curFieldBufIndex]);

        field->type = parse.typeMap[i];
        field->nameOffset = static_cast<uint16_t>(numStringBytesWritten);
        field->valueOffset = parse.offsetMap[i];
        field->subLayoutIndex = hasSubLayouts ? parse.indexMap[i] : 0;

        curFieldBufIndex += sizeof(SettingsField_s);

        const std::string& fieldName = parse.fieldNames[i];
        const size_t fieldNameLen = fieldName.size() + 1;

        memcpy(&dataLump.data[curStringBufIndex], fieldName.c_str(), fieldNameLen);

        curStringBufIndex += fieldNameLen;
        numStringBytesWritten += fieldNameLen;
    }
}

static void SettingsLayout_WriteData(CPakFileBuilder* const pak, PakPageLump_s& dataLump, SettingsLayoutParseResult_s& parse,
    size_t& curFieldBufIndex, size_t& curSubHeadersIndex, size_t& curStringBufIndex, size_t& numStringBytesWritten)
{
    const size_t numFields = parse.fieldNames.size();
    SettingsLayoutHeader_s* const header = reinterpret_cast<SettingsLayoutHeader_s*>(&dataLump.data[curSubHeadersIndex]);

    header->hashTableSize = parse.hashTableSize;
    header->fieldCount = static_cast<uint32_t>(numFields);
    header->extraDataSizeIndex = parse.extraDataSizeIndex;
    header->hashStepScale = parse.hashStepScale;
    header->hashSeed = parse.hashSeed;
    header->arrayElemCount = parse.arrayElemCount;
    header->layoutSize = parse.totalValueBufferSize;

    pak->AddPointer(dataLump, curSubHeadersIndex + offsetof(SettingsLayoutHeader_s, fieldMap), dataLump, curFieldBufIndex);
    pak->AddPointer(dataLump, curSubHeadersIndex + offsetof(SettingsLayoutHeader_s, fieldNames), dataLump, curStringBufIndex);

    curSubHeadersIndex += sizeof(SettingsLayoutHeader_s);

    SettingsLayout_WriteFieldData(dataLump, parse, curFieldBufIndex, curStringBufIndex, numStringBytesWritten);
}

static void SettingsLayout_InternalAddLayoutAsset(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
    SettingsLayoutAsset_s layoutAsset;
    SettingsLayout_ParseMap(pak, assetPath, layoutAsset);

    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
    PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(SettingsLayoutHeader_s), SF_HEAD, 8);

    SettingsLayoutHeader_s* const layoutHeader = reinterpret_cast<SettingsLayoutHeader_s*>(hdrLump.data);
    SettingsLayout_InitializeHeader(layoutHeader, layoutAsset.rootLayout);

    size_t outFieldBufSize = 0, outSubHeadersBufSize = 0, outStringBufLen = 0;
    SettingsLayout_CalculateBufferSizes(layoutAsset, outFieldBufSize, outSubHeadersBufSize, outStringBufLen);

    const size_t assetNameBufLen = strlen(assetPath)+1;
    const size_t totalDataBufSize = outFieldBufSize + outSubHeadersBufSize + outStringBufLen + assetNameBufLen;

    PakPageLump_s dataLump = pak->CreatePageLump(totalDataBufSize, SF_CPU, 8);
    const size_t assetNameOffset = totalDataBufSize - assetNameBufLen;

    memcpy(&dataLump.data[assetNameOffset], assetPath, assetNameBufLen);

    size_t curFieldBufIndex = 0;
    size_t curSubHeadersBufIndex = outFieldBufSize;
    size_t curStringBufIndex = outFieldBufSize + outSubHeadersBufSize;

    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, name), dataLump, assetNameOffset);
    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, fieldMap), dataLump, curFieldBufIndex);
    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, fieldNames), dataLump, curStringBufIndex);
    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, subHeaders), dataLump, curSubHeadersBufIndex);

    // Write out the root layout data.
    size_t numRootStringBufBytes = 0;
    SettingsLayout_WriteFieldData(dataLump, layoutAsset.rootLayout, curFieldBufIndex, curStringBufIndex, numRootStringBufBytes);

    // Write out all the sub layout data.
    for (size_t i = 0; i < layoutAsset.subLayouts.size(); i++)
    {
        size_t numSubStringBufBytes = 0;
        SettingsLayout_WriteData(pak, dataLump, layoutAsset.subLayouts[i], 
            curFieldBufIndex, curSubHeadersBufIndex, curStringBufIndex, numSubStringBufBytes);
    }

    asset.InitAsset(hdrLump.GetPointer(), sizeof(SettingsLayoutHeader_s), PagePtr_t::NullPtr(), STLT_VERSION, AssetType::STLT);
    asset.SetHeaderPointer(hdrLump.data);

    pak->FinishAsset();
}

void Assets::AddSettingsLayout_v0(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    SettingsLayout_InternalAddLayoutAsset(pak, assetGuid, assetPath);
}
