#include "pch.h"
#include "assets.h"
#include "public/settings.h"
#include <public/settings_layout.h>

#undef GetObject

//
// Settings asset todo:
// - asset fields must be added as a dependency field for this settings asset.
// 
// Settings layout todo:
// - handle arrays
// - pack layouts
//

static void SettingsAsset_OpenFile(CPakFileBuilder* const pak, const char* const assetPath, rapidjson::Document& document)
{
    const string fileName = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".json");

    if (!JSON_ParseFromFile(fileName.c_str(), "settings asset", document))
        Error("Failed to open settings asset \"%s\".\n", fileName.c_str());
}

extern void SettingsLayout_ParseLayout(CPakFileBuilder* const pak, const char* const assetPath, rapidcsv::Document& document, SettingsLayoutParseResult_s& result);

static int64_t FindColumnCell(const std::vector<std::string>& column, const char* const name, const int occurrence)
{
    const int64_t numColumns = static_cast<int64_t>(column.size());
    int numFound = 0;

    for (int64_t i = 0; i < numColumns; i++)
    {
        const std::string& entry = column[i];

        if (entry.compare(name) == 0)
        {
            if (numFound++ == occurrence)
                return i;
        }
    }

    return -1;
}

static JSONFieldType_e GetJsonMemberTypeForSettingsType(const SettingsFieldType_e type)
{
    switch (type)
    {
    case SettingsFieldType_e::ST_Bool:
        return JSONFieldType_e::kBool;
    case SettingsFieldType_e::ST_Int:
        return JSONFieldType_e::kSint32;
    case SettingsFieldType_e::ST_Float:
        return JSONFieldType_e::kFloat;

    case SettingsFieldType_e::ST_Float2:
    case SettingsFieldType_e::ST_Float3:
    case SettingsFieldType_e::ST_String:
    case SettingsFieldType_e::ST_Asset:
    case SettingsFieldType_e::ST_Asset_2:
        return JSONFieldType_e::kString;

    case SettingsFieldType_e::ST_StaticArray:
    case SettingsFieldType_e::ST_DynamicArray:
        return JSONFieldType_e::kArray;

    default: return JSONFieldType_e::kInvalid;
    }
}

static void SettingsAsset_CalcBufferSizes(const char* const layoutAsset, std::vector<SettingsAssetFieldEntry_s>& entryMap,
    const SettingsLayoutParseResult_s& layoutData, const rapidjson::Value& entries, size_t& stringBufSize, size_t& guidBufSize)
{
    uint32_t minValueBufSize = 0;
    std::map<std::string, int> occurenceMap;

    for (const auto& it : entries.GetObject())
    {
        const char* const fieldName = it.name.GetString();
        int& occurence = occurenceMap[fieldName];

        const int64_t cellIndex = FindColumnCell(layoutData.fieldNames, fieldName, occurence);

        if (cellIndex == -1)
            Error("Field \"%s\" with occurrence #%d does not exist in settings layout \"%s\".\n", fieldName, occurence, layoutAsset);

        occurence++;

        const SettingsFieldType_e typeToUse = layoutData.typeMap[cellIndex];
        entryMap.push_back({&it.value, cellIndex });

        const JSONFieldType_e expectJsonType = GetJsonMemberTypeForSettingsType(typeToUse);
        const JSONFieldType_e hasJsonType = JSON_ExtractType(it.value);

        if (hasJsonType != expectJsonType)
            Error("Field \"%s\" is detected as type %s, but layout expects %s for settings field type %s.\n",
                fieldName, JSON_TypeToString(hasJsonType), JSON_TypeToString(expectJsonType), s_settingsFieldTypeNames[typeToUse]);

        // Calculate the buffer sizes
        minValueBufSize += SettingsLayout_GetFieldSizeForType(typeToUse);

        if (expectJsonType == JSONFieldType_e::kString)
            stringBufSize += it.value.GetStringLength()+1; // +1 for the null char.
    }

    //assert(entryMap.size() == entries.Size());

    if (minValueBufSize > layoutData.totalValueBufferSize)
        Error("Value buffer overflow; settings layout has a maximum of %u while settings asset requires a minimum of %u.\n", 
            layoutData.totalValueBufferSize, minValueBufSize);
}

static void SettingsAsset_WriteStringValue(const char* const string, const size_t stringLen, const size_t valueOffset,
    CPakFileBuilder* const pak, PakPageLump_s& dataLump, size_t& currentStringIndex)
{
    pak->AddPointer(dataLump, valueOffset, dataLump, currentStringIndex);

    const size_t strBufLen = stringLen+1; // +1 for null char.
    memcpy(&dataLump.data[currentStringIndex], string, strBufLen);

    currentStringIndex += strBufLen;
}

static void SettingsAsset_WriteVectorValue(const SettingsLayoutParseResult_s& layoutData, const SettingsAssetFieldEntry_s& entry,
    const size_t valueOffset, const bool isVec2, PakPageLump_s& dataLump)
{
    std::cmatch sm; // get values from format "<x,y>", or "<x,y,z>".
    const char* const value = entry.val->GetString();

    std::regex_search(value, sm, std::regex(isVec2 ? "<(.*),(.*)>" : "<(.*),(.*),(.*)>"));

    // 0 - all
    // 1 - x
    // 2 - y
    // 3 - z (if !isVec2)
    if (sm.size() == (isVec2 ? 3 : 4))
    {
        Vector3* const vec = reinterpret_cast<Vector3*>(&dataLump.data[valueOffset]);
        vec->x = static_cast<float>(atof(sm[1].str().c_str()));
        vec->y = static_cast<float>(atof(sm[2].str().c_str()));

        if (!isVec2)
            vec->z = static_cast<float>(atof(sm[3].str().c_str()));
    }
    else
    {
        const string& fieldName = layoutData.fieldNames[entry.cellIndex];
        const char* const targetType = isVec2 ? "float2" : "float3";

        Error("Field \"%s\" has value '%s' that cannot be parsed as %s.\n", fieldName.c_str(), value, targetType);
    }
}

