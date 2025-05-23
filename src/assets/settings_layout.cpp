#include "pch.h"
#include "assets.h"
#include "public/settings_layout.h"

// Maximum number of retries to find a good hashing configuration
// with the least amount of collisions.
#define SETTINGS_LAYOUT_MAX_HASH_RETRIES 128

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
    case SettingsFieldType_e::ST_AssetNoPrecache:
        return sizeof(PagePtr_t);
    case SettingsFieldType_e::ST_DynamicArray:
        return sizeof(SettingsDynamicArray_s);

    default: assert(0); return 0;
    }
}

uint32_t SettingsLayout_GetFieldAlignmentForType(const SettingsFieldType_e type)
{
    switch (type)
    {
    case SettingsFieldType_e::ST_Bool:
        return sizeof(bool);
    case SettingsFieldType_e::ST_Int:
    case SettingsFieldType_e::ST_DynamicArray:
        return sizeof(int);
    case SettingsFieldType_e::ST_Float:
    case SettingsFieldType_e::ST_Float2:
    case SettingsFieldType_e::ST_Float3:
        return sizeof(float);
    case SettingsFieldType_e::ST_String:
    case SettingsFieldType_e::ST_Asset:
    case SettingsFieldType_e::ST_AssetNoPrecache:
        return sizeof(void*);

    default: assert(0); return 0;
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

bool SettingsFieldFinder_FindFieldByAbsoluteOffset(const SettingsLayoutAsset_s& layout, const uint32_t targetOffset, SettingsLayoutFindByOffsetResult_s& result)
{
    for (size_t i = 0; i < layout.rootLayout.typeMap.size(); i++)
    {
        const uint32_t totalValueBufSizeAligned = IALIGN(layout.rootLayout.totalValueBufferSize, layout.rootLayout.alignment);

        if (targetOffset > result.currentBase + (layout.rootLayout.arrayElemCount * totalValueBufSizeAligned))
            return false; // Beyond this layout.

        const uint32_t fieldOffset = layout.rootLayout.offsetMap[i];

        if (targetOffset < fieldOffset)
            return false; // Invalid offset (i.e. we have 2 ints at 4 and 8, but target was 5).

        const uint32_t originalBase = result.currentBase;

        for (int currArrayIdx = 0; currArrayIdx < layout.rootLayout.arrayElemCount; currArrayIdx++)
        {
            const uint32_t elementBase = result.currentBase + (currArrayIdx * totalValueBufSizeAligned);
            const uint32_t absoluteFieldOffset = elementBase + fieldOffset;

            const SettingsFieldType_e fieldType = layout.rootLayout.typeMap[i];
            const bool isStaticArray = fieldType == SettingsFieldType_e::ST_StaticArray;

            // note(amos): the first member of an element in a static array
            // will always share the same offset as the static array its
            // contained in. Delay it off to the next recursion so we return
            // the name of the member of the element in the array instead since
            // this function does a lookup by absolute offsets, and static
            // arrays technically don't exist in that context. This is also
            // required for constructing the field access path correctly for
            // a given offset.
            if (!isStaticArray && targetOffset == absoluteFieldOffset)
            {
                // note(amos): we use `i` here instead of `currArrayIdx`
                //             because array fields descriptors are only
                //             stored once in a given layout.
                result.fieldAccessPath.insert(0, layout.rootLayout.fieldNames[i]);
                result.type = layout.rootLayout.typeMap[i];
                result.lastArrayIdx = currArrayIdx;

                return true;
            }

            // note(amos): getting offsets to dynamic arrays items outside the
            //             game's runtime is not supported! Only static arrays.
            if (isStaticArray)
            {
                const SettingsLayoutAsset_s& subLayout = layout.subLayouts[layout.rootLayout.indexMap[i]];
                result.currentBase = IALIGN(absoluteFieldOffset, subLayout.rootLayout.alignment);

                // Recurse into sub-layout for array elements.
                if (SettingsFieldFinder_FindFieldByAbsoluteOffset(subLayout, targetOffset, result))
                {
                    result.fieldAccessPath.insert(0, Utils::VFormat("%s[%i].", layout.rootLayout.fieldNames[i].c_str(), result.lastArrayIdx));
                    result.lastArrayIdx = currArrayIdx;

                    return true;
                }

                result.currentBase = originalBase;
            }
        }
    }

    // Not found.
    return false;
}

bool SettingsFieldFinder_FindFieldByAbsoluteName(const SettingsLayoutAsset_s& layout, const char* const targetName, SettingsLayoutFindByNameResult_s& result)
{
    const char* lookupName = targetName;
    const char* nextLookup = nullptr;

    std::string lookupCapture;

    const char* const brackBegin = strchr(targetName, '[');
    int lookupLayoutIndex = 0;

    // If we have a bracket, parse the subscript out and recursively process
    // the rest. I.e. we have "itemNames[2].flavorDesc[1].featureFlags", then
    // only "itemNames[2]." gets consumed here, the rest is tokenized as one
    // solid string and consumed in the subsequent recursive calls.
    if (brackBegin)
    {
        if (brackBegin == targetName)
        {
            Error("%s: expected an identifier before '[' token in \"%s\".\n", __FUNCTION__, targetName);
            return false;
        }

        const char* const numStart = brackBegin + 1;
        const char* const brackEnd = strchr(numStart, ']');

        if (!brackEnd)
        {
            Error("%s: expected a ']' token after expression in \"%s\".\n", __FUNCTION__, targetName);
            return false;
        }

        if (brackEnd == numStart)
        {
            Error("%s: expected an expression inside array subscript operator in \"%s\".\n", __FUNCTION__, targetName);
            return false;
        }

        char* endptr;
        lookupLayoutIndex = strtol(numStart, &endptr, 0);

        if (endptr != brackEnd)
        {
            Error("%s: failed to parse expression inside array subscript operator in \"%s\".\n", __FUNCTION__, targetName);
            return false;
        }

        if (brackEnd[1] != '.')
        {
            Error("%s: expected a '.' token after array subscript operator in \"%s\".\n", __FUNCTION__, targetName);
            return false;
        }

        if (brackEnd[2] == '\0')
        {
            Error("%s: expected an identifier after member access operator in \"%s\".\n", __FUNCTION__, targetName);
            return false;
        }

        nextLookup = &brackEnd[2];

        lookupCapture.assign(targetName, brackBegin);
        lookupName = lookupCapture.c_str();
    }

    // Look the field name up.
    for (size_t i = 0; i < layout.rootLayout.fieldNames.size(); i++)
    {
        const std::string& currFieldName = layout.rootLayout.fieldNames[i];

        if (currFieldName.compare(lookupName) != 0)
            continue;

        const SettingsFieldType_e currFieldType = layout.rootLayout.typeMap[i];
        const uint32_t fieldOffset = layout.rootLayout.offsetMap[i];

        if (brackBegin)
        {
            if (currFieldType == SettingsFieldType_e::ST_DynamicArray)
            {
                Error("%s: field \"%s\" is a dynamic array; dynamic array lookup can only be done at runtime.\n",
                    __FUNCTION__, lookupName);
                return false;
            }

            if (currFieldType != SettingsFieldType_e::ST_StaticArray)
            {
                Error("%s: field \"%s\" is of type %s, but an array subscript was used.\n",
                    __FUNCTION__, lookupName, s_settingsFieldTypeNames[currFieldType]);
                return false;
            }

            const SettingsLayoutAsset_s& subLayout = layout.subLayouts[layout.rootLayout.indexMap[i]];
            const int arrayElemCount = subLayout.rootLayout.arrayElemCount;

            if (lookupLayoutIndex < 0 || lookupLayoutIndex >= arrayElemCount)
            {
                Error("%s: field \"%s\" is an array with range interval [0,%d), but provided array subscript was %d.\n",
                    __FUNCTION__, lookupName, arrayElemCount-1, lookupLayoutIndex);
                return false;
            }

            // note(amos): no alignment needed on `fieldOffset` because it will
            //             already be on an offset aligned to its root alignment.
            const uint32_t totalValueBufSizeAligned = IALIGN(subLayout.rootLayout.totalValueBufferSize, subLayout.rootLayout.alignment);
            const uint32_t accum = fieldOffset + (lookupLayoutIndex * totalValueBufSizeAligned);

            result.currentBase += accum;

            // Look into the array.
            if (SettingsFieldFinder_FindFieldByAbsoluteName(subLayout, nextLookup, result))
                return true;

            result.currentBase -= accum;
        }

        result.valueOffset = result.currentBase + fieldOffset;
        result.type = currFieldType;

        return true;
    }

    // Not found.
    return false;
}

static uint32_t SettingsFieldFinder_HashFieldName(const char* const name, const uint32_t stepScale, const uint32_t seed)
{
    uint32_t hash = 0;
    uint32_t it;
    char lastChar = *name;
    char store;

    for (const char* curChar = name; *curChar; hash = seed + it + store)
    {
        ++curChar;
        it = hash * stepScale;
        store = lastChar;
        lastChar = *curChar;
    }

    return hash ^ (hash >> 4);
}

static uint32_t SettingsFieldFinder_GetFieldNameBucket(const char* const name, const uint32_t stepScale, const uint32_t seed, const uint32_t tableSize)
{
    const uint32_t hash = SettingsFieldFinder_HashFieldName(name, stepScale, seed);
    const uint32_t bucketMask = tableSize - 1;

    return hash & bucketMask;
}

static void SettingsLayout_ComputeHashParameters(SettingsLayoutParseResult_s& result)
{
    const size_t numFields = result.fieldNames.size();

    const uint32_t numBuckets = static_cast<uint32_t>(NextPowerOfTwo(numFields + 1));
    const uint32_t bucketMask = numBuckets - 1;

    uint32_t bestStepScale = 1;
    uint32_t bestSeed = 0;
    uint32_t minCollisions = UINT32_MAX; // Track the minimum number of collisions.

    std::vector<uint32_t> bestBucketMap(numFields, 0);

    for (uint32_t retryCount = 0; retryCount < SETTINGS_LAYOUT_MAX_HASH_RETRIES; retryCount++)
    {
        const uint32_t currentStepScale = retryCount * 2 + 1;
        const uint32_t currentSeed = retryCount;

        std::vector<bool> collisionVec(numBuckets, false);
        std::vector<uint32_t> bucketMap(numFields, 0);

        uint32_t currentCollisions = 0;

        for (size_t i = 0; i < numFields; i++)
        {
            const std::string& fieldName = result.fieldNames[i];
            const uint32_t bucket = SettingsFieldFinder_GetFieldNameBucket(fieldName.c_str(), currentStepScale, currentSeed, numBuckets);

            if (collisionVec[bucket])
            {
                currentCollisions++;

                // Note(amos): collisions are expected, the game does linear
                // probing to minimize the number of string comparisons while
                // trying to solve these. We must place our field contiguously
                // after the colliding bucket index, at the first free bucket
                // as the lookup in SettingsFieldFinder_FindFieldByName() stops
                // when it encounters an empty bucket.
                for (size_t j = 1; j < numBuckets; j++)
                {
                    const uint32_t curBucket = (bucket+j) & bucketMask;

                    if (collisionVec[curBucket])
                        continue;

                    collisionVec[curBucket] = true;
                    bucketMap[i] = curBucket;

                    break;
                }
            }
            else
            {
                collisionVec[bucket] = true;
                bucketMap[i] = bucket;
            }
        }

        // Check if this configuration is better than the previous best.
        if (currentCollisions < minCollisions)
        {
            minCollisions = currentCollisions;
            bestStepScale = currentStepScale;
            bestSeed = currentSeed;

            bestBucketMap = bucketMap;

            if (minCollisions == 0)
                break; // No collisions at all, stop early.
        }
    }

    result.bucketMap = std::move(bestBucketMap);
    result.hashTableSize = numBuckets;
    result.hashStepScale = bestStepScale;
    result.hashSeed = bestSeed;
}

static void SettingsLayout_ComputeHashParametersRecursive(SettingsLayoutAsset_s& layoutAsset)
{
    SettingsLayout_ComputeHashParameters(layoutAsset.rootLayout);

    for (SettingsLayoutAsset_s& subLayout : layoutAsset.subLayouts)
        SettingsLayout_ComputeHashParametersRecursive(subLayout);
}

static void SettingsLayout_ParseTable(CPakFileBuilder* const pak, const char* const assetPath, SettingsLayoutParseResult_s& result)
{
    const std::string settingsLayoutFile = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".csv");
    std::ifstream datatableStream(settingsLayoutFile);

    if (!datatableStream.is_open())
        Error("Failed to open settings layout table \"%s\".\n", settingsLayoutFile.c_str());

    rapidcsv::Document document(datatableStream);
    const size_t columnCount = document.GetColumnCount();

    // Rows: fieldName, dataType, layoutIndex, helpText.
    constexpr size_t expectedColumnCount = 4;

    if (columnCount != expectedColumnCount)
        Error("Settings layout table \"%s\" has %zu columns, but %zu were expected.\n", settingsLayoutFile.c_str(), columnCount, expectedColumnCount);

    result.fieldNames = document.GetColumn<std::string>(0);
    const size_t numFieldNames = result.fieldNames.size();

    if (!numFieldNames)
        Error("Settings layout table \"%s\" is empty.\n", settingsLayoutFile.c_str());

    result.typeNames = document.GetColumn<std::string>(1);
    result.indexMap = document.GetColumn<uint32_t>(2);
    result.helpTexts = document.GetColumn<std::string>(3);

    const size_t numTypeNames = result.typeNames.size();
    const size_t numIndices = result.indexMap.size();
    const size_t numHelpTexts = result.helpTexts.size();

    if (numFieldNames != numTypeNames || numTypeNames != numIndices || numIndices != numHelpTexts)
    {
        Error("Settings layout table \"%s\" has columns with mismatching row counts (%zu != %zu || %zu != %zu || %zu != %zu).\n",
            settingsLayoutFile.c_str(), numFieldNames, numTypeNames, numTypeNames, numIndices, numIndices, numHelpTexts);
    }

    result.typeMap.resize(numTypeNames);

    uint32_t lastUsedSublayout = 0;
    uint32_t numSubLayouts = 0;

    for (size_t i = 0; i < numTypeNames; i++)
    {
        const std::string& typeName = result.typeNames[i];
        const SettingsFieldType_e typeToUse = SettingsLayout_GetFieldTypeForString(typeName.c_str());

        if (typeToUse == SettingsFieldType_e::ST_Invalid)
        {
            const std::string& fieldName = result.fieldNames[i];
            Error("Settings layout table \"%s\" contains field \"%s\" that is using an unknown type \"%s\".\n",
                settingsLayoutFile.c_str(), fieldName.c_str(), typeName.c_str());
        }

        result.typeMap[i] = typeToUse;

        if (typeToUse == SettingsFieldType_e::ST_StaticArray || 
            typeToUse == SettingsFieldType_e::ST_DynamicArray)
        {
            const uint32_t curLayoutIndex = result.indexMap[i];

            if (numSubLayouts && curLayoutIndex <= lastUsedSublayout)
            {
                Error("Settings layout table \"%s\" contains field \"%s\" that is mapped to sub-layout #%u, but this index has already been used.\n",
                    settingsLayoutFile.c_str(), result.fieldNames[i].c_str(), curLayoutIndex);
            }

            lastUsedSublayout = numSubLayouts++;
        }
    }

    result.subLayoutCount = numSubLayouts;
    result.highestSubLayoutIndex = numSubLayouts;
}

static void SettingsLayout_ParseMap(CPakFileBuilder* const pak, const char* const assetPath, SettingsLayoutAsset_s& asset)
{
    const std::string settingsLayoutFile = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".json");
    rapidjson::Document document;

    if (!JSON_ParseFromFile(settingsLayoutFile.c_str(), "settings layout", document, true))
        Error("Failed to open settings layout \"%s\".\n", settingsLayoutFile.c_str());

    SettingsLayoutParseResult_s& rootParseResult = asset.rootLayout;

    rootParseResult.arrayElemCount = max(JSON_GetValueOrDefault(document, "elementCount", 1u), 1u);
    rootParseResult.extraDataSizeIndex = JSON_GetValueRequired<uint32_t>(document, "extraDataSizeIndex");

    // Parse the root layout and figure out what the highest sub-layout index is.
    SettingsLayout_ParseTable(pak, assetPath, rootParseResult);

    if (rootParseResult.subLayoutCount > 0)
    {
        rapidjson::Document::ConstMemberIterator subLayoutsIt;
        JSON_GetRequired(document, "subLayouts", JSONFieldType_e::kArray, subLayoutsIt);

        const rapidjson::Value::ConstArray& subLayoutArray = subLayoutsIt->value.GetArray();
        const uint32_t numSubLayouts = static_cast<uint32_t>(subLayoutArray.Size());

        if (rootParseResult.subLayoutCount != numSubLayouts)
        {
            Error("Settings layout \"%s\" requested %u sub-layouts, but only %u were listed.\n",
                settingsLayoutFile.c_str(), rootParseResult.highestSubLayoutIndex, numSubLayouts);
        }

        for (uint32_t i = 0; i < numSubLayouts; i++)
        {
            const rapidjson::Value& subPath = subLayoutArray[i];
            
            if (!subPath.IsString())
            {
                Error("Settings layout \"%s\" contains a sub-layout at index #%u that is not of type %s.\n",
                    settingsLayoutFile.c_str(), i, JSON_TypeToString(JSONFieldType_e::kString));
            }

            SettingsLayoutAsset_s& subAsset = asset.subLayouts.emplace_back();
            SettingsLayout_ParseMap(pak, subPath.GetString(), subAsset);
        }
    }

    const uint32_t parsedSubLayouts = static_cast<uint32_t>(asset.subLayouts.size());

    if (rootParseResult.subLayoutCount != parsedSubLayouts)
    {
        Error("Settings layout \"%s\" listed %u sub-layouts, but only %u were parsed.\n",
            settingsLayoutFile.c_str(), rootParseResult.subLayoutCount, parsedSubLayouts);
    }
}

