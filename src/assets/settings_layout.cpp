#include "pch.h"
#include "assets.h"
#include "public/settings_layout.h"

//
// layout todo:
// - array elem count must be -1 for dynamic arrays?
//

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
        return sizeof(bool);
    case SettingsFieldType_e::ST_Int:
    case SettingsFieldType_e::ST_Float:
    case SettingsFieldType_e::ST_Float2:
    case SettingsFieldType_e::ST_Float3:
        return sizeof(float);
    case SettingsFieldType_e::ST_String:
    case SettingsFieldType_e::ST_Asset:
    case SettingsFieldType_e::ST_Asset_2:
    case SettingsFieldType_e::ST_StaticArray:
    case SettingsFieldType_e::ST_DynamicArray:
        return sizeof(void*);

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

void SettingsLayout_ParseLayout(CPakFileBuilder* const pak, const char* const assetPath, rapidcsv::Document& document, SettingsLayoutParseResult_s& result)
{
    const std::string settingsLayoutFile = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".csv");
    std::ifstream datatableStream(settingsLayoutFile);

    if (!datatableStream.is_open())
        Error("Failed to open settings layout table \"%s\".\n", settingsLayoutFile.c_str());

    document.ReadCsv(datatableStream);

    result.fieldNames = document.GetColumn<std::string>(0);
    const size_t numFieldNames = result.fieldNames.size();

    if (!numFieldNames)
        Error("Settings layout table \"%s\" is empty.\n", settingsLayoutFile.c_str());

    result.typeNames = document.GetColumn<std::string>(1);
    result.offsetMap = document.GetColumn<uint32_t>(2);
    result.indexMap = document.GetColumn<uint32_t>(3);

    const size_t numTypeNames = result.typeNames.size();
    const size_t numOffsets = result.offsetMap.size();
    const size_t numIndices = result.indexMap.size();

    if (numFieldNames != numTypeNames || numTypeNames != numOffsets || numOffsets != numIndices)
    {
        Error("Settings layout column count mismatch (%zu != %zu || %zu != %zu || %zu != %zu).\n",
            numFieldNames, numTypeNames, numTypeNames, numOffsets, numOffsets, numIndices);
    }

    result.typeMap.resize(numTypeNames);
    uint32_t lastFieldAlign = 0;

    uint32_t lastUsedSublayout = 0;
    uint32_t numSubLayouts = 0;

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

        if (typeToUse == SettingsFieldType_e::ST_StaticArray || 
            typeToUse == SettingsFieldType_e::ST_DynamicArray)
        {
            const uint32_t curLayoutIndex = result.indexMap[i];

            if (numSubLayouts && curLayoutIndex <= lastUsedSublayout)
            {
                Error("Settings layout field \"%s\" is mapped to sub-layout #%u, but the previous array was already using this index.\n",
                    result.fieldNames[i].c_str(), curLayoutIndex);
            }

            lastUsedSublayout = numSubLayouts++;
        }
    }

    result.subLayoutCount = numSubLayouts;
    result.highestSubLayoutIndex = numSubLayouts;

    // Get the total layout value buffer size, and make sure we don't have any
    // overlapping fields.
    uint32_t nextOffset = 0;

    for (size_t i = 0; i < numOffsets; i++)
    {
        const uint32_t curOffset = result.offsetMap[i];

        if (curOffset < nextOffset)
        {
            const std::string& fieldName = result.fieldNames[i];
            Error("Settings layout field \"%s\" has an offset that overlaps (%u < %u).\n", fieldName.c_str(), curOffset, nextOffset);
        }

        nextOffset = curOffset + SettingsLayout_GetFieldSizeForType(result.typeMap[i]);
    }

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

    SettingsLayoutParseResult_s& rootParseResult = asset.rootLayout;

    rootParseResult.totalValueBufferSize = JSON_GetValueRequired<uint32_t>(document, "size");
    rootParseResult.arrayElemCount = JSON_GetValueRequired<uint32_t>(document, "elementCount");

    // Parse the root layout and figure out what the highest sub-layout index is.
    rapidcsv::Document rootLayoutTable;

    SettingsLayout_ParseLayout(pak, assetPath, rootLayoutTable, rootParseResult);
    SettingsLayout_ValidateBufferUsage(rootParseResult);

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
        return outFieldBufSize + outSubHeadersBufSize + outStringBufLen;
    }

    inline void InitCurrentIndices()
    {
        curFieldBufIndex = 0;
        curSubHeadersBufIndex = outFieldBufSize;
        curStringBufIndex = outFieldBufSize + outSubHeadersBufSize;
    }

    size_t outFieldBufSize;
    size_t outSubHeadersBufSize;
    size_t outStringBufLen;

    size_t curFieldBufIndex;
    size_t curSubHeadersBufIndex;
    size_t curStringBufIndex;
};

