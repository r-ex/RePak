#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "utils/utils.h"
#include "public/shaderset.h"
#include "public/shader.h"

// Parse numSamplers from pixelShader
// Parse textureInputCount from pixelShader
// Figure out the other count variable before texture input count
// See if any of the other unknown variables are actually required

void Assets::AddShaderSetAsset_v8(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	Log("Adding shds asset '%s'\n", assetPath);

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderSetAssetHeader_v8_t), SF_HEAD, 8);

	// uhhhh uhhhmmm ummmm uhhhhh
	const std::string assetPathWithoutExtension = fs::path(assetPath).replace_extension("").string();

	CPakDataChunk nameChunk = pak->CreateDataChunk(assetPathWithoutExtension.length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), assetPathWithoutExtension.c_str());

	ShaderSetAssetHeader_v8_t* const hdr = reinterpret_cast<ShaderSetAssetHeader_v8_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v8_t, name)));

	// === Shader Inputs === //
	const std::string vertexShaderInput = JSON_GET_STR(mapEntry, "vertexShader", "");
	const std::string pixelShaderInput = JSON_GET_STR(mapEntry, "pixelShader", "");

	const uint64_t vertexShaderGuid = RTech::GetAssetGUIDFromString(vertexShaderInput.c_str(), true);
	const uint64_t pixelShaderGuid = RTech::GetAssetGUIDFromString(pixelShaderInput.c_str(), true);

	if (vertexShaderGuid == 0)
		Error("Invalid vertexShader field for shaderset '%s'. Expected a string.\n", assetPathWithoutExtension.c_str());

	if (pixelShaderGuid == 0)
		Error("Invalid pixelShader field for shaderset '%s'. Expected a string.\n", assetPathWithoutExtension.c_str());

	hdr->vertexShader = vertexShaderGuid;
	hdr->pixelShader = pixelShaderGuid;

	std::vector<PakGuidRefHdr_t> guids{};
	pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v8_t, vertexShader)));
	pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v8_t, pixelShader)));

	// TEMPORARY VARS

	hdr->unk_10 = JSON_GET_UINT64(mapEntry, "unk_10", 0);
	hdr->unk_18 = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "unk_18", 0));
	hdr->textureInputCount = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "textureInputCount", 0));
	hdr->samplerCount = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "samplerCount", 0));
	hdr->unk_1E = static_cast<uint8_t>(JSON_GET_UINT(mapEntry, "unk_1E", 0));
	hdr->unk_1F = static_cast<uint8_t>(JSON_GET_UINT(mapEntry, "unk_1F", 0));

	PakAsset_t asset;
	asset.InitAsset(
		assetPathWithoutExtension + ".rpak",
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::SHDS);
	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = 8;

	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	asset.remainingDependencyCount = static_cast<short>(guids.size() + 1);

	asset.EnsureUnique(assetEntries);
	assetEntries->push_back(asset);

	printf("\n");
}