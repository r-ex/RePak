#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

#define TEXTURE_USAGE_FLAGS_FIELD "usageFlags"
#define TEXTURE_MIP_INFO_FIELD "mipInfo"

// If the texture has additional metadata, parse it.
static void Texture_ProcessMetaData(CPakFileBuilder* const pak, const char* const assetPath, 
                                    TextureAssetHeader_t* const hdr, const int totalMipCount)
{
    const std::string metaFilePath = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".json");
    rapidjson::Document document;

    if (!JSON_ParseFromFile(metaFilePath.c_str(), "texture metadata", document))
        return;

    rapidjson::Value::ConstMemberIterator usageFlagsIt;

    if (JSON_GetIterator(document, TEXTURE_USAGE_FLAGS_FIELD, usageFlagsIt))
    {
        int32_t usageFlags;

        if (!JSON_ParseNumber(usageFlagsIt->value, usageFlags))
            Error("Failed to parse \"" TEXTURE_USAGE_FLAGS_FIELD "\" from texture metadata.\n");

        hdr->usageFlags = static_cast<uint8_t>(usageFlags);
    }

    rapidjson::Value::ConstMemberIterator mipInfoIt;

    if (JSON_GetIterator(document, TEXTURE_MIP_INFO_FIELD, mipInfoIt))
    {
        if (!JSON_IsOfType(mipInfoIt->value, JSONFieldType_e::kArray))
            Error("Field \"" TEXTURE_MIP_INFO_FIELD "\" in texture metadata must be an array.\n");

        const rapidjson::Value::ConstArray& mipInfoArray = mipInfoIt->value.GetArray();

        if (mipInfoArray.Empty())
            Error("Array \"" TEXTURE_MIP_INFO_FIELD "\" was found empty in texture metadata.\n");

        const size_t arraySize = mipInfoArray.Size();

        if (arraySize != totalMipCount-1)
            Error("Array \"" TEXTURE_MIP_INFO_FIELD "\" in texture metadata must cover all mips except the base (expected %i, got %i).\n", 
                totalMipCount-1, static_cast<int>(arraySize));

        // note: unclamped loop and write into static sized array because we
        // have already confirmed that the texture mip count is sane.
        uint32_t index = 0;

        for (const auto& mipInfoEntry : mipInfoArray)
        {
            int32_t mipInfo;

            if (!JSON_ParseNumber(mipInfoEntry, mipInfo))
                Error("Failed to parse \"" TEXTURE_MIP_INFO_FIELD "\" #%u from texture metadata.\n", index);

            hdr->unkPerMip[index++] = static_cast<uint8_t>(mipInfo);
        }
    }
}

