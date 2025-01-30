#include "pch.h"
#include "assets.h"
#include "public/settings_layout.h"
#include "public/settings.h"

#undef GetObject

//
// Settings asset todo:
// - make sure all fields in settings layout are defined and initialized in settings asset
//      (error checking on this is currently disabled to make debugging and development
//      easier, but in the runtime, not initializing these results in undefined behavior most likely.)
//

static void SettingsAsset_OpenFile(CPakFileBuilder* const pak, const char* const assetPath, rapidjson::Document& document)
{
    const string fileName = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".json");

    if (!JSON_ParseFromFile(fileName.c_str(), "settings asset", document))
        Error("Failed to open settings asset \"%s\".\n", fileName.c_str());
}

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

struct SettingsAssetMemory_s
{
    inline size_t GetTotalBufferSize() const
    {
        return guidBufSize + valueBufSize + stringBufSize;
    }

    inline void InitCurrentIndices()
    {
        curGuidBufIndex = 0;
        valueBufIndex = guidBufSize;
        curStringBufIndex = guidBufSize + valueBufSize;
    }

    size_t guidBufSize;
    size_t curGuidBufIndex;

    size_t valueBufSize;
    size_t valueBufIndex;

    size_t stringBufSize;
    size_t curStringBufIndex;
};

static void SettingsAsset_InitializeAndMap(const char* const layoutAssetPath, SettingsAsset_s& settingsAsset,
    const SettingsLayoutAsset_s& layoutAsset, const rapidjson::Value& value, SettingsAssetMemory_s& settingsMemory, const size_t bufferBase)
{
    if (!value.IsObject())
        Error("Settings asset object using settings layout \"%s\" was not an object.\n", layoutAssetPath);

    const size_t numSettingsFields = value.GetObject().MemberCount();
    const size_t numLayoutFields = layoutAsset.rootLayout.fieldNames.size();

    if (numSettingsFields != numLayoutFields)
        Error("Settings asset has %zu fields, but settings layout \"%s\" lists %zu fields.\n", numSettingsFields, layoutAssetPath, numLayoutFields);

    settingsAsset.value = &value;
    settingsAsset.layout = &layoutAsset;

    if (bufferBase != SIZE_MAX)
        settingsAsset.bufferBase = bufferBase;
    else
    {
        settingsAsset.bufferBase = settingsMemory.valueBufSize;
        settingsMemory.valueBufSize += layoutAsset.rootLayout.totalValueBufferSize;
    }

    std::map<std::string, int> occurenceMap;

    for (const auto& it : value.GetObject())
    {
        const char* const fieldName = it.name.GetString();
        int& occurence = occurenceMap[fieldName];

        const int64_t cellIndex = FindColumnCell(layoutAsset.rootLayout.fieldNames, fieldName, occurence);

        if (cellIndex == -1)
            Error("Field \"%s\" with occurrence #%d does not exist in settings layout \"%s\".\n", fieldName, occurence, layoutAssetPath);

        occurence++;
        settingsAsset.fieldIndexMap.push_back(cellIndex);

        const SettingsFieldType_e typeToUse = layoutAsset.rootLayout.typeMap[cellIndex];

        const JSONFieldType_e expectJsonType = GetJsonMemberTypeForSettingsType(typeToUse);
        const JSONFieldType_e hasJsonType = JSON_ExtractType(it.value);

        if (hasJsonType != expectJsonType)
        {
            Error("Field \"%s\" is detected as type %s, but layout expects type %s for settings field type %s.\n",
                fieldName, JSON_TypeToString(hasJsonType), JSON_TypeToString(expectJsonType), s_settingsFieldTypeNames[typeToUse]);
        }

        if (expectJsonType == JSONFieldType_e::kString)
        {
            settingsMemory.stringBufSize += it.value.GetStringLength() + 1; // +1 for the null char.

            // Note(amos): only precached asset fields need to have their value
            // hashed and stored as a GUID dependency. Only on string types.
            if (typeToUse == SettingsFieldType_e::ST_Asset)
                settingsMemory.guidBufSize += sizeof(PakGuid_t);
        }

        // Recursively process all arrays and nested arrays.
        if (hasJsonType == JSONFieldType_e::kArray)
        {
            const SettingsLayoutAsset_s& subLayout = layoutAsset.subLayouts[layoutAsset.rootLayout.indexMap[cellIndex]];
            const rapidjson::Value::ConstArray array = it.value.GetArray();

            size_t bufferBaseOverride = SIZE_MAX;

            if (typeToUse == SettingsFieldType_e::ST_StaticArray)
            {
                const uint32_t arraySize = static_cast<uint32_t>(array.Size());
                const uint32_t layoutArraySize = subLayout.rootLayout.arrayElemCount;

                if (arraySize != layoutArraySize)
                {
                    Error("Field \"%s\" is defined as a static array of size %u, but listed array has a size of %u.\n",
                        fieldName, layoutArraySize, arraySize);
                }

                // Static arrays use predefined offsets within parent layout.
                bufferBaseOverride = settingsAsset.bufferBase + layoutAsset.rootLayout.offsetMap[cellIndex];
            }
            else
            {
                // Note(amos): allocate buffer upfront as we would otherwise end up
                // interleaving nested array items into our parent array which is
                // not allowed. All array elements must be contiguous.
                const size_t arrayMemorySize = array.Size() * subLayout.rootLayout.totalValueBufferSize;
                const size_t arrayBufferBase = settingsMemory.valueBufSize;

                settingsMemory.valueBufSize += arrayMemorySize;
                bufferBaseOverride = arrayBufferBase;
            }

            size_t elementOffset = 0;

            for (const rapidjson::Value& nit : array)
            {
                SettingsAsset_s& subAsset = settingsAsset.subAssets.emplace_back();
                SettingsAsset_InitializeAndMap(layoutAssetPath, subAsset, subLayout, nit, settingsMemory, bufferBaseOverride + elementOffset);

                elementOffset += subLayout.rootLayout.totalValueBufferSize;
            }
        }
    }
}

