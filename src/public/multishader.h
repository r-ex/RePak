#pragma once
#include <stdlib.h>

#undef DOMAIN // go away

// Definitions for the custom MultiShaderWrapper format
// This file is used for exporting and storing all information relating to both Shader and ShaderSet assets
//
// Types of file:
// - Shader
//   - Stores all compiled shader buffers as exported from the game asset CPU data
//   - This uses multiple buffers because shader assets can have different versions of the same shader to support different features 
//     (such as dither fading, instancing, or a combination of them (and maybe others, but this is all i have seen actually being used))
// - ShaderSet
//   - Stores information for both of the shaders referenced by the game asset.
//   - Ultimately points to two Shader data headers, as if the file contained two separate "Shader" MSW files.


// Shader files:
//
// File Header        - MultiShaderWrapper_Header_t       - Defines file-wide information.
// Shader Header      - MultiShaderWrapper_Shader_t       - Defines shader-specific information.
// Shader Descriptors - MultiShaderWrapper_ShaderDesc_t[] - Describes location of individual shader files within the wrapper.
//                                                        - Array size depends on Shader Header variable.

#ifndef MSW_FOURCC
#define MSW_FOURCC(a, b, c, d) (a | (b << 8) | (c << 16) | (d << 24))
#endif

#define MSW_FILE_MAGIC ('M' | ('S' << 8) | ('W' << 16))

// Only updated for breaking changes.
#define MSW_FILE_VER 1

enum class MultiShaderWrapperFileType_e : unsigned char
{
	SHADER = 0,
	SHADERSET = 1,
};

enum class MultiShaderWrapperShaderType_e : unsigned char
{
	PIXEL = 0,
	VERTEX = 1,
	GEOMETRY = 2,
	HULL = 3,
	DOMAIN = 4,
	COMPUTE = 5,
	INVALID = 0xFF
};

#pragma pack(push, 1)
struct MultiShaderWrapper_Header_t
{
	unsigned int magic : 24;    // Identifies this file as being a MSW file.
	unsigned int version : 8; // Used to return early when loading an older (incompatible) version of the file format.
	MultiShaderWrapperFileType_e fileType; // What type of wrapper is this?
};
static_assert(sizeof(MultiShaderWrapper_Header_t) == 5);

struct MultiShaderWrapper_ShaderSet_t
{
	// For R5-exported assets, these should both always be set, but in the event that a shaderset doesn't have one of the shader types,
	// we need to make sure that the parser only tries to read valid data.
	bool hasPixelShader : 1;
	bool hasVertexShader : 1;
};
static_assert(sizeof(MultiShaderWrapper_ShaderSet_t) == 1);

struct MultiShaderWrapper_Shader_t
{
	// shaderFeatures MUST be the first member of this struct! see WriteShader.
	unsigned __int64 shaderFeatures : 56; // Defines some shader counts.
	unsigned __int64 numShaderDescriptors : 8;  // Number of MultiShaderWrapper_ShaderDesc_t struct instances before file data.
};
static_assert(sizeof(MultiShaderWrapper_Shader_t) == 8);

struct MultiShaderWrapper_ShaderDesc_t
{
	// Represents a regular shader entry. These shaders have their own file data, pointed to by the "bufferOffset" member.
	// This offset is relative to the start of the MSW_ShaderDesc_t struct instance in the file.
	struct _StandardShader
	{
		unsigned int bufferLength;
		unsigned int bufferOffset;
	};

	// Represents a reference shader entry. These shaders do not have their own file data, and point to a lower index of standard shader
	// to re-use their shader buffer.
	struct _ReferenceShader
	{
		unsigned int bufferIndex;

		// This upper DWORD must be 0 on reference shaders so that they can be identified as references.
		// As the buffer offset of standard shaders are relative to the start of the MSW_ShaderDesc_t struct, the value will never be 0.
		// Therefore, if the value is 0, the shader is a reference to another shader's data.
		unsigned int _reserved;
	};

	union
	{
		_StandardShader u_standard;
		_ReferenceShader u_ref;
	};
};
#pragma pack(pop)

#include <vector> // this will be removed eventuallytm

// class for handling reading/writing of MSW files from memory structures
class CMultiShaderWrapperIO
{
public:
	struct ShaderEntry_t
	{
		const char* buffer;
		unsigned int size;

		unsigned short refIndex; // if this shader entry is a reference. do not set "buffer" if this is used
	};

