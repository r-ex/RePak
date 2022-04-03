#include "pch.h"
#include "Assets.h"

std::vector<RPakVirtualSegment> g_vvSegments{};
std::vector<RPakVirtualSegmentBlock> g_vvSegmentBlocks{};
std::vector<RPakDescriptor> g_vDescriptors{};
std::vector<RPakGuidDescriptor> g_vGuidDescriptors{};
std::vector<RPakRelationBlock> g_vFileRelations{};
std::vector<RPakRawDataBlock> g_vSubHeaderBlocks{};
std::vector<RPakRawDataBlock> g_vRawDataBlocks{};
std::vector<SRPkDataEntry> g_vSRPkDataEntries{};
std::vector<std::string> g_vsStarpakPaths{};
std::vector<std::string> g_vsOptStarpakPaths{};

uint64_t nextStarpakOffset = 0x1000;

using namespace rapidjson;

// flags_maybe is used because it's required for some segments but i have no way of knowing how it's supposed to be set
// idrk if it's even used for flags tbh
uint32_t RePak::CreateNewSegment(uint64_t size, uint32_t flags_maybe, uint32_t alignment, RPakVirtualSegment& seg, uint32_t vsegAlignment)
{
    uint32_t idx = g_vvSegmentBlocks.size();

    RPakVirtualSegment vseg{flags_maybe, vsegAlignment == -1 ? alignment : vsegAlignment, size};
    RPakVirtualSegmentBlock vsegblock{g_vvSegments.size(), alignment, size};

    g_vvSegments.emplace_back(vseg);
    g_vvSegmentBlocks.emplace_back(vsegblock);

    seg = vseg;
    return idx;
}

void RePak::AddStarpakReference(std::string path)
{
    for (auto& it : g_vsStarpakPaths)
    {
        if (it == path)
            return;
    }
    g_vsStarpakPaths.push_back(path);
}

// returns offset in starpak
uint64_t RePak::AddStarpakDataEntry(SRPkDataEntry block)
{
    size_t ns = Utils::PadBuffer((char**)&block.dataPtr, block.dataSize, 4096);

    block.dataSize = ns;
    block.offset = nextStarpakOffset;

    g_vSRPkDataEntries.push_back(block);

    nextStarpakOffset += block.dataSize;

    return block.offset;
}

void RePak::AddRawDataBlock(RPakRawDataBlock block)
{
    // this messes with descriptor registration
    //uint32_t alignment = g_vvSegmentBlocks[block.pageIdx].Alignment;
    //size_t ns = Utils::PadBuffer((char**)&block.dataPtr, block.dataSize, alignment);
    //
    //if(block.dataSize != ns)
    //    Debug("Aligned data block to %i bytes (%llx to %llx)\n", alignment, block.dataSize, ns);

    //block.dataSize = ns;

    g_vRawDataBlocks.push_back(block);
    return;
};

void RePak::RegisterDescriptor(uint32_t pageIdx, uint32_t pageOffset)
{
    g_vDescriptors.push_back({ pageIdx, pageOffset });
    return;
}

void RePak::RegisterGuidDescriptor(uint32_t pageIdx, uint32_t pageOffset)
{
    g_vGuidDescriptors.push_back({ pageIdx, pageOffset });
    return;
}

size_t RePak::AddFileRelation(uint32_t assetIdx, uint32_t count)
{
    for(int i = 0; i < count; ++i)
        g_vFileRelations.push_back({ assetIdx });
    return g_vFileRelations.size()-count; // return the index of the file relation(s)
}