static void SettingsAsset_WriteStringValue(const char* const string, const size_t stringLen, const size_t valueOffset,
    CPakFileBuilder* const pak, PakPageLump_s& dataLump, SettingsAssetMemory_s& settingsMemory)
{
    pak->AddPointer(dataLump, valueOffset, dataLump, settingsMemory.curStringBufIndex);

    const size_t strBufLen = stringLen+1; // +1 for null char.
    memcpy(&dataLump.data[settingsMemory.curStringBufIndex], string, strBufLen);

    settingsMemory.curStringBufIndex += strBufLen;
}

static void SettingsAsset_WriteAssetValue(const char* const string, const size_t stringLen, const size_t valueOffset,
    PakAsset_t& asset, CPakFileBuilder* const pak, PakPageLump_s& dataLump, SettingsAssetMemory_s& settingsMemory)
{
    const PakGuid_t assetGuid = RTech::StringToGuid(string);
    *(PakGuid_t*)reinterpret_cast<char*>(&dataLump.data[settingsMemory.curGuidBufIndex]) = assetGuid;

    Pak_RegisterGuidRefAtOffset(assetGuid, settingsMemory.curGuidBufIndex, dataLump, asset);
    settingsMemory.curGuidBufIndex += sizeof(PakGuid_t);

    SettingsAsset_WriteStringValue(string, stringLen, valueOffset, pak, dataLump, settingsMemory);
}

static void SettingsAsset_WriteVectorValue(const char* const value, const char* const fieldName,
    const size_t valueOffset, const bool isVec2, PakPageLump_s& dataLump)
{
    std::cmatch sm; // get values from format "<x,y>", or "<x,y,z>".
    const bool result = std::regex_search(value, sm, std::regex(isVec2 ? "<(.*),(.*)>" : "<(.*),(.*),(.*)>"));

    // 0 - all
    // 1 - x
    // 2 - y
    // 3 - z (if !isVec2)
    if (result && (sm.size() == (isVec2 ? 3 : 4)))
    {
        Vector3* const vec = reinterpret_cast<Vector3*>(&dataLump.data[valueOffset]);
        vec->x = static_cast<float>(atof(sm[1].str().c_str()));
        vec->y = static_cast<float>(atof(sm[2].str().c_str()));

        if (!isVec2)
            vec->z = static_cast<float>(atof(sm[3].str().c_str()));
    }
    else
    {
        const char* const targetType = isVec2 ? "float2" : "float3";
        Error("Field \"%s\" has value '%s' that cannot be parsed as %s.\n", fieldName, value, targetType);
    }
}