	struct Shader_t
	{
		Shader_t() : shaderType(MultiShaderWrapperShaderType_e::INVALID), features{} {};
		Shader_t(MultiShaderWrapperShaderType_e type) : shaderType(type), features{} {};

		std::vector<ShaderEntry_t> entries;
		MultiShaderWrapperShaderType_e shaderType;
		unsigned char features[7];
	};

	struct ShaderFile_t
	{
		MultiShaderWrapperFileType_e type;

		union
		{
			Shader_t* shader; // Used if type == SHADER

			struct {
				Shader_t* pixelShader;
				Shader_t* vertexShader;
			} shaderSet; // Used if type == SHADERSET
		};
	};
public:
	CMultiShaderWrapperIO() = default;

	inline void SetFileType(MultiShaderWrapperFileType_e type) { _fileType = type; };

	inline void SetShader(Shader_t* shader)
	{
		if (!shader)
			return;

		if (_fileType == MultiShaderWrapperFileType_e::SHADER)
			this->_storedShaders.shader = shader;
		else if (_fileType == MultiShaderWrapperFileType_e::SHADERSET)
		{
			switch (shader->shaderType)
			{
			case MultiShaderWrapperShaderType_e::PIXEL:
			{
				this->_storedShaders.shaderSet.pixelShader = shader;
				break;
			}
			case MultiShaderWrapperShaderType_e::VERTEX:
			{
				this->_storedShaders.shaderSet.vertexShader = shader;
				break;
			}
			default:
				return;
			}
		}

		writtenAnything = true;
	}

	bool ReadFile(const char* filePath, ShaderFile_t* outFile)
	{
		if (!outFile)
			return false;

		FILE* f = NULL;

		if (fopen_s(&f, filePath, "rb") == 0)
		{
			MultiShaderWrapper_Header_t fileHeader = {};

			fread(&fileHeader, sizeof(fileHeader), 1, f);
			outFile->type = fileHeader.fileType;

			if (fileHeader.fileType == MultiShaderWrapperFileType_e::SHADERSET)
			{
				// will be supported eventuallytm
				return false;
			}
			else if (fileHeader.fileType == MultiShaderWrapperFileType_e::SHADER)
			{
				Shader_t* shader = new Shader_t;

				ReadShader(f, shader);

				outFile->shader = shader;
			}

			return true;
		}
		else
			return false;
	}

	void ReadShaderSet() {};

	// Allocate a shader before calling this
	void ReadShader(FILE* f, Shader_t* outShader)
	{
		MultiShaderWrapper_Shader_t shdr = {};

		fread(&shdr, sizeof(shdr), 1, f);

		const size_t descStartOffset = ftell(f);

		MultiShaderWrapper_ShaderDesc_t* descriptors = new MultiShaderWrapper_ShaderDesc_t[shdr.numShaderDescriptors];
		for (int i = 0; i < shdr.numShaderDescriptors; ++i)
		{
			const size_t thisDescOffset = descStartOffset + (i * sizeof(MultiShaderWrapper_ShaderDesc_t));

			fseek(f, static_cast<long>(thisDescOffset), SEEK_SET);

			MultiShaderWrapper_ShaderDesc_t* desc = &descriptors[i];
			fread(desc, sizeof(MultiShaderWrapper_ShaderDesc_t), 1, f);

			ShaderEntry_t entry = {};

			if (desc->u_ref.bufferIndex == UINT32_MAX && desc->u_ref._reserved == UINT32_MAX)
			{
				// null shader entry
				entry.refIndex = UINT16_MAX;
			}
			else if (desc->u_ref._reserved == 0 && desc->u_ref.bufferIndex != UINT32_MAX)
			{
				entry.refIndex = static_cast<unsigned short>(desc->u_ref.bufferIndex);
			}
			else
			{
				// regular shader - get buffer and allocate it some new space to go into the shader entry vector
				fseek(f, static_cast<long>(thisDescOffset + desc->u_standard.bufferOffset), SEEK_SET);

				char* buffer = new char[desc->u_standard.bufferLength];

				fread(buffer, sizeof(char), desc->u_standard.bufferLength, f);

				entry.buffer = buffer;
				entry.size = desc->u_standard.bufferLength;
				entry.refIndex = UINT16_MAX;
			}

			outShader->entries.push_back(entry);

			// shader type isn't saved, so it has to be found from the shader bytecode separately
			outShader->shaderType = MultiShaderWrapperShaderType_e::INVALID;

			memcpy(outShader->features, &shdr, sizeof(outShader->features));
		}
	}

