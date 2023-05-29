#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

__forceinline int BitsPerPixel(DXGI_FORMAT format, int& compressionAlignment)
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;
        break;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;
        break;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        return 64;
        break;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
        return 32;
        break;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_V408:
        return 24;
        break;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
        return 16;
        break;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
        return 12;
        break;
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
        return 8;
        break;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        compressionAlignment = 16; // bytes
        return 8;
        break;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        compressionAlignment = 8;
        return 4;
        break;

    case DXGI_FORMAT_R1_UNORM:
        return 1;
        break;

    case DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE:
    case DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE:
    case DXGI_FORMAT_FORCE_UINT:
    case DXGI_FORMAT_UNKNOWN:
    default:
        return -1;
        break;
    }
}

void Assets::AddTextureAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, bool forceDisableStreaming)
{
    Log("Adding txtr asset '%s'\n", assetPath);

    PakAsset_t* existingAsset = pak->GetAssetByGuid(RTech::GetAssetGUIDFromString(assetPath, true), nullptr, true);
    if (existingAsset)
    {
        Warning("Tried to add texture asset '%s' twice. Skipping redefinition...\n", assetPath);
        return;
    }

    std::string filePath = pak->GetAssetPath() + assetPath + ".dds";

    if (!FILE_EXISTS(filePath))
        Error("Failed to find texture source file %s. Exiting...\n", filePath.c_str());

    CPakDataChunk& hdrChunk = pak->CreateDataChunk(sizeof(TextureHeader), SF_HEAD, 16);

    TextureHeader* hdr = reinterpret_cast<TextureHeader*>(hdrChunk.Data());

    BinaryIO input(filePath, BinaryIOMode::Read);

    size_t ddsFileSize = Utils::GetFileSize(filePath); // this gets used to check if we should stream this texture

    std::string sAssetName = assetPath;

    __int64 pitchOrLinearSize = 0; // carried from dds header for math later
    uint32_t mipMapCount = 0;
    uint32_t sizeOfStreamedMips = 0;
    uint32_t sizeOfOptStreamedMips = 0;

    int bpp = 0;
    int compressionAlignment = 0;

    bool isStreamable = false; // does this texture require streaming? true if total size of mip levels would exceed 64KiB. can be forced to false.
    bool isStreamableOpt = false; // can this texture use optional starpaks? can only be set if pak is version v8

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
        }
        else {
            dxgiFormat = DXUtils::GetFormatFromHeader(ddsh);

            if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
                Error("Attempted to add txtr asset '%s' that was not using a supported DDS type. Exiting...\n", assetPath);
        }

        const char* pDxgiFormat = DXUtils::GetFormatAsString(dxgiFormat);

        // set streamable boolean based on if we have disabled it, also don't stream if we have only one mip
        if (!forceDisableStreaming && ddsh.dwMipMapCount > 1)
            isStreamable = true;

        if (isStreamable && pak->GetVersion() >= 8)
            isStreamableOpt = true;

        isStreamableOpt = false; // force false until we have proper optional starpaks

        bpp = BitsPerPixel(dxgiFormat, compressionAlignment);
        //float bytesPerPixel = static_cast<float>(bpp / 8.f);

        // only needed because the dds file is out of scope later
        pitchOrLinearSize = ddsh.dwFlags & 0x8 ? ddsh.dwWidth * ddsh.dwHeight : ddsh.dwPitchOrLinearSize; // set for later usage
        mipMapCount = ddsh.dwMipMapCount;

        if (compressionAlignment == 0)
            pitchOrLinearSize *= static_cast<__int64>(bpp / 8.f);

        for (unsigned int mipLevel = 0; mipLevel < mipMapCount; mipLevel++)
        {
            uint32_t currentMipSize = static_cast<uint32_t>(ceilf(pitchOrLinearSize / std::pow(4, mipLevel)));

            if (compressionAlignment > 0)
                currentMipSize = IALIGN(currentMipSize, compressionAlignment);

            // respawn aligns all mips to 16 bytes
            // something gets packed in the lower mips
            hdr->dataSize += IALIGN16(currentMipSize);

            // check if we has streamble set to true, and if this mip should be streamed
            if (isStreamableOpt && currentMipSize >= MAX_STREAM_MIP_SIZE)
            {
                sizeOfOptStreamedMips += currentMipSize; // only reason this is done is to create the data buffers
                hdr->optStreamedMipLevels++; // add a streamed mip level

                continue;
            }

            // check if we has streamble set to true, and if this mip should be streamed
            if (isStreamable && currentMipSize >= MAX_PERM_MIP_SIZE)
            {
                sizeOfStreamedMips += currentMipSize; // only reason this is done is to create the data buffers
                hdr->streamedMipLevels++; // add a streamed mip level

                continue;
            }
        }

        //printf("size of streamed mips %x\n", sizeOfStreamedMips);

        hdr->width = static_cast<uint16_t>(ddsh.dwWidth);
        hdr->height = static_cast<uint16_t>(ddsh.dwHeight);
        hdr->mipLevels = static_cast<uint8_t>(ddsh.dwMipMapCount - hdr->streamedMipLevels - hdr->optStreamedMipLevels);

        Log("-> dimensions: %ix%i\n", ddsh.dwWidth, ddsh.dwHeight);
        Log("-> total mipmaps permanent:streamed:streamed opt : %i:%i:%i\n", hdr->mipLevels, hdr->streamedMipLevels, hdr->optStreamedMipLevels);

        Log("-> fmt: %s\n", pDxgiFormat);
        hdr->imgFormat = s_txtrFormatMap.at(dxgiFormat);
    }

    hdr->guid = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    if (pak->IsFlagSet(PF_KEEP_DEV))
    {
        CPakDataChunk& nameChunk = pak->CreateDataChunk(sAssetName.size() + 1, SF_DEV | SF_CPU, 1);

        sprintf_s(nameChunk.Data(), sAssetName.length() + 1, "%s", sAssetName.c_str());

        hdr->pName = nameChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(TextureHeader, pName)));
    }

    CPakDataChunk& dataChunk = pak->CreateDataChunk(hdr->dataSize - sizeOfStreamedMips - sizeOfOptStreamedMips, SF_CPU | SF_TEMP, 16);

    char* streamedbuf = new char[sizeOfStreamedMips];
    char* optstreamedbuf = new char[sizeOfOptStreamedMips];

    // not a fan of this
    uint32_t textureMipOffset = ddsFileSize;
    uint32_t rpakMipOffset = 0;
    uint32_t starpakMipOffset = 0;
    uint32_t starpakOptMipOffset = 0;

    for (uint8_t mipLevel = 0; mipLevel < mipMapCount; mipLevel++)
    {
        uint8_t textureMipLevel = static_cast<uint8_t>((mipMapCount - 1) - mipLevel); // set the mip level we are getting in dds
        uint32_t unalignedMipSize = static_cast<uint32_t>(ceilf(pitchOrLinearSize / std::pow(4, textureMipLevel)));
        uint32_t alignedMipSize = IALIGN16(unalignedMipSize);

        if (compressionAlignment > 0)
            unalignedMipSize = IALIGN(unalignedMipSize, compressionAlignment);

        //printf("mipLevel %i, unaligned mip size %x, aligned mip size %x\n", mipLevel, unalignedMipSize, alignedMipSize);

        textureMipOffset -= unalignedMipSize; // subtract first so our position is at the mips start
        input.seek(textureMipOffset, std::ios::beg); // seek to the mips position

        if (isStreamableOpt && alignedMipSize >= MAX_STREAM_MIP_SIZE)
        {
            input.getReader()->read(optstreamedbuf + starpakOptMipOffset, unalignedMipSize);
            starpakOptMipOffset += alignedMipSize;

            continue;
        }

        if (isStreamable && alignedMipSize >= MAX_PERM_MIP_SIZE)
        {
            input.getReader()->read(streamedbuf + starpakMipOffset, unalignedMipSize);
            starpakMipOffset += alignedMipSize;

            continue;
        }

        input.getReader()->read(dataChunk.Data() + rpakMipOffset, unalignedMipSize);
        rpakMipOffset += alignedMipSize;
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

        StreamableDataEntry de{ 0, sizeOfStreamedMips, (uint8_t*)streamedbuf };
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