static void SettingsLayout_BuildOffsetMap(SettingsLayoutAsset_s& layoutAsset)
{
    // Here we need to parse everything in the hierarchy first, starting at the
    // deepest object. If we have static arrays we need to know the size before
    // we build out the root offset map.
    for (SettingsLayoutAsset_s& subLayout : layoutAsset.subLayouts)
        SettingsLayout_BuildOffsetMap(subLayout);

    SettingsLayoutParseResult_s& root = layoutAsset.rootLayout;

    const size_t numFields = root.fieldNames.size();
    root.offsetMap.resize(numFields);

    // Padding is only allowed on static arrays to make sure we align the next
    // field to its boundary, since the only alternative is to force the user
    // to pad it out manually.
    bool verifyPadding = true;
    uint32_t lastFieldAlign = 0;

    for (size_t i = 0; i < numFields; i++)
    {
        const SettingsFieldType_e typeToUse = root.typeMap[i];
        uint32_t typeSize = 0;

        if (typeToUse == SettingsFieldType_e::ST_StaticArray)
        {
            const uint32_t subLayoutIndex = root.indexMap[i];

            SettingsLayoutAsset_s& subLayout = layoutAsset.subLayouts[subLayoutIndex];
            SettingsLayoutParseResult_s& subRoot = subLayout.rootLayout;

            // If we have sub-layouts in our static array, the root of our
            // current sub-layout must be aligned to the nested sub-layout
            // with the highest alignment.
            for (const SettingsLayoutAsset_s& nestedSubLayouts : subLayout.subLayouts)
            {
                if (nestedSubLayouts.rootLayout.alignment > subRoot.alignment)
                    subRoot.alignment = nestedSubLayouts.rootLayout.alignment;
            }

            subRoot.totalValueBufferSize = IALIGN(subRoot.totalValueBufferSize, subRoot.alignment);
            typeSize = subRoot.arrayElemCount * subRoot.totalValueBufferSize;

            verifyPadding = false;
        }
        else
        {
            // At this stage we know what is dynamic and what isn't, dynamic
            // arrays must have this set to -1 as its size is determined by
            // the SettingsDynamicArray_s::size.
            if (typeToUse == SettingsFieldType_e::ST_DynamicArray)
            {
                const uint32_t subLayoutIndex = root.indexMap[i];
                SettingsLayoutParseResult_s& sub = layoutAsset.subLayouts[subLayoutIndex].rootLayout;

                sub.arrayElemCount = -1;
                sub.totalValueBufferSize = IALIGN(sub.totalValueBufferSize, sub.alignment);
            }

            const uint32_t curTypeAlign = SettingsLayout_GetFieldAlignmentForType(typeToUse);

            if (curTypeAlign > root.alignment)
                root.alignment = curTypeAlign;

            if (!verifyPadding)
            {
                root.totalValueBufferSize = IALIGN(root.totalValueBufferSize, curTypeAlign);
                verifyPadding = true;
            }
            else
            {
                // All fields in the settings layout must be sorted by their alignments.
                // Fields with higher alignments must come first as to avoid padding which
                // the original assets do not support on non-static arrays, so we follow
                // the same scheme here.
                if (lastFieldAlign > 0 && curTypeAlign > lastFieldAlign)
                {
                    Error("Settings layout field \"%s\" is of type %s which has an alignment of %u, but the previous field was aligned to %u; padding is not allowed.\n",
                        root.fieldNames[i].c_str(), s_settingsFieldTypeNames[typeToUse], curTypeAlign, lastFieldAlign);
                }
            }

            lastFieldAlign = curTypeAlign;
            typeSize = SettingsLayout_GetFieldSizeForType(typeToUse);
        }

        root.offsetMap[i] = root.totalValueBufferSize;
        root.totalValueBufferSize += typeSize;
    }
}