RPakAssetEntryV8* RePak::GetAssetByGuid(std::vector<RPakAssetEntryV8>* assets, uint64_t guid, uint32_t* idx)
{
    uint32_t i = 0;
    for (auto& it : *assets)
    {
        if (it.GUID == guid)
        {
            if (idx)
                *idx = i;
            return &it;
        }
        i++;
    }
    return nullptr;
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

    std::ifstream ifs(argv[1]);

    if (!ifs.is_open())
    {
        printf("couldn't open map file. does it exist?\n");
        return EXIT_FAILURE;
    }

    IStreamWrapper isw{ ifs };

    Document doc{ };

    doc.ParseStream(isw);

    std::string sRpakName = DEFAULT_RPAK_NAME;

    if (doc.HasMember("name") && doc["name"].IsString())
        sRpakName = doc["name"].GetStdString();
    else
        Warning("Map file should have a 'name' field containing the string name for the new rpak, but none was provided. Defaulting to '%s.rpak' and continuing...\n", DEFAULT_RPAK_NAME);


    if (!doc.HasMember("assetsDir"))
    {
        Warning("No assetsDir field provided. Assuming that everything is relative to the working directory.\n");
        Assets::g_sAssetsDir = ".\\";
    }
    else {
        Assets::g_sAssetsDir = doc["assetsDir"].GetStdString();
        Utils::AppendSlash(Assets::g_sAssetsDir);
        Debug("assetsDir: %s\n", Assets::g_sAssetsDir.c_str());
    }

    std::string sOutputDir = "build/";

    if (doc.HasMember("outputDir"))
    {
        sOutputDir = doc["outputDir"].GetStdString();
        Utils::AppendSlash(sOutputDir);
    }

    Log("building rpak %s.rpak\n\n", sRpakName.c_str());

    std::vector<RPakAssetEntryV8> assetEntries{ };

    // loop through all assets defined in the map json
    for (auto& file : doc["files"].GetArray())
    {
        ASSET_HANDLER("txtr", file, assetEntries, Assets::AddTextureAsset);
        ASSET_HANDLER("uimg", file, assetEntries, Assets::AddUIImageAsset);
        ASSET_HANDLER("Ptch", file, assetEntries, Assets::AddPatchAsset);
        ASSET_HANDLER("dtbl", file, assetEntries, Assets::AddDataTableAsset);
        ASSET_HANDLER("rmdl", file, assetEntries, Assets::AddModelAsset);
        ASSET_HANDLER("matl", file, assetEntries, Assets::AddMaterialAsset);
    }

    std::filesystem::create_directories(sOutputDir); // create directory if it does not exist yet.

    BinaryIO out{ };

    // todo: v7 support?
    // No fuck you we won't support titanfall if apex repak doesn't even work
    RPakFileHeaderV8 rpakHeader{ };

    out.open(sOutputDir + sRpakName + ".rpak", BinaryIOMode::BinaryIOMode_Write); // open a new stream to the new file.
    out.write(rpakHeader); // write a placeholder rpakHeader that will be updated later with all the right values

    size_t StarpakRefLength = Utils::WriteStringVector(out, g_vsStarpakPaths);
    size_t OptStarpakRefLength = Utils::WriteStringVector(out, g_vsOptStarpakPaths);

    Utils::WriteVector(out, g_vvSegments);
    Utils::WriteVector(out, g_vvSegmentBlocks);
    Utils::WriteVector(out, g_vDescriptors);
    Utils::WriteVector(out, assetEntries);
    Utils::WriteVector(out, g_vGuidDescriptors);
    Utils::WriteVector(out, g_vFileRelations);
    WriteRPakRawDataBlock(out, g_vRawDataBlocks);

    FILETIME ft = Utils::GetFileTimeBySystem(); // Get system time as filetime.

    // set our header values since now we should have all the required info
    rpakHeader.CreatedTime = static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime; // File time to uint64_t for the created time.
    rpakHeader.CompressedSize = out.tell(); // Since we can't compress rpaks at the moment set both compressed and decompressed size as the full rpak size.
    rpakHeader.DecompressedSize = out.tell();
    rpakHeader.VirtualSegmentCount = g_vvSegments.size();
    rpakHeader.PageCount = g_vvSegmentBlocks.size();
    rpakHeader.DescriptorCount = g_vDescriptors.size();
    rpakHeader.GuidDescriptorCount = g_vGuidDescriptors.size();
    rpakHeader.RelationsCount = g_vFileRelations.size();
    rpakHeader.AssetEntryCount = assetEntries.size();
    rpakHeader.StarpakReferenceSize = StarpakRefLength;
    rpakHeader.StarpakOptReferenceSize = OptStarpakRefLength;

    out.seek(0); // Go back to the beginning to finally write the rpakHeader now.

    out.write(rpakHeader); // Re-write rpak header.
    
    out.close();

    // TODO: support multiple starpaks?
    if (g_vsStarpakPaths.size() == 1)
    {
        std::string sFullPath = g_vsStarpakPaths[0];
        std::filesystem::path path(sFullPath);

        std::string filename = path.filename().u8string();

        BinaryIO srpkOut;

        srpkOut.open(sOutputDir + filename, BinaryIOMode::BinaryIOMode_Write);

        int magic = 'kPRS';
        int version = 1;
        uint64_t entryCount = g_vSRPkDataEntries.size();

        srpkOut.write(magic);
        srpkOut.write(version);

        // data blocks in starpaks are all aligned to 4096 bytes, including the header which gets filled with 0xCB after the magic
        // and version
        char* why = new char[4088];
        memset(why, 0xCB, 4088);

        srpkOut.getWriter()->write(why, 4088);

        for (auto& it : g_vSRPkDataEntries)
        {
            srpkOut.getWriter()->write((const char*)it.dataPtr, it.dataSize);
        }

        // starpaks have a table of sorts at the end of the file, containing the offsets and data sizes for every data block
        // as far as i'm aware, this isn't even used by the game, so i'm not entirely sure why it exists?
        for (auto& it : g_vSRPkDataEntries)
        {
            SRPkFileEntry fe{};
            fe.offset = it.offset;
            fe.size = it.dataSize;

            srpkOut.write(fe);
        }

        srpkOut.write(entryCount);
        srpkOut.close();
    }
    return EXIT_SUCCESS;
}