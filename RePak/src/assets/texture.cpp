#include "pch.h"
#include "rmem.h"
#include "Assets.h"
#include "dxutils.h"

void Assets::AddTextureAsset_v8(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding txtr asset '%s'\n", assetPath);

    std::string filePath = g_sAssetsDir + assetPath + ".dds";

    if (!FILE_EXISTS(filePath))
        Error("Failed to find texture source file %s. Exiting...\n", filePath.c_str());

    TextureHeader* hdr = new TextureHeader();

    BinaryIO input;
    input.open(filePath, BinaryIOMode::Read);

    uint64_t nInputFileSize = Utils::GetFileSize(filePath);

    std::string sAssetName = assetPath;

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
        for (unsigned int ml = 0; ml < ddsh.dwMipMapCount; ml++)
        {
            uint32_t nCurrentMipSize = (ddsh.dwPitchOrLinearSize / std::pow(4, ml));

            // add 16 bytes if mip data size is below 8 bytes else add calculated size
            nTotalSize += (nCurrentMipSize <= 8 ? 16 : nCurrentMipSize);

            // if this texture and mip are streaming
            if (bStreamable && ml < (ddsh.dwMipMapCount - 9))
                nStreamedMipSize += nCurrentMipSize;
        }

        hdr->dataSize = nTotalSize;
        hdr->width = (uint16_t)ddsh.dwWidth;
        hdr->height = (uint16_t)ddsh.dwHeight;

        Log("-> dimensions: %ix%i\n", ddsh.dwWidth, ddsh.dwHeight);

        hdr->mipLevels = (uint8_t)(ddsh.dwMipMapCount - nStreamedMipCount);
        hdr->streamedMipLevels = nStreamedMipCount;

        Log("-> total mipmaps permanent:streamed : %i:%i\n", hdr->mipLevels, hdr->streamedMipLevels);

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
        case '1ITA':
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

        hdr->imgFormat = s_txtrFormatMap[dxgiFormat];
    }

    hdr->guid = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    bool bSaveDebugName = pak->IsFlagSet(PF_KEEP_DEV) || (mapEntry.HasMember("saveDebugName") && mapEntry["saveDebugName"].GetBool());

    // asset header
    _vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(TextureHeader), SF_HEAD, 8);

    _vseginfo_t nameseginfo{};

    char* namebuf = new char[sAssetName.size() + 1];

    if (bSaveDebugName)
    {
        sprintf_s(namebuf, sAssetName.length() + 1, "%s", sAssetName.c_str());
        nameseginfo = pak->CreateNewSegment(sAssetName.size() + 1, SF_DEV | SF_CPU, 1);
    }
    else delete[] namebuf;

    // woo more segments
    // cpu data

    _vseginfo_t dataseginfo = pak->CreateNewSegment(hdr->dataSize - nStreamedMipSize, SF_CPU | SF_TEMP, 16);

    char* databuf = new char[hdr->dataSize - nStreamedMipSize];

    char* streamedbuf = new char[nStreamedMipSize];

    int currentDDSOffset = 0;
    int remainingDDSData = hdr->dataSize;
    int remainingStreamedData = nStreamedMipSize;

    for (int ml = 0; ml < (hdr->mipLevels + hdr->streamedMipLevels); ml++)
    {
        uint32_t nCurrentMipSize = (unsigned int)(nLargestMipSize / std::pow(4, ml));
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

        if (bStreamable && ml < hdr->streamedMipLevels)
        {
            remainingStreamedData -= nCurrentMipSize;
            input.getReader()->read(streamedbuf + remainingStreamedData, mipSizeDDS);
        }
        else
        {
            input.getReader()->read(databuf + remainingDDSData, mipSizeDDS);
        }
    }

    pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)hdr });

    if (bSaveDebugName)
    {
        pak->AddRawDataBlock({ nameseginfo.index, nameseginfo.size, (uint8_t*)namebuf });
        hdr->pName = { nameseginfo.index, 0 };

        pak->AddPointer(subhdrinfo.index, offsetof(TextureHeader, pName));
    }

    pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)databuf });

    // now time to add the higher level asset entry
    RPakAssetEntry asset;

    // this should hopefully fix some crashing
    uint64_t starpakOffset = -1;

    if (bStreamable)
    {
        std::string starpakPath = pak->primaryStarpakPath;

        // check per texture just in case for whatever reason you want stuff in different starpaks (if it ever gets fixed).
        if (mapEntry.HasMember("starpakPath"))
            starpakPath = mapEntry["starpakPath"].GetString();

        if (starpakPath.length() == 0)
            Error("attempted to add asset '%s' as a streaming asset, but no starpak files were available.\nto fix: add 'starpakPath' as an rpak-wide variable\nor: add 'starpakPath' as an asset specific variable\n", assetPath);
       
        pak->AddStarpakReference(starpakPath);

        SRPkDataEntry de{ 0, nStreamedMipSize, (uint8_t*)streamedbuf };
        de = pak->AddStarpakDataEntry(de);
        starpakOffset = de.m_nOffset;
    }

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, starpakOffset, -1, (std::uint32_t)AssetType::TXTR);
    asset.version = TXTR_VERSION;

    asset.pageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 1;

    assetEntries->push_back(asset);

    input.close();
    printf("\n");
}
