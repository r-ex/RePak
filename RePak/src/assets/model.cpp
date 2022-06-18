#include "pch.h"
#include "Assets.h"

void Assets::AddModelAsset(std::vector<RPakAssetEntryV7>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding mdl_ asset '%s'\n", assetPath);

    std::string sAssetName = std::string(assetPath) + ".rmdl";

    ModelHeader* pHdr = new ModelHeader();

    std::string rmdlFilePath = g_sAssetsDir + sAssetName;
    std::string vgFilePath = g_sAssetsDir + std::string(assetPath) + ".vg";

    ///-------------------
    // Begin skeleton(.rmdl) input
    BinaryIO skelInput;
    skelInput.open(rmdlFilePath, BinaryIOMode::Read);

    studiohdr_t mdlhdr = skelInput.read<studiohdr_t>();

    if (mdlhdr.id != 0x54534449)
    {
        Warning("invalid file magic for model asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x54534449, mdlhdr.id);
        return;
    }

    if (mdlhdr.version != 54)
    {
        Warning("invalid version for model asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 0x36, mdlhdr.version);
        return;
    }

    uint32_t fileNameDataSize = sAssetName.length() + 1;

    char* pDataBuf = new char[fileNameDataSize + mdlhdr.dataLength];

    // write the model file path into the data buffer
    snprintf(pDataBuf, fileNameDataSize, "%s", sAssetName.c_str());

    // go back to the beginning of the file to read all the data
    skelInput.seek(0);

    // write the skeleton data into the data buffer
    skelInput.getReader()->read(pDataBuf + fileNameDataSize, mdlhdr.dataLength);
    skelInput.close();

    ///--------------------
    // Add VG data
    BinaryIO vgInput;
    vgInput.open(vgFilePath, BinaryIOMode::Read);

    BasicRMDLVGHeader bvgh = vgInput.read<BasicRMDLVGHeader>();

    if (bvgh.magic != 0x47567430)
    {
        Warning("invalid vg file magic for model asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x47567430, bvgh.magic);
        return;
    }

    if (bvgh.version != 1)
    {
        Warning("invalid vg version for model asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 1, bvgh.version);
        return;
    }

    vgInput.seek(0, std::ios::end);

    uint32_t vgFileSize = vgInput.tell();
    char* pVGBuf = new char[vgFileSize];

    vgInput.seek(0);
    vgInput.getReader()->read(pVGBuf, vgFileSize);
    vgInput.close();

    // static name for now
    RePak::AddStarpakReference("paks/Win64/repak.starpak");

    SRPkDataEntry de{ -1, vgFileSize, (uint8_t*)pVGBuf };
    uint64_t starpakOffset = RePak::AddStarpakDataEntry(de);

    pHdr->DataCacheSize = vgFileSize;

    // asset header
    _vseginfo_t subhdrinfo = RePak::CreateNewSegment(sizeof(ModelHeader), 0, 8);

    // data segment
    _vseginfo_t dataseginfo = RePak::CreateNewSegment(mdlhdr.dataLength + fileNameDataSize, 1, 64);

    //RPakVirtualSegment VGSegment{};
    //uint32_t vgIdx = RePak::CreateNewSegment(vgFileSize, 67, 1, DataSegment);

    pHdr->NamePtr = { dataseginfo.index, 0 };

    pHdr->SkeletonPtr = { dataseginfo.index, fileNameDataSize };

    //pHdr->VGPtr = { vgIdx, 0 };
    //pHdr->DataCacheSize = vgFileSize;

    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(ModelHeader, SkeletonPtr));
    RePak::RegisterDescriptor(subhdrinfo.index, offsetof(ModelHeader, NamePtr));
    //RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, VGPtr));

    rmem dataBuf(pDataBuf);
    dataBuf.seek(fileNameDataSize + mdlhdr.texture_offset, rseekdir::beg);

    // this shouldn't be needed - the game doesn't register these
    for (int i = 0; i < mdlhdr.texture_count; ++i)
    {
        materialref_t ref = dataBuf.read<materialref_t>();
        
        if(ref.guid != 0)
            RePak::RegisterGuidDescriptor(dataseginfo.index, dataBuf.getPosition()-8);
    }


    RPakRawDataBlock shdb{ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf };
    RePak::AddRawDataBlock(rdb);

    //RPakRawDataBlock vgdb{ vgIdx, vgFileSize, (uint8_t*)pVGBuf };
    //RePak::AddRawDataBlock(vgdb);

    RPakAssetEntryV7 asset;

    asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), subhdrinfo.index, 0, subhdrinfo.size, -1, 0, starpakOffset, -1, (std::uint32_t)AssetType::RMDL);
    asset.m_nVersion = RMDL_VERSION;
    // i have literally no idea what these are
    asset.m_nPageEnd = dataseginfo.index + 1;
    asset.unk1 = 2;

    // note: models use an implicit guid reference to their materials
    // the guids aren't registered but are converted during the loading of the material asset
    // during (what i assume is) regular reference conversion
    // a potential solution to the material guid conversion issue could be just registering the guids?
    // 
    size_t fileRelationIdx = RePak::AddFileRelation(assetEntries->size());
    asset.m_nUsesStartIdx = fileRelationIdx;
    asset.m_nUsesCount = 1;

    assetEntries->push_back(asset);

    Log("%i\n", g_vsStarpakPaths.size());
}