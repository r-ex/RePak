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

void Assets::AddShaderSetAsset_v8(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	std::vector<PakGuidRefHdr_t> guids{};

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderSetAssetHeader_v8_t), SF_HEAD, 8);

	const std::string assetPathWithoutExtension = Utils::ChangeExtension(assetPath, "");

	CPakDataChunk nameChunk = pak->CreateDataChunk(assetPathWithoutExtension.length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), assetPathWithoutExtension.c_str());

	ShaderSetAssetHeader_v8_t* const hdr = reinterpret_cast<ShaderSetAssetHeader_v8_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v8_t, name)));

	// dedup
	// === Shader Inputs ===
	const PakGuid_t vertexShaderGuid = Pak_ParseGuidRequired(mapEntry, "vertexShader");
	const PakGuid_t pixelShaderGuid = Pak_ParseGuidRequired(mapEntry, "pixelShader");
	// end dedup

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

	// dedup
	// TEMPORARY VARS

	uint32_t numVertexShaderTextures;

	if (JSON_GetValue(mapEntry, "$numVertexShaderTextures", numVertexShaderTextures))
	{
		Debug("Overriding field \"$numVertexShaderTextures\" for shader set '%s'.\n", assetPath);
		hdr->textureInputCounts[0] = static_cast<uint16_t>(numVertexShaderTextures);
	}
	else if (!vertexShader && hdr->vertexShader != 0) // warn if there is an asset guid, but it's not present in the current pak
		Warning("Creating shader set '%s' without a local vertex shader asset and without a \'$numVertexShaderTextures\' field. Shader set will assume 0 vertex shader textures.\n", assetPath);

	uint32_t numPixelShaderTextures;

	if (JSON_GetValue(mapEntry, "$numPixelShaderTextures", numPixelShaderTextures))
	{
		Debug("Overriding field \"$numPixelShaderTextures\" for shader set '%s'.\n", assetPathWithoutExtension.c_str());
		hdr->textureInputCounts[1] = static_cast<uint16_t>(numPixelShaderTextures);
	}
	else if (!pixelShader && hdr->pixelShader != 0) // warn if there is an asset guid, but it's not present in the current pak
		Warning("Creating shader set '%s' without a local pixel shader asset and without a \'$numPixelShaderTextures\' field. Shader set will assume 0 pixel shader textures.\n", assetPath);

	// end dedup

	hdr->numSamplers = static_cast<uint16_t>(JSON_GetNumberOrDefault(mapEntry, "numSamplers", 0u));

	// only used for ui/ui_world shadersets in R5
	// for now this will have to be manually set if used, because i cannot figure out a way to programmatically
	// detect these resources without incorrectly identifying some, and i doubt that's a good thing
	hdr->firstResourceBindPoint = static_cast<uint8_t>(JSON_GetNumberOrDefault(mapEntry, "firstResource", 0u));
	hdr->numResources = static_cast<uint8_t>(JSON_GetNumberOrDefault(mapEntry, "numResources", 0u));

	if (hdr->numResources != 0)
		Warning("Shader set '%s' has requested a non-zero number of shader resources. This feature is only intended for use on UI shaders, and may result in unexpected crashes or errors when used with incompatible shader code.\n", assetPath);
	
	const PakGuid_t assetGuid = Pak_GetGuidOverridable(mapEntry, assetPath);

	PakAsset_t asset;
	asset.InitAsset(
		assetPath,
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

void Assets::AddShaderSetAsset_v11(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	std::vector<PakGuidRefHdr_t> guids{};

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderSetAssetHeader_v11_t), SF_HEAD, 8);

	const std::string assetPathWithoutExtension = Utils::ChangeExtension(assetPath, "");

	CPakDataChunk nameChunk = pak->CreateDataChunk(assetPathWithoutExtension.length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), assetPathWithoutExtension.c_str());

	ShaderSetAssetHeader_v11_t* const hdr = reinterpret_cast<ShaderSetAssetHeader_v11_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_v11_t, name)));

	// dedup
	// === Shader Inputs ===
	const PakGuid_t vertexShaderGuid = Pak_ParseGuidRequired(mapEntry, "vertexShader");
	const PakGuid_t pixelShaderGuid = Pak_ParseGuidRequired(mapEntry, "pixelShader");
	// end dedup

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

	// dedup
	// TEMPORARY VARS

	uint32_t numVertexShaderTextures;

	if (JSON_GetValue(mapEntry, "$numVertexShaderTextures", numVertexShaderTextures))
	{
		Debug("Overriding field \"$numVertexShaderTextures\" for shader set '%s'.\n", assetPath);
		hdr->textureInputCounts[0] = static_cast<uint16_t>(numVertexShaderTextures);
	}
	else if (!vertexShader && hdr->vertexShader != 0) // warn if there is an asset guid, but it's not present in the current pak
		Warning("Creating shader set '%s' without a local vertex shader asset and without a \'numVertexShaderTextures\' field. Shader set will assume 0 vertex shader textures.\n", assetPath);

	uint32_t numPixelShaderTextures;

	if (JSON_GetValue(mapEntry, "$numPixelShaderTextures", numPixelShaderTextures))
	{
		Debug("Overriding field \"$numPixelShaderTextures\" for shader set '%s'.\n", assetPath);
		hdr->textureInputCounts[1] = static_cast<uint16_t>(numPixelShaderTextures);
	}
	else if (!pixelShader && hdr->pixelShader != 0) // warn if there is an asset guid, but it's not present in the current pak
		Warning("Creating shader set '%s' without a local pixel shader asset and without a \'numPixelShaderTextures\' field. Shader set will assume 0 pixel shader textures.\n", assetPath);

	// end dedup

	hdr->numSamplers = static_cast<uint16_t>(JSON_GetNumberOrDefault(mapEntry, "numSamplers", 0u));

	// only used for ui/ui_world shadersets in R5
	// for now this will have to be manually set if used, because i cannot figure out a way to programmatically
	// detect these resources without incorrectly identifying some, and i doubt that's a good thing
	hdr->firstResourceBindPoint = static_cast<uint8_t>(JSON_GetNumberOrDefault(mapEntry, "firstResource", 0u));
	hdr->numResources = static_cast<uint8_t>(JSON_GetNumberOrDefault(mapEntry, "numResources", 0u));

	if (hdr->numResources != 0)
		Warning("Shader set '%s' has requested a non-zero number of shader resources. This feature is only intended for use on UI shaders, and may result in unexpected crashes or errors when used with incompatible shader code.\n", assetPath);

	const PakGuid_t assetGuid = Pak_GetGuidOverridable(mapEntry, assetPath);

	PakAsset_t asset;
	asset.InitAsset(
		assetPath,
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