// materialGeneratedTexture - whether this texture's creation was invoked by material automatic texture generation
static void Texture_InternalAddTexture(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const bool forceDisableStreaming)
{
    PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);

    const std::string textureFilePath = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, ".dds");
    BinaryIO input;

    if (!input.Open(textureFilePath, BinaryIO::Mode_e::Read))
        Error("Failed to open texture asset \"%s\".\n", textureFilePath.c_str());

    PakPageLump_s hdrChunk = pak->CreatePageLump(sizeof(TextureAssetHeader_t), SF_HEAD, 8);
    TextureAssetHeader_t* const hdr = reinterpret_cast<TextureAssetHeader_t*>(hdrChunk.data);

    // used for creating data buffers
    struct {
        size_t staticSize;
        size_t streamedSize;
        size_t streamedOptSize;
    } mipSizes{};

    // parse input image file
    int magic;
    input.Read(magic);

    if (magic != DDS_MAGIC) // b'DDS '
        Error("Attempted to add a texture asset that was not a valid DDS file (invalid magic).\n");

    DDS_HEADER ddsh;
    input.Read(ddsh);

    if (ddsh.dwMipMapCount > MAX_MIPS_PER_TEXTURE)
        Error("Attempted to add a texture asset with too many mipmaps (max %u, got %u).\n", MAX_MIPS_PER_TEXTURE, ddsh.dwMipMapCount);

    Texture_ProcessMetaData(pak, assetPath, hdr, ddsh.dwMipMapCount);

    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;

    uint8_t arraySize = 1;
    bool isDX10 = false;

    // Go to the end of the DX10 header if it exists.
    if (ddsh.ddspf.dwFourCC == '01XD')
    {
        DDS_HEADER_DXT10 ddsh_dx10;
        input.Read(ddsh_dx10);

        dxgiFormat = ddsh_dx10.dxgiFormat;
        arraySize = static_cast<uint8_t>(ddsh_dx10.arraySize);
        isDX10 = true;
    }
    else {
        dxgiFormat = DXUtils::GetFormatFromHeader(ddsh);

        if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
            Error("Attempted to add a texture asset from which the format type couldn't be classified.\n");
    }

    const char* const pDxgiFormat = DXUtils::GetFormatAsString(dxgiFormat);
    const uint16_t imageFormat = Texture_DXGIToImageFormat(dxgiFormat);

    if (imageFormat == TEXTURE_INVALID_FORMAT_INDEX)
        Error("Attempted to add a texture asset using an unsupported format type \"%s\".\n", pDxgiFormat);

    hdr->imageFormat = imageFormat;
    Log("-> fmt: %s\n", pDxgiFormat);

    hdr->width = static_cast<uint16_t>(ddsh.dwWidth);
    hdr->height = static_cast<uint16_t>(ddsh.dwHeight);
    Log("-> dimensions: %ux%u\n", ddsh.dwWidth, ddsh.dwHeight);

    bool isStreamable = false; // does this texture require streaming? true if total size of mip levels would exceed 64KiB. can be forced to false.
    bool isStreamableOpt = false; // can this texture use optional starpaks? can only be set if pak is version v8

    // set streamable boolean based on if we have disabled it, also don't stream if we have only one mip
    if (!forceDisableStreaming && ddsh.dwMipMapCount > 1)
        isStreamable = true;

    if (isStreamable && pak->GetVersion() >= 8)
        isStreamableOpt = true;

    /*MIPMAP HANDLING*/
    hdr->arraySize = arraySize;
    std::vector<std::vector<mipLevel_t>> textureArray(arraySize);

    for (auto& mips : textureArray)
        mips.resize(ddsh.dwMipMapCount);

    size_t mipOffset = isDX10 ? 0x94 : 0x80; // add header length
    unsigned int streamedMipCount = 0;

    bool firstTexture = true;

    for (auto& mips : textureArray)
    {
        for (unsigned int mipLevel = 0; mipLevel < mips.size(); mipLevel++)
        {
            // subtracts 1 so skip mips w/h at 1, gets added back when setting in mipLevel_t
            uint16_t mipWidth = 0;
            if (hdr->width >> mipLevel > 1)
                mipWidth = (hdr->width >> mipLevel) - 1;

            uint16_t mipHeight = 0;
            if (hdr->height >> mipLevel > 1)
                mipHeight = (hdr->height >> mipLevel) - 1;

            const auto& bytesPerPixel = s_pBytesPerPixel[hdr->imageFormat];

            const uint8_t x = bytesPerPixel.x;
            const uint8_t y = bytesPerPixel.y;

            const uint32_t bppWidth = (y + mipWidth) >> (y >> 1);
            const uint32_t bppHeight = (y + mipHeight) >> (y >> 1);

            const uint32_t slicePitch = x * bppWidth * bppHeight;
            const uint32_t alignedSize = IALIGN16(slicePitch);

            mipLevel_t& mipMap = mips[mipLevel];

            mipMap.mipOffset = mipOffset;
            mipMap.mipSize = slicePitch;
            mipMap.mipSizeAligned = alignedSize;
            mipMap.mipWidth = static_cast<uint16_t>(mipWidth + 1);
            mipMap.mipHeight = static_cast<uint16_t>(mipHeight + 1);
            mipMap.mipLevel = static_cast<uint8_t>(mipLevel + 1);
            mipMap.mipType = mipType_e::INVALID;

            hdr->dataSize += alignedSize; // all mips are aligned to 16 bytes within rpak/starpak
            mipOffset += slicePitch; // add size for the next mip's offset

            // important:
            // - texture arrays cannot be streamed, the mips for each texture
            //   must be equally mapped and they can only be stored permanently.
            // 
            // - we cannot have more than 4 streamed mip levels; the engine can
            //   only store up to 4 streamable mip handles per texture, anything
            //   else must be stored as a permanent mip!
            //
            // - there must always be at least 1 permanent mip level, regardless
            //   of its size. not adhering to this rule will result in a failure
            //   in ID3D11Device::CreateTexture2D during the runtime. we make
            //   sure that the smallest mip is always permanent (static) here.
            if (arraySize == 1 && (streamedMipCount < MAX_STREAMED_TEXTURE_MIPS) && (mipLevel != (ddsh.dwMipMapCount - 1)))
            {
                // if opt streamable textures are enabled, check if this mip is supposed to be opt streamed
                if (isStreamableOpt && alignedSize > MAX_STREAM_MIP_SIZE)
                {
                    mipSizes.streamedOptSize += alignedSize; // only reason this is done is to create the data buffers
                    hdr->optStreamedMipLevels++; // add a streamed mip level

                    mipMap.mipType = mipType_e::STREAMED_OPT;
                }

                // if streamable textures are enabled, check if this mip is supposed to be streamed
                else if (isStreamable && alignedSize > MAX_PERM_MIP_SIZE)
                {
                    mipSizes.streamedSize += alignedSize; // only reason this is done is to create the data buffers
                    hdr->streamedMipLevels++; // add a streamed mip level

                    mipMap.mipType = mipType_e::STREAMED;
                }

                streamedMipCount++;
            }

            // texture was not streamed, make it permanent.
            if (mipMap.mipType == mipType_e::INVALID)
            {
                mipSizes.staticSize += alignedSize;

                // Only count mips for the first texture, other textures in the
                // array must have an equal amount of mips as all mips between
                // textures in the array are grouped together into a contiguous
                // block of memory, and indexed by the aligned mip size times
                // the texture index.
                if (firstTexture)
                    hdr->mipLevels++;

                mipMap.mipType = mipType_e::STATIC;
            }
        }

        firstTexture = false;
    }

    hdr->guid = assetGuid;
    Log("-> total mipmaps permanent:mandatory:optional : %hhu:%hhu:%hhu\n", hdr->mipLevels, hdr->streamedMipLevels, hdr->optStreamedMipLevels);

    if (pak->IsFlagSet(PF_KEEP_DEV))
    {
        char pathStem[256];
        const size_t stemLen = Pak_ExtractAssetStem(assetPath, pathStem, sizeof(pathStem));

        if (stemLen > 0)
        {
            PakPageLump_s nameChunk = pak->CreatePageLump(stemLen + 1, SF_CPU | SF_DEV, 1);
            memcpy(nameChunk.data, pathStem, stemLen + 1);

            pak->AddPointer(hdrChunk, offsetof(TextureAssetHeader_t, name), nameChunk, 0);
        }
    }

    PakPageLump_s dataChunk = pak->CreatePageLump(mipSizes.staticSize, SF_CPU | SF_TEMP, 16);
    char* const streamedbuf = new char[mipSizes.streamedSize];
    char* const optstreamedbuf = new char[mipSizes.streamedOptSize];

    for (size_t i = 0; i < textureArray.size(); i++)
    {
        const auto& mips = textureArray[i];

        char* pCurrentPosStatic = dataChunk.data;
        char* pCurrentPosStreamed = streamedbuf;
        char* pCurrentPosStreamedOpt = optstreamedbuf;

        for (auto mipIter = mips.rbegin(); mipIter != mips.rend(); ++mipIter)
        {
            const mipLevel_t& mipMap = *mipIter;
            input.SeekGet(mipMap.mipOffset);

            // only used by static mip types, used for offsetting the pointer
            // from mip base into the actual mip corresponding to the texture
            // in the array.
            char* targetPos;

            switch (mipMap.mipType)
            {
            case mipType_e::STATIC:
                targetPos = pCurrentPosStatic + (mipMap.mipSizeAligned * i);
                input.Read(targetPos, mipMap.mipSize);

                // texture arrays group mips together, i.e. mip 1 of texture 1
                // 2 and 3 are directly placed into a contiguous block, and to
                // access the second one, the mip size must be multiplied by
                // the texture index.
                pCurrentPosStatic += mipMap.mipSizeAligned * textureArray.size(); // move ptr

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
        }
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

    asset.InitAsset(hdrChunk.GetPointer(), sizeof(TextureAssetHeader_t), dataChunk.GetPointer(), mandatoryStreamDataOffset, optionalStreamDataOffset, TXTR_VERSION, AssetType::TXTR);
    asset.SetHeaderPointer(hdrChunk.data);

    pak->FinishAsset();
}

bool Texture_AutoAddTexture(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const bool forceDisableStreaming)
{
    PakAsset_t* const existingAsset = pak->GetAssetByGuid(assetGuid, nullptr, true);

    if (existingAsset)
        return false; // already present in the pak.

    Log("Auto-adding 'txtr' asset \"%s\".\n", assetPath);
    Texture_InternalAddTexture(pak, assetGuid, assetPath, forceDisableStreaming);

    return true;
}

void Assets::AddTextureAsset_v8(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    const bool disableStreaming = JSON_GetValueOrDefault(mapEntry, "$disableStreaming", false);
    Texture_InternalAddTexture(pak, assetGuid, assetPath, disableStreaming);
}