static void SettingsAsset_WriteValues(const SettingsLayoutAsset_s& layoutAsset, SettingsAsset_s& settingsAsset, 
    SettingsAssetMemory_s& settingsMemory, PakAsset_t& asset, CPakFileBuilder* const pak, PakPageLump_s& dataLump)
{
    size_t fieldIndex = 0;
    size_t arrayIndex = 0;

    for (const auto& it : settingsAsset.value->GetObject())
    {
        const int64_t cellIndex = settingsAsset.fieldIndexMap[fieldIndex++];

        const uint32_t localValueOffset = layoutAsset.rootLayout.offsetMap[cellIndex];
        const size_t targetOffset = settingsAsset.bufferBase + settingsMemory.valueBufIndex + localValueOffset;

        const std::string& fieldName = layoutAsset.rootLayout.fieldNames[cellIndex];
        const SettingsFieldType_e fieldType = layoutAsset.rootLayout.typeMap[cellIndex];

        switch (fieldType)
        {
        case SettingsFieldType_e::ST_Bool:
            *(bool*)&dataLump.data[targetOffset] = it.value.GetBool();
            break;
        case SettingsFieldType_e::ST_Int:
            *(int*)&dataLump.data[targetOffset] = it.value.GetInt();
            break;
        case SettingsFieldType_e::ST_Float:
            *(float*)&dataLump.data[targetOffset] = it.value.GetFloat();
            break;
        case SettingsFieldType_e::ST_Float2:
            SettingsAsset_WriteVectorValue(it.value.GetString(), fieldName.c_str(), targetOffset, true, dataLump);
            break;
        case SettingsFieldType_e::ST_Float3:
            SettingsAsset_WriteVectorValue(it.value.GetString(), fieldName.c_str(), targetOffset, false, dataLump);
            break;
        case SettingsFieldType_e::ST_Asset:
            SettingsAsset_WriteAssetValue(it.value.GetString(), it.value.GetStringLength(), targetOffset, asset, pak, dataLump, settingsMemory);
            break;
        case SettingsFieldType_e::ST_String:
        case SettingsFieldType_e::ST_Asset_2:
            SettingsAsset_WriteStringValue(it.value.GetString(), it.value.GetStringLength(), targetOffset, pak, dataLump, settingsMemory);
            break;
        case SettingsFieldType_e::ST_StaticArray:
        {
            // Note(amos): array elem count was already checked in SettingsAsset_InitializeAndMap().
            // No need to do it again here.
            const rapidjson::Value::ConstArray statArray = it.value.GetArray();
            const SettingsLayoutAsset_s& subLayout = layoutAsset.subLayouts[layoutAsset.rootLayout.indexMap[cellIndex]];

            // Values in array are already attached to the settings block at this stage.
            // We should only traverse the array from here.
            for (auto nit = statArray.begin(); nit != statArray.End(); ++nit)
            {
                SettingsAsset_s& subAsset = settingsAsset.subAssets[arrayIndex++];
                SettingsAsset_WriteValues(subLayout, subAsset, settingsMemory, asset, pak, dataLump);
            }

            break;
        }
        case SettingsFieldType_e::ST_DynamicArray:
        {
            const rapidjson::Value::ConstArray dynArray = it.value.GetArray();
            const uint32_t arraySize = static_cast<uint32_t>(dynArray.Size());

            SettingsDynamicArray_s* const dynHdr = reinterpret_cast<SettingsDynamicArray_s*>(&dataLump.data[targetOffset]);
            dynHdr->arraySize = arraySize;

            const SettingsLayoutAsset_s& subLayout = layoutAsset.subLayouts[layoutAsset.rootLayout.indexMap[cellIndex]];
            bool initOffset = false;

            // Values in array are already attached to the settings block at this stage.
            // We should only traverse the array from here.
            for (auto nit = dynArray.begin(); nit != dynArray.End(); ++nit)
            {
                SettingsAsset_s& subAsset = settingsAsset.subAssets[arrayIndex++];

                if (!initOffset)
                {
                    dynHdr->arrayOffset = static_cast<uint32_t>(subAsset.bufferBase - settingsAsset.bufferBase);
                    initOffset = true;
                }

                SettingsAsset_WriteValues(subLayout, subAsset, settingsMemory, asset, pak, dataLump);
            }

            break;
        }
        }
    }
}

