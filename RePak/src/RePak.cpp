#include "pch.h"
#include "Assets.h"

using namespace rapidjson;

// purpose: create page and segment with the specified parameters
// idk what the second field is so "a2" is good enough
RPakVirtualSegment GetMatchingSegment(uint32_t flags, uint32_t a2, uint32_t* segidx)
{
    uint32_t i = 0;
    for (auto& it : g_vvSegments)
    {
        if (it.m_nDataFlag == flags && it.m_nSomeType == a2)
        {
            *segidx = i;
            return it;
        }
        i++;
    }

    return { flags, a2, 0 };
}

// purpose: create page and segment with the specified parameters
// returns: page index
_vseginfo_t RePak::CreateNewSegment(uint32_t size, uint32_t flags_maybe, uint32_t alignment, uint32_t vsegAlignment)
{
    uint32_t vsegidx = (uint32_t)g_vvSegments.size();
    
    // find existing "segment" with the same values or create a new one, this is to overcome the engine's limit of having max 20 of these
    // since otherwise we write into unintended parts of the stack, and that's bad
    RPakVirtualSegment seg = GetMatchingSegment(flags_maybe, vsegAlignment == -1 ? alignment : vsegAlignment, &vsegidx);

    bool bShouldAddVSeg = seg.m_nDataSize == 0;
    seg.m_nDataSize += size;

    if (bShouldAddVSeg)
        g_vvSegments.emplace_back(seg);
    else
        g_vvSegments[vsegidx] = seg;

    RPakPageInfo vsegblock{ vsegidx, alignment, size };

    g_vPages.emplace_back(vsegblock);
    uint32_t pageidx = (uint32_t)g_vPages.size() - 1;

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

RPakAssetEntry* RePak::GetAssetByGuid(std::vector<RPakAssetEntry>* assets, uint64_t guid, uint32_t* idx)
{
    uint32_t i = 0;
    for (auto& it : *assets)
    {
        if (it.m_nGUID == guid)
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
        out.getWriter()->write((char*)it->m_nDataPtr, it->m_nDataSize);
    }
}

const char startupVersion[] = {
    "RePak - Built "
    __DATE__
    " "
    __TIME__
    "\n\n"
};

int main(int argc, char** argv)
{
    printf(startupVersion);

    if (argc < 2)
        Error("invalid usage\n");

    std::filesystem::path mapPath(argv[1]);

    if (!FILE_EXISTS(argv[1]))
        Error("couldn't find map file\n");

    std::ifstream ifs(argv[1]);

    if (!ifs.is_open())
        Error("couldn't open map file.\n");

    // begin json parsing
    IStreamWrapper isw{ ifs };

    Document doc{ };

    doc.ParseStream(isw);

    std::string sRpakName = DEFAULT_RPAK_NAME;

    if (doc.HasMember("name") && doc["name"].IsString())
        sRpakName = doc["name"].GetStdString();
    else
        Warning("Map file should have a 'name' field containing the string name for the new rpak, but none was provided. Defaulting to '%s.rpak' and continuing...\n", DEFAULT_RPAK_NAME);

    Log("build settings:\n");
    Log("filename: %s\n", sRpakName.c_str());

    if (!doc.HasMember("assetsDir"))
    {
        Warning("No assetsDir field provided. Assuming that everything is relative to the working directory.\n");
        if (mapPath.has_parent_path())
            Assets::g_sAssetsDir = mapPath.parent_path().u8string();
        else
            Assets::g_sAssetsDir = ".\\";
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
        Log("assetsDir: %s\n", Assets::g_sAssetsDir.c_str());
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
        Log("outputDir: %s\n", sOutputDir.c_str());
    }

    if (!doc.HasMember("files"))
        Warning("No 'files' field specified, the RPak will contain no assets...\n");
    else if (!doc["files"].IsArray())
        Error("'files' field is not of required type 'array'. Exiting...\n");

    // end json parsing
    RPakFileBase* rpakFile = new RPakFileBase();
    
    if (!doc.HasMember("version"))
        Error("No RPak file version specified. Valid options:\n7 - Titanfall 2\n8 - Apex Legends\nExiting...\n");
    else if ( !doc["version"].IsInt() )
        Error("Invalid RPak file version specified. Valid options:\n7 - Titanfall 2\n8 - Apex Legends\nExiting...\n");

    int rpakVersion = doc["version"].GetInt();

    rpakFile->SetVersion(rpakVersion);

    Log("version: %i\n\n", rpakVersion);

    // build asset data
    // loop through all assets defined in the map json
    for (auto& file : doc["files"].GetArray())
    {
        rpakFile->HandleAsset(file);
    }

    std::filesystem::create_directories(sOutputDir); // create directory if it does not exist yet.

    BinaryIO out{ };

    out.open(sOutputDir + sRpakName + ".rpak", BinaryIOMode::Write);

    // write a placeholder header so we can come back and complete it
    // when we have all the info
    rpakFile->WriteHeader(&out);

    // write string vectors for starpak paths and get the total length of each vector
    size_t StarpakRefLength = Utils::WriteStringVector(out, Assets::g_vsStarpakPaths);
    size_t OptStarpakRefLength = Utils::WriteStringVector(out, Assets::g_vsOptStarpakPaths);

    // write the non-paged data to the file first
    WRITE_VECTOR(out, g_vvSegments);
    WRITE_VECTOR(out, g_vPages);
    WRITE_VECTOR(out, g_vDescriptors);
    rpakFile->WriteAssets(&out);
    WRITE_VECTOR(out, g_vGuidDescriptors);
    WRITE_VECTOR(out, g_vFileRelations);

    // write the external asset references here

    // now the actual paged data
    // this should probably be writing by page instead of just hoping that
    // the data blocks are in the right order
    WriteRPakRawDataBlock(out, g_vRawDataBlocks);

    // get current time as FILETIME
    FILETIME ft = Utils::GetFileTimeBySystem();

    rpakFile->header.m_nCreatedTime = static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime; // write the current time into the file as FILETIME
    rpakFile->header.m_nSizeDisk = out.tell();
    rpakFile->header.m_nSizeMemory = out.tell();
    rpakFile->header.m_nVirtualSegmentCount = (uint16_t)g_vvSegments.size();
    rpakFile->header.m_nPageCount = (uint16_t)g_vPages.size();
    rpakFile->header.m_nDescriptorCount = (uint32_t)g_vDescriptors.size();
    rpakFile->header.m_nGuidDescriptorCount = (uint32_t)g_vGuidDescriptors.size();
    rpakFile->header.m_nRelationsCount = (uint32_t)g_vFileRelations.size();
    rpakFile->header.m_nAssetEntryCount = rpakFile->GetAssetCount();
    rpakFile->header.m_nStarpakReferenceSize = (uint16_t)StarpakRefLength;
    rpakFile->header.m_nStarpakOptReferenceSize = (uint16_t)OptStarpakRefLength;

    out.seek(0); // Go back to the beginning to finally write the rpakHeader now.

    rpakFile->WriteHeader(&out);

    out.close();

    Debug("written rpak file with size %lld\n", rpakFile->header.m_nSizeDisk);

    // free the memory
    for (auto& it : g_vRawDataBlocks)
    {
        delete it.m_nDataPtr;
    }
    delete rpakFile;

    // write starpak data
    if (Assets::g_vsStarpakPaths.size() == 1)
    {
        std::string sFullPath = Assets::g_vsStarpakPaths[0];
        std::filesystem::path path(sFullPath);

        std::string filename = path.filename().u8string();

        Debug("writing starpak %s with %lld data entries\n", filename.c_str(), g_vSRPkDataEntries.size());
        BinaryIO srpkOut;

        srpkOut.open(sOutputDir + filename, BinaryIOMode::Write);

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
            srpkOut.getWriter()->write((const char*)it.m_nDataPtr, it.m_nDataSize);
        }

        // starpaks have a table of sorts at the end of the file, containing the offsets and data sizes for every data block
        // as far as i'm aware, this isn't even used by the game, so i'm not entirely sure why it exists?
        for (auto& it : g_vSRPkDataEntries)
        {
            SRPkFileEntry fe{};
            fe.m_nOffset= it.m_nOffset;
            fe.m_nSize = it.m_nDataSize;

            srpkOut.write(fe);
        }

        srpkOut.write(entryCount);

        Debug("written starpak file with size %lld\n", srpkOut.tell());

        srpkOut.close();
    }
    return EXIT_SUCCESS;
}