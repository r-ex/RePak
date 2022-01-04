#include "pch.h"
#include "rtech.h"
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <sstream>

std::vector<RPakVirtualSegment> g_vvSegments{};
std::vector<RPakVirtualSegmentBlock> g_vvSegmentBlocks{};
std::vector<RPakUnknownBlockThree> g_vUnkThree{};
std::vector<RPakUnknownBlockFive> g_vUnkFive{};
std::vector<RPakRelationBlock> g_vUnkSix{};;
std::vector<RPakRawDataBlock> g_vSubHeaderBlocks{};
std::vector<RPakRawDataBlock> g_vRawDataBlocks{};

using namespace rapidjson;

///
//
//  THIS ENTIRE APP IS PAINFULLY HARDCODED
//  IT NEEDS TO BE MADE MORE DYNAMIC
//  OTHERWISE WE JUST GET SINGLE ASSET RPAKS EVERYWHERE
//
///

uint32_t CreateNewSegment(uint64_t size, SegmentType type, RPakVirtualSegment& seg)
{
    uint32_t idx = g_vvSegments.size();

    RPakVirtualSegment vseg{0, (uint32_t)type, size};
    RPakVirtualSegmentBlock vsegblock{g_vvSegments.size(), (uint32_t)type, size};

    g_vvSegments.emplace_back(vseg);
    g_vvSegmentBlocks.emplace_back(vsegblock);

    seg = vseg;
    return idx;
}

FILETIME GetFileTimeBySystem()
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ft;
}

void AddTextureAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* pszFilePath)
{
    TextureHeader* hdr = new TextureHeader();

    BinaryIO input;
    input.open(pszFilePath, BinaryIOMode::BinaryIOMode_Read);

    uint64_t nInputFileSize = Utils::GetFileSize(pszFilePath);

    std::string sAssetName = pszFilePath; // todo: this needs to be changed to the actual name

    {
        int magic;
        input.read(magic);
        
        if (magic != 0x20534444) // b'DDS '
        {
            printf("WARNING: Attempted to add txtr asset '%s' that was not a valid DDS file (invalid magic). Skipping asset...\n", pszFilePath);
            return;
        }

        DDS_HEADER ddsh = input.read<DDS_HEADER>();


        if (ddsh.pixelfmt.fourCC != '1TXD')
        {
            printf("WARNING: Attempted to add txtr asset '%s' that was not using a supported DDS type (currently only DXT1). Skipping asset...\n", pszFilePath);
            return;
        }

        hdr->DataSize = ddsh.pitchOrLinearSize;
        hdr->Width = ddsh.width;
        hdr->Height = ddsh.height;

        // TODO: support other texture formats
        hdr->Format = (uint8_t)TXTRFormat::DXT1;

        ///
        // ddsh.size is the size of the primary rpakHeader after the "DDS "
        ///
        // NOTE: when adding support for other formats, there may be a "secondary" rpakHeader after this point
        //       this rpakHeader is ONLY used when ddsh.pixelfmt.fourCC is "DX10"
        input.seek(ddsh.size + 4);
    }

    hdr->NameHash = StringToGuid(sAssetName.c_str());
    // rspn doesn't use named textures so why should we
    hdr->NameIndex = 0;
    hdr->NameOffset = 0;

    // unfortunately i'm not a respawn engineer so 1 (unstreamed) mip level will have to do
    hdr->MipLevels = 1;

    //memset(&hdr.UnknownPad, 0, sizeof(hdr.UnknownPad));

    // give us a segment to use for the subheader
    RPakVirtualSegment SubHeaderSegment;
    uint32_t shsIdx = CreateNewSegment(sizeof(TextureHeader), SegmentType::AssetSubHeader, SubHeaderSegment);

    // woo more segments
    RPakVirtualSegment RawDataSegment;
    uint32_t rdsIdx = CreateNewSegment(hdr->DataSize, SegmentType::AssetRawData, RawDataSegment);

    char* databuf = new char[hdr->DataSize];

    input.getReader()->read(databuf, hdr->DataSize);

    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)hdr };
    g_vRawDataBlocks.push_back(shdb);

    RPakRawDataBlock rdb{ rdsIdx, RawDataSegment.DataSize, (uint8_t*)databuf};
    g_vRawDataBlocks.push_back(rdb);

    // now time to add the higher level asset entry
    RPakAssetEntryV8 asset;
    asset.InitAsset(StringToGuid(sAssetName.c_str()), shsIdx, 0, SubHeaderSegment.DataSize, rdsIdx, 0, -1, -1, (std::uint32_t)AssetType::TEXTURE);
    assetEntries->push_back(asset);
}

