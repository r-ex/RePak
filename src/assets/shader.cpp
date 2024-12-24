#include "pch.h"
#include "assets.h"
#include "public/shader.h"
#include "public/multishader.h"
#include "utils/dxutils.h"

static void Shader_LoadFromMSW(CPakFileBuilder* const pak, const char* const assetPath, CMultiShaderWrapperIO::ShaderCache_t& shaderCache)
{
	const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("msw");
	MSW_ParseFile(inputFilePath, shaderCache, MultiShaderWrapperFileType_e::SHADER);
}

template <typename ShaderAssetHeader_t>
static void Shader_CreateFromMSW(CPakFileBuilder* const pak, PakPageLump_s& cpuDataChunk, ParsedDXShaderData_t* const firstShaderData,
								const CMultiShaderWrapperIO::Shader_t* shader, PakPageLump_s& hdrChunk, ShaderAssetHeader_t* const hdr)
{
	const size_t numShaderBuffers = shader->entries.size();
	size_t totalShaderDataSize = 0;

	for (auto& it : shader->entries)
	{
		if (it.buffer != nullptr)
			totalShaderDataSize += IALIGN(it.size, 8);

		// If the shader type hasn't been found yet, parse this buffer and find out what we want to set it as.
		if (hdr->type == eShaderType::Invalid)
		{
			if (DXUtils::GetParsedShaderData(it.buffer, it.size, firstShaderData))
			{
				if (firstShaderData->foundFlags & SHDR_FOUND_RDEF)
				{
					hdr->type = static_cast<eShaderType>(firstShaderData->pakShaderType);
				}
			}
		}
	}
	assert(totalShaderDataSize != 0);

	const int8_t entrySize = hdr->type == eShaderType::Vertex ? 24 : 16;

	// Size of the data that describes each shader bytecode buffer
	const size_t descriptorSize = numShaderBuffers * entrySize;

	const size_t shaderBufferChunkSize = descriptorSize + totalShaderDataSize;
	cpuDataChunk = pak->CreatePageLump(shaderBufferChunkSize, SF_CPU | SF_TEMP, 8);

	// Offset at which the next bytecode buffer will be written.
	// Initially starts at the end of the descriptors and then gets increased every time a buffer is written.
	size_t nextBytecodeBufferOffset = descriptorSize;

	for (size_t i = 0; i < numShaderBuffers; ++i)
	{
		const CMultiShaderWrapperIO::ShaderEntry_t& entry = shader->entries[i];

		ShaderByteCode_t* bc = reinterpret_cast<ShaderByteCode_t*>(cpuDataChunk.data + (i * entrySize));

		if (entry.buffer)
		{
			assert(entry.size > 0);

			// Register the data pointer at the byte code.
			pak->AddPointer(cpuDataChunk, (i * entrySize) + offsetof(ShaderByteCode_t, data), cpuDataChunk, nextBytecodeBufferOffset);
			bc->dataSize = entry.size;

			if (hdr->type == eShaderType::Vertex)
			{
				pak->AddPointer(cpuDataChunk, (i * entrySize) + offsetof(ShaderByteCode_t, inputSignatureBlob), cpuDataChunk, nextBytecodeBufferOffset);
				bc->inputSignatureBlobSize = bc->dataSize;
			}

			memcpy_s(cpuDataChunk.data + nextBytecodeBufferOffset, entry.size, entry.buffer, entry.size);
			nextBytecodeBufferOffset += IALIGN(entry.size, 8);
		}
		else
		{
			// Null shader
			if (entry.refIndex == UINT16_MAX)
			{
				// technically, these shaders can actually have a pointer to an empty dx bytecode buffer (with no actual bytecode from what i can tell?)
				// but the logic of the function that reads them should mean that the pointer is never accessed if entry.size is <= 0
			}
			else // Shader entry references another shader instead of defining its own buffer
			{
				bc->dataSize = ~entry.refIndex;
			}
		}
	}

	// Data chunk used by pointer "unk_10" and "shaderInputFlags"
	// The first set of data in the buffer is reserved data for when the asset is loaded (equal to 16 bytes per shader entry)
	// 
	// The remainder of the data is used for the input layout flags of each shader.
	// This data consists of 2 QWORDs per shader entry, with the first being set to the flags
	// and the second being unknown.
	const size_t inputFlagsDataSize = numShaderBuffers * (2 * sizeof(uint64_t));
	const size_t reservedDataSize = numShaderBuffers * (16);
	PakPageLump_s shaderInfoChunk = pak->CreatePageLump(reservedDataSize + inputFlagsDataSize, SF_CPU, 1);

	pak->AddPointer(hdrChunk, offsetof(ShaderAssetHeader_t, unk_10), shaderInfoChunk, 0);
	pak->AddPointer(hdrChunk, offsetof(ShaderAssetHeader_t, shaderInputFlags), shaderInfoChunk, reservedDataSize);

	uint64_t* const inputFlags = reinterpret_cast<uint64_t*>(shaderInfoChunk.data + reservedDataSize);
	size_t i = 0;

	// vertex shaders seem to have data every 8 bytes, unlike (seemingly) every other shader that only uses 8 out of every 16 bytes
	for (auto& it : shader->entries)
	{
		inputFlags[i] = it.flags[0];
		inputFlags[i+1] = it.flags[1];

		i+=2;
	}
}

