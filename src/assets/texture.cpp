#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

void Assets::AddTextureAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, bool forceDisableStreaming)
{
    Log("Adding txtr asset '%s'\n", assetPath);

    PakAsset_t* existingAsset = pak->GetAssetByGuid(RTech::GetAssetGUIDFromString(assetPath, true), nullptr, true);
    if (existingAsset)
    {
        Warning("Tried to add texture asset '%s' twice.Skipping redefinition...\n", assetPath);
        return;
    }

    std::string filePath = pak->GetAssetPath() + assetPath + ".dds";

    if (!FILE_EXISTS(filePath))
        Error("Failed to find texture source file %s. Exiting...\n", filePath.c_str());

    CPakDataChunk& hdrChunk = pak->CreateDataChunk(sizeof(TextureHeader), SF_HEAD, 16);

    TextureHeader* hdr = reinterpret_cast<TextureHeader*>(hdrChunk.Data());

    BinaryIO input(filePath, BinaryIOMode::Read);

    std::string sAssetName = assetPath;

    struct {
        size_t staticSize;
        size_t streamedSize;
        size_t streamedOptSize;
    } mipSizes{};

    std::vector<mipLevel_t> mips;

    bool isStreamable = false; // does this texture require streaming? true if total size of mip levels would exceed 64KiB. can be forced to false.
    bool isStreamableOpt = false; // can this texture use optional starpaks? can only be set if pak is version v8
    bool isDX10 = false;

    // parse input image file
    {
        int magic;
        input.read(magic);

        if (magic != DDS_MAGIC) // b'DDS '
            Error("Attempted to add txtr asset '%s' that was not a valid DDS file (invalid magic). Exiting...\n", assetPath);

        DDS_HEADER ddsh = input.read<DDS_HEADER>();

        DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;

        // Go to the end of the DX10 header if it exists.
        if (ddsh.ddspf.dwFourCC == '01XD')
        {
            DDS_HEADER_DXT10 ddsh_dx10 = input.read<DDS_HEADER_DXT10>();

            dxgiFormat = ddsh_dx10.dxgiFormat;

            if (s_txtrFormatMap.count(dxgiFormat) == 0)
                Error("Attempted to add txtr asset '%s' using unsupported DDS type '%s'. Exiting...\n", assetPath, DXUtils::GetFormatAsString(dxgiFormat));

            isDX10 = true;
        }
        else {
            dxgiFormat = DXUtils::GetFormatFromHeader(ddsh);

            if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
                Error("Attempted to add txtr asset '%s' that was not using a supported DDS type. Exiting...\n", assetPath);
        }

        const char* pDxgiFormat = DXUtils::GetFormatAsString(dxgiFormat);

        Log("-> fmt: %s\n", pDxgiFormat);
        hdr->imgFormat = s_txtrFormatMap.at(dxgiFormat);

        Log("-> dimensions: %ix%i\n", ddsh.dwWidth, ddsh.dwHeight);
        hdr->width = static_cast<uint16_t>(ddsh.dwWidth);
        hdr->height = static_cast<uint16_t>(ddsh.dwHeight);

        /*MIPMAP HANDLING*/
        // set streamable boolean based on if we have disabled it, also don't stream if we have only one mip
        if (!forceDisableStreaming && ddsh.dwMipMapCount > 1)
            isStreamable = true;

        if (isStreamable && pak->GetVersion() >= 8)
            isStreamableOpt = true;

        isStreamableOpt = false; // force false until we have proper optional starpaks

        size_t mipOffset = isDX10 ? 0x94 : 0x80; // add heeder length

        for (unsigned int mipLevel = 0; mipLevel < ddsh.dwMipMapCount; mipLevel++)
        {

            int mipWidth = 0;
            if (hdr->width >> mipLevel > 1)
                mipWidth = (hdr->width >> mipLevel) - 1;

            int mipHeight = 0;
            if (hdr->height >> mipLevel > 1)
                mipHeight = (hdr->height >> mipLevel) - 1;
            
            uint8_t x = s_pBytesPerPixel[hdr->imgFormat].first;
            uint8_t y = s_pBytesPerPixel[hdr->imgFormat].second;

            uint32_t bppWidth = (y + mipWidth) >> (y >> 1);
            uint32_t bppHeight = (y + mipHeight) >> (y >> 1);
            
            size_t slicePitch = x * bppWidth * bppHeight;

            mipLevel_t mipMap{ mipOffset, slicePitch, IALIGN16(slicePitch), mipWidth + 1, mipHeight + 1, mipLevel + 1 };

            // respawn aligns all mips to 16 bytes
            hdr->dataSize += IALIGN16(slicePitch);
            mipOffset += slicePitch;

            // check if we has streamble set to true, and if this mip should be streamed
            if (isStreamableOpt && mipMap.mipSizeAligned >= MAX_STREAM_MIP_SIZE)
            {
                mipSizes.streamedOptSize += mipMap.mipSizeAligned; // only reason this is done is to create the data buffers
                hdr->optStreamedMipLevels++; // add a streamed mip level

                mipMap.mipType = STREAMED_OPT;
                mips.push_back(mipMap);

                continue;
            }

            // check if we has streamble set to true, and if this mip should be streamed
            if (isStreamable && mipMap.mipSizeAligned >= MAX_PERM_MIP_SIZE)
            {
                mipSizes.streamedSize += mipMap.mipSizeAligned; // only reason this is done is to create the data buffers
                hdr->streamedMipLevels++; // add a streamed mip level

                mipMap.mipType = STREAMED;
                mips.push_back(mipMap);

                continue;
            }

            mipSizes.staticSize += mipMap.mipSizeAligned;
            hdr->mipLevels++;

            mipMap.mipType = STATIC;
            mips.push_back(mipMap);
        }

        Log("-> total mipmaps permanent:streamed:streamed opt : %i:%i:%i\n", hdr->mipLevels, hdr->streamedMipLevels, hdr->optStreamedMipLevels);
    }

    hdr->guid = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    if (pak->IsFlagSet(PF_KEEP_DEV))
    {
        CPakDataChunk& nameChunk = pak->CreateDataChunk(sAssetName.size() + 1, SF_DEV | SF_CPU, 1);

        sprintf_s(nameChunk.Data(), sAssetName.length() + 1, "%s", sAssetName.c_str());

        hdr->pName = nameChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(TextureHeader, pName)));
    }

    CPakDataChunk& dataChunk = pak->CreateDataChunk(mipSizes.staticSize, SF_CPU | SF_TEMP, 16);
    char* streamedbuf = new char[mipSizes.streamedSize];
    char* optstreamedbuf = new char[mipSizes.streamedOptSize];

    char* pCurrentPosStatic = dataChunk.Data();
    char* pCurrentPosStreamed = streamedbuf;
    char* pCurrentPosStreamedOpt = optstreamedbuf;

    for (uint8_t mipLevel = 0; mipLevel < mips.size(); mipLevel++)
    {
        mipLevel_t& mipMap = mips.at((mips.size() - 1) - mipLevel);

        input.seek(mipMap.mipOffset, std::ios::beg);

        switch (mipMap.mipType)
        {
        case STATIC:
            input.getReader()->read(pCurrentPosStatic, mipMap.mipSize);
            pCurrentPosStatic += mipMap.mipSizeAligned; // move ptr

            break;
        case STREAMED:
            input.getReader()->read(pCurrentPosStreamed, mipMap.mipSize);
            pCurrentPosStreamed += mipMap.mipSizeAligned; // move ptr

            break;
        case STREAMED_OPT:
            input.getReader()->read(pCurrentPosStreamedOpt, mipMap.mipSize);
            pCurrentPosStreamedOpt += mipMap.mipSizeAligned; // move ptr

            break;
        default:
            break;
        }
    }

    // now time to add the higher level asset entry
    PakAsset_t asset;

    // this should hopefully fix some crashing
    uint64_t starpakOffset = -1;

    if (isStreamable && hdr->streamedMipLevels > 0)
    {
        std::string starpakPath = pak->GetPrimaryStarpakPath();

        // check per texture just in case for whatever reason you want stuff in different starpaks (if it ever gets fixed).
        //if (mapEntry.HasMember("starpakPath"))
        //    starpakPath = mapEntry["starpakPath"].GetString();

        if (starpakPath.length() == 0)
            Error("attempted to add asset '%s' as a streaming asset, but no starpak files were available.\nto fix: add 'starpakPath' as an rpak-wide variable\nor: add 'starpakPath' as an asset specific variable\n", assetPath);

        pak->AddStarpakReference(starpakPath);

        StreamableDataEntry de{ 0, mipSizes.streamedSize, (uint8_t*)streamedbuf };
        de = pak->AddStarpakDataEntry(de);
        starpakOffset = de.m_nOffset;
    }

    if (isStreamableOpt && hdr->optStreamedMipLevels > 0)
    {
        // do stuff
    }

    asset.InitAsset(RTech::StringToGuid((sAssetName + ".rpak").c_str()), hdrChunk.GetPointer(), hdrChunk.GetSize(), dataChunk.GetPointer(), starpakOffset, -1, (std::uint32_t)AssetType::TXTR);
    asset.version = TXTR_VERSION;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 1;

    assetEntries->push_back(asset);

    input.close();
    printf("\n");
}

void Assets::AddTextureAsset_v8(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    AddTextureAsset(pak, assetEntries, assetPath, mapEntry.HasMember("disableStreaming") && mapEntry["disableStreaming"].GetBool());
}