template <typename T>
void WriteVector(BinaryIO& out, std::vector<T>& dataVector)
{
    for (auto it = dataVector.begin(); it != dataVector.end(); ++it)
    {
        out.write(*it);
    }
}

void WriteRPakRawDataBlock(BinaryIO& out, std::vector<RPakRawDataBlock>& rawDataBlock)
{
    for (auto it = rawDataBlock.begin(); it != rawDataBlock.end(); ++it)
    {
        out.getWriter()->write((char*)it->dataPtr, it->dataSize);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("invalid usage\n");
        return EXIT_FAILURE;
    }

    printf("==================\nbuilding rpak %s.rpak\n\n", argv[1]);

    std::ifstream ifs(argv[2]);

    if (!ifs.is_open())
    {
        printf("couldn't open map file. does it exist?\n");
        return EXIT_FAILURE;
    }

    IStreamWrapper isw{ ifs };

    Document doc{ };

    doc.ParseStream(isw);

    std::string assetsDir = std::string();

    if (!doc.HasMember("assetsDir"))
    {
        printf("!!! - No assets dir found. Assuming that everything is relative to the working directory.\n");
        assetsDir = ".\\";
    }
    else {
        assetsDir = doc["assetsDir"].GetStdString();
        char lchar = assetsDir[assetsDir.size() - 1];
        if (lchar != '\\' && lchar != '/')
        {
            assetsDir.append("/");
        }
    }

    std::vector<RPakAssetEntryV8> assetEntries{ };

    for (auto& file : doc["files"].GetArray())
    {
        if (file["$type"].GetStdString() == std::string("txtr"))
            AddTextureAsset(&assetEntries, std::string(assetsDir + file["path"].GetStdString() + ".dds").c_str());
    }

    std::filesystem::create_directory("build"); // create directory if it does not exist yet.

    BinaryIO out{ };
    RPakFileHeaderV8 rpakHeader{ };
    out.open("build/" + std::string(argv[1]) + ".rpak", BinaryIOMode::BinaryIOMode_Write); // open a new stream to the new file.
    out.write(rpakHeader); // write a placeholder rpakHeader that will be updated later with all the right values

    WriteVector(out, g_vvSegments);
    WriteVector(out, g_vvSegmentBlocks);
    WriteVector(out, g_vUnkThree);
    WriteVector(out, assetEntries);
    WriteVector(out, g_vUnkFive);
    WriteVector(out, g_vUnkSix);
    //WriteRPakRawDataBlock(out, g_vSubHeaderBlocks);
    WriteRPakRawDataBlock(out, g_vRawDataBlocks);


    FILETIME ft = GetFileTimeBySystem(); // Get system time as filetime.

    rpakHeader.CreatedTime = static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime; // File time to uint64_t for the created time.
    rpakHeader.CompressedSize = out.tell(); // Since we can't compress rpaks at the moment set both compressed and decompressed size as the full rpak size.
    rpakHeader.DecompressedSize = out.tell();
    rpakHeader.VirtualSegmentCount = g_vvSegments.size();
    rpakHeader.VirtualSegmentBlockCount = g_vvSegmentBlocks.size();
    rpakHeader.AssetEntryCount = assetEntries.size();

    out.seek(0); // Go back to the beginning to finally write the rpakHeader now.

    out.write(rpakHeader); // Re-write rpak header.
    return EXIT_SUCCESS;
}