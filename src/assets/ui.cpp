#include "pch.h"
#include "assets.h"
#include "public/ui.h"
#include "public/rui_package.h"

void UI_loadFromPackage(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath) {
    
    UNUSED(assetGuid);
    const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("ruip");
    RuiPackage rui{inputFilePath};
    
    PakAsset_t& asset = pak->BeginAsset(assetGuid,assetPath);
    PakPageLump_s hdrChunk = pak->CreatePageLump(sizeof(RuiHeader_v30_s),SF_HEAD|SF_CLIENT,8);
    RuiHeader_v30_s* ruiHdr = reinterpret_cast<RuiHeader_v30_s*>(hdrChunk.data);
    *ruiHdr = rui.CreateRuiHeader_v30();
    
    PakPageLump_s nameChunk = pak->CreatePageLump(rui.name.size(),SF_CPU|SF_CLIENT,8);
    memcpy(nameChunk.data,rui.name.data(),rui.name.size());
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,name),nameChunk,0);

    PakPageLump_s defaultValuesChunk = pak->CreatePageLump(rui.defaultData.size()+rui.defaultStrings.size(),SF_CPU|SF_CLIENT,8);
    memcpy(defaultValuesChunk.data,rui.defaultData.data(),rui.defaultData.size());
    memcpy(&defaultValuesChunk.data[rui.defaultData.size()],rui.defaultStrings.data(),rui.defaultStrings.size());
    for (uint16_t offset : rui.defaultStringOffsets) {
        uint64_t stringOffset = *reinterpret_cast<uint64_t*>(&rui.defaultData[offset])+rui.defaultData.size();
        pak->AddPointer(defaultValuesChunk,offset,defaultValuesChunk,stringOffset);
    }
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,dataStructInitData),defaultValuesChunk,0);
   
    PakPageLump_s transformDataChunk = pak->CreatePageLump(rui.transformData.size(),SF_CPU|SF_CLIENT,8);
    memcpy(transformDataChunk.data,rui.transformData.data(),rui.transformData.size());
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,transformData),transformDataChunk,0);

    PakPageLump_s argClustersChunk = pak->CreatePageLump(rui.argCluster.size()*sizeof(ArgCluster_s),SF_CPU|SF_CLIENT,8);
    memcpy(argClustersChunk.data,rui.argCluster.data(),rui.argCluster.size()*sizeof(ArgCluster_s));
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,argClusters),argClustersChunk,0);

    PakPageLump_s argumentsChunk = pak->CreatePageLump(rui.arguments.size()*sizeof(Argument_s),SF_CPU|SF_CLIENT,8);
    memcpy(argumentsChunk.data,rui.arguments.data(),rui.arguments.size()*sizeof(Argument_s));
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,arguments),argumentsChunk,0);
    
    ruiHdr->argNames = 0;

    PakPageLump_s styleDescriptorChunk = pak->CreatePageLump(rui.styleDescriptors.size(),SF_CPU|SF_CLIENT,8);
    memcpy(styleDescriptorChunk.data,rui.styleDescriptors.data(),rui.styleDescriptors.size());
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,styleDescriptors),styleDescriptorChunk,0);
    
    PakPageLump_s renderJobChunk = pak->CreatePageLump(rui.renderJobs.size(),SF_CPU|SF_CLIENT,8);
    memcpy(renderJobChunk.data,rui.renderJobs.data(),rui.renderJobs.size());
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,renderJobData),renderJobChunk,0);

    asset.InitAsset(hdrChunk.GetPointer(),sizeof(RuiHeader_v30_s),
        PagePtr_t::NullPtr(),30,AssetType::UI);
    asset.SetHeaderPointer(hdrChunk.data);

    pak->FinishAsset();

}



void Assets::AddRuiAsset_v30(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    UI_loadFromPackage(pak,assetGuid,assetPath);
}