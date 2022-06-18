#include "pch.h"
#include "Assets.h"

using namespace rapidjson;

// purpose: create page and segment with the specified parameters
// idk what the second field is so "a2" is good enough
RPakVirtualSegment& GetMatchingSegment(uint32_t flags, uint32_t a2, uint32_t* segidx)
{
    uint32_t i = 0;
    for (auto& it : g_vvSegments)
    {
        if (it.DataFlag == flags && it.SomeType == a2)
        {
            *segidx = i;
            return it;
        }
        i++;
    }
    RPakVirtualSegment ret{ flags, a2, 0 };
    return ret;
}

// purpose: create page and segment with the specified parameters
// returns: page index
_vseginfo_t RePak::CreateNewSegment(uint32_t size, uint32_t flags_maybe, uint32_t alignment, uint32_t vsegAlignment)
{
    uint32_t vsegidx = g_vvSegments.size();
    
    // find existing "segment" with the same values or create a new one, this is to overcome the engine's limit of having max 20 of these
    // since otherwise we write into unintended parts of the stack, and that's bad
    RPakVirtualSegment& seg = GetMatchingSegment(flags_maybe, vsegAlignment == -1 ? alignment : vsegAlignment, &vsegidx);

    bool bShouldAddVSeg = seg.DataSize == 0;
    seg.DataSize += size;

    if(bShouldAddVSeg)
        g_vvSegments.emplace_back(seg);

    RPakPageInfo vsegblock{ vsegidx, alignment, size };

    g_vPages.emplace_back(vsegblock);
    uint32_t pageidx = g_vPages.size() - 1;

    return { pageidx, size};
}

void RePak::AddRawDataBlock(RPakRawDataBlock block)
{
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
    for(uint32_t i = 0; i < count; ++i)
        g_vFileRelations.push_back({ assetIdx });
    return g_vFileRelations.size()-count; // return the index of the file relation(s)
}

RPakAssetEntryV7* RePak::GetAssetByGuid(std::vector<RPakAssetEntryV7>* assets, uint64_t guid, uint32_t* idx)
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
    Debug("failed to find asset with guid %llX\n", guid);
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
        Error("invalid usage\n");
        return EXIT_FAILURE;
    }

    std::filesystem::path mapPath(argv[1]);
    if (!FILE_EXISTS(argv[1]))
    {
        Error("couldn't find map file\n");
        return EXIT_FAILURE;
    }

    std::ifstream ifs(argv[1]);

    if (!ifs.is_open())
    {
        Error("couldn't open map file.\n");
        return EXIT_FAILURE;
    }

    // begin json parsing
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
        if (mapPath.has_parent_path())
        {
            Assets::g_sAssetsDir = mapPath.parent_path().u8string();
        }
        else
        {
            Assets::g_sAssetsDir = ".\\";
        }
    }
    else
    {
        std::filesystem::path assetsDirPath(doc["assetsDir"].GetStdString());
        if (assetsDirPath.is_relative() && mapPath.has_parent_path())
            Assets::g_sAssetsDir = std::filesystem::canonical(mapPath.parent_path() / assetsDirPath).u8string();
        else
            Assets::g_sAssetsDir = assetsDirPath.u8string();

        // ensure that the path has a slash at the end
        Utils::AppendSlash(Assets::g_sAssetsDir);
        Debug("assetsDir: %s\n", Assets::g_sAssetsDir.c_str());
    }

    std::string sOutputDir = "build/";

    if (doc.HasMember("outputDir"))
    {
        std::filesystem::path outputDirPath(doc["outputDir"].GetStdString());

        if (outputDirPath.is_relative() && mapPath.has_parent_path())
            sOutputDir = std::filesystem::canonical(mapPath.parent_path() / outputDirPath).u8string();
        else
            sOutputDir = outputDirPath.u8string();

        // ensure that the path has a slash at the end
        Utils::AppendSlash(sOutputDir);
    }
    // end json parsing

    Log("building rpak %s.rpak\n\n", sRpakName.c_str());

    std::vector<RPakAssetEntryV7> assetEntries{ };

    // build asset data
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

    out.open(sOutputDir + sRpakName + ".rpak", BinaryIOMode::Write);

    // write a placeholder header so we can come back and complete it
    // when we have all the info
    RPakFileHeaderV7 rpakHeader{ };
    out.write(rpakHeader);

    // write string vectors for starpak paths and get the total length of each vector
    size_t StarpakRefLength = Utils::WriteStringVector(out, Assets::g_vsStarpakPaths);
    size_t OptStarpakRefLength = Utils::WriteStringVector(out, Assets::g_vsOptStarpakPaths);

    // write the non-paged data to the file first
    WRITE_VECTOR(out, g_vvSegments);
    WRITE_VECTOR(out, g_vPages);
    WRITE_VECTOR(out, g_vDescriptors);
    WRITE_VECTOR(out, assetEntries);
    WRITE_VECTOR(out, g_vGuidDescriptors);
    WRITE_VECTOR(out, g_vFileRelations);

    // now the actual paged data
    // this should probably be writing by page instead of just hoping that
    // the data blocks are in the right order
    WriteRPakRawDataBlock(out, g_vRawDataBlocks);

    // get current time as FILETIME
    FILETIME ft = Utils::GetFileTimeBySystem();

    // set up the file header
    rpakHeader.CreatedTime = static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime; // write the current time into the file as FILETIME
    rpakHeader.CompressedSize = out.tell();
    rpakHeader.DecompressedSize = out.tell();
    rpakHeader.VirtualSegmentCount = g_vvSegments.size();
    rpakHeader.PageCount = g_vPages.size();
    rpakHeader.DescriptorCount = g_vDescriptors.size();
    rpakHeader.GuidDescriptorCount = g_vGuidDescriptors.size();
    rpakHeader.RelationsCount = g_vFileRelations.size();
    rpakHeader.AssetEntryCount = assetEntries.size();
    rpakHeader.StarpakReferenceSize = StarpakRefLength;
    //rpakHeader.StarpakOptReferenceSize = OptStarpakRefLength;

    out.seek(0); // Go back to the beginning to finally write the rpakHeader now.

    out.write(rpakHeader); // Re-write rpak header.
    
    out.close();

    // free the memory
    for (auto& it : g_vRawDataBlocks)
    {
        delete it.dataPtr;
    }

    // write starpak data
    if (Assets::g_vsStarpakPaths.size() == 1)
    {
        std::string sFullPath = Assets::g_vsStarpakPaths[0];
        std::filesystem::path path(sFullPath);

        std::string filename = path.filename().u8string();

        BinaryIO srpkOut;

        srpkOut.open(sOutputDir + filename, BinaryIOMode::Write);

        int magic = 'kPRS';
        int version = 1;
        uint64_t entryCount = Assets::g_vSRPkDataEntries.size();

        srpkOut.write(magic);
        srpkOut.write(version);

        // data blocks in starpaks are all aligned to 4096 bytes, including the header which gets filled with 0xCB after the magic
        // and version
        char* why = new char[4088];
        memset(why, 0xCB, 4088);

        srpkOut.getWriter()->write(why, 4088);

        for (auto& it : Assets::g_vSRPkDataEntries)
        {
            srpkOut.getWriter()->write((const char*)it.dataPtr, it.dataSize);
        }

        // starpaks have a table of sorts at the end of the file, containing the offsets and data sizes for every data block
        // as far as i'm aware, this isn't even used by the game, so i'm not entirely sure why it exists?
        for (auto& it : Assets::g_vSRPkDataEntries)
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