void SettingsLayout_ParseLayout(CPakFileBuilder* const pak, const char* const assetPath, SettingsLayoutAsset_s& layoutAsset)
{
    SettingsLayout_ParseMap(pak, assetPath, layoutAsset);
    SettingsLayout_BuildOffsetMap(layoutAsset);
}

static void SettingsLayout_InitializeHeader(SettingsLayoutHeader_s* const header, const SettingsLayoutParseResult_s& parse)
{
    header->hashTableSize = parse.hashTableSize;
    header->fieldCount = static_cast<uint32_t>(parse.fieldNames.size());
    header->extraDataSizeIndex = parse.extraDataSizeIndex;
    header->hashStepScale = parse.hashStepScale;
    header->hashSeed = parse.hashSeed;

    header->arrayElemCount = parse.arrayElemCount;
    header->layoutSize = parse.totalValueBufferSize;
}

struct SettingsLayoutMemory_s
{
    inline size_t GetTotalBufferSize() const
    {
        return outFieldBufSize + outSubHeadersBufSize + outFieldMapSize + outStringBufLen;
    }

    inline void InitCurrentIndices()
    {
        curFieldBufIndex = 0;
        subHeadersBufIndex = curFieldBufIndex + outFieldBufSize;
        curFieldMapIndex = subHeadersBufIndex + outSubHeadersBufSize;
        curStringBufIndex = curFieldMapIndex + outFieldMapSize;
    }

