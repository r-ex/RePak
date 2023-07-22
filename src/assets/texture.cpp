#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "public/texture.h"

#define TXTR_ARRAY_SIZE(arraysize) (arraySize > 0 ? arraySize : 1)

bool CompareDxTextHeader(DDS_HEADER& source1, DDS_HEADER& source2)
{
    // check dimensions
    if ((source1.dwHeight != source2.dwHeight) || (source1.dwWidth != source2.dwWidth) || (source1.dwPitchOrLinearSize != source2.dwPitchOrLinearSize))
        return false;

    // check flags/data
    if ((source1.dwMipMapCount != source2.dwMipMapCount) || (source1.dwFlags != source2.dwFlags) || (source1.dwDepth != source2.dwDepth))
        return false;

    // check pre dx10 format
    if ((source1.ddspf.dwFourCC != source2.ddspf.dwFourCC) || (source1.ddspf.dwFlags != source1.ddspf.dwFlags))
        return false;

    return true;
}

void Assets::AddTextureAsset(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, bool forceDisableStreaming, int arraySize)
{
    Log("Adding txtr asset '%s'\n", assetPath);

    PakAsset_t* existingAsset = pak->GetAssetByGuid(RTech::GetAssetGUIDFromString(assetPath, true), nullptr, true);
    if (existingAsset)
    {
        Warning("Tried to add texture asset '%s' twice. Skipping redefinition...\n", assetPath);
        return;
    }

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(TextureAssetHeader_v8_t), SF_HEAD, 8);

    //TextureAssetHeader_t* hdr = reinterpret_cast<TextureAssetHeader_t*>(hdrChunk.Data());
    TextureAsset_t* hdr = new TextureAsset_t{};

    hdr->arraySize = arraySize;
    
    std::vector<std::string> inputs;

    if (arraySize > 0)
    {
        const char* fmt = "%s%s_%03d%s\0";
        char path[MAX_PATH]{};

        for (int i = 0; i < arraySize; i++)
        {
            snprintf(path, MAX_PATH, fmt, pak->GetAssetPath().c_str(), assetPath, i, ".dds");

            if (!FILE_EXISTS(std::filesystem::path(path)))
                Error("Texture %s did not exist, exiting...\n", path);

            inputs.push_back(std::string(path));

            BinaryIO texture(inputs.at(0), BinaryIOMode::Read);

            DDS_HEADER texBase = texture.read<DDS_HEADER>();

            texture.close();
            texture.open(inputs.back(), BinaryIOMode::Read);

            DDS_HEADER texCur = texture.read<DDS_HEADER>();

            texture.close();

            if (!CompareDxTextHeader(texBase, texCur))
                Error("Texture %s was not the same type as other members of the array, exiting...\n", path);
        }
    }
    else
    {
        std::string filePath = pak->GetAssetPath() + assetPath + ".dds";

        if (!FILE_EXISTS(filePath))
            Error("Failed to find texture source file %s. Exiting...\n", filePath.c_str());

        inputs.push_back(filePath);
    }

    std::string sAssetName = assetPath;

    bool isStreamable = false; // does this texture require streaming? true if total size of mip levels would exceed 64KiB. can be forced to false.
    bool isStreamableOpt = false; // can this texture use optional starpaks? can only be set if pak is version v8
    bool isDX10 = false;

    // parse input image file
    {
        BinaryIO input(inputs.at(0), BinaryIOMode::Read);

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

        size_t mipOffset = isDX10 ? 0x94 : 0x80; // add header length

        for (unsigned int i = 0; i  < ddsh.dwMipMapCount; i++)
        {
            // subtracts 1 so skip mips w/h at 1, gets added back when setting in mipLevel_t
            int mipWidth = 0;
            if (hdr->width >> i > 1)
                mipWidth = (hdr->width >> i) - 1;

            int mipHeight = 0;
            if (hdr->height >> i > 1)
                mipHeight = (hdr->height >> i) - 1;
            
            uint8_t x = s_pBytesPerPixel[hdr->imgFormat].first;
            uint8_t y = s_pBytesPerPixel[hdr->imgFormat].second;

            uint32_t bppWidth = (y + mipWidth) >> (y >> 1);
            uint32_t bppHeight = (y + mipHeight) >> (y >> 1);
            uint32_t sliceWidth = x * (y >> (y >> 1));

            uint32_t pitch = sliceWidth * bppWidth;
            uint32_t slicePitch = x * bppWidth * bppHeight;

            TextureMip_t mip{ mipOffset, slicePitch, IALIGN16(slicePitch), mipWidth + 1, mipHeight + 1, i + 1 };

            mip.offset = mipOffset;
            mip.size = slicePitch;
            mip.sizeAligned = IALIGN(slicePitch, 16); // 16 for pc, this would change if we wanted to pack for consoles

            mip.width = mipWidth + 1;
            mip.height = mipHeight + 1;

            mip.pitch = pitch;
            mip.slicePitch = slicePitch;

            mip.level = i;

            // if opt streamable textures are enabled, check if this mip is supposed to be opt streamed
            if (isStreamableOpt && mip.sizeAligned > MAX_PERM_MIP_SIZE && hdr->optStreamedMipLevels < 3) // max of two opt steamed mips like most paks
                mip.type = OPTSTREAMED;
            else if (isStreamable && mip.sizeAligned > MAX_PERM_MIP_SIZE)
                mip.type = STREAMED;
            else
                mip.type = PERM;

            mipOffset += mip.size; // add size for the next mip's offset

            switch (mip.type)
            {
            case PERM:
            {
                hdr->permanentSize += mip.sizeAligned;
                hdr->permanentMipLevels++;

                break;
            }
            case STREAMED:
            {
                hdr->streamedSize += mip.sizeAligned;
                hdr->streamedMipLevels++;

                break;
            }
            case OPTSTREAMED:
            {
                hdr->optStreamedSize += mip.sizeAligned; // only reason this is done is to create the data buffers
                hdr->optStreamedMipLevels++; // add an opt streamed mip level

                break;
            }
            default:
                break;
            }

            hdr->mips.push_back(mip);
        }

        hdr->totalMipLevels = static_cast<uint32_t>(ddsh.dwMipMapCount);
        hdr->dataSize = (hdr->permanentSize + hdr->streamedSize + hdr->optStreamedSize) * TXTR_ARRAY_SIZE(hdr->arraySize);
        Log("-> total mipmaps permanent:streamed:streamed opt : %i:%i:%i\n", hdr->permanentMipLevels, hdr->streamedMipLevels, hdr->optStreamedMipLevels);

        input.close();
    }

    hdr->guid = RTech::StringToGuid((sAssetName + ".rpak").c_str());

    if (pak->IsFlagSet(PF_KEEP_DEV))
    {
        CPakDataChunk nameChunk = pak->CreateDataChunk(sAssetName.size() + 1, SF_DEV | SF_CPU, 1);

        sprintf_s(nameChunk.Data(), sAssetName.length() + 1, "%s", sAssetName.c_str());

        hdr->name = nameChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(TextureAssetHeader_v8_t, name)));
    }

    hdr->WriteToBuffer(hdrChunk.Data(), 8);

    CPakDataChunk dataChunk = pak->CreateDataChunk(hdr->permanentSize * TXTR_ARRAY_SIZE(hdr->arraySize), SF_CPU | SF_TEMP, 16);
    char* streamedbuf = new char[hdr->streamedSize * TXTR_ARRAY_SIZE(hdr->arraySize)];
    char* optstreamedbuf = new char[hdr->optStreamedSize * TXTR_ARRAY_SIZE(hdr->arraySize)];

    char* pCurrentPosStatic = dataChunk.Data();
    char* pCurrentPosStreamed = streamedbuf;
    char* pCurrentPosStreamedOpt = optstreamedbuf;

    for (int i = hdr->totalMipLevels - 1; i >= 0; --i)
    {
        TextureMip_t& mip = hdr->mips.at(i);

        for (uint8_t arrayIdx = 0; arrayIdx < TXTR_ARRAY_SIZE(hdr->arraySize); arrayIdx++)
        {
            // [rika]: would it be faster to cycle through using one file at a time? could be done but setup like this for now as it's the easiest.
            BinaryIO curText(inputs.at(arrayIdx), BinaryIOMode::Read);
            curText.seek(mip.offset, std::ios::beg);

            switch (mip.type)
            {
            case PERM:
                curText.getReader()->read(pCurrentPosStatic, mip.size);
                pCurrentPosStatic += mip.sizeAligned; // move ptr

                break;
            case STREAMED:
                curText.getReader()->read(pCurrentPosStreamed, mip.size);
                pCurrentPosStreamed += mip.sizeAligned; // move ptr

                break;
            case OPTSTREAMED:
                curText.getReader()->read(pCurrentPosStreamedOpt, mip.size);
                pCurrentPosStreamedOpt += mip.sizeAligned; // move ptr

                break;
            default:
                break;
            }

            curText.close();
        }
    }

    // now time to add the higher level asset entry
    PakAsset_t asset;

    // this should hopefully fix some crashing
    uint64_t starpakOffset = -1;

    if (isStreamable && hdr->streamedMipLevels > 0)
    {
        StreamableDataEntry de{ 0, hdr->streamedSize * TXTR_ARRAY_SIZE(hdr->arraySize), (uint8_t*)streamedbuf};
        de = pak->AddStarpakDataEntry(de);
        starpakOffset = de.offset;
    }

    if (isStreamableOpt && hdr->optStreamedMipLevels > 0)
    {
        // do stuff
    }

    asset.InitAsset(sAssetName + ".rpak", hdrChunk.GetPointer(), hdrChunk.GetSize(), dataChunk.GetPointer(), starpakOffset, -1, (std::uint32_t)AssetType::TXTR);
    asset.version = 8;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 1; // only depends on itself

    asset.EnsureUnique(assetEntries);
    assetEntries->push_back(asset);

    printf("\n");

    delete hdr;
}

void Assets::AddTextureAsset_v8(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    AddTextureAsset(pak, assetEntries, assetPath, JSON_GET_BOOL(mapEntry, "disableStreaming", false), JSON_GET_INT(mapEntry, "arraySize", 0));
}
