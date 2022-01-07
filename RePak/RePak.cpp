#include "pch.h"
#include "Assets.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

std::vector<RPakVirtualSegment> g_vvSegments{};
std::vector<RPakVirtualSegmentBlock> g_vvSegmentBlocks{};
std::vector<RPakUnknownBlockThree> g_vUnkThree{};
std::vector<RPakUnknownBlockFive> g_vUnkFive{};
std::vector<RPakRelationBlock> g_vUnkSix{};;
std::vector<RPakRawDataBlock> g_vSubHeaderBlocks{};
std::vector<RPakRawDataBlock> g_vRawDataBlocks{};


using namespace rapidjson;

uint32_t RePak::CreateNewSegment(uint64_t size, SegmentType type, RPakVirtualSegment& seg)
{
    uint32_t idx = g_vvSegments.size();

    RPakVirtualSegment vseg{0, (uint32_t)type, size};
    RPakVirtualSegmentBlock vsegblock{g_vvSegments.size(), (uint32_t)type, size};

    g_vvSegments.emplace_back(vseg);
    g_vvSegmentBlocks.emplace_back(vsegblock);

    seg = vseg;
    return idx;
}

inline void RePak::AddRawDataBlock(RPakRawDataBlock block)
{
    g_vRawDataBlocks.push_back(block);
    return;
};

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

    if (!doc.HasMember("assetsDir"))
    {
        printf("!!! - No assets dir found. Assuming that everything is relative to the working directory.\n");
        g_sAssetsDir = ".\\";
    }
    else {
        g_sAssetsDir = doc["assetsDir"].GetStdString();
        char lchar = g_sAssetsDir[g_sAssetsDir.size() - 1];
        if (lchar != '\\' && lchar != '/')
        {
            g_sAssetsDir.append("/");
        }
    }

    std::vector<RPakAssetEntryV8> assetEntries{ };

    for (auto& file : doc["files"].GetArray())
    {
        if (file["$type"].GetStdString() == std::string("txtr"))
            Assets::AddTextureAsset(&assetEntries, file["path"].GetString());
    }

    std::filesystem::create_directory("build"); // create directory if it does not exist yet.

    BinaryIO out{ };
    RPakFileHeaderV8 rpakHeader{ };
    out.open("build/" + std::string(argv[1]) + ".rpak", BinaryIOMode::BinaryIOMode_Write); // open a new stream to the new file.
    out.write(rpakHeader); // write a placeholder rpakHeader that will be updated later with all the right values

    Utils::WriteVector(out, g_vvSegments);
    Utils::WriteVector(out, g_vvSegmentBlocks);
    Utils::WriteVector(out, g_vUnkThree);
    Utils::WriteVector(out, assetEntries);
    Utils::WriteVector(out, g_vUnkFive);
    Utils::WriteVector(out, g_vUnkSix);
    //WriteRPakRawDataBlock(out, g_vSubHeaderBlocks);
    WriteRPakRawDataBlock(out, g_vRawDataBlocks);

    FILETIME ft = Utils::GetFileTimeBySystem(); // Get system time as filetime.

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