#include "pch.h"
#include "assets.h"

// only tested for apex, should be identical on tf2
void Assets::AddPatchAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding Ptch asset '%s'\n", assetPath);

    PtchHeader* pHdr = new PtchHeader();

    pHdr->patchedPakCount = mapEntry["entries"].GetArray().Size();

    std::vector<PtchEntry> patchEntries{};
    uint32_t entryNamesSectionSize = 0;

    for (auto& it : mapEntry["entries"].GetArray())
    {
        std::string name = it["name"].GetStdString();
        uint8_t patchNum = it["version"].GetInt();

        patchEntries.push_back({ name, patchNum, entryNamesSectionSize });

        entryNamesSectionSize += name.length() + 1;
    }

    size_t dataPageSize = (sizeof(PagePtr_t) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + entryNamesSectionSize;

    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(PtchHeader), SF_HEAD, 8);

    // data segment
    _vseginfo_t dataseginfo = pak->CreateNewSegment(dataPageSize, SF_CPU, 8);

    pHdr->pPakNames = { dataseginfo.index, 0 };
    pHdr->pPakPatchNums = { dataseginfo.index, (int)sizeof(PagePtr_t) * pHdr->patchedPakCount };

    pak->AddPointer(subhdrinfo.index, offsetof(PtchHeader, pPakNames));
    pak->AddPointer(subhdrinfo.index, offsetof(PtchHeader, pPakPatchNums));

    char* pDataBuf = new char[dataPageSize];
    rmem dataBuf(pDataBuf);

    uint32_t i = 0;
    for (auto& it : patchEntries)
    {
        uint32_t fileNameOffset = (sizeof(PagePtr_t) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + it.pakFileNameOffset;

        // write the ptr to the file name into the buffer
        dataBuf.write<PagePtr_t>({ dataseginfo.index, fileNameOffset }, sizeof(PagePtr_t) * i);
        // write the patch number for this entry into the buffer
        dataBuf.write<uint8_t>(it.highestPatchNum, pHdr->pPakPatchNums.offset + i);

        snprintf(pDataBuf + fileNameOffset, it.pakFileName.length() + 1, "%s", it.pakFileName.c_str());

        pak->AddPointer(dataseginfo.index, sizeof(PagePtr_t) * i);
        i++;
    }

    RPakRawDataBlock shdb{ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr };
    pak->AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ dataseginfo.index, dataPageSize, (uint8_t*)pDataBuf };
    pak->AddRawDataBlock(rdb);

    // create and init the asset entry
    PakAsset_t asset;

    // hardcoded guid because it's the only Ptch asset guid
    asset.InitAsset(0x6fc6fa5ad8f8bc9c, subhdrinfo.index, 0, subhdrinfo.size, -1, 0, -1, -1, (std::uint32_t)AssetType::PTCH);
    asset.version = 1;

    asset.pageEnd = dataseginfo.index + 1;
    asset.remainingDependencyCount = 1;

    assetEntries->push_back(asset);
}