#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

void Assets::AddTextureAsset_v8(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding txtr asset '%s'\n", assetPath);

    std::string filePath = pak->GetAssetPath() + assetPath + ".dds";

    if (!FILE_EXISTS(filePath))
        Error("Failed to find texture source file %s. Exiting...\n", filePath.c_str());

    TextureHeader* hdr = new TextureHeader();

    BinaryIO input;
    input.open(filePath, BinaryIOMode::Read);

    size_t ddsFileSize = Utils::GetFileSize(filePath); // this gets used to check if we should stream this texture

    std::string sAssetName = assetPath;

    uint32_t pitchOrLinearSize = 0; // carried from dds header for math later
    uint32_t sizeOfStreamedMips = 0;

    bool hasDx10Header = false; // does this dds file have a 'dx10' header for newer style compressions
    bool isStreamable = false; // does this texture require streaming? true if total size of mip levels would exceed 64KiB. can be forced to false.

    // parse input image file
    {
        int magic;
        input.read(magic);

        if (magic != ID_DDS) // b'DDS '
            Error("Attempted to add txtr asset '%s' that was not a valid DDS file (invalid magic). Exiting...\n", assetPath);

        DDS_HEADER ddsh = input.read<DDS_HEADER>();

        // set streamable boolean based on if we have disabled it
        if (!( mapEntry.HasMember("disableStreaming") && mapEntry["disableStreaming"].GetBool() ))
            isStreamable = true;

        uint32_t sizeOfAllMips = 0;

        for (unsigned int mipLevel = 0; mipLevel < ddsh.dwMipMapCount; mipLevel++)
        {
            uint32_t currentMipSize = (ddsh.dwPitchOrLinearSize / std::pow(4, mipLevel));

            // respawn aligns all mips to 16 bytes
            sizeOfAllMips += IALIGN16(currentMipSize);

            // check if we has streamble set to true, and if this mip should be streamed
            if (isStreamable && currentMipSize > MAX_PERM_MIP_SIZE)
            {
                sizeOfStreamedMips += currentMipSize;
                hdr->streamedMipLevels++; // add a streamed mip level
            }
        }

        pitchOrLinearSize = ddsh.dwPitchOrLinearSize; // set for later usage

        hdr->dataSize = sizeOfAllMips;
        hdr->width = static_cast<uint16_t>(ddsh.dwWidth);
        hdr->height = static_cast<uint16_t>(ddsh.dwHeight);
        hdr->mipLevels = static_cast<uint8_t>(ddsh.dwMipMapCount - hdr->streamedMipLevels);

        Log("-> dimensions: %ix%i\n", ddsh.dwWidth, ddsh.dwHeight);
        Log("-> total mipmaps permanent:streamed : %i:%i\n", hdr->mipLevels, hdr->streamedMipLevels);

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

        // Go to the end of the DX10 header if it exists.
        if (ddsh.ddspf.dwFourCC == '01XD')
        {
            DDS_HEADER_DXT10 ddsh_dx10 = input.read<DDS_HEADER_DXT10>();

            dxgiFormat = ddsh_dx10.dxgiFormat;

            if (s_txtrFormatMap.count(dxgiFormat) == 0)
                Error("Attempted to add txtr asset '%s' using unsupported DDS type '%s'. Exiting...\n", assetPath, dxutils::GetFormatAsString(dxgiFormat).c_str());

            hasDx10Header = true;
        }

        Log("-> fmt: %s\n", dxutils::GetFormatAsString(dxgiFormat).c_str());

        hdr->imgFormat = s_txtrFormatMap.at(dxgiFormat);
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

    _vseginfo_t dataseginfo = pak->CreateNewSegment(hdr->dataSize - sizeOfStreamedMips, SF_CPU | SF_TEMP, 16);

    char* databuf = new char[hdr->dataSize - sizeOfStreamedMips];
    char* streamedbuf = new char[sizeOfStreamedMips];

    int currentDataOffset = 0; // current offset into dds texture data
    int remainingDataToWrite = hdr->dataSize; // amount of data remaining that needs to be written into rpak/starpak
    int remainingStreamedData = sizeOfStreamedMips;

    size_t ddsHeaderSize = hasDx10Header ? sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10) + 4 : sizeof(DDS_HEADER) + 4; // add four here for magic/id, used to offset into raw dx texture data

    for (int mipLevel = 0; mipLevel < (hdr->mipLevels + hdr->streamedMipLevels); mipLevel++)
    {
        uint32_t unalignedMipSize = static_cast<uint32_t>(pitchOrLinearSize / std::pow(4, mipLevel));
        uint32_t alignedMipSize = IALIGN16(unalignedMipSize);

        input.seek(ddsHeaderSize + currentDataOffset, std::ios::beg);

        currentDataOffset += unalignedMipSize;
        remainingDataToWrite -= alignedMipSize;

        if (isStreamable && mipLevel < hdr->streamedMipLevels)
        {
            remainingStreamedData -= unalignedMipSize;
            input.getReader()->read(streamedbuf + remainingStreamedData, unalignedMipSize);

            continue;
        }
        
        input.getReader()->read(databuf + remainingDataToWrite, unalignedMipSize);
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

    if (isStreamable && sizeOfStreamedMips > 0)
    {
        std::string starpakPath = pak->GetPrimaryStarpakPath();

        // check per texture just in case for whatever reason you want stuff in different starpaks (if it ever gets fixed).
        if (mapEntry.HasMember("starpakPath"))
            starpakPath = mapEntry["starpakPath"].GetString();

        if (starpakPath.length() == 0)
            Error("attempted to add asset '%s' as a streaming asset, but no starpak files were available.\nto fix: add 'starpakPath' as an rpak-wide variable\nor: add 'starpakPath' as an asset specific variable\n", assetPath);
       
        pak->AddStarpakReference(starpakPath);

        StreamableDataEntry de{ 0, sizeOfStreamedMips, (uint8_t*)streamedbuf };
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
