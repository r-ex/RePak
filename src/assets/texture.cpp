#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"


void Assets::AddTextureAsset_v8(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding txtr asset '%s'\n", assetPath);

    std::string filePath = pak->GetAssetPath() + assetPath + ".dds";

    if (!FILE_EXISTS(filePath))
        Error("Failed to find texture source file %s. Exiting...\n", filePath.c_str());

    CPakDataChunk& hdrChunk = pak->CreateDataChunk(sizeof(TextureHeader), SF_HEAD, 8);

    TextureHeader* hdr = reinterpret_cast<TextureHeader*>(hdrChunk.Data());

    BinaryIO input(filePath, BinaryIOMode::Read);

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

        DXGI_FORMAT dxgiFormat = DXUtils::GetFormatFromHeader(ddsh);

        if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
            Error("Attempted to add txtr asset '%s' that was not using a supported DDS type. Exiting...\n", assetPath);

        // Go to the end of the main header.
        input.seek(ddsh.dwSize + 4);

        // this is used for some math later
        nDDSHeaderSize = ddsh.dwSize + 4;


        const char* pDxgiFormat = DXUtils::GetFormatAsString(dxgiFormat);

        // Go to the end of the DX10 header if it exists.
        if (ddsh.ddspf.dwFourCC == '01XD')
        {
            DDS_HEADER_DXT10 ddsh_dx10 = input.read<DDS_HEADER_DXT10>();

            dxgiFormat = ddsh_dx10.dxgiFormat;

            if (s_txtrFormatMap.count(dxgiFormat) == 0)
                Error("Attempted to add txtr asset '%s' using unsupported DDS type '%s'. Exiting...\n", assetPath, pDxgiFormat);

            nDDSHeaderSize += 20;
        }

        Log("-> fmt: %s\n", pDxgiFormat);
        hdr->imgFormat = s_txtrFormatMap.at(dxgiFormat);
    }

    hdr->guid = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    bool bSaveDebugName = pak->IsFlagSet(PF_KEEP_DEV) || (mapEntry.HasMember("saveDebugName") && mapEntry["saveDebugName"].GetBool());


    CPakDataChunk nameChunk;
    if (bSaveDebugName)
    {
        nameChunk = pak->CreateDataChunk(sAssetName.size() + 1, SF_DEV | SF_CPU, 1);

        sprintf_s(nameChunk.Data(), sAssetName.length() + 1, "%s", sAssetName.c_str());
    }

    // woo more segments
    // cpu data

    CPakDataChunk& dataChunk = pak->CreateDataChunk(hdr->dataSize - nStreamedMipSize, SF_CPU | SF_TEMP, 16);

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
            input.getReader()->read(dataChunk.Data() + remainingDDSData, mipSizeDDS);
        }
    }

    if (bSaveDebugName)
    {
        hdr->pName = nameChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(TextureHeader, pName)));
    }

    // now time to add the higher level asset entry
    PakAsset_t asset;

    // this should hopefully fix some crashing
    uint64_t starpakOffset = -1;

    if (bStreamable)
    {
        std::string starpakPath = pak->GetPrimaryStarpakPath();

        // check per texture just in case for whatever reason you want stuff in different starpaks (if it ever gets fixed).
        if (mapEntry.HasMember("starpakPath"))
            starpakPath = mapEntry["starpakPath"].GetString();

        if (starpakPath.length() == 0)
            Error("attempted to add asset '%s' as a streaming asset, but no starpak files were available.\nto fix: add 'starpakPath' as an rpak-wide variable\nor: add 'starpakPath' as an asset specific variable\n", assetPath);
       
        pak->AddStarpakReference(starpakPath);

        StreamableDataEntry de{ 0, nStreamedMipSize, (uint8_t*)streamedbuf };
        de = pak->AddStarpakDataEntry(de);
        starpakOffset = de.m_nOffset;
    }

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), hdrChunk.GetPointer(), hdrChunk.GetSize(), dataChunk.GetPointer(), starpakOffset, -1, (std::uint32_t)AssetType::TXTR);
    asset.version = TXTR_VERSION;

    asset.pageEnd = pak->GetNumPages(); // dataseginfo.index + 1; // number of the highest page that the asset references pageidx + 1
    asset.remainingDependencyCount = 1;

    assetEntries->push_back(asset);

    input.close();
    printf("\n");
}
