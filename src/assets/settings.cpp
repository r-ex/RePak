#include "pch.h"
#include "assets.h"
#include "public/settings_layout.h"
#include "public/settings.h"

#undef GetObject
#define SETTINGS_MODS_NAMES_FIELD "$modNames"
#define SETTINGS_MODS_VALUES_FIELD "$modValues"

static void SettingsAsset_OpenFile(CPakFileBuilder* const pak, const char* const assetPath, rapidjson::Document& document)
{
    const string fileName = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".json");

    if (!JSON_ParseFromFile(fileName.c_str(), "settings asset", document, true))
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
    case SettingsFieldType_e::ST_AssetNoPrecache:
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
        return guidBufSize + modNamesPtrBufSize + modValuesBufSize + valueBufSize + stringBufSize;
    }

    inline void InitCurrentIndices()
    {
        curGuidBufIndex = 0;
        curModNamesPtrBufIndex = guidBufSize;
        curModValuesBufIndex = guidBufSize + modNamesPtrBufSize;
        valueBufIndex = guidBufSize + modNamesPtrBufSize + modValuesBufSize;
        curStringBufIndex = guidBufSize + modNamesPtrBufSize + modValuesBufSize + valueBufSize;
    }

    size_t guidBufSize;
    size_t curGuidBufIndex;

    size_t modNamesPtrBufSize;
    size_t curModNamesPtrBufIndex;

    size_t modValuesBufSize;
    size_t curModValuesBufIndex;

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

static void SettingsAsset_CalculateModNamesBuffers(const rapidjson::Value& modNamesValue, SettingsAssetMemory_s& settingsMemory)
{
    const rapidjson::Value::ConstArray array = modNamesValue.GetArray();
    size_t elemIndex = 0;

    for (const rapidjson::Value& elem : array)
    {
        if (!elem.IsString())
        {
            Error("Settings mod name #%zu is of type %s, but %s was expected.\n",
                elemIndex, JSON_TypeToString(JSON_ExtractType(elem)), JSON_TypeToString(JSONFieldType_e::kString));
        }

        settingsMemory.modNamesPtrBufSize += sizeof(PagePtr_t);
        settingsMemory.stringBufSize += elem.GetStringLength() + 1;

        elemIndex++;
    }
}

static bool SettingsAsset_ModStringToType(const char* const typeName, SettingsModType_e& typeOut)
{
    for (unsigned short i = 0; i < SettingsModType_e::SETTINGS_MOD_COUNT; i++)
    {
        if (strcmp(typeName, g_settingsModType[i]) != 0)
            continue;

        typeOut = (SettingsModType_e)i;
        return true;
    }

    return false;
}

struct SettingsModValueItem_s
{
    uint16_t nameIndex;
    bool isNumericInt;
    SettingsModType_e modType;
    uint32_t valueOffset;
    rapidjson::Value::ConstMemberIterator valueIt;
};

struct SettingsModCache_s
{
    const rapidjson::Value* nameValues;
    std::vector<SettingsModValueItem_s> modValues;
};