    size_t outFieldBufSize;
    size_t curFieldBufIndex;

    size_t outSubHeadersBufSize;
    size_t subHeadersBufIndex;

    size_t outFieldMapSize;
    size_t curFieldMapIndex;

    size_t outStringBufLen;
    size_t curStringBufIndex;
};

static void SettingsLayout_CalculateBufferSizes(SettingsLayoutAsset_s& layoutAsset, SettingsLayoutMemory_s& layoutMemory, size_t& bufBaseIndexer, const bool isInitialRoot)
{
    SettingsLayoutParseResult_s& rootParseResult = layoutAsset.rootLayout;

    layoutMemory.outFieldBufSize += (rootParseResult.hashTableSize * sizeof(SettingsField_s));

    // Note(amos): the root asset header is stored in a HEAD page, only all
    // the sub headers are stored in the CPU page. Don't count the root in.
    if (!isInitialRoot)
        layoutMemory.outSubHeadersBufSize += sizeof(SettingsLayoutHeader_s);

    layoutMemory.outFieldMapSize += rootParseResult.fieldNames.size() * sizeof(SettingsFieldMap_s);

    for (const std::string& fieldName : rootParseResult.fieldNames)
        layoutMemory.outStringBufLen += fieldName.size() + 1;

    for (const std::string& debugText : rootParseResult.helpTexts)
        layoutMemory.outStringBufLen += debugText.size() + 1;

    // Note(amos): string buf len must be total + 1 per settings layout because
    // the first field to be written in the settings layout will have the
    // smallest offset into the string buffer, but its offset cannot be 0 as
    // the runtime uses that to check if the field isn't a NULL field in the
    // function SettingsFieldFinder_FindFieldByName(). So the first byte in the
    // string buffer must be a placeholder byte to offset the first field name
    // entry to make it non-null.
    layoutMemory.outStringBufLen += 1;

    const size_t subLayoutCount = layoutAsset.subLayouts.size();

    if (subLayoutCount > 0)
    {
        rootParseResult.subHeadersBufBase = bufBaseIndexer;
        bufBaseIndexer += layoutAsset.subLayouts.size() * sizeof(SettingsLayoutHeader_s);

        for (SettingsLayoutAsset_s& subLayout : layoutAsset.subLayouts)
            SettingsLayout_CalculateBufferSizes(subLayout, layoutMemory, bufBaseIndexer, false);
    }
}