static void Shader_SetupHeader(ShaderAssetHeader_v8_t* const hdr, const CMultiShaderWrapperIO::Shader_t* const shader)
{
	// Set to invalid so we can update it when a buffer is found, and detect that it has been set.
	hdr->type = eShaderType::Invalid;
	hdr->numShaderBuffers = static_cast<int>(shader->entries.size());
}

static void Shader_SetupHeader(ShaderAssetHeader_v12_t* const hdr, const CMultiShaderWrapperIO::Shader_t* const shader)
{
	// Set to invalid so we can update it when a buffer is found, and detect that it has been set.
	hdr->type = eShaderType::Invalid;
	memcpy_s(hdr->shaderFeatures, sizeof(hdr->shaderFeatures), shader->features, sizeof(shader->features));
}

template<typename ShaderAssetHeader_t>
static void Shader_InternalAddShader(CPakFileBuilder* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, 
									const PakGuid_t shaderGuid, const int assetVersion)
{
	PakAsset_t& asset = pak->BeginAsset(shaderGuid, assetPath);
	PakPageLump_s hdrChunk = pak->CreatePageLump(sizeof(ShaderAssetHeader_t), SF_HEAD, 8);

	ShaderAssetHeader_t* const hdr = reinterpret_cast<ShaderAssetHeader_t*>(hdrChunk.data);
	Shader_SetupHeader(hdr, shader);

	if (pak->IsFlagSet(PF_KEEP_DEV) && shader->name.length() > 0)
	{
		char pathStem[256];
		const size_t stemLen = Pak_ExtractAssetStem(shader->name.c_str(), pathStem, sizeof(pathStem));

		if (stemLen > 0)
		{
			PakPageLump_s nameChunk = pak->CreatePageLump(stemLen + 1, SF_CPU | SF_DEV, 1);
			memcpy(nameChunk.data, pathStem, stemLen + 1);

			pak->AddPointer(hdrChunk, offsetof(ShaderAssetHeader_t, name), nameChunk, 0);
		}
	}

	ParsedDXShaderData_t* const shaderData = new ParsedDXShaderData_t;
	PakPageLump_s dataChunk;

	Shader_CreateFromMSW(pak, dataChunk, shaderData, shader, hdrChunk, hdr);
	// =======================================

	asset.InitAsset(
		hdrChunk.GetPointer(), hdrChunk.size,
		dataChunk.GetPointer(), -1, -1, assetVersion, AssetType::SHDR);

	asset.SetHeaderPointer(hdrChunk.data);
	asset.SetPublicData(shaderData);

	pak->FinishAsset();
}

static void Shader_AddShaderV8(CPakFileBuilder* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid)
{
	Shader_InternalAddShader<ShaderAssetHeader_v8_t>(pak, assetPath, shader, shaderGuid, 8);
}

static void Shader_AddShaderV12(CPakFileBuilder* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid)
{
	Shader_InternalAddShader<ShaderAssetHeader_v12_t>(pak, assetPath, shader, shaderGuid, 12);
}

bool Shader_AutoAddShader(CPakFileBuilder* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid, const int shaderAssetVersion)
{
	PakAsset_t* const existingAsset = pak->GetAssetByGuid(shaderGuid, nullptr, true);

	if (existingAsset)
		return false;

	Log("Auto-adding 'shdr' asset \"%s\".\n", assetPath);

	const auto func = shaderAssetVersion == 8 ? Shader_AddShaderV8 : Shader_AddShaderV12;
	func(pak, assetPath, shader, shaderGuid);

	return true;
}

void Assets::AddShaderAsset_v8(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	UNUSED(mapEntry);
	CMultiShaderWrapperIO::ShaderCache_t cache = {};

	Shader_LoadFromMSW(pak, assetPath, cache);
	Shader_AddShaderV8(pak, assetPath, cache.shader, assetGuid);
}

void Assets::AddShaderAsset_v12(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	UNUSED(mapEntry);
	CMultiShaderWrapperIO::ShaderCache_t cache = {};

	Shader_LoadFromMSW(pak, assetPath, cache);
	Shader_AddShaderV12(pak, assetPath, cache.shader, assetGuid);
}
