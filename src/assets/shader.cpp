#include "pch.h"
#include "assets.h"
#include "public/shader.h"
#include "public/multishader.h"
#include "utils/dxutils.h"

static bool Shader_ParseFromMSW(const fs::path& inputPath, CMultiShaderWrapperIO::ShaderFile_t& file)
{
	CMultiShaderWrapperIO io = {};

	if (!io.ReadFile(inputPath.string().c_str(), &file))
	{
		Error("Failed to parse MSW file \"%s\".\n", inputPath.string().c_str());
		return false;
	}

	return true;
}

template <typename ShaderAssetHeader_t>
static int Shader_CreateFromMSW(CPakFile* const pak, CPakDataChunk& cpuDataChunk, ParsedDXShaderData_t* const firstShaderData,
								const CMultiShaderWrapperIO::ShaderFile_t& file, ShaderAssetHeader_t* const hdr)
{
	const size_t numShaderBuffers = file.shader->entries.size();
	size_t totalShaderDataSize = 0;

	for (auto& it : file.shader->entries)
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
	cpuDataChunk = pak->CreateDataChunk(shaderBufferChunkSize, SF_CPU | SF_TEMP, 8);

	// Offset at which the next bytecode buffer will be written.
	// Initially starts at the end of the descriptors and then gets increased every time a buffer is written.
	size_t nextBytecodeBufferOffset = descriptorSize;

	for (size_t i = 0; i < numShaderBuffers; ++i)
	{
		const CMultiShaderWrapperIO::ShaderEntry_t& entry = file.shader->entries[i];

		ShaderByteCode_t* bc = reinterpret_cast<ShaderByteCode_t*>(cpuDataChunk.Data() + (i * entrySize));

		if (entry.buffer)
		{
			assert(entry.size > 0);

			bc->data = cpuDataChunk.GetPointer(nextBytecodeBufferOffset);
			bc->dataSize = entry.size;

			if (hdr->type == eShaderType::Vertex)
			{
				bc->inputSignatureBlobSize = bc->dataSize;
				bc->inputSignatureBlob = bc->data;

				pak->AddPointer(cpuDataChunk.GetPointer((i * entrySize) + offsetof(ShaderByteCode_t, inputSignatureBlob)));
			}

			// Register the data pointer at the 
			pak->AddPointer(cpuDataChunk.GetPointer((i * entrySize) + offsetof(ShaderByteCode_t, data)));

			memcpy_s(cpuDataChunk.Data() + nextBytecodeBufferOffset, entry.size, entry.buffer, entry.size);

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

	return static_cast<int>(file.shader->entries.size());
}

static int Shader_CreateShader(CPakFile* pak, ShaderAssetHeader_v8_t* const hdr, const fs::path& inputPath, CPakDataChunk& cpuDataChunk, ParsedDXShaderData_t* firstShaderData)
{
	CMultiShaderWrapperIO::ShaderFile_t file = {};

	if (!Shader_ParseFromMSW(inputPath, file))
		return -1;

	const size_t numShaderBuffers = file.shader->entries.size();

	// Set to invalid so we can update it when a buffer is found, and detect that it has been set.
	hdr->type = eShaderType::Invalid;
	hdr->numShaderBuffers = static_cast<int>(numShaderBuffers);

	return Shader_CreateFromMSW(pak, cpuDataChunk, firstShaderData, file, hdr);
}

static int Shader_CreateShader(CPakFile* pak, ShaderAssetHeader_v12_t* const hdr, const fs::path& inputPath, CPakDataChunk& cpuDataChunk, ParsedDXShaderData_t* firstShaderData)
{
	CMultiShaderWrapperIO::ShaderFile_t file = {};

	if (!Shader_ParseFromMSW(inputPath, file))
		return -1;

	// Set to invalid so we can update it when a buffer is found, and detect that it has been set.
	hdr->type = eShaderType::Invalid;
	memcpy_s(hdr->shaderFeatures, sizeof(hdr->shaderFeatures), file.shader->features, sizeof(file.shader->features));

	return Shader_CreateFromMSW(pak, cpuDataChunk, firstShaderData, file, hdr);
}

template<typename ShaderAssetHeader_t>
static void Shader_InternalAddShader(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry, const int assetVersion)
{
	const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("msw");

	if (!fs::exists(inputFilePath))
		Error("Failed to find compiled shader file for asset '%s' (%s).\n", assetPath, inputFilePath.string().c_str());

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderAssetHeader_t), SF_HEAD, 8);

	const fs::path pakFilePath = fs::path(assetPath).replace_extension(""); // Make sure the path has no extension, just in case.
	CPakDataChunk nameChunk = pak->CreateDataChunk(pakFilePath.string().length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), pakFilePath.string().c_str());

	ShaderAssetHeader_t* const hdr = reinterpret_cast<ShaderAssetHeader_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_t, name)));

	CPakDataChunk dataChunk = {};
	ParsedDXShaderData_t* shaderData = new ParsedDXShaderData_t;
	const int numShaders = Shader_CreateShader(pak, hdr, inputFilePath, dataChunk, shaderData);

	// Data chunk used by pointer "unk_10" and "shaderInputFlags"
	// The first set of data in the buffer is reserved data for when the asset is loaded (equal to 16 bytes per shader entry)
	// 
	// The remainder of the data is used for the input layout flags of each shader.
	// This data consists of 2 QWORDs per shader entry, with the first being set to the flags
	// and the second being unknown.
	const size_t inputFlagsDataSize = numShaders * (2 * sizeof(uint64_t));
	const size_t reservedDataSize = numShaders * (16);
	CPakDataChunk shaderInfoChunk = pak->CreateDataChunk(reservedDataSize + inputFlagsDataSize, SF_CPU, 1);

	// Get a pointer to the beginning of the reserved data section
	hdr->unk_10 = shaderInfoChunk.GetPointer();
	// Get a pointer after the end of the reserved data section
	hdr->shaderInputFlags = shaderInfoChunk.GetPointer(reservedDataSize);

	// Register the pointers we have just written.
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_t, unk_10)));
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_t, shaderInputFlags)));

	const uint8_t numFlagsPerShader = hdr->type == eShaderType::Vertex ? 2 : 1;

	// get input layout flags from json
	// this will eventually be taken from the bytecode itself, but that code will be very annoying so for now
	// the values are manually provided

	rapidjson::Value::ConstMemberIterator flagsIt;

	if (!JSON_GetIterator(mapEntry, "inputFlags", JSONFieldType_e::kArray, flagsIt))
		Error("Invalid \"inputFlags\" field for shader asset %s. Must be an array with the same number of elements as there are shader buffers in the MSW file (%i).\n", assetPath, numShaders);

	uint64_t* const inputFlags = reinterpret_cast<uint64_t*>(shaderInfoChunk.Data() + reservedDataSize);

	size_t i = 0;
	for (const auto& elem : flagsIt->value.GetArray())
	{
		uint64_t flag = 0;

		if (!JSON_ParseNumber(elem, flag))
			Error("Found invalid input flag (idx %zu) for shader asset %s. Flag must be either an integer literal or a number as a string.\n", i, assetPath);

		if (flag == 0)
			Warning("Found potentially invalid input flag (idx %zu) for shader asset %s. Flag value is zero.\n", i, assetPath);

		// vertex shaders seem to have data every 8 bytes, unlike (seemingly) every other shader that only uses 8 out of every 16 bytes

		inputFlags[(3 - numFlagsPerShader) * i] = flag;
		i++;
	}

	// =======================================

	PakAsset_t asset;
	asset.InitAsset(
		assetPath,
		Pak_GetGuidOverridable(mapEntry, assetPath),
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		dataChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::SHDR);

	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = assetVersion;
	asset.SetPublicData(shaderData);

	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	// note(amos): do shaders even depend on external dependencies?
	//             remove the comments if its confirmed to not depend on external dependencies.
	asset.remainingDependencyCount = 1;

	pak->PushAsset(asset);

	printf("\n");
}

void Assets::AddShaderAsset_v8(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	Shader_InternalAddShader<ShaderAssetHeader_v8_t>(pak, assetPath, mapEntry, 8);
}

void Assets::AddShaderAsset_v12(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	Shader_InternalAddShader<ShaderAssetHeader_v12_t>(pak, assetPath, mapEntry, 12);
}
