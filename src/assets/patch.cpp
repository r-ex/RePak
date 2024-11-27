#include "pch.h"
#include "assets.h"

// only tested for apex, should be identical on tf2
void Assets::AddPatchAsset(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding Ptch asset '%s'\n", assetPath);

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(PatchAssetHeader_t), SF_HEAD, 8);

    PatchAssetHeader_t* pHdr = reinterpret_cast<PatchAssetHeader_t*>(hdrChunk.Data());

    pHdr->unknown_1 = 0xFF;
    pHdr->patchedPakCount = mapEntry["entries"].GetArray().Size();

    std::vector<PtchEntry> patchEntries{};
    uint32_t entryNamesSectionSize = 0;

    for (auto& it : mapEntry["entries"].GetArray())
    {
        std::string name = it["name"].GetStdString();
        uint8_t patchNum = static_cast<uint8_t>(it["version"].GetInt());

        patchEntries.push_back({ name, patchNum, entryNamesSectionSize });

        entryNamesSectionSize += static_cast<uint32_t>(name.length() + 1);
    }

    size_t dataPageSize = (sizeof(PagePtr_t) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + entryNamesSectionSize;

    CPakDataChunk dataChunk = pak->CreateDataChunk(dataPageSize, SF_CPU, 8);

    int patchNumbersOffset = sizeof(PagePtr_t) * pHdr->patchedPakCount;
    pHdr->pPakNames = dataChunk.GetPointer();
    pHdr->pPakPatchNums = dataChunk.GetPointer(patchNumbersOffset);

    pak->AddPointer(hdrChunk.GetPointer(offsetof(PatchAssetHeader_t, pPakNames)));
    pak->AddPointer(hdrChunk.GetPointer(offsetof(PatchAssetHeader_t, pPakPatchNums)));

    rmem dataBuf(dataChunk.Data());

    uint32_t i = 0;
    for (auto& it : patchEntries)
    {
        int fileNameOffset = (sizeof(PagePtr_t) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + it.pakFileNameOffset;

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

    // hardcoded guid because it's the only Ptch asset guid

    asset.InitAsset(0x6fc6fa5ad8f8bc9c, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::PTCH);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = 1;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 1;

    pak->PushAsset(asset);
}