static void SettingsAsset_CalculateModValuesBuffers(const rapidjson::Value& modValuesValue, const uint32_t modNamesCount,
    const SettingsLayoutAsset_s& layout, SettingsModCache_s& modCache, SettingsAssetMemory_s& settingsMemory)
{
    const rapidjson::Value::ConstArray array = modValuesValue.GetArray();
    size_t elemIndex = 0;

    modCache.modValues.resize(modValuesValue.Size());

    for (const rapidjson::Value& elem : array)
    {
        if (!elem.IsObject())
        {
            Error("Settings mod value #%zu is of type %s, but %s was expected.\n",
                elemIndex, JSON_TypeToString(JSON_ExtractType(elem)), JSON_TypeToString(JSONFieldType_e::kObject));
        }

        SettingsModValueItem_s& item = modCache.modValues[elemIndex];

        uint32_t nameIndex;
        JSON_ParseNumberRequired(elem, "index", nameIndex);

        if (nameIndex > SETTINGS_MAX_MODS)
            Error("Settings mod value #%zu has a mod name index of %u which exceeds the maximum of %hu.\n", elemIndex, nameIndex, SETTINGS_MAX_MODS);

        item.nameIndex = static_cast<uint16_t>(nameIndex);

        if (item.nameIndex > modNamesCount)
            Error("Settings mod value #%zu indexes outside mod names array (%hu > %hu).\n", elemIndex, item.nameIndex, static_cast<uint16_t>(modNamesCount));

        const char* const typeName = JSON_GetValueRequired<const char*>(elem, "type");

        if (!SettingsAsset_ModStringToType(typeName, item.modType))
            Error("Settings mod value #%zu has an invalid type (\"%s\").\n", elemIndex, typeName);

        JSON_GetRequired(elem, "value", item.valueIt);
        const rapidjson::Value& value = item.valueIt->value;

        JSONFieldType_e valueTypeRequested = JSONFieldType_e::kInvalid;
        SettingsFieldType_e fieldTypeRequested = SettingsFieldType_e::ST_Invalid;

        bool isNumericType = false;
        bool isStringType = false;

        switch (item.modType)
        {
        case SettingsModType_e::kIntPlus:
        case SettingsModType_e::kIntMultiply:
            valueTypeRequested = JSONFieldType_e::kSint32;
            fieldTypeRequested = SettingsFieldType_e::ST_Int;
            break;
        case SettingsModType_e::kFloatPlus:
        case SettingsModType_e::kFloatMultiply:
            valueTypeRequested = JSONFieldType_e::kFloat;
            fieldTypeRequested = SettingsFieldType_e::ST_Float;
            break;
        case SettingsModType_e::kBool:
            valueTypeRequested = JSONFieldType_e::kBool;
            fieldTypeRequested = SettingsFieldType_e::ST_Bool;
            break;
        case SettingsModType_e::kNumber:
            valueTypeRequested = JSONFieldType_e::kNumber;
            if (value.IsInt())
            {
                item.isNumericInt = true;
                fieldTypeRequested = SettingsFieldType_e::ST_Int;
            }
            else if (value.IsFloat())
            {
                item.isNumericInt = false;
                fieldTypeRequested = SettingsFieldType_e::ST_Float;
            }
            // else if its neither, keep it invalid as we will error with it.

            isNumericType = true;
            break;
        case SettingsModType_e::kString:
            valueTypeRequested = JSONFieldType_e::kString;
            fieldTypeRequested = SettingsFieldType_e::ST_String;
            isStringType = true;
            break;
        }

        if (!JSON_IsOfType(value, valueTypeRequested))
        {
            Error("Settings mod value #%zu expects a value type of \"%s\", but given value was of type \"%s\".\n",
                elemIndex, JSON_TypeToString(valueTypeRequested), JSON_TypeToString(JSON_ExtractType(value)));
        }

        // Strings are stored in the string buffer, and the offset to it will
        // be stored as value instead.
        if (isStringType)
            settingsMemory.stringBufSize += value.GetStringLength() + 1;

        const char* targetFieldName;
        SettingsFieldType_e fieldTypeExpected;

        SettingsLayoutFindByOffsetResult_s findByOffset;
        rapidjson::Value::ConstMemberIterator fieldDescIt;

        if (JSON_GetIterator(elem, "offset", fieldDescIt)) // Use offset instead of field names if available.
        {
            uint32_t targetOffset;

            if (!JSON_ParseNumber(fieldDescIt->value, targetOffset))
                Error("Settings mod value #%zu has an invalid offset.\n", elemIndex);

            if (!SettingsFieldFinder_FindFieldByAbsoluteOffset(layout, targetOffset, findByOffset))
                Error("Settings mod value #%zu has an absolute offset of %u which doesn't map to a field in the given settings layout.\n", elemIndex, targetOffset);

            targetFieldName = findByOffset.fieldAccessPath.c_str();
            fieldTypeExpected = findByOffset.type;

            item.valueOffset = targetOffset;
        }
        else // Parse it from the field name.
        {
            targetFieldName = JSON_GetValueRequired<const char*>(elem, "field");
            SettingsLayoutFindByNameResult_s findByName;

            if (!SettingsFieldFinder_FindFieldByAbsoluteName(layout, targetFieldName, findByName))
                Error("Settings mod value #%zu has an absolute field name of \"%s\" which doesn't exist in the given settings layout.\n", elemIndex, targetFieldName);

            fieldTypeExpected = findByName.type;
            item.valueOffset = findByName.valueOffset;
        }

        // Make sure the mod type is compatible with the settings field type.
        bool fieldTypeMismatch = false;

        if (isNumericType)
        {
            if (fieldTypeExpected == SettingsFieldType_e::ST_Int)
            {
                fieldTypeMismatch = fieldTypeRequested != SettingsFieldType_e::ST_Int;
            }
            else if (fieldTypeExpected == SettingsFieldType_e::ST_Float)
            {
                fieldTypeMismatch = fieldTypeRequested != SettingsFieldType_e::ST_Float;
            }
            else
                fieldTypeMismatch = true; // Not a numeric type.
        }
        else if (isStringType)
        {
            // Settings field type 'asset' and 'asset_noprecache' are internally
            // of type string, so it can match any of these.
            if (fieldTypeExpected != SettingsFieldType_e::ST_String &&
                fieldTypeExpected != SettingsFieldType_e::ST_Asset &&
                fieldTypeExpected != SettingsFieldType_e::ST_AssetNoPrecache)
            {
                fieldTypeMismatch = true;
            }
        }
        else
        {
            if (fieldTypeRequested != fieldTypeExpected)
                fieldTypeMismatch = true;
        }

        if (fieldTypeMismatch)
        {
            Error("Settings mod value #%zu is a modifier for field \"%s\" which is of type %s, but the given value classifies as type %s.\n",
                elemIndex, targetFieldName, s_settingsFieldTypeNames[fieldTypeExpected], s_settingsFieldTypeNames[fieldTypeRequested]);
        }

        settingsMemory.modValuesBufSize += sizeof(SettingsMod_s);
        elemIndex++;
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
        case SettingsFieldType_e::ST_AssetNoPrecache:
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

static void SettingsAsset_WriteModNames(const SettingsModCache_s& modCache, CPakFileBuilder* const pak, 
    SettingsAssetMemory_s& settingsMemory, PakPageLump_s& dataLump)
{
    const rapidjson::Value::ConstArray array = modCache.nameValues->GetArray();

    for (const rapidjson::Value& elem : array)
    {
        const size_t stringBufLen = elem.GetStringLength() + 1;
        memcpy(&dataLump.data[settingsMemory.curStringBufIndex], elem.GetString(), stringBufLen);

        pak->AddPointer(dataLump, settingsMemory.curModNamesPtrBufIndex, dataLump, settingsMemory.curStringBufIndex);

        settingsMemory.curModNamesPtrBufIndex += sizeof(PagePtr_t);
        settingsMemory.curStringBufIndex += stringBufLen;
    }
}

static void SettingsAsset_WriteModValues(const SettingsModCache_s& modCache, const size_t stringBufferBase,
    SettingsAssetMemory_s& settingsMemory, PakPageLump_s& dataLump)
{
    for (const SettingsModValueItem_s& item : modCache.modValues)
    {
        char* const currPtr = &dataLump.data[settingsMemory.curModValuesBufIndex];
        SettingsMod_s* const mod = reinterpret_cast<SettingsMod_s*>(currPtr);

        mod->nameIndex = item.nameIndex;
        mod->type = item.modType;
        mod->valueOffset = item.valueOffset;

        switch (item.modType)
        {
        case SettingsModType_e::kIntPlus:
        case SettingsModType_e::kIntMultiply:
            mod->value.intValue = item.valueIt->value.GetInt();
            break;
        case SettingsModType_e::kFloatPlus:
        case SettingsModType_e::kFloatMultiply:
            mod->value.floatValue = item.valueIt->value.GetFloat();
            break;
        case SettingsModType_e::kBool:
            mod->value.boolValue = item.valueIt->value.GetBool();
            break;
        case SettingsModType_e::kNumber:
            if (item.isNumericInt)
                mod->value.intValue = item.valueIt->value.GetInt();
            else
                mod->value.floatValue = item.valueIt->value.GetFloat();
            break;
        case SettingsModType_e::kString:
        {
            const rapidjson::Value& val = item.valueIt->value;
            const size_t strBufLen = val.GetStringLength() + 1; // +1 for null char.

            memcpy(&dataLump.data[settingsMemory.curStringBufIndex], val.GetString(), strBufLen);
            mod->value.stringOffset = static_cast<uint32_t>(settingsMemory.curStringBufIndex - stringBufferBase);

            settingsMemory.curStringBufIndex += strBufLen;
            break;
        }
        }

        settingsMemory.curModValuesBufIndex += sizeof(SettingsMod_s);
    }
}

static void SettingsAsset_ValidateModsArray(rapidjson::Value::ConstMemberIterator arrayIt, const char* const fieldName)
{
    if (!arrayIt->value.IsArray())
    {
        Error("Settings asset contains array field \"%s\" and is of type %s, but %s was expected.\n",
            fieldName, JSON_TypeToString(JSON_ExtractType(arrayIt->value)), JSON_TypeToString(JSONFieldType_e::kArray));
    }

    const size_t nameElemCount = arrayIt->value.GetArray().Size();

    if (nameElemCount == 0)
        Error("Settings asset contains array field \"%s\" that is empty.\n", fieldName);
    else if (nameElemCount > SETTINGS_MAX_MODS)
        Error("Settings asset contains array field \"%s\" that has too many elements (max %hu, got %zu).\n",
            fieldName, SETTINGS_MAX_MODS, nameElemCount);
}

static bool SettingsAsset_ParseMods(SettingsAssetHeader_s* const setHdr, const SettingsLayoutAsset_s& layoutAsset,
    SettingsAssetMemory_s& settingsMemory, SettingsModCache_s& modCache, const rapidjson::Document& settings)
{
    rapidjson::Value::ConstMemberIterator modNamesIt;
    const bool hasModNames = JSON_GetIterator(settings, SETTINGS_MODS_NAMES_FIELD, modNamesIt);

    rapidjson::Value::ConstMemberIterator modValuesIt;
    const bool hasModValues = JSON_GetIterator(settings, SETTINGS_MODS_VALUES_FIELD, modValuesIt);

    if (hasModNames != hasModValues)
        Error("Settings asset contains array field \"%s\", but the corresponding array field \"%s\" is missing.\n",
            hasModNames ? SETTINGS_MODS_NAMES_FIELD : SETTINGS_MODS_VALUES_FIELD,
            hasModNames ? SETTINGS_MODS_VALUES_FIELD : SETTINGS_MODS_NAMES_FIELD);

    if (!hasModNames || !hasModValues)
        return false;

    SettingsAsset_ValidateModsArray(modNamesIt, SETTINGS_MODS_NAMES_FIELD);
    SettingsAsset_ValidateModsArray(modValuesIt, SETTINGS_MODS_VALUES_FIELD);

    modCache.nameValues = &modNamesIt->value;

    SettingsAsset_CalculateModNamesBuffers(modNamesIt->value, settingsMemory);
    setHdr->modNameCount = static_cast<uint32_t>(modNamesIt->value.GetArray().Size());

    SettingsAsset_CalculateModValuesBuffers(modValuesIt->value, setHdr->modNameCount, layoutAsset, modCache, settingsMemory);
    setHdr->modValuesCount = static_cast<uint32_t>(modValuesIt->value.GetArray().Size());

    setHdr->modFlags = JSON_GetNumberOrDefault(settings, "$modFlags", 0);
    return true;
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
    setHdr->valueBufSize = static_cast<uint32_t>(settingsMemory.valueBufSize);

    SettingsModCache_s modCache{};
    const bool hasMods = SettingsAsset_ParseMods(setHdr, layoutAsset, settingsMemory, modCache, settings);

    const size_t assetNameBufLen = strlen(assetPath)+1;
    settingsMemory.stringBufSize += assetNameBufLen;

    settingsMemory.InitCurrentIndices();
    const size_t totalPageSize = settingsMemory.GetTotalBufferSize();

    PakPageLump_s dataLump = pak->CreatePageLump(totalPageSize, SF_CPU, 8);
    const size_t assetNameOffset = totalPageSize - assetNameBufLen;

    memcpy(&dataLump.data[assetNameOffset], assetPath, assetNameBufLen);

    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, valueData), dataLump, settingsMemory.valueBufIndex);
    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, name), dataLump, assetNameOffset);
    pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, stringData), dataLump, settingsMemory.curStringBufIndex);

    SettingsAsset_WriteValues(layoutAsset, settingsAsset, settingsMemory, asset, pak, dataLump);

    if (hasMods)
    {
        pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, modNames), dataLump, settingsMemory.curModNamesPtrBufIndex);
        pak->AddPointer(hdrLump, offsetof(SettingsAssetHeader_s, modValues), dataLump, settingsMemory.curModValuesBufIndex);

        SettingsAsset_WriteModNames(modCache, pak, settingsMemory, dataLump);
        SettingsAsset_WriteModValues(modCache, settingsMemory.curStringBufIndex, settingsMemory, dataLump);
    }

    asset.InitAsset(hdrLump.GetPointer(), sizeof(SettingsAssetHeader_s), PagePtr_t::NullPtr(), STGS_VERSION, AssetType::STGS);
    asset.SetHeaderPointer(hdrLump.data);

    pak->FinishAsset();
}

void Assets::AddSettingsAsset_v1(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    SettingsAsset_InternalAddSettingsAsset(pak, assetGuid, assetPath);
}
