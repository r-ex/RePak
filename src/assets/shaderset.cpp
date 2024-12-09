#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "utils/utils.h"
#include "public/shaderset.h"
#include "public/shader.h"

template <typename ShaderSetAssetHeader_t>
static void ShaderSet_SetInputSlots(ShaderSetAssetHeader_t* const hdr, PakAsset_t* const shader, const bool isVertexShader, const rapidjson::Value& mapEntry, const int assetVersion)
{
	uint16_t* inputCount;

	// note: in v8 shader sets the vertex shader is set in the first slot, whereas
	// in v11 the vertex shader is set in the second slot and the first texture
	if (assetVersion == 8)
		inputCount = isVertexShader ? &hdr->textureInputCounts[0] : &hdr->textureInputCounts[1];
	else
		inputCount = isVertexShader ? &hdr->textureInputCounts[1] : &hdr->textureInputCounts[0];

	uint32_t numShaderTextures;

	const char* const shaderName = isVertexShader ? "vertex" : "pixel";
	const char* const fieldName = isVertexShader ? "$numVertexShaderTextures" : "$numPixelShaderTextures";

	if (JSON_GetValue(mapEntry, fieldName, numShaderTextures))
	{
		Log("Overriding number of %s shader textures to %u\n", shaderName, numShaderTextures);
		*inputCount = static_cast<uint16_t>(numShaderTextures);
	}
	else if (shader)
	{
		const ParsedDXShaderData_t* const shaderData = reinterpret_cast<const ParsedDXShaderData_t*>(shader->PublicData());

		// cannot be null at this point
		assert(shaderData);
		*inputCount = shaderData->mtlTexSlotCount;
	}
	else
	{
		const PakGuid_t shaderGuid = isVertexShader ? hdr->vertexShader : hdr->pixelShader;

		// todo(amos): this should probably be an error?
		if (shaderGuid != 0) // warn if there is an asset guid, but it's not present in the current pak
			Warning("Creating shader set without a local %s shader asset and \"%s\" wasn't provided; assuming 0 shader textures.\n", shaderName, fieldName);
	}
}

template <typename ShaderSetAssetHeader_t>
void ShaderSet_CreateSet(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry, const int assetVersion)
{
	std::vector<PakGuidRefHdr_t> guids{};

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderSetAssetHeader_t), SF_HEAD, 8);

	const std::string assetPathWithoutExtension = Utils::ChangeExtension(assetPath, "");

	CPakDataChunk nameChunk = pak->CreateDataChunk(assetPathWithoutExtension.length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), assetPathWithoutExtension.c_str());

	ShaderSetAssetHeader_t* const hdr = reinterpret_cast<ShaderSetAssetHeader_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_t, name)));

	// === Shader Inputs ===
	const PakGuid_t vertexShaderGuid = Pak_ParseGuidRequired(mapEntry, "vertexShader");
	const PakGuid_t pixelShaderGuid = Pak_ParseGuidRequired(mapEntry, "pixelShader");

	hdr->vertexShader = vertexShaderGuid;
	hdr->pixelShader = pixelShaderGuid;

	// todo: can shader refs be null???
	if (hdr->vertexShader != 0)
		pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_t, vertexShader)));

	if (hdr->pixelShader != 0)
		pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_t, pixelShader)));

	PakAsset_t* const vertexShader = vertexShaderGuid != 0 ? pak->GetAssetByGuid(vertexShaderGuid, nullptr, true) : nullptr;
	ShaderSet_SetInputSlots(hdr, vertexShader, true, mapEntry, assetVersion);

	PakAsset_t* const pixelShader = pixelShaderGuid != 0 ? pak->GetAssetByGuid(pixelShaderGuid, nullptr, true) : nullptr;
	ShaderSet_SetInputSlots(hdr, pixelShader, false, mapEntry, assetVersion);

	// On v11 shaders, the input count is added on top of the second one.
	if (assetVersion == 11)
		hdr->textureInputCounts[1] += hdr->textureInputCounts[0];

	hdr->numSamplers = static_cast<uint16_t>(JSON_GetNumberRequired(mapEntry, "numSamplers"));

	// only used for ui/ui_world shadersets in R5
	// for now this will have to be manually set if used, because i cannot figure out a way to programmatically
	// detect these resources without incorrectly identifying some, and i doubt that's a good thing
	hdr->firstResourceBindPoint = static_cast<uint8_t>(JSON_GetNumberOrDefault(mapEntry, "$firstResource", 0u)); // overridable
	hdr->numResources = static_cast<uint8_t>(JSON_GetNumberOrDefault(mapEntry, "$numResources", 0u)); // overridable

	if (hdr->numResources != 0)
		Warning("Shader set '%s' has requested a non-zero number of shader resources. This feature is only intended for use on UI shaders, and may result in unexpected crashes or errors when used with incompatible shader code.\n", assetPath);

	PakAsset_t asset;
	asset.InitAsset(
		assetPath,
		Pak_GetGuidOverridable(mapEntry, assetPath),
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::SHDS);
	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = assetVersion;
	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	// todo: can shader sets have external dependencies?
	asset.remainingDependencyCount = static_cast<short>(guids.size() + 1);

	asset.AddGuids(&guids);
	pak->PushAsset(asset);

	printf("\n");
}

// TODO:
// Parse numSamplers from pixelShader
// Parse textureInputCount from pixelShader
// Figure out the other count variable before texture input count
// See if any of the other unknown variables are actually required

void Assets::AddShaderSetAsset_v8(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	ShaderSet_CreateSet<ShaderSetAssetHeader_v8_t>(pak, assetPath, mapEntry, 8);
}

void Assets::AddShaderSetAsset_v11(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	ShaderSet_CreateSet<ShaderSetAssetHeader_v11_t>(pak, assetPath, mapEntry, 11);
}