static void SettingsLayout_WriteFieldData(PakPageLump_s& dataLump, const SettingsLayoutParseResult_s& parse, size_t& numStringBytesWritten, SettingsLayoutMemory_s& layoutMemory)
{
    // +1 for string place holder, see comment in SettingsLayout_CalculateBufferSizes().
    layoutMemory.curStringBufIndex += 1;

    const size_t numFields = parse.fieldNames.size();
    const bool hasSubLayouts = parse.indexMap.size() > 0;

    for (size_t i = 0; i < numFields; i++)
    {
        const uint32_t localBucket = parse.bucketMap[i];
        const uint32_t bucketIndex = (localBucket * sizeof(SettingsField_s));
        SettingsField_s* const field = reinterpret_cast<SettingsField_s*>(&dataLump.data[layoutMemory.curFieldBufIndex + bucketIndex]);

        field->type = parse.typeMap[i];
        field->nameOffset = static_cast<uint16_t>(numStringBytesWritten);
        field->valueOffset = parse.offsetMap[i];
        field->subLayoutIndex = hasSubLayouts ? parse.indexMap[i] : 0;

        const std::string& fieldName = parse.fieldNames[i];
        const size_t fieldNameLen = fieldName.size() + 1;

        memcpy(&dataLump.data[layoutMemory.curStringBufIndex], fieldName.c_str(), fieldNameLen);

        layoutMemory.curStringBufIndex += fieldNameLen;
        numStringBytesWritten += fieldNameLen;

        const size_t mapIndex = i * sizeof(SettingsFieldMap_s);
        SettingsFieldMap_s* const map = reinterpret_cast<SettingsFieldMap_s*>(&dataLump.data[layoutMemory.curFieldMapIndex + mapIndex]);

        map->fieldBucketIndex = static_cast<uint16_t>(localBucket);
        map->debugTextIndex = static_cast<uint16_t>(numStringBytesWritten);

        const std::string& debugText = parse.helpTexts[i];
        const size_t debugTextLen = debugText.size() + 1;

        memcpy(&dataLump.data[layoutMemory.curStringBufIndex], debugText.c_str(), debugTextLen);

        layoutMemory.curStringBufIndex += debugTextLen;
        numStringBytesWritten += debugTextLen;
    }

    layoutMemory.curFieldBufIndex += parse.hashTableSize * sizeof(SettingsField_s);
    layoutMemory.curFieldMapIndex += parse.fieldNames.size() * sizeof(SettingsFieldMap_s);
}

