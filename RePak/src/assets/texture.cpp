#include "pch.h"
#include "rmem.h"
#include "Assets.h"

// VERSION 7
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
 
    uint32_t nLargestMipSize = 0;
    uint32_t nDDSHeaderSize = 0;
    uint32_t nStreamedMipSize = 0;

    int nStreamedMipCount = 0;
    int nTotalMipCount = 1;  

    bool bStreamable = false;

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

        if (ddsh.mipMapCount > 9)
            bStreamable = true;

        // this loop can probably be removed and added to the lower one if done correctly.
        uint32_t nTotalSize = 0;
        for (int ml = 0; ml < ddsh.mipMapCount; ml++)
        {
            uint32_t ms = (ddsh.pitchOrLinearSize / std::pow(4, ml));
            //uint32_t mss = 0;
            //int streamed = 0;
            if (ms <= 8) 
            {
                // respawn adds eight bytes of padding after these lower mips, Very Cool.
                nTotalSize += 16;
                //mss = 16;
            }
            else
            {
                nTotalSize += ms;
                //mss = ms;
            }

            if (bStreamable && ml < (ddsh.mipMapCount - 9))
            {
                nStreamedMipSize += ms;
                //streamed = 1;
                nStreamedMipCount++;
            }
               
            //Log("current mip level, mip size, and streamed: %ix%ix%i\n", ml, mss, streamed);

        }

        //Log("total size %i\n", nTotalSize);
        //Log("streamed size %i\n", nStreamedMipSize);
;
        hdr->m_nDataLength = nTotalSize;
        hdr->m_nWidth = ddsh.width;
        hdr->m_nHeight = ddsh.height;
        Log("-> dimensions: %ix%i\n", ddsh.width, ddsh.height);

        nTotalMipCount = ddsh.mipMapCount;
        nLargestMipSize = ddsh.pitchOrLinearSize;

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
        nDDSHeaderSize += ddsh.size + 4;

        // Go to the end of the DX10 header if it exists.
        if (ddsh.pixelfmt.fourCC == '01XD') {
            input.seek(20, std::ios::cur);
            nDDSHeaderSize += 20;
        }
            
    }

    hdr->m_nAssetGUID = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    // unfortunately i'm not a respawn engineer so 1 (unstreamed) mip level will have to do
    // I am also not a respawn engineer.
    hdr->m_nPermanentMipLevels = (nTotalMipCount - nStreamedMipCount);
    hdr->m_nStreamedMipLevels = nStreamedMipCount;
    
    Log("-> total mipmaps permanent:streamed : %i:%i\n", (nTotalMipCount - nStreamedMipCount), nStreamedMipCount);

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

    uint32_t buffSize = 0;
    uint32_t currentDDSOffset = 0;
    int remainingDDSData = hdr->m_nDataLength;
    int remainingStreamedData = nStreamedMipSize;

    for (int ml = 0; ml < nTotalMipCount; ml++)
    {
        uint32_t ms = (nLargestMipSize / std::pow(4, ml));
        // I can probably just declare these in the if statments I think.
        uint32_t mipSizeDDS = 0;
        uint32_t mipSizeRpak = 0;

        if (ms <= 8)
        {
            currentDDSOffset += 8;
            mipSizeDDS = 8;
            mipSizeRpak = 16;
        }
        else
        {
            currentDDSOffset += ms;
            mipSizeDDS = ms;
            mipSizeRpak = ms;
        }

        remainingDDSData -= mipSizeRpak;

        input.seek(nDDSHeaderSize + (currentDDSOffset - mipSizeDDS), std::ios::beg);

        if (bStreamable && ml < (nTotalMipCount - 9))
        {
            remainingStreamedData -= ms;
            input.getReader()->read(streamedbuf + remainingStreamedData, mipSizeDDS);
        }
        else
        {
            input.getReader()->read(databuf + remainingDDSData, mipSizeDDS);
        }
        
        //input.getReader()->read(databuf + remainingDDSData, mipSizeDDS);

        //Log("current mip level, dds mip size, and rpak mip size: %ix%ix%i\n", ml, mipSizeDDS, mipSizeRpak);
        //Log("current offset: %i\n", currentDDSOffset);
        //Log("remaining data: %i\n", remainingDDSData);

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
    RPakAssetEntryV7 asset;

    // this should hopefully fix some crashing
    uint64_t starpakOffset = -1;

    if(bStreamable) 
    {
        RePak::AddStarpakReference("paks/Win64/repak.starpak");

        SRPkDataEntry de{ -1, nStreamedMipSize, (uint8_t*)streamedbuf };
        starpakOffset = RePak::AddStarpakDataEntry(de);
    }

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, starpakOffset, -1, (std::uint32_t)AssetType::TEXTURE);
    asset.m_nVersion = TXTR_VERSION;

    asset.m_nPageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 1;

    assetEntries->push_back(asset);

    input.close();
}

// VERSION 8
void Assets::AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
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
        if (ddsh.pixelfmt.fourCC == '01XD') {
            input.seek(20, std::ios::cur);
        }
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
    RPakAssetEntryV8 asset;

    uint64_t starpakOffset = -1;

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, starpakOffset, -1, (std::uint32_t)AssetType::TEXTURE);
    asset.m_nVersion = TXTR_VERSION;

    asset.m_nPageEnd = dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.unk1 = 1;

    assetEntries->push_back(asset);

    input.close();
}