static void SettingsLayout_CalculateBufferSizes(const SettingsLayoutAsset_s& layoutAsset, SettingsLayoutMemory_s& layoutMemory, const bool isInitialRoot)
{
    const SettingsLayoutParseResult_s& rootParseResult = layoutAsset.rootLayout;

    layoutMemory.outFieldBufSize += (rootParseResult.hashTableSize * sizeof(SettingsField_s));

    // Note(amos): the root asset header is stored in a HEAD page, only all
    // the sub headers are stored in the CPU page. Don't count the root in.
    if (!isInitialRoot)
        layoutMemory.outSubHeadersBufSize += sizeof(SettingsLayoutHeader_s);

    for (const std::string& fieldName : rootParseResult.fieldNames)
        layoutMemory.outStringBufLen += fieldName.size() + 1;

    // Note(amos): string buf len must be total + 1 per settings layout because
    // the first field to be written in the settings layout will have the
    // smallest offset into the string buffer, but its offset cannot be 0 as
    // the runtime uses that to check if the field isn't a NULL field in the
    // function SettingsFieldFinder_FindFieldByName(). So the first byte in the
    // string buffer must be a placeholder byte to offset the first field name
    // entry to make it non-null.
    layoutMemory.outStringBufLen += 1;

    for (const SettingsLayoutAsset_s& subLayout : layoutAsset.subLayouts)
        SettingsLayout_CalculateBufferSizes(subLayout, layoutMemory, false);
}

static void SettingsLayout_WriteFieldData(PakPageLump_s& dataLump, const SettingsLayoutParseResult_s& parse, size_t& numStringBytesWritten, SettingsLayoutMemory_s& layoutMemory)
{
    // +1 for string place holder, see comment in SettingsLayout_CalculateBufferSizes().
    layoutMemory.curStringBufIndex += 1;

    const size_t numFields = parse.fieldNames.size();
    const bool hasSubLayouts = parse.indexMap.size() > 0;

    for (size_t i = 0; i < numFields; i++)
    {
        const uint32_t bucketIndex = (parse.bucketMap[i] * sizeof(SettingsField_s));
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
    }

    layoutMemory.curFieldBufIndex += parse.hashTableSize * sizeof(SettingsField_s);
}

static void SettingsLayout_WriteLayoutRecursive(CPakFileBuilder* const pak, const SettingsLayoutAsset_s& layoutAsset, 
    SettingsLayoutMemory_s& layoutMemory, PakPageLump_s& dataLump, const bool isInitialRoot)
{
    const SettingsLayoutParseResult_s& parse = layoutAsset.rootLayout;

    if (!isInitialRoot)
    {
        SettingsLayoutHeader_s* const header = reinterpret_cast<SettingsLayoutHeader_s*>(&dataLump.data[layoutMemory.curSubHeadersBufIndex]);
        SettingsLayout_InitializeHeader(header, parse);

        const size_t rootIndex = layoutMemory.curSubHeadersBufIndex;

        pak->AddPointer(dataLump, rootIndex + offsetof(SettingsLayoutHeader_s, fieldMap), dataLump, layoutMemory.curFieldBufIndex);
        pak->AddPointer(dataLump, rootIndex + offsetof(SettingsLayoutHeader_s, fieldNames), dataLump, layoutMemory.curStringBufIndex);

        // Increment by the header size, anything past this will either be the
        // sub-layouts if this layout has any, or the next layout outside this
        // scope.
        layoutMemory.curSubHeadersBufIndex += sizeof(SettingsLayoutHeader_s);

        if (!layoutAsset.subLayouts.empty())
            pak->AddPointer(dataLump, rootIndex + offsetof(SettingsLayoutHeader_s, subHeaders), dataLump, layoutMemory.curSubHeadersBufIndex);
    }

    // +1 for string place holder, see comment in SettingsLayout_CalculateBufferSizes().
    size_t numRootStringBufBytes = 1;
    SettingsLayout_WriteFieldData(dataLump, parse, numRootStringBufBytes, layoutMemory);

    for (const SettingsLayoutAsset_s& subLayout : layoutAsset.subLayouts)
        SettingsLayout_WriteLayoutRecursive(pak, subLayout, layoutMemory, dataLump, false);
}

static void SettingsLayout_InternalAddLayoutAsset(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
    SettingsLayoutAsset_s layoutAsset;
    SettingsLayout_ParseMap(pak, assetPath, layoutAsset);

    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
    PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(SettingsLayoutHeader_s), SF_HEAD, 8);

    SettingsLayout_ComputeHashParametersRecursive(layoutAsset);

    SettingsLayoutMemory_s layoutMemory{};
    SettingsLayout_CalculateBufferSizes(layoutAsset, layoutMemory, true);

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
    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, fieldMap), dataLump, layoutMemory.curFieldBufIndex);
    pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, fieldNames), dataLump, layoutMemory.curStringBufIndex);

    if (!layoutAsset.subLayouts.empty())
        pak->AddPointer(hdrLump, offsetof(SettingsLayoutHeader_s, subHeaders), dataLump, layoutMemory.curSubHeadersBufIndex);

    SettingsLayout_WriteLayoutRecursive(pak, layoutAsset, layoutMemory, dataLump, true);

    asset.InitAsset(hdrLump.GetPointer(), sizeof(SettingsLayoutHeader_s), PagePtr_t::NullPtr(), STLT_VERSION, AssetType::STLT);
    asset.SetHeaderPointer(hdrLump.data);

    pak->FinishAsset();
}

void Assets::AddSettingsLayout_v0(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    SettingsLayout_InternalAddLayoutAsset(pak, assetGuid, assetPath);
}
