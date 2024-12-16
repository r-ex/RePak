#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

// materialGeneratedTexture - whether this texture's creation was invoked by material automatic texture generation
static void Texture_InternalAddTexture(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const bool forceDisableStreaming)
{
    const std::string textureFilePath = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".dds");
    BinaryIO input;

    if (!input.Open(textureFilePath, BinaryIO::Mode_e::Read))
        Error("Failed to open texture asset \"%s\".\n", textureFilePath.c_str());

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(TextureAssetHeader_t), SF_HEAD, 8);
    TextureAssetHeader_t* const hdr = reinterpret_cast<TextureAssetHeader_t*>(hdrChunk.Data());

    // used for creating data buffers
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
        input.Read(magic);

        if (magic != DDS_MAGIC) // b'DDS '
            Error("Attempted to add a texture asset that was not a valid DDS file (invalid magic), exiting...\n");

        DDS_HEADER ddsh;
        input.Read(ddsh);

        DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;

        // Go to the end of the DX10 header if it exists.
        if (ddsh.ddspf.dwFourCC == '01XD')
        {
            DDS_HEADER_DXT10 ddsh_dx10;
            input.Read(ddsh_dx10);

            dxgiFormat = ddsh_dx10.dxgiFormat;

            if (s_txtrFormatMap.count(dxgiFormat) == 0)
                Error("Attempted to add a texture asset using unsupported DDS type \"%s\", exiting...\n", DXUtils::GetFormatAsString(dxgiFormat));

            isDX10 = true;
        }
        else {
            dxgiFormat = DXUtils::GetFormatFromHeader(ddsh);

            if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
                Error("Attempted to add a texture asset that was not using a supported DDS type, exiting...\n");
        }

        const char* const pDxgiFormat = DXUtils::GetFormatAsString(dxgiFormat);

        Log("-> fmt: %s\n", pDxgiFormat);
        hdr->imgFormat = s_txtrFormatMap.at(dxgiFormat);

        Log("-> dimensions: %ux%u\n", ddsh.dwWidth, ddsh.dwHeight);
        hdr->width = static_cast<uint16_t>(ddsh.dwWidth);
        hdr->height = static_cast<uint16_t>(ddsh.dwHeight);

        /*MIPMAP HANDLING*/
        // set streamable boolean based on if we have disabled it, also don't stream if we have only one mip
        if (!forceDisableStreaming && ddsh.dwMipMapCount > 1)
            isStreamable = true;

        if (isStreamable && pak->GetVersion() >= 8)
            isStreamableOpt = true;

        mips.resize(ddsh.dwMipMapCount);
        size_t mipOffset = isDX10 ? 0x94 : 0x80; // add header length

        unsigned int streamedMipCount = 0;

        for (unsigned int mipLevel = 0; mipLevel < ddsh.dwMipMapCount; mipLevel++)
        {
            // subtracts 1 so skip mips w/h at 1, gets added back when setting in mipLevel_t
            uint16_t mipWidth = 0;
            if (hdr->width >> mipLevel > 1)
                mipWidth = (hdr->width >> mipLevel) - 1;

            uint16_t mipHeight = 0;
            if (hdr->height >> mipLevel > 1)
                mipHeight = (hdr->height >> mipLevel) - 1;

            const auto& bytesPerPixel = s_pBytesPerPixel[hdr->imgFormat];

            const uint8_t x = bytesPerPixel.first;
            const uint8_t y = bytesPerPixel.second;

            const uint32_t bppWidth = (y + mipWidth) >> (y >> 1);
            const uint32_t bppHeight = (y + mipHeight) >> (y >> 1);
            
            const size_t slicePitch = x * bppWidth * bppHeight;
            const size_t alignedSize = IALIGN16(slicePitch);

            mipLevel_t& mipMap = mips[mipLevel];

            mipMap.mipOffset = mipOffset;
            mipMap.mipSize = slicePitch;
            mipMap.mipSizeAligned = alignedSize;
            mipMap.mipWidth = static_cast<uint16_t>(mipWidth + 1);
            mipMap.mipHeight = static_cast<uint16_t>(mipHeight + 1);
            mipMap.mipLevel = static_cast<uint8_t>(mipLevel + 1);
            mipMap.mipType = mipType_e::INVALID;

            hdr->dataSize += static_cast<uint32_t>(alignedSize); // all mips are aligned to 16 bytes within rpak/starpak
            mipOffset += slicePitch; // add size for the next mip's offset

            // important:
            // - we cannot have more than 4 streamed mip levels; the engine can
            //   only store up to 4 streamable mip handles per texture, anything
            //   else must be stored as a permanent mip!
            //
            // - there must always be at least 1 permanent mip level, regardless
            //   of its size. not adhering to this rule will result in a failure
            //   in ID3D11Device::CreateTexture2D during the runtime. we make
            //   sure that the smallest mip is always permanent (static) here.
            if ((streamedMipCount < MAX_STREAMED_TEXTURE_MIPS) && (mipLevel != (ddsh.dwMipMapCount - 1)))
            {
                // if opt streamable textures are enabled, check if this mip is supposed to be opt streamed
                if (isStreamableOpt && mipMap.mipSizeAligned > MAX_STREAM_MIP_SIZE)
                {
                    mipSizes.streamedOptSize += mipMap.mipSizeAligned; // only reason this is done is to create the data buffers
                    hdr->optStreamedMipLevels++; // add a streamed mip level

                    mipMap.mipType = mipType_e::STREAMED_OPT;
                }

                // if streamable textures are enabled, check if this mip is supposed to be streamed
                else if (isStreamable && mipMap.mipSizeAligned > MAX_PERM_MIP_SIZE)
                {
                    mipSizes.streamedSize += mipMap.mipSizeAligned; // only reason this is done is to create the data buffers
                    hdr->streamedMipLevels++; // add a streamed mip level

                    mipMap.mipType = mipType_e::STREAMED;
                }

                streamedMipCount++;
            }

            // texture was not streamed, make it permanent.
            if (mipMap.mipType == mipType_e::INVALID)
            {
                mipSizes.staticSize += mipMap.mipSizeAligned;
                hdr->mipLevels++;

                mipMap.mipType = mipType_e::STATIC;
            }
        }

        Log("-> total mipmaps permanent:mandatory:optional : %hhu:%hhu:%hhu\n", hdr->mipLevels, hdr->streamedMipLevels, hdr->optStreamedMipLevels);
    }

    hdr->guid = assetGuid;

    if (pak->IsFlagSet(PF_KEEP_DEV))
    {
        const size_t nameBufLen = strlen(assetPath);

        if (nameBufLen > 0)
        {
            CPakDataChunk nameChunk = pak->CreateDataChunk(nameBufLen + 1, SF_CPU | SF_DEV, 1);
            memcpy(nameChunk.Data(), assetPath, nameBufLen + 1);

            hdr->pName = nameChunk.GetPointer();
            pak->AddPointer(hdrChunk.GetPointer(offsetof(TextureAssetHeader_t, pName)));
        }
    }

    CPakDataChunk dataChunk = pak->CreateDataChunk(mipSizes.staticSize, SF_CPU | SF_TEMP, 16);
    char* const streamedbuf = new char[mipSizes.streamedSize];
    char* const optstreamedbuf = new char[mipSizes.streamedOptSize];

    char* pCurrentPosStatic = dataChunk.Data();
    char* pCurrentPosStreamed = streamedbuf;
    char* pCurrentPosStreamedOpt = optstreamedbuf;

    uint8_t mipLevel = static_cast<uint8_t>(mips.size());

    while (mipLevel > 0)
    {
        const mipLevel_t& mipMap = mips.at(mipLevel - 1);
        input.SeekGet(mipMap.mipOffset);

        switch (mipMap.mipType)
        {
        case mipType_e::STATIC:
            input.Read(pCurrentPosStatic, mipMap.mipSize);
            pCurrentPosStatic += mipMap.mipSizeAligned; // move ptr

            break;
        case mipType_e::STREAMED:
            input.Read(pCurrentPosStreamed, mipMap.mipSize);
            pCurrentPosStreamed += mipMap.mipSizeAligned; // move ptr

            break;
        case mipType_e::STREAMED_OPT:
            input.Read(pCurrentPosStreamedOpt, mipMap.mipSize);
            pCurrentPosStreamedOpt += mipMap.mipSizeAligned; // move ptr

            break;
        default:
            break;
        }

        mipLevel--;
    }

    // now time to add the higher level asset entry
    uint64_t mandatoryStreamDataOffset = UINT64_MAX;

    if (isStreamable && hdr->streamedMipLevels > 0)
    {
        PakStreamSetEntry_s de{ 0, mipSizes.streamedSize };
        pak->AddStreamingDataEntry(de, (uint8_t*)streamedbuf, STREAMING_SET_MANDATORY);

        mandatoryStreamDataOffset = de.offset;
    }

    delete[] streamedbuf;

    uint64_t optionalStreamDataOffset = UINT64_MAX;

    if (isStreamableOpt && hdr->optStreamedMipLevels > 0)
    {
        PakStreamSetEntry_s de{ 0, mipSizes.streamedOptSize };
        pak->AddStreamingDataEntry(de, (uint8_t*)optstreamedbuf, STREAMING_SET_OPTIONAL);

        optionalStreamDataOffset = de.offset;
    }

    delete[] optstreamedbuf;

    PakAsset_t asset;
    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), dataChunk.GetPointer(), mandatoryStreamDataOffset, optionalStreamDataOffset, AssetType::TXTR);
    asset.SetHeaderPointer(hdrChunk.Data());

    asset.version = TXTR_VERSION;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 1;

    pak->PushAsset(asset);

    input.Close();
}

bool Texture_AutoAddTexture(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const bool forceDisableStreaming)
{
    PakAsset_t* const existingAsset = pak->GetAssetByGuid(assetGuid, nullptr, true);

    if (existingAsset)
        return false; // already present in the pak.

    Log("Auto-adding 'txtr' asset \"%s\".\n", assetPath);
    Texture_InternalAddTexture(pak, assetGuid, assetPath, forceDisableStreaming);

    return true;
}

void Assets::AddTextureAsset_v8(CPakFile* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    const bool disableStreaming = JSON_GetValueOrDefault(mapEntry, "$disableStreaming", false);
    Texture_InternalAddTexture(pak, assetGuid, assetPath, disableStreaming);
}
