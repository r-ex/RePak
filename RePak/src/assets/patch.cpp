#include "pch.h"
#include "Assets.h"

// only tested for apex, should be identical on tf2
void Assets::AddPatchAsset(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
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

    size_t dataPageSize = (sizeof(RPakPtr) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + entryNamesSectionSize;

    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(PtchHeader), SF_HEAD, 8);

    // data segment
    _vseginfo_t dataseginfo = pak->CreateNewSegment(dataPageSize, SF_CPU, 8);

    pHdr->pPakNames = { dataseginfo.index, 0 };
    pHdr->pPakPatchNums = { dataseginfo.index, (int)sizeof(RPakPtr) * pHdr->patchedPakCount };

    pak->AddPointer(subhdrinfo.index, offsetof(PtchHeader, pPakNames));
    pak->AddPointer(subhdrinfo.index, offsetof(PtchHeader, pPakPatchNums));

    char* pDataBuf = new char[dataPageSize];
    rmem dataBuf(pDataBuf);

    uint32_t i = 0;
    for (auto& it : patchEntries)
    {
        uint32_t fileNameOffset = (sizeof(RPakPtr) * pHdr->patchedPakCount) + (sizeof(uint8_t) * pHdr->patchedPakCount) + it.FileNamePageOffset;

        // write the ptr to the file name into the buffer
        dataBuf.write<RPakPtr>({ dataseginfo.index, fileNameOffset }, sizeof(RPakPtr) * i);
        // write the patch number for this entry into the buffer
        dataBuf.write<uint8_t>(it.PatchNum, pHdr->pPakPatchNums.offset + i);

        snprintf(pDataBuf + fileNameOffset, it.FileName.length() + 1, "%s", it.FileName.c_str());

        pak->AddPointer(dataseginfo.index, sizeof(RPakPtr) * i);
        i++;
    }

    RPakRawDataBlock shdb{ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr };
    pak->AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ dataseginfo.index, dataPageSize, (uint8_t*)pDataBuf };
    pak->AddRawDataBlock(rdb);

    // create and init the asset entry
    RPakAssetEntry asset;

    // hardcoded guid because it's the only Ptch asset guid
    asset.InitAsset(0x6fc6fa5ad8f8bc9c, subhdrinfo.index, 0, subhdrinfo.size, -1, 0, -1, -1, (std::uint32_t)AssetType::PTCH);
    asset.version = 1;

    asset.pageEnd = dataseginfo.index + 1;
    asset.unk1 = 1;

    assetEntries->push_back(asset);
}