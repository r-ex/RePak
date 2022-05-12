#include "pch.h"
#include "rmem.h"
#include "Assets.h"

void Assets::AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding txtr asset '%s'\n", assetPath);

    TextureHeader* hdr = new TextureHeader();

    std::string filePath = g_sAssetsDir + assetPath + ".dds";

    BinaryIO input;
    input.open(filePath, BinaryIOMode::BinaryIOMode_Read);

    uint64_t nInputFileSize = Utils::GetFileSize(filePath);

    std::string sAssetName = assetPath; // todo: this needs to be changed to the actual name

    // parse input image file
    {
        int magic;
        input.read(magic);

        if (magic != 0x20534444) // b'DDS '
        {
            Warning("Attempted to add txtr asset '%s' that was not a valid DDS file (invalid magic). Skipping asset...\n", assetPath);
            return;
        }

        DDS_HEADER ddsh = input.read<DDS_HEADER>();

        hdr->dataLength = ddsh.pitchOrLinearSize;
        hdr->width = ddsh.width;
        hdr->height = ddsh.height;

        DXGI_FORMAT dxgiFormat;

        switch (ddsh.pixelfmt.fourCC)
        {
        case '1TXD':
            Log("-> fmt: DXT1\n");
            dxgiFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
            break;
        case 'U4CB':
            Log("-> fmt: BC4U\n");
            dxgiFormat = DXGI_FORMAT_BC4_UNORM;
            break;
        case 'U5CB':
            Log("-> fmt: BC5U\n");
            dxgiFormat = DXGI_FORMAT_BC5_UNORM;
            break;
        case '01XD':
            Log("-> fmt: DX10\n");
            dxgiFormat = DXGI_FORMAT_BC7_UNORM;
            break;
        default:
            Warning("Attempted to add txtr asset '%s' that was not using a supported DDS type. Skipping asset...\n", assetPath);
            return;
        }

        hdr->format = (uint16_t)TxtrFormatMap[dxgiFormat];

        // go to the end of the main header
        input.seek(ddsh.size + 4);

        if (dxgiFormat == DXGI_FORMAT_BC7_UNORM || dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB)
            input.seek(20, std::ios::cur);
    }

    hdr->assetGuid = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    hdr->permanentMipLevels = 1;

    bool bSaveDebugName = mapEntry.HasMember("saveDebugName") && mapEntry["saveDebugName"].GetBool();

    // give us a segment to use for the subheader
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(TextureHeader), 0, 8, SubHeaderSegment);

    RPakVirtualSegment DebugNameSegment;
    char* namebuf = new char[sAssetName.size() + 1];
    uint32_t nsIdx = -1;

    if (bSaveDebugName)
    {
        sprintf_s(namebuf, sAssetName.length() + 1, "%s", sAssetName.c_str());
        nsIdx = RePak::CreateNewSegment(sAssetName.size() + 1, 129, 1, DebugNameSegment);
    }
    else {
        delete[] namebuf;
    }

    // woo more segments
    RPakVirtualSegment RawDataSegment;
    uint32_t rdsIdx = RePak::CreateNewSegment(hdr->dataLength, 3, 16, RawDataSegment);

    char* databuf = new char[hdr->dataLength];

    input.getReader()->read(databuf, hdr->dataLength);

    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    RePak::AddRawDataBlock(shdb);

    if (bSaveDebugName)
    {
        RPakRawDataBlock ndb{ nsIdx, DebugNameSegment.DataSize, (uint8_t*)namebuf };
        RePak::AddRawDataBlock(ndb);
        hdr->pDebugName = { nsIdx, 0 };
    }

    RPakRawDataBlock rdb{ rdsIdx, RawDataSegment.DataSize, (uint8_t*)databuf };
    RePak::AddRawDataBlock(rdb);

    // now time to add the higher level asset entry
    RPakAssetEntryV8 asset;

    uint64_t StarpakOffset = -1;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), shsIdx, 0, SubHeaderSegment.DataSize, rdsIdx, 0, StarpakOffset, -1, (std::uint32_t)AssetType::TEXTURE);
    asset.Version = TXTR_VERSION;

    asset.HighestPageNum = rdsIdx + 1; // number of the highest page that the asset references pageidx + 1
    asset.Un2 = 1;

    assetEntries->push_back(asset);

    input.close();
}