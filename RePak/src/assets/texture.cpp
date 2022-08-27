#include "pch.h"
#include "rmem.h"
#include "Assets.h"
#include "dxutils.h"

void Assets::AddTextureAsset_v8(std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding txtr asset '%s'\n", assetPath);

    std::string filePath = g_sAssetsDir + assetPath + ".dds";

    if (!FILE_EXISTS(filePath))
    {
        // this is a fatal error because if this asset is a dependency for another asset and we just ignore it
        // we will crash later when trying to reference it
        Error("Failed to find texture source file %s. Exiting...\n", filePath.c_str());
    }

    TextureHeader* hdr = new TextureHeader();

    BinaryIO input;
    input.open(filePath, BinaryIOMode::Read);

    uint64_t nInputFileSize = Utils::GetFileSize(filePath);

    std::string sAssetName = assetPath; // todo: this needs to be changed to the actual name

    uint32_t nLargestMipSize = 0;
    uint32_t nStreamedMipSize = 0;
    uint32_t nDDSHeaderSize = 0;

    bool bStreamable = false;

    // parse input image file
    {
        int magic;
        input.read(magic);

        if (magic != 0x20534444) // b'DDS '
            Error("Attempted to add txtr asset '%s' that was not a valid DDS file (invalid magic). Exiting...\n", assetPath);

        DDS_HEADER ddsh = input.read<DDS_HEADER>();

        int nStreamedMipCount = 0;

        if (ddsh.dwMipMapCount > 9)
        {
            if (mapEntry.HasMember("disableStreaming") && mapEntry["disableStreaming"].GetBool())
            {
                nStreamedMipCount = 0;
                bStreamable = false;
            }
            else
            {
                nStreamedMipCount = ddsh.dwMipMapCount - 9;
                bStreamable = true;
            }
        }

        uint32_t nTotalSize = 0;
        for (int ml = 0; ml < ddsh.dwMipMapCount; ml++)
        {
            uint32_t nCurrentMipSize = (ddsh.dwPitchOrLinearSize / std::pow(4, ml));

            // add 16 bytes if mip data size is below 8 bytes else add calculated size
            nTotalSize += (nCurrentMipSize <= 8 ? 16 : nCurrentMipSize);

            // if this texture and mip are streaming
            if (bStreamable && ml < (ddsh.dwMipMapCount - 9))
                nStreamedMipSize += nCurrentMipSize;
        }

        hdr->m_nDataLength = nTotalSize;
        hdr->m_nWidth = ddsh.dwWidth;
        hdr->m_nHeight = ddsh.dwHeight;

        Log("-> dimensions: %ix%i\n", ddsh.dwWidth, ddsh.dwHeight);

        hdr->m_nPermanentMipLevels = (ddsh.dwMipMapCount - nStreamedMipCount);
        hdr->m_nStreamedMipLevels = nStreamedMipCount;

        Log("-> total mipmaps permanent:streamed : %i:%i\n", hdr->m_nPermanentMipLevels, hdr->m_nStreamedMipLevels);

        nLargestMipSize = ddsh.dwPitchOrLinearSize;

        DXGI_FORMAT dxgiFormat;

        switch (ddsh.ddspf.dwFourCC)
        {
        case '1TXD': // DXT1
            dxgiFormat = DXGI_FORMAT_BC1_UNORM;
            break;
        case '3TXD': // DXT3
            dxgiFormat = DXGI_FORMAT_BC2_UNORM;
            break;
        case '5TXD': // DXT5
            dxgiFormat = DXGI_FORMAT_BC3_UNORM;
            break;
        case 'U4CB': // BC4U
            dxgiFormat = DXGI_FORMAT_BC4_UNORM;
            break;
        case 'S4CB':
            dxgiFormat = DXGI_FORMAT_BC4_SNORM;
            break;
        case '2ITA': // ATI2
        case 'U5CB': // BC5U
            dxgiFormat = DXGI_FORMAT_BC5_UNORM;
            break;
        case 'S5CB': // BC5S
            dxgiFormat = DXGI_FORMAT_BC5_SNORM;
            break;
        case '01XD': // DX10
            dxgiFormat = DXGI_FORMAT_UNKNOWN;
            break;
        // legacy format codes
        case 36:
            dxgiFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
            break;
        case 110:
            dxgiFormat = DXGI_FORMAT_R16G16B16A16_SNORM;
            break;
        case 111:
            dxgiFormat = DXGI_FORMAT_R16_FLOAT;
            break;
        case 112:
            dxgiFormat = DXGI_FORMAT_R16G16_FLOAT;
            break;
        case 113:
            dxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
            break;
        case 114:
            dxgiFormat = DXGI_FORMAT_R32_FLOAT;
            break;
        case 115:
            dxgiFormat = DXGI_FORMAT_R32G32_FLOAT;
            break;
        case 116:
            dxgiFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;
        default:
            dxgiFormat = dxutils::GetFormatFromHeader(ddsh);
            
            if(dxgiFormat == DXGI_FORMAT_UNKNOWN)
                Error("Attempted to add txtr asset '%s' that was not using a supported DDS type. Exiting...\n", assetPath);
            
            return;
        }

        // Go to the end of the main header.
        input.seek(ddsh.dwSize + 4);

        // this is used for some math later
        nDDSHeaderSize = ddsh.dwSize + 4;

        // Go to the end of the DX10 header if it exists.
        if (ddsh.ddspf.dwFourCC == '01XD')
        {
            DDS_HEADER_DXT10 ddsh_dx10 = input.read<DDS_HEADER_DXT10>();

            dxgiFormat = ddsh_dx10.dxgiFormat;

            if (s_txtrFormatMap.count(dxgiFormat) == 0)
                Error("Attempted to add txtr asset '%s' using unsupported DDS type '%s'. Exiting...\n", assetPath, dxutils::GetFormatAsString(dxgiFormat).c_str());

            nDDSHeaderSize += 20;
        }

        Log("-> fmt: %s\n", dxutils::GetFormatAsString(dxgiFormat).c_str());

        hdr->m_nFormat = s_txtrFormatMap[dxgiFormat];
    }

    hdr->m_nAssetGUID = RTech::StringToGuid((sAssetName + ".rpak").c_str());

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

    _vseginfo_t dataseginfo = RePak::CreateNewSegment(hdr->m_nDataLength - nStreamedMipSize, 3, 16);

    char* databuf = new char[hdr->m_nDataLength - nStreamedMipSize];

    char* streamedbuf = new char[nStreamedMipSize];

    int currentDDSOffset = 0;
    int remainingDDSData = hdr->m_nDataLength;
    int remainingStreamedData = nStreamedMipSize;

    for (int ml = 0; ml < (hdr->m_nPermanentMipLevels + hdr->m_nStreamedMipLevels); ml++)
    {
        uint32_t nCurrentMipSize = (nLargestMipSize / std::pow(4, ml));
        uint32_t mipSizeDDS = 0;
        uint32_t mipSizeRpak = 0;

        if (nCurrentMipSize <= 8)
        {
            currentDDSOffset += 8;
            mipSizeDDS = 8;
            mipSizeRpak = 16;
        }
        else
        {
            currentDDSOffset += nCurrentMipSize;
            mipSizeDDS = nCurrentMipSize;
            mipSizeRpak = nCurrentMipSize;
        }

        remainingDDSData -= mipSizeRpak;

        input.seek(nDDSHeaderSize + (currentDDSOffset - mipSizeDDS), std::ios::beg);

        if (bStreamable && ml < hdr->m_nStreamedMipLevels)
        {
            remainingStreamedData -= nCurrentMipSize;
            input.getReader()->read(streamedbuf + remainingStreamedData, mipSizeDDS);
        }
        else
        {
            input.getReader()->read(databuf + remainingDDSData, mipSizeDDS);
        }
    }

    RePak::AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)hdr });

    if (bSaveDebugName)
    {
        RePak::AddRawDataBlock({ nameseginfo.index, nameseginfo.size, (uint8_t*)namebuf });
        hdr->m_pDebugName = { nameseginfo.index, 0 };

        RePak::RegisterDescriptor(subhdrinfo.index, offsetof(TextureHeader, m_pDebugName));
    }

    RePak::AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)databuf });

    // now time to add the higher level asset entry
    RPakAssetEntry asset;

    // this should hopefully fix some crashing
    uint64_t starpakOffset = -1;

    if (bStreamable)
    {
        std::string sStarpakPath = "paks/Win64/repak.starpak";

        // check per texture just in case for whatever reason you want stuff in different starpaks (if it ever gets fixed).
        if (mapEntry.HasMember("starpakPath"))
            sStarpakPath = mapEntry["starpakPath"].GetString();

        RePak::AddStarpakReference(sStarpakPath);

        SRPkDataEntry de{ 0, nStreamedMipSize, (uint8_t*)streamedbuf };
        starpakOffset = RePak::AddStarpakDataEntry(de);
    }

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, starpakOffset, -1, (std::uint32_t)AssetType::TEXTURE);
    asset.m_nVersion = TXTR_VERSION;

    asset.m_nPageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 1;

    assetEntries->push_back(asset);

    input.close();
    printf("\n");
}
