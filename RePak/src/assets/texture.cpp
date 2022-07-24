#include "pch.h"
#include "rmem.h"
#include "Assets.h"

void Assets::AddTextureAsset(std::vector<RPakAssetEntryV7>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding txtr asset '%s'\n", assetPath);


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

        hdr->m_nDataLength = ddsh.pitchOrLinearSize;
        hdr->m_nWidth = ddsh.width;
        hdr->m_nHeight = ddsh.height;
        Log("-> dimensions: %ix%i\n", ddsh.width, ddsh.height);

        DXGI_FORMAT dxgiFormat;

        // Checks if the texture is DX10+, this is needed for SRGB.
        if (ddsh.pixelfmt.fourCC == '01XD') {

            DDS_HEADER_DXT10 ddsh_dx10 = input.read<DDS_HEADER_DXT10>();

            switch (ddsh_dx10.dxgiFormat)
            {
            case 72:
                Log("-> fmt: BC1 SRGB\n");
                dxgiFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
                break;
            case 75:
                Log("-> fmt: BC2 SRGB\n");
                dxgiFormat = DXGI_FORMAT_BC2_UNORM_SRGB;
                break;
            case 78:
                Log("-> fmt: BC2 SRGB\n");
                dxgiFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
                break;
            case 98:
                Log("-> fmt: BC7\n");
                dxgiFormat = DXGI_FORMAT_BC7_UNORM;
                break;
            case 99:
                Log("-> fmt: BC7 SRGB\n");
                dxgiFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
                break;
            default:
                Error("Attempted to add txtr asset '%s' that was not using a supported DDS type. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
                break;
            }

        }
        // Non SRGB texture processing.
        else {

            switch (ddsh.pixelfmt.fourCC)
            {
            case '1TXD':
                Log("-> fmt: DXT1\n");
                dxgiFormat = DXGI_FORMAT_BC1_UNORM;
                break;
            case '3TXD':
                Log("-> fmt: DXT3\n");
                dxgiFormat = DXGI_FORMAT_BC2_UNORM;
                break;
            case '5TXD':
                Log("-> fmt: DXT5\n");
                dxgiFormat = DXGI_FORMAT_BC3_UNORM;
                break;
            case 'U4CB':
                Log("-> fmt: BC4U\n");
                dxgiFormat = DXGI_FORMAT_BC4_UNORM;
                break;
            case 'U5CB':
                Log("-> fmt: BC5U\n");
                dxgiFormat = DXGI_FORMAT_BC5_UNORM;
                break;
            case 'S5CB':
                Log("-> fmt: BC5U\n");
                dxgiFormat = DXGI_FORMAT_BC5_SNORM;
                break;
            default:
                Error("Attempted to add txtr asset '%s' that was not using a supported DDS type. Exiting...\n", assetPath);
                exit(EXIT_FAILURE);
                return;
            }

        }

        hdr->m_nFormat = s_txtrFormatMap[dxgiFormat];

        // Go to the end of the main header.
        input.seek(ddsh.size + 4);

        // Go to the end of the DX10 header if it exists.
        if (ddsh.pixelfmt.fourCC == '01XD')
            input.seek(20, std::ios::cur);
    }

    hdr->m_nAssetGUID = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    // unfortunately i'm not a respawn engineer so 1 (unstreamed) mip level will have to do
    hdr->m_nPermanentMipLevels = 1;

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
    _vseginfo_t dataseginfo = RePak::CreateNewSegment(hdr->m_nDataLength, 3, 16);

    char* databuf = new char[hdr->m_nDataLength];

    input.getReader()->read(databuf, hdr->m_nDataLength);

    RePak::AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)hdr });

    if (bSaveDebugName)
    {
        RePak::AddRawDataBlock({ nameseginfo.index, nameseginfo.size, (uint8_t*)namebuf });
        hdr->m_pDebugName = { nameseginfo.index, 0 };

        RePak::RegisterDescriptor(subhdrinfo.index, offsetof(TextureHeader, m_pDebugName));
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
