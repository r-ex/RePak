#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "utils/utils.h"
#include "public/shaderset.h"
#include "public/shader.h"
#include "public/multishader.h"

static void ShaderSet_LoadFromMSW(CPakFile* const pak, const char* const assetPath, CMultiShaderWrapperIO::ShaderCache_t& shaderCache)
{
	const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("msw");
	MSW_ParseFile(inputFilePath, shaderCache, MultiShaderWrapperFileType_e::SHADERSET);
}

template <typename ShaderSetAssetHeader_t>
static void ShaderSet_SetInputSlots(ShaderSetAssetHeader_t* const hdr, PakAsset_t* const shader, const bool isVertexShader, const CMultiShaderWrapperIO::ShaderSet_t* const shaderSet, const int assetVersion)
{
	uint16_t* inputCount;

	// note: in v8 shader sets the vertex shader is set in the first slot, whereas
	// in v11 the vertex shader is set in the second slot and the first texture
	if (assetVersion == 8)
		inputCount = isVertexShader ? &hdr->textureInputCounts[0] : &hdr->textureInputCounts[1];
	else
		inputCount = isVertexShader ? &hdr->textureInputCounts[1] : &hdr->textureInputCounts[0];

	const uint8_t textureCount = isVertexShader ? (uint8_t)shaderSet->numVertexShaderTextures : (uint8_t)shaderSet->numPixelShaderTextures;
	const char* const shaderName = isVertexShader ? "vertex" : "pixel";

	// If we have the shader in this pak, perform this check to make sure the
	// shader provided by the user isn't corrupt or otherwise incompatible.
	if (shader)
	{
		const ParsedDXShaderData_t* const shaderData = reinterpret_cast<const ParsedDXShaderData_t*>(shader->PublicData());

		// cannot be null at this point
		assert(shaderData);

		if (shaderData->mtlTexSlotCount != textureCount)
			Error("Texture slot count mismatch between shader set and %s shader (expected %hhu, got %hhu).\n", shaderName, shaderData->mtlTexSlotCount, textureCount);
	}

	*inputCount = textureCount;
}

extern void Shader_AddShaderV8(CPakFile* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid, const bool errorOnDuplicate);
extern void Shader_AddShaderV12(CPakFile* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid, const bool errorOnDuplicate);

static void ShaderSet_AutoAddEmbeddedShader(CPakFile* const pak, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid, const int assetVersion)
{
	if (shader)
	{
		const std::string assetPath = Utils::VFormat("embedded_0x%llX", shaderGuid);

		const auto func = assetVersion == 8 ? Shader_AddShaderV8 : Shader_AddShaderV12;
		func(pak, assetPath.c_str(), shader, shaderGuid, false);
	}
}

template <typename ShaderSetAssetHeader_t>
void ShaderSet_InternalCreateSet(CPakFile* const pak, const char* const assetPath, const CMultiShaderWrapperIO::ShaderSet_t* const shaderSet, const PakGuid_t shaderSetGuid, const int assetVersion)
{
	ShaderSet_AutoAddEmbeddedShader(pak, shaderSet->vertexShader, shaderSet->vertexShaderGuid, assetVersion);
	ShaderSet_AutoAddEmbeddedShader(pak, shaderSet->pixelShader, shaderSet->pixelShaderGuid, assetVersion);

	std::vector<PakGuidRefHdr_t> guids{};

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderSetAssetHeader_t), SF_HEAD, 8);

	const std::string assetPathWithoutExtension = Utils::ChangeExtension(assetPath, "");

	CPakDataChunk nameChunk = pak->CreateDataChunk(assetPathWithoutExtension.length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), assetPathWithoutExtension.c_str());

	ShaderSetAssetHeader_t* const hdr = reinterpret_cast<ShaderSetAssetHeader_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_t, name)));

	// === Shader Inputs ===
	hdr->vertexShader = shaderSet->vertexShaderGuid;
	hdr->pixelShader = shaderSet->pixelShaderGuid;

	// todo: can shader refs be null???
	if (hdr->vertexShader != 0)
		pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_t, vertexShader)));

	if (hdr->pixelShader != 0)
		pak->AddGuidDescriptor(&guids, hdrChunk.GetPointer(offsetof(ShaderSetAssetHeader_t, pixelShader)));

	PakAsset_t* const vertexShader = hdr->vertexShader != 0 ? pak->GetAssetByGuid(hdr->vertexShader, nullptr, true) : nullptr;
	ShaderSet_SetInputSlots(hdr, vertexShader, true, shaderSet, assetVersion);

	PakAsset_t* const pixelShader = hdr->pixelShader != 0 ? pak->GetAssetByGuid(hdr->pixelShader, nullptr, true) : nullptr;
	ShaderSet_SetInputSlots(hdr, pixelShader, false, shaderSet, assetVersion);

	// On v11 shaders, the input count is added on top of the second one.
	if (assetVersion == 11)
		hdr->textureInputCounts[1] += hdr->textureInputCounts[0];

	hdr->numSamplers = shaderSet->numSamplers;

	// only used for ui/ui_world shadersets in R5
	// for now this will have to be manually set if used, because i cannot figure out a way to programmatically
	// detect these resources without incorrectly identifying some, and i doubt that's a good thing
	hdr->firstResourceBindPoint = shaderSet->firstResourceBindPoint;
	hdr->numResources = shaderSet->numResources;

	PakAsset_t asset;
	asset.InitAsset(
		assetPath,
		shaderSetGuid,
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
}

// TODO:
// Parse numSamplers from pixelShader
// Parse textureInputCount from pixelShader
// Figure out the other count variable before texture input count
// See if any of the other unknown variables are actually required

template <typename ShaderSetAssetHeader_t>
static void ShaderSet_AddFromMap(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry, const int assetVersion)
{
	CMultiShaderWrapperIO::ShaderCache_t cache = {};
	ShaderSet_LoadFromMSW(pak, assetPath, cache);

	ShaderSet_InternalCreateSet<ShaderSetAssetHeader_t>(pak, assetPath, &cache.shaderSet, Pak_GetGuidOverridable(mapEntry, assetPath), assetVersion);
}

void Assets::AddShaderSetAsset_v8(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	ShaderSet_AddFromMap<ShaderSetAssetHeader_v8_t>(pak, assetPath, mapEntry, 8);
}

void Assets::AddShaderSetAsset_v11(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	ShaderSet_AddFromMap<ShaderSetAssetHeader_v11_t>(pak, assetPath, mapEntry, 11);
}
