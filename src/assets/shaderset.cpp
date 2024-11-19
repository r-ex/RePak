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

void Assets::AddShaderSetAsset_v8(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
	Log("Adding shds asset '%s'\n", assetPath);

	std::vector<PakGuidRefHdr_t> guids{};

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
		Error("No vertexShader field provided for shader set '%s'.\n", assetPathWithoutExtension.c_str());

	if (pixelShaderGuid == 0)
		Error("No pixelShader field provided for shader set '%s'.\n", assetPathWithoutExtension.c_str());

	hdr->vertexShader = vertexShaderGuid;
	hdr->pixelShader = pixelShaderGuid;

	if(hdr->vertexShader != 0)
		pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v8_t, vertexShader)));

	if(hdr->pixelShader != 0)
		pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v8_t, pixelShader)));

	PakAsset_t* const vertexShader = vertexShaderGuid != 0 ? pak->GetAssetByGuid(vertexShaderGuid, nullptr, true) : nullptr;
	PakAsset_t* const pixelShader = pixelShaderGuid != 0 ? pak->GetAssetByGuid(pixelShaderGuid, nullptr, true) : nullptr;

	if (vertexShader)
	{
		const ParsedDXShaderData_t* vtxShaderData = reinterpret_cast<const ParsedDXShaderData_t*>(vertexShader->PublicData());

		if (vtxShaderData)
		{
			hdr->textureInputCounts[0] = vtxShaderData->mtlTexSlotCount;
		}
	}

	if (pixelShader)
	{
		const ParsedDXShaderData_t* pixelShaderData = reinterpret_cast<const ParsedDXShaderData_t*>(pixelShader->PublicData());

		if (pixelShaderData)
		{
			hdr->textureInputCounts[1] = pixelShaderData->mtlTexSlotCount;
		}
	}

	// TEMPORARY VARS

	if (JSON_IS_UINT(mapEntry, "numVertexShaderTextures"))
	{
		Debug("Overriding field \"numVertexShaderTextures\" for shader set '%s'.\n", assetPathWithoutExtension.c_str());
		hdr->textureInputCounts[0] = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "numVertexShaderTextures", 0));
	}
	else if (!vertexShader && hdr->vertexShader != 0) // warn if there is an asset guid, but it's not present in the current pak
		Warning("Creating shader set '%s' without a local vertex shader asset and without a \'numVertexShaderTextures\' field. Shader set will assume 0 vertex shader textures.\n", assetPath);

	if (JSON_IS_UINT(mapEntry, "numPixelShaderTextures"))
	{
		Debug("Overriding field \"numPixelShaderTextures\" for shader set '%s'.\n", assetPathWithoutExtension.c_str());
		hdr->textureInputCounts[1] = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "numPixelShaderTextures", 0));
	}
	else if (!pixelShader && hdr->pixelShader != 0) // warn if there is an asset guid, but it's not present in the current pak
		Warning("Creating shader set '%s' without a local pixel shader asset and without a \'numPixelShaderTextures\' field. Shader set will assume 0 pixel shader textures.\n", assetPath);

	hdr->numSamplers = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "numSamplers", 0));

	// only used for ui/ui_world shadersets in R5
	// for now this will have to be manually set if used, because i cannot figure out a way to programmatically
	// detect these resources without incorrectly identifying some, and i doubt that's a good thing
	hdr->firstResourceBindPoint = static_cast<uint8_t>(JSON_GET_UINT(mapEntry, "firstResource", 0));
	hdr->numResources = static_cast<uint8_t>(JSON_GET_UINT(mapEntry, "numResources", 0));

	if (hdr->numResources != 0)
		Warning("Shader set '%s' has requested a non-zero number of shader resources. This feature is only intended for use on UI shaders, and may result in unexpected crashes or errors when used with incompatible shader code.\n", assetPath);
	
	uint64_t assetGuid = 0;

	if (JSON_IS_UINT64(mapEntry, "$guid"))
	{
		assetGuid = JSON_GET_UINT64(mapEntry, "$guid", 0);
	}
	else if (JSON_IS_STR(mapEntry, "$guid"))
	{
		RTech::ParseGUIDFromString(JSON_GET_STR(mapEntry, "$guid", ""), &assetGuid);
	}
	else
	{
		assetGuid = RTech::StringToGuid((assetPathWithoutExtension + ".rpak").c_str());
	}

	if (assetGuid == 0)
		Error("Invalid GUID provided for asset '%s'.\n", assetPath);

	PakAsset_t asset;
	asset.InitAsset(
		assetGuid,
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::SHDS);
	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = 8;

	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	asset.remainingDependencyCount = static_cast<short>(guids.size() + 1);

	asset.AddGuids(&guids);

	pak->PushAsset(asset);

	printf("\n");
}

