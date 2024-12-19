#include "pch.h"
#include "assets.h"

// only tested for apex, should be identical on tf2
void Assets::AddPatchAsset(CPakFile* const pak, const PakGuid_t assetGuid, const char* assetPath, const rapidjson::Value& mapEntry)
{
    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(PatchAssetHeader_t), SF_HEAD, 8);

    PatchAssetHeader_t* const pHdr = reinterpret_cast<PatchAssetHeader_t*>(hdrChunk.Data());

    rapidjson::Value::ConstMemberIterator entryIt;
    JSON_GetRequired(mapEntry, "entries", JSONFieldType_e::kArray, entryIt);

    const rapidjson::Value::ConstArray entryArray = entryIt->value.GetArray();

    pHdr->unknown_1 = 0xFF;
    pHdr->patchedPakCount = (uint32_t)entryArray.Size();

    std::vector<PtchEntry> patchEntries;
    uint32_t entryNamesSectionSize = 0;

    for (const rapidjson::Value& entry : entryArray)
    {
        const char* const name = JSON_GetValueRequired<const char*>(entry, "name");
        const uint8_t patchNum = (uint8_t)JSON_GetValueRequired<int>(entry, "version");

        PtchEntry& patchEntry = patchEntries.emplace_back();

        patchEntry.pakFileName = name;
        patchEntry.highestPatchNum = patchNum;
        patchEntry.pakFileNameOffset = entryNamesSectionSize;

        entryNamesSectionSize += static_cast<uint32_t>(patchEntry.pakFileName.length() + 1);
    }

    const size_t dataPageSize = (sizeof(PagePtr_t) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + entryNamesSectionSize;
    CPakDataChunk dataChunk = pak->CreateDataChunk(dataPageSize, SF_CPU, 8);

    const int patchNumbersOffset = sizeof(PagePtr_t) * pHdr->patchedPakCount;

    pHdr->pPakNames = dataChunk.GetPointer();
    pHdr->pPakPatchNums = dataChunk.GetPointer(patchNumbersOffset);

    pak->AddPointer(hdrChunk.GetPointer(offsetof(PatchAssetHeader_t, pPakNames)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(PatchAssetHeader_t, pPakPatchNums)));

    rmem dataBuf(dataChunk.Data());

    uint32_t i = 0;
    for (const PtchEntry& it : patchEntries)
    {
        const int fileNameOffset = (sizeof(PagePtr_t) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + it.pakFileNameOffset;

        // write the ptr to the file name into the buffer
        dataBuf.write<PagePtr_t>(dataChunk.GetPointer(fileNameOffset), sizeof(PagePtr_t) * i);
        // write the patch number for this entry into the buffer
        dataBuf.write<uint8_t>(it.highestPatchNum, pHdr->pPakPatchNums.offset + i);

        snprintf(dataChunk.Data() + fileNameOffset, it.pakFileName.length() + 1, "%s", it.pakFileName.c_str());

        pak->AddPointer(dataChunk.GetPointer(sizeof(PagePtr_t) * i));
        i++;
    }

    // create and init the asset entry
    PakAsset_t asset;

    // NOTE: the only Ptch asset in the game has guid 0x6fc6fa5ad8f8bc9c
    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::PTCH);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = 1;
    asset.pageEnd = pak->GetNumPages();

    pak->PushAsset(asset);
}