static void SettingsAsset_WriteValue(const SettingsLayoutParseResult_s& layoutData, const SettingsAssetFieldEntry_s& entry,
    CPakFileBuilder* const pak, PakPageLump_s& dataLump, const size_t valueBaseIndex, size_t& currentStringIndex)
{
    const int64_t cellIndex = entry.cellIndex;
    const uint32_t valueOffset = layoutData.offsetMap[cellIndex];
    const SettingsFieldType_e fieldType = layoutData.typeMap[cellIndex];

    switch (fieldType)
    {
    case SettingsFieldType_e::ST_Bool:
        *(bool*)&dataLump.data[valueBaseIndex + valueOffset] = entry.val->GetBool();
        break;
    case SettingsFieldType_e::ST_Int:
        *(int*)&dataLump.data[valueBaseIndex + valueOffset] = entry.val->GetInt();
        break;
    case SettingsFieldType_e::ST_Float:
        *(float*)&dataLump.data[valueBaseIndex + valueOffset] = entry.val->GetFloat();
        break;
    case SettingsFieldType_e::ST_Float2:
        SettingsAsset_WriteVectorValue(layoutData, entry, valueBaseIndex + valueOffset, true, dataLump);
        break;
    case SettingsFieldType_e::ST_Float3:
        SettingsAsset_WriteVectorValue(layoutData, entry, valueBaseIndex + valueOffset, false, dataLump);
        break;
    case SettingsFieldType_e::ST_String:
    case SettingsFieldType_e::ST_Asset:
    case SettingsFieldType_e::ST_Asset_2:

        SettingsAsset_WriteStringValue(entry.val->GetString(), entry.val->GetStringLength(), valueBaseIndex + valueOffset, pak, dataLump, currentStringIndex);
        break;
    }
}

static void SettingsAsset_WriteValues(const SettingsLayoutParseResult_s& layoutData, const std::vector<SettingsAssetFieldEntry_s>& entryMap,
    CPakFileBuilder* const pak, PakPageLump_s& dataLump, const size_t valueBaseIndex, const size_t stringBaseIndex)
{
    const size_t entryCount = entryMap.size();
    size_t curStringPos = stringBaseIndex;

    for (size_t i = 0; i < entryCount; i++)
        SettingsAsset_WriteValue(layoutData, entryMap[i], pak, dataLump, valueBaseIndex, curStringPos);
}

static void SettingsAsset_InternalAddSettingsAsset(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
    rapidjson::Document settings;
    SettingsAsset_OpenFile(pak, assetPath, settings);

    rapidcsv::Document layout;
    const char* const layoutAsset = JSON_GetValueRequired<const char*>(settings, "layoutAsset");

    SettingsLayoutParseResult_s layoutData;
    SettingsLayout_ParseLayout(pak, layoutAsset, layout, layoutData);

    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
    PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(SettingsAssetHeader_s), SF_HEAD, 8);

    SettingsAssetHeader_s* const setHdr = reinterpret_cast<SettingsAssetHeader_s*>(hdrLump.data);
    const PakGuid_t layoutGuid = RTech::StringToGuid(layoutAsset);

    setHdr->settingsLayoutGuid = layoutGuid;
    Pak_RegisterGuidRefAtOffset(layoutGuid, offsetof(SettingsAssetHeader_s, settingsLayoutGuid), hdrLump, asset);

    setHdr->uniqueID = JSON_GetNumberRequired<uint32_t>(settings, "uniqueID");

    std::vector<SettingsAssetFieldEntry_s> entryMap;
    rapidjson::Value::ConstMemberIterator setIt;

    JSON_GetRequired(settings, "settings", JSONFieldType_e::kObject, setIt);
    const rapidjson::Value& entries = setIt->value;

    size_t stringBufSize = 0;
    SettingsAsset_CalcBufferSizes(layoutAsset, entryMap, layoutData, entries, stringBufSize);

    const size_t assetNameBufLen = IALIGN8(strlen(assetPath)+1);
    PakPageLump_s dataLump = pak->CreatePageLump(assetNameBufLen + layoutData.totalValueBufferSize + stringBufSize, SF_CPU, 8);

    memcpy(dataLump.data, assetPath, assetNameBufLen);
    SettingsAsset_WriteValues(layoutData, entryMap, pak, dataLump, assetNameBufLen, assetNameBufLen + layoutData.totalValueBufferSize);

    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, name), dataLump, 0);
    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, valueData), dataLump, assetNameBufLen);
    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, stringData), dataLump, assetNameBufLen + layoutData.totalValueBufferSize);

    setHdr->valueBufSize = layoutData.totalValueBufferSize;

    asset.InitAsset(hdrLump.GetPointer(), sizeof(SettingsAssetHeader_s), PagePtr_t::NullPtr(), STGS_VERSION, AssetType::STGS);
    asset.SetHeaderPointer(hdrLump.data);

    pak->FinishAsset();
}

void Assets::AddSettingsAsset_v1(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    SettingsAsset_InternalAddSettingsAsset(pak, assetGuid, assetPath);
}