void Assets::AddShaderSetAsset_v11(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
	Log("Adding shds asset '%s'\n", assetPath);

	std::vector<PakGuidRefHdr_t> guids{};

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderSetAssetHeader_v11_t), SF_HEAD, 8);

	// uhhhh uhhhmmm ummmm uhhhhh
	const std::string assetPathWithoutExtension = fs::path(assetPath).replace_extension("").string();

	CPakDataChunk nameChunk = pak->CreateDataChunk(assetPathWithoutExtension.length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), assetPathWithoutExtension.c_str());

	ShaderSetAssetHeader_v11_t* const hdr = reinterpret_cast<ShaderSetAssetHeader_v11_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v11_t, name)));

	// === Shader Inputs === //
	const std::string vertexShaderInput = JSON_GET_STR(mapEntry, "vertexShader", "");
	const std::string pixelShaderInput = JSON_GET_STR(mapEntry, "pixelShader", "");

	const uint64_t vertexShaderGuid = RTech::GetAssetGUIDFromString(vertexShaderInput.c_str(), true);
	const uint64_t pixelShaderGuid = RTech::GetAssetGUIDFromString(pixelShaderInput.c_str(), true);

	if (vertexShaderGuid == 0)
		Error("No vertexShader field provided for shader set '%s'.\n", assetPathWithoutExtension.c_str());

	if (pixelShaderGuid == 0)
		Error("No pixelShader field provided for shader set '%s'.\n", assetPathWithoutExtension.c_str());

	hdr->vertexShader = vertexShaderGuid;
	hdr->pixelShader = pixelShaderGuid;

	if (hdr->vertexShader != 0)
		pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v11_t, vertexShader)));

	if (hdr->pixelShader != 0)
		pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v11_t, pixelShader)));

	PakAsset_t* const vertexShader = vertexShaderGuid != 0 ? pak->GetAssetByGuid(vertexShaderGuid, nullptr, true) : nullptr;
	PakAsset_t* const pixelShader = pixelShaderGuid != 0 ? pak->GetAssetByGuid(pixelShaderGuid, nullptr, true) : nullptr;

	if (pixelShader)
	{
		const ParsedDXShaderData_t* pixelShaderData = reinterpret_cast<const ParsedDXShaderData_t*>(pixelShader->PublicData());

		if (pixelShaderData)
		{
			hdr->textureInputCounts[0] = pixelShaderData->mtlTexSlotCount;
		}
	}

	if (vertexShader)
	{
		const ParsedDXShaderData_t* vtxShaderData = reinterpret_cast<const ParsedDXShaderData_t*>(vertexShader->PublicData());

		if (vtxShaderData)
		{
			hdr->textureInputCounts[1] = vtxShaderData->mtlTexSlotCount;
		}
	}

	hdr->textureInputCounts[1] += hdr->textureInputCounts[0];

	// TEMPORARY VARS

	if (JSON_IS_UINT(mapEntry, "numVertexShaderTextures"))
	{
		Warning("Overriding field \"numVertexShaderTextures\" for shader set '%s'.\n", assetPathWithoutExtension.c_str());
		hdr->textureInputCounts[0] = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "numVertexShaderTextures", 0));
	}
	else if (!vertexShader && hdr->vertexShader != 0) // warn if there is an asset guid, but it's not present in the current pak
		Warning("Creating shader set '%s' without a local vertex shader asset and without a \'numVertexShaderTextures\' field. Shader set will assume 0 vertex shader textures.\n", assetPath);

	if (JSON_IS_UINT(mapEntry, "numPixelShaderTextures"))
	{
		Warning("Overriding field \"numPixelShaderTextures\" for shader set '%s'.\n", assetPathWithoutExtension.c_str());
		hdr->textureInputCounts[1] = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "numPixelShaderTextures", 0));
	}
	else if (!pixelShader && hdr->pixelShader != 0) // warn if there is an asset guid, but it's not present in the current pak
		Warning("Creating shader set '%s' without a local pixel shader asset and without a \'numPixelShaderTextures\' field. Shader set will assume 0 pixel shader textures.\n", assetPath);

	hdr->numSamplers = static_cast<uint16_t>(JSON_GET_UINT(mapEntry, "numSamplers", 0));

	// only used for ui/ui_world shadersets in R5
	// for now this will have to be manually set if used, because i cannot figure out a way to programmatically
	// detect these resources without incorrectly identifying some, and i doubt that's a good thing
	hdr->firstResourceBindPoint = static_cast<uint8_t>(JSON_GET_UINT(mapEntry, "firstResource", 0));
	hdr->numResources = static_cast<uint8_t>(JSON_GET_UINT(mapEntry, "numResources", 0));

	if (hdr->numResources != 0)
		Warning("Shader set '%s' has requested a non-zero number of shader resources. This feature is only intended for use on UI shaders, and may result in unexpected crashes or errors when used with incompatible shader code.\n", assetPath);

	uint64_t assetGuid = 0;

	if (JSON_IS_UINT64(mapEntry, "$guid"))
	{
		assetGuid = JSON_GET_UINT64(mapEntry, "$guid", 0);
	}
	else if(JSON_IS_STR(mapEntry, "$guid"))
	{
		RTech::ParseGUIDFromString(JSON_GET_STR(mapEntry, "$guid", ""), &assetGuid);
	}
	else
	{
		assetGuid = RTech::StringToGuid((assetPathWithoutExtension + ".rpak").c_str());
	}

	if (assetGuid == 0)
		Error("Invalid GUID provided for asset '%s'.\n", assetPath);

	PakAsset_t asset;
	asset.InitAsset(
		assetGuid,
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::SHDS);
	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = 11;

	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	asset.remainingDependencyCount = static_cast<short>(guids.size() + 1);

	asset.AddGuids(&guids);

	pak->PushAsset(asset);

	printf("\n");
}