static void SettingsLayout_WriteLayoutRecursive(CPakFileBuilder* const pak, SettingsLayoutAsset_s& layoutAsset,
    SettingsLayoutMemory_s& layoutMemory, PakPageLump_s& dataLump, SettingsLayoutParseResult_s* const parent)
{
    SettingsLayoutParseResult_s& parse = layoutAsset.rootLayout;

    if (parent)
    {
        const size_t rootIndex = layoutMemory.subHeadersBufIndex + parent->subHeadersBufBase + parent->curSubHeaderBufIndex;
        SettingsLayoutHeader_s* const header = reinterpret_cast<SettingsLayoutHeader_s*>(&dataLump.data[rootIndex]);

        SettingsLayout_InitializeHeader(header, parse);

        pak->AddPointer(dataLump, rootIndex + offsetof(SettingsLayoutHeader_s, fieldData), dataLump, layoutMemory.curFieldBufIndex);
        pak->AddPointer(dataLump, rootIndex + offsetof(SettingsLayoutHeader_s, fieldMap), dataLump, layoutMemory.curFieldMapIndex);
        pak->AddPointer(dataLump, rootIndex + offsetof(SettingsLayoutHeader_s, fieldNames), dataLump, layoutMemory.curStringBufIndex);

        parent->curSubHeaderBufIndex += sizeof(SettingsLayoutHeader_s);

        if (!layoutAsset.subLayouts.empty())
        {
            const size_t subIndex = layoutMemory.subHeadersBufIndex + parse.subHeadersBufBase + parse.curSubHeaderBufIndex;
            pak->AddPointer(dataLump, rootIndex + offsetof(SettingsLayoutHeader_s, subHeaders), dataLump, subIndex);
        }
    }

    // +1 for string place holder, see comment in SettingsLayout_CalculateBufferSizes().
    size_t numRootStringBufBytes = 1;
    SettingsLayout_WriteFieldData(dataLump, parse, numRootStringBufBytes, layoutMemory);

    for (SettingsLayoutAsset_s& subLayout : layoutAsset.subLayouts)
        SettingsLayout_WriteLayoutRecursive(pak, subLayout, layoutMemory, dataLump, &parse);
}

static void SettingsLayout_InternalAddLayoutAsset(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
    SettingsLayoutAsset_s layoutAsset;
    SettingsLayout_ParseLayout(pak, assetPath, layoutAsset);

    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
    PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(SettingsLayoutHeader_s), SF_HEAD, 8);

    SettingsLayout_ComputeHashParametersRecursive(layoutAsset);

    SettingsLayoutMemory_s layoutMemory{};
    size_t headersBufIndexer = 0;

    SettingsLayout_CalculateBufferSizes(layoutAsset, layoutMemory, headersBufIndexer, true);

    // The asset name is only stored for the root layout.
    const size_t assetNameBufLen = strlen(assetPath) + 1;
    layoutMemory.outStringBufLen += assetNameBufLen;

    layoutMemory.InitCurrentIndices();
    const size_t totalPageSize = layoutMemory.GetTotalBufferSize();

    PakPageLump_s dataLump = pak->CreatePageLump(totalPageSize, SF_CPU, 8);
    const size_t assetNameOffset = totalPageSize - assetNameBufLen;

    memcpy(&dataLump.data[assetNameOffset], assetPath, assetNameBufLen);

    SettingsLayoutHeader_s* const layoutHeader = reinterpret_cast<SettingsLayoutHeader_s*>(hdrLump.data);
    SettingsLayout_InitializeHeader(layoutHeader, layoutAsset.rootLayout);

    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, name), dataLump, assetNameOffset);
    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, fieldData), dataLump, layoutMemory.curFieldBufIndex);
    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, fieldMap), dataLump, layoutMemory.curFieldMapIndex);
    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, fieldNames), dataLump, layoutMemory.curStringBufIndex);

    if (!layoutAsset.subLayouts.empty())
        pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, subHeaders), dataLump, layoutMemory.subHeadersBufIndex);

    SettingsLayout_WriteLayoutRecursive(pak, layoutAsset, layoutMemory, dataLump, nullptr);

    asset.InitAsset(hdrLump.GetPointer(), sizeof(SettingsLayoutHeader_s), PagePtr_t::NullPtr(), STLT_VERSION, AssetType::STLT);
    asset.SetHeaderPointer(hdrLump.data);

    pak->FinishAsset();
}

void Assets::AddSettingsLayout_v0(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    SettingsLayout_InternalAddLayoutAsset(pak, assetGuid, assetPath);
}
