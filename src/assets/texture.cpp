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
    TextureAssetHeader_t* hdr = reinterpret_cast<TextureAssetHeader_t*>(hdrChunk.Data());

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

        DDS_HEADER ddsh = input.Read<DDS_HEADER>();
        DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;

        // Go to the end of the DX10 header if it exists.
        if (ddsh.ddspf.dwFourCC == '01XD')
        {
            DDS_HEADER_DXT10 ddsh_dx10 = input.Read<DDS_HEADER_DXT10>();

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

        const char* pDxgiFormat = DXUtils::GetFormatAsString(dxgiFormat);

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

        isStreamableOpt = false; // force false until we have proper optional starpaks

        size_t mipOffset = isDX10 ? 0x94 : 0x80; // add header length

        for (unsigned int mipLevel = 0; mipLevel < ddsh.dwMipMapCount; mipLevel++)
        {
            // subtracts 1 so skip mips w/h at 1, gets added back when setting in mipLevel_t
            uint16_t mipWidth = 0;
            if (hdr->width >> mipLevel > 1)
                mipWidth = (hdr->width >> mipLevel) - 1;

            uint16_t mipHeight = 0;
            if (hdr->height >> mipLevel > 1)
                mipHeight = (hdr->height >> mipLevel) - 1;
            
            uint8_t x = s_pBytesPerPixel[hdr->imgFormat].first;
            uint8_t y = s_pBytesPerPixel[hdr->imgFormat].second;

            uint32_t bppWidth = (y + mipWidth) >> (y >> 1);
            uint32_t bppHeight = (y + mipHeight) >> (y >> 1);
            
            size_t slicePitch = x * bppWidth * bppHeight;

            mipLevel_t mipMap{ mipOffset, slicePitch, IALIGN16(slicePitch),
                static_cast<uint16_t>(mipWidth + 1), static_cast<uint16_t>(mipHeight + 1), static_cast<uint8_t>(mipLevel + 1) };

            hdr->dataSize += IALIGN16(slicePitch); // all mips are aligned to 16 bytes within rpak/starpak
            mipOffset += slicePitch; // add size for the next mip's offset

            // if opt streamable textures are enabled, check if this mip is supposed to be opt streamed
            if (isStreamableOpt && mipMap.mipSizeAligned >= MAX_STREAM_MIP_SIZE)
            {
                mipSizes.streamedOptSize += mipMap.mipSizeAligned; // only reason this is done is to create the data buffers
                hdr->optStreamedMipLevels++; // add a streamed mip level

                mipMap.mipType = STREAMED_OPT;
                mips.push_back(mipMap);

                continue;
            }

            // if streamable textures are enabled, check if this mip is supposed to be streamed
            if (isStreamable && mipMap.mipSizeAligned > MAX_PERM_MIP_SIZE)
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

        Log("-> total mipmaps permanent:streamed:streamed opt : %hhu:%hhu:%hhu\n", hdr->mipLevels, hdr->streamedMipLevels, hdr->optStreamedMipLevels);
    }

    hdr->guid = assetGuid;

    if (pak->IsFlagSet(PF_KEEP_DEV))
    {
        const size_t nameBufLen = strlen(assetPath) + 1;
        CPakDataChunk nameChunk = pak->CreateDataChunk(nameBufLen, SF_DEV | SF_CPU, 1);

        sprintf_s(nameChunk.Data(), nameBufLen, "%s", assetPath);

        hdr->pName = nameChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(TextureAssetHeader_t, pName)));
    }

    CPakDataChunk dataChunk = pak->CreateDataChunk(mipSizes.staticSize, SF_CPU | SF_TEMP, 16);
    char* streamedbuf = new char[mipSizes.streamedSize];
    char* optstreamedbuf = new char[mipSizes.streamedOptSize];

    char* pCurrentPosStatic = dataChunk.Data();
    char* pCurrentPosStreamed = streamedbuf;
    char* pCurrentPosStreamedOpt = optstreamedbuf;

    uint8_t mipLevel = static_cast<uint8_t>(mips.size());

    while (mipLevel > 0)
    {
        mipLevel_t& mipMap = mips.at(mipLevel - 1);

        input.SeekGet(mipMap.mipOffset);

        switch (mipMap.mipType)
        {
        case STATIC:
            input.Read(pCurrentPosStatic, mipMap.mipSize);
            pCurrentPosStatic += mipMap.mipSizeAligned; // move ptr

            break;
        case STREAMED:
            input.Read(pCurrentPosStreamed, mipMap.mipSize);
            pCurrentPosStreamed += mipMap.mipSizeAligned; // move ptr

            break;
        case STREAMED_OPT:
            input.Read(pCurrentPosStreamedOpt, mipMap.mipSize);
            pCurrentPosStreamedOpt += mipMap.mipSizeAligned; // move ptr

            break;
        default:
            break;
        }

        mipLevel--;
    }

    // now time to add the higher level asset entry
    PakAsset_t asset;

    // this should hopefully fix some crashing
    uint64_t starpakOffset = UINT64_MAX;

    if (isStreamable && hdr->streamedMipLevels > 0)
    {
        StreamableDataEntry de{ 0, mipSizes.streamedSize, (uint8_t*)streamedbuf };
        pak->AddStarpakDataEntry(de);

        starpakOffset = de.offset;
    }

    if (isStreamableOpt && hdr->optStreamedMipLevels > 0)
    {
        // do stuff
    }

    asset.InitAsset(assetPath, assetGuid, hdrChunk.GetPointer(), hdrChunk.GetSize(), dataChunk.GetPointer(), starpakOffset, UINT64_MAX, AssetType::TXTR);
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