	__forceinline bool WriteFile(const char* filePath)
	{
		if (!writtenAnything)
			return false;

		FILE* f = NULL;

		if (fopen_s(&f, filePath, "wb") == 0)
		{
			MultiShaderWrapper_Header_t fileHeader =
			{
				.magic = MSW_FILE_MAGIC,
				.version = MSW_FILE_VER,
				.fileType = this->_fileType
			};

			fwrite(&fileHeader, sizeof(MultiShaderWrapper_Header_t), 1, f);

			if (_fileType == MultiShaderWrapperFileType_e::SHADERSET)
				WriteShaderSet(f);
			else if (_fileType == MultiShaderWrapperFileType_e::SHADER)
				WriteShader(f, _storedShaders.shader);


			fclose(f);
			return true;
		}
		return false;
	}

private:
	inline void WriteShaderSet(FILE* f)
	{
		MultiShaderWrapper_ShaderSet_t shaderSet =
		{
			.hasPixelShader = _storedShaders.shaderSet.pixelShader != nullptr,
			.hasVertexShader = _storedShaders.shaderSet.vertexShader != nullptr
		};

		fwrite(&shaderSet, sizeof(shaderSet), 1, f);

		if (shaderSet.hasPixelShader)
			this->WriteShader(f, _storedShaders.shaderSet.pixelShader);
		if (shaderSet.hasVertexShader)
			this->WriteShader(f, _storedShaders.shaderSet.vertexShader);
	}

	inline void WriteShader(FILE* f, const Shader_t* shader)
	{
		MultiShaderWrapper_Shader_t shdr =
		{
			.numShaderDescriptors = shader->entries.size()
		};

		// Copy to the start of the struct, since we can't copy directly to a bitfield
		memcpy_s(&shdr, sizeof(shader->features), shader->features, sizeof(shader->features));

		fwrite(&shdr, sizeof(shdr), 1, f);

		const size_t descStartOffset = ftell(f);
		size_t bufferWriteOffset = descStartOffset + (shader->entries.size() * sizeof(MultiShaderWrapper_ShaderDesc_t));

		// Skip the description structs for now, since we can write those at the same time as the buffers.
		// Buffer writing has to come back here to set the offset, so we might as well do it all in one go!
		fseek(f, static_cast<long>(bufferWriteOffset), SEEK_SET);

		size_t entryIndex = 0;
		for (auto& entry : shader->entries)
		{
			const size_t thisDescOffset = descStartOffset + (entryIndex * sizeof(MultiShaderWrapper_ShaderDesc_t));

			MultiShaderWrapper_ShaderDesc_t desc = {};

			// If there is a valid buffer pointer, this entry is standard.
			if (entry.buffer)
			{
				// Seek to the position of the next shader buffer to write.
				fseek(f, static_cast<long>(bufferWriteOffset), SEEK_SET);
				fwrite(entry.buffer, sizeof(char), entry.size, f);

				// Buffer offsets are relative to the start of the desc structure, so subtract one from the other.
				desc.u_standard.bufferOffset = static_cast<unsigned int>(bufferWriteOffset - thisDescOffset);
				desc.u_standard.bufferLength = entry.size;

				bufferWriteOffset += entry.size;
			}
			else if (entry.refIndex != UINT16_MAX) // If the ref index is not 0xFFFF, this entry is a reference.
			{
				desc.u_ref.bufferIndex = entry.refIndex;
			}
			else // If there is no valid buffer and no valid reference, this is a null entry.
			{
				desc.u_ref.bufferIndex = UINT32_MAX;
				desc.u_ref._reserved = UINT32_MAX;
			}

			// Seek back to the desc location so we can write it now that the buffer is in place 
			fseek(f, static_cast<long>(thisDescOffset), SEEK_SET);
			fwrite(&desc, sizeof(desc), 1, f);

			entryIndex++;
		}
	}

private:
	MultiShaderWrapperFileType_e _fileType;
	bool writtenAnything;

	union
	{
		Shader_t* shader;
		struct ShaderSet
		{
			Shader_t* pixelShader;
			Shader_t* vertexShader;
		} shaderSet;
	} _storedShaders;
};