extern void SettingsLayout_ParseLayout(CPakFileBuilder* const pak, const char* const assetPath, SettingsLayoutAsset_s& layoutAsset);

static void SettingsAsset_InternalAddSettingsAsset(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
    rapidjson::Document settings;
    SettingsAsset_OpenFile(pak, assetPath, settings);

    const char* const layoutAssetPath = JSON_GetValueRequired<const char*>(settings, "layoutAsset");

    SettingsLayoutAsset_s layoutAsset;
    SettingsLayout_ParseLayout(pak, layoutAssetPath, layoutAsset);

    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
    PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(SettingsAssetHeader_s), SF_HEAD, 8);

    SettingsAssetHeader_s* const setHdr = reinterpret_cast<SettingsAssetHeader_s*>(hdrLump.data);
    const PakGuid_t layoutGuid = RTech::StringToGuid(layoutAssetPath);

    setHdr->settingsLayoutGuid = layoutGuid;
    Pak_RegisterGuidRefAtOffset(layoutGuid, offsetof(SettingsAssetHeader_s, settingsLayoutGuid), hdrLump, asset);

    setHdr->uniqueId = JSON_GetNumberOrDefault(settings, "uniqueId", (uint32_t)0);

    rapidjson::Value::ConstMemberIterator setIt;

    JSON_GetRequired(settings, "settings", JSONFieldType_e::kObject, setIt);
    const rapidjson::Value& entries = setIt->value;

    SettingsAsset_s settingsAsset;
    SettingsAssetMemory_s settingsMemory{};

    SettingsAsset_InitializeAndMap(layoutAssetPath, settingsAsset, layoutAsset, entries, settingsMemory, SIZE_MAX);

    const size_t assetNameBufLen = strlen(assetPath)+1;
    settingsMemory.stringBufSize += assetNameBufLen;

    settingsMemory.InitCurrentIndices();
    const size_t totalPageSize = settingsMemory.GetTotalBufferSize();

    // todo: check if layout val buffer is larger and if so max it to that.
    PakPageLump_s dataLump = pak->CreatePageLump(totalPageSize, SF_CPU, 8);
    const size_t assetNameOffset = totalPageSize - assetNameBufLen;

    memcpy(&dataLump.data[assetNameOffset], assetPath, assetNameBufLen);

    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, name), dataLump, assetNameOffset);
    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, valueData), dataLump, settingsMemory.valueBufIndex);
    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, stringData), dataLump, settingsMemory.curStringBufIndex);

    SettingsAsset_WriteValues(layoutAsset, settingsAsset, settingsMemory, asset, pak, dataLump);

    // todo: does this also count the arrays and dynamic array values?
    // if yes, calculate the size in SettingsAsset_InitializeAndMap().
    //setHdr->valueBufSize = layoutAsset.totalValueBufferSize;

    asset.InitAsset(hdrLump.GetPointer(), sizeof(SettingsAssetHeader_s), PagePtr_t::NullPtr(), STGS_VERSION, AssetType::STGS);
    asset.SetHeaderPointer(hdrLump.data);

    pak->FinishAsset();
}

void Assets::AddSettingsAsset_v1(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    SettingsAsset_InternalAddSettingsAsset(pak, assetGuid, assetPath);
}
