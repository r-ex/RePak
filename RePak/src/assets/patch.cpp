#include "pch.h"
#include "Assets.h"

void Assets::AddPatchAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding Ptch asset '%s'\n", assetPath);

    PtchHeader* pHdr = new PtchHeader();

    pHdr->patchedPakCount = mapEntry["entries"].GetArray().Size();

    std::vector<PtchEntry> patchEntries{};
    uint32_t entryNamesSectionSize = 0;

    for (auto& it : mapEntry["entries"].GetArray())
    {
        std::string name = it["name"].GetStdString();
        uint8_t patchNum = it["patchnum"].GetInt();

        patchEntries.push_back({ name, patchNum, entryNamesSectionSize });

        entryNamesSectionSize += name.length() + 1;
    }

    size_t dataPageSize = (sizeof(RPakPtr) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + entryNamesSectionSize;

    RPakVirtualSegment SubHeaderPage;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(PtchHeader), 0, 8, SubHeaderPage);

    RPakVirtualSegment DataPage;
    uint32_t rdsIdx = RePak::CreateNewSegment(dataPageSize, 1, 8, DataPage);

    pHdr->pPakNames = { rdsIdx, 0 };
    pHdr->pPakPatchNums = { rdsIdx, (int)sizeof(RPakPtr) * pHdr->patchedPakCount };

    RePak::RegisterDescriptor(shsIdx, offsetof(PtchHeader, pPakNames));
    RePak::RegisterDescriptor(shsIdx, offsetof(PtchHeader, pPakPatchNums));

    char* pDataBuf = new char[dataPageSize];
    rmem dataBuf(pDataBuf);

    uint32_t i = 0;
    for (auto& it : patchEntries)
    {
        uint32_t fileNameOffset = (sizeof(RPakPtr) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + it.FileNamePageOffset;

        // write the ptr to the file name into the buffer
        dataBuf.write<RPakPtr>({ rdsIdx, fileNameOffset }, sizeof(RPakPtr) * i);
        // write the patch number for this entry into the buffer
        dataBuf.write<uint8_t>(it.PatchNum, pHdr->pPakPatchNums.Offset + i);

        snprintf(pDataBuf + fileNameOffset, it.FileName.length() + 1, "%s", it.FileName.c_str());

        RePak::RegisterDescriptor(rdsIdx, sizeof(RPakPtr) * i);
        i++;
    }

    RPakRawDataBlock shdb{ shsIdx, SubHeaderPage.DataSize, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ rdsIdx, dataPageSize, (uint8_t*)pDataBuf };
    RePak::AddRawDataBlock(rdb);

    // create and init the asset entry
    RPakAssetEntryV8 asset;

    asset.InitAsset(0x6fc6fa5ad8f8bc9c, shsIdx, 0, SubHeaderPage.DataSize, -1, 0, -1, -1, (std::uint32_t)AssetType::PTCH);
    asset.Version = 1;

    asset.HighestPageNum = rdsIdx + 1;
    asset.Un2 = 1;

    assetEntries->push_back(asset);
}