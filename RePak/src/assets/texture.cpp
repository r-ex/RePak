#include "pch.h"
#include "rmem.h"
#include "Assets.h"

void Assets::AddTextureAsset(std::vector<RPakAssetEntryV7>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding txtr asset '%s'\n", assetPath);


    std::string filePath = g_sAssetsDir + assetPath + ".dds";

    if (!FILE_EXISTS(filePath))
    {
        // this is a fatal error because if this asset is a dependency for another asset and we just ignore it
        // we will crash later when trying to reference it
        Error("Failed to find texture source file %s. Exiting...\n", filePath.c_str());
        exit(EXIT_FAILURE);
    }

    TextureHeader* hdr = new TextureHeader();

    BinaryIO input;
    input.open(filePath, BinaryIOMode::Read);

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
            Error("Attempted to add txtr asset '%s' that was not using a supported DDS type. Exiting...\n", assetPath);
            exit(EXIT_FAILURE);
            return;
        }

        hdr->format = (uint16_t)TxtrFormatMap[dxgiFormat];

        // go to the end of the main header
        input.seek(ddsh.size + 4);

        if (dxgiFormat == DXGI_FORMAT_BC7_UNORM || dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB)
            input.seek(20, std::ios::cur);
    }

    hdr->assetGuid = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    // unfortunately i'm not a respawn engineer so 1 (unstreamed) mip level will have to do
    hdr->permanentMipLevels = 1;

    bool bSaveDebugName = mapEntry.HasMember("saveDebugName") && mapEntry["saveDebugName"].GetBool();

    // asset header
    _vseginfo_t subhdrinfo = RePak::CreateNewSegment(sizeof(TextureHeader), 0, 8);

    _vseginfo_t nameseginfo{};

    char* namebuf = new char[sAssetName.size() + 1];

    if (bSaveDebugName)
    {
        sprintf_s(namebuf, sAssetName.length() + 1, "%s", sAssetName.c_str());
        nameseginfo = RePak::CreateNewSegment(sAssetName.size() + 1, 129, 1);
    }
    else
    {
        delete[] namebuf;
    }

    // woo more segments
    // cpu data
    _vseginfo_t dataseginfo = RePak::CreateNewSegment(hdr->dataLength, 3, 16);

    char* databuf = new char[hdr->dataLength];

    input.getReader()->read(databuf, hdr->dataLength);

    RePak::AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)hdr });

    if (bSaveDebugName)
    {
        RePak::AddRawDataBlock({ nameseginfo.index, nameseginfo.size, (uint8_t*)namebuf });
        hdr->pDebugName = { nameseginfo.index, 0 };

        RePak::RegisterDescriptor(nameseginfo.index, offsetof(TextureHeader, pDebugName));
    }

    RePak::AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)databuf });

    // now time to add the higher level asset entry
    RPakAssetEntryV7 asset;

    uint64_t starpakOffset = -1;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, starpakOffset, -1, (std::uint32_t)AssetType::TEXTURE);
    asset.m_nVersion = TXTR_VERSION;

    asset.m_nPageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 1;

    assetEntries->push_back(asset);

    input.close();
}