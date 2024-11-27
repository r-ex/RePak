//=============================================================================//
//
// purpose: Microsoft DirectX utilities
//
//=============================================================================//
#include "pch.h"
#include "dxutils.h"

//-----------------------------------------------------------------------------
// purpose: gets the dxgi format from header
// returns: DXGI_FORMAT
//-----------------------------------------------------------------------------
DXGI_FORMAT DXUtils::GetFormatFromHeader(const DDS_HEADER& ddsh)
{
	switch (ddsh.ddspf.dwFourCC)
	{
	case '1TXD': // DXT1
		return DXGI_FORMAT_BC1_UNORM;
	case '3TXD': // DXT3
		return DXGI_FORMAT_BC2_UNORM;
	case '5TXD': // DXT5
		return DXGI_FORMAT_BC3_UNORM;
	case '1ITA':
	case 'U4CB': // BC4U
		return DXGI_FORMAT_BC4_UNORM;
	case 'S4CB':
		return DXGI_FORMAT_BC4_SNORM;
	case '2ITA': // ATI2
	case 'U5CB': // BC5U
		return DXGI_FORMAT_BC5_UNORM;
	case 'S5CB': // BC5S
		return DXGI_FORMAT_BC5_SNORM;
	case '01XD': // DX10
		return DXGI_FORMAT_UNKNOWN;
	// legacy format codes
	case 36:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	case 110:
		return DXGI_FORMAT_R16G16B16A16_SNORM;
	case 111:
		return DXGI_FORMAT_R16_FLOAT;
	case 112:
		return DXGI_FORMAT_R16G16_FLOAT;
	case 113:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case 114:
		return DXGI_FORMAT_R32_FLOAT;
	case 115:
		return DXGI_FORMAT_R32G32_FLOAT;
	case 116:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	default:
		return GetFormatFromHeaderEx(ddsh);
	}
}

//-----------------------------------------------------------------------------
// purpose: gets the dxgi format from header (internal)
// returns: DXGI_FORMAT
//-----------------------------------------------------------------------------
DXGI_FORMAT DXUtils::GetFormatFromHeaderEx(const DDS_HEADER& hdr)
{
	DDS_PIXELFORMAT pf = hdr.ddspf;
	if (pf.dwRGBBitCount == 32)
	{
		if (pf.dwFlags & DDS_RGBA)
		{
			if (pf.dwRBitMask == 0xff && pf.dwGBitMask == 0xff00 && pf.dwBBitMask == 0xff0000 && pf.dwABitMask == 0xff000000)
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			else if (pf.dwRBitMask == 0xffff && pf.dwGBitMask == 0xffff0000)
				return DXGI_FORMAT_R16G16_UNORM;
			else if (pf.dwRBitMask == 0x3ff && pf.dwGBitMask == 0xffc00 && pf.dwBBitMask == 0x3ff00000)
				return DXGI_FORMAT_R10G10B10A2_UNORM;
		}
		else if (pf.dwFlags & DDS_RGB)
		{
			if (pf.dwRBitMask == 0xffff && pf.dwGBitMask == 0xffff0000)
				return DXGI_FORMAT_R16G16_UNORM;
		}
	}
	else if (pf.dwRGBBitCount == 8)
	{
		if (pf.dwFlags & DDS_ALPHA)
		{
			if (pf.dwABitMask == 0xff)
				return DXGI_FORMAT_A8_UNORM;
		}
		else if (pf.dwRBitMask == 0xff)
			return DXGI_FORMAT_R8_UNORM;
	}

	// unsupported
	return DXGI_FORMAT_UNKNOWN;
}

//-----------------------------------------------------------------------------
// purpose: gets the dxgi format as string
// returns: dxgi format as const char*
//-----------------------------------------------------------------------------
const char* DXUtils::GetFormatAsString(DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_UNKNOWN: return "DXGI_FORMAT_UNKNOWN";
	case DXGI_FORMAT_R32G32B32A32_TYPELESS: return "DXGI_FORMAT_R32G32B32A32_TYPELESS";
	case DXGI_FORMAT_R32G32B32A32_FLOAT: return "DXGI_FORMAT_R32G32B32A32_FLOAT";
	case DXGI_FORMAT_R32G32B32A32_UINT: return "DXGI_FORMAT_R32G32B32A32_UINT";
	case DXGI_FORMAT_R32G32B32A32_SINT: return "DXGI_FORMAT_R32G32B32A32_SINT";
	case DXGI_FORMAT_R32G32B32_TYPELESS: return "DXGI_FORMAT_R32G32B32_TYPELESS";
	case DXGI_FORMAT_R32G32B32_FLOAT: return "DXGI_FORMAT_R32G32B32_FLOAT";
	case DXGI_FORMAT_R32G32B32_UINT: return "DXGI_FORMAT_R32G32B32_UINT";
	case DXGI_FORMAT_R32G32B32_SINT: return "DXGI_FORMAT_R32G32B32_SINT";
	case DXGI_FORMAT_R16G16B16A16_TYPELESS: return "DXGI_FORMAT_R16G16B16A16_TYPELESS";
	case DXGI_FORMAT_R16G16B16A16_FLOAT: return "DXGI_FORMAT_R16G16B16A16_FLOAT";
	case DXGI_FORMAT_R16G16B16A16_UNORM: return "DXGI_FORMAT_R16G16B16A16_UNORM";
	case DXGI_FORMAT_R16G16B16A16_UINT: return "DXGI_FORMAT_R16G16B16A16_UINT";
	case DXGI_FORMAT_R16G16B16A16_SNORM: return "DXGI_FORMAT_R16G16B16A16_SNORM";
	case DXGI_FORMAT_R16G16B16A16_SINT: return "DXGI_FORMAT_R16G16B16A16_SINT";
	case DXGI_FORMAT_R32G32_TYPELESS: return "DXGI_FORMAT_R32G32_TYPELESS";
	case DXGI_FORMAT_R32G32_FLOAT: return "DXGI_FORMAT_R32G32_FLOAT";
	case DXGI_FORMAT_R32G32_UINT: return "DXGI_FORMAT_R32G32_UINT";
	case DXGI_FORMAT_R32G32_SINT: return "DXGI_FORMAT_R32G32_SINT";
	case DXGI_FORMAT_R32G8X24_TYPELESS: return "DXGI_FORMAT_R32G8X24_TYPELESS";
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return "DXGI_FORMAT_D32_FLOAT_S8X24_UINT";
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: return "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS";
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT";
	case DXGI_FORMAT_R10G10B10A2_TYPELESS: return "DXGI_FORMAT_R10G10B10A2_TYPELESS";
	case DXGI_FORMAT_R10G10B10A2_UNORM: return "DXGI_FORMAT_R10G10B10A2_UNORM";
	case DXGI_FORMAT_R10G10B10A2_UINT: return "DXGI_FORMAT_R10G10B10A2_UINT";
	case DXGI_FORMAT_R11G11B10_FLOAT: return "DXGI_FORMAT_R11G11B10_FLOAT";
	case DXGI_FORMAT_R8G8B8A8_TYPELESS: return "DXGI_FORMAT_R8G8B8A8_TYPELESS";
	case DXGI_FORMAT_R8G8B8A8_UNORM: return "DXGI_FORMAT_R8G8B8A8_UNORM";
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
	case DXGI_FORMAT_R8G8B8A8_UINT: return "DXGI_FORMAT_R8G8B8A8_UINT";
	case DXGI_FORMAT_R8G8B8A8_SNORM: return "DXGI_FORMAT_R8G8B8A8_SNORM";
	case DXGI_FORMAT_R8G8B8A8_SINT: return "DXGI_FORMAT_R8G8B8A8_SINT";
	case DXGI_FORMAT_R16G16_TYPELESS: return "DXGI_FORMAT_R16G16_TYPELESS";
	case DXGI_FORMAT_R16G16_FLOAT: return "DXGI_FORMAT_R16G16_FLOAT";
	case DXGI_FORMAT_R16G16_UNORM: return "DXGI_FORMAT_R16G16_UNORM";
	case DXGI_FORMAT_R16G16_UINT: return "DXGI_FORMAT_R16G16_UINT";
	case DXGI_FORMAT_R16G16_SNORM: return "DXGI_FORMAT_R16G16_SNORM";
	case DXGI_FORMAT_R16G16_SINT: return "DXGI_FORMAT_R16G16_SINT";
	case DXGI_FORMAT_R32_TYPELESS: return "DXGI_FORMAT_R32_TYPELESS";
	case DXGI_FORMAT_D32_FLOAT: return "DXGI_FORMAT_D32_FLOAT";
	case DXGI_FORMAT_R32_FLOAT: return "DXGI_FORMAT_R32_FLOAT";
	case DXGI_FORMAT_R32_UINT: return "DXGI_FORMAT_R32_UINT";
	case DXGI_FORMAT_R32_SINT: return "DXGI_FORMAT_R32_SINT";
	case DXGI_FORMAT_R24G8_TYPELESS: return "DXGI_FORMAT_R24G8_TYPELESS";
	case DXGI_FORMAT_D24_UNORM_S8_UINT: return "DXGI_FORMAT_D24_UNORM_S8_UINT";
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: return "DXGI_FORMAT_R24_UNORM_X8_TYPELESS";
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return "DXGI_FORMAT_X24_TYPELESS_G8_UINT";
	case DXGI_FORMAT_R8G8_TYPELESS: return "DXGI_FORMAT_R8G8_TYPELESS";
	case DXGI_FORMAT_R8G8_UNORM: return "DXGI_FORMAT_R8G8_UNORM";
	case DXGI_FORMAT_R8G8_UINT: return "DXGI_FORMAT_R8G8_UINT";
	case DXGI_FORMAT_R8G8_SNORM: return "DXGI_FORMAT_R8G8_SNORM";
	case DXGI_FORMAT_R8G8_SINT: return "DXGI_FORMAT_R8G8_SINT";
	case DXGI_FORMAT_R16_TYPELESS: return "DXGI_FORMAT_R16_TYPELESS";
	case DXGI_FORMAT_R16_FLOAT: return "DXGI_FORMAT_R16_FLOAT";
	case DXGI_FORMAT_D16_UNORM: return "DXGI_FORMAT_D16_UNORM";
	case DXGI_FORMAT_R16_UNORM: return "DXGI_FORMAT_R16_UNORM";
	case DXGI_FORMAT_R16_UINT: return "DXGI_FORMAT_R16_UINT";
	case DXGI_FORMAT_R16_SNORM: return "DXGI_FORMAT_R16_SNORM";
	case DXGI_FORMAT_R16_SINT: return "DXGI_FORMAT_R16_SINT";
	case DXGI_FORMAT_R8_TYPELESS: return "DXGI_FORMAT_R8_TYPELESS";
	case DXGI_FORMAT_R8_UNORM: return "DXGI_FORMAT_R8_UNORM";
	case DXGI_FORMAT_R8_UINT: return "DXGI_FORMAT_R8_UINT";
	case DXGI_FORMAT_R8_SNORM: return "DXGI_FORMAT_R8_SNORM";
	case DXGI_FORMAT_R8_SINT: return "DXGI_FORMAT_R8_SINT";
	case DXGI_FORMAT_A8_UNORM: return "DXGI_FORMAT_A8_UNORM";
	case DXGI_FORMAT_R1_UNORM: return "DXGI_FORMAT_R1_UNORM";
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: return "DXGI_FORMAT_R9G9B9E5_SHAREDEXP";
	case DXGI_FORMAT_R8G8_B8G8_UNORM: return "DXGI_FORMAT_R8G8_B8G8_UNORM";
	case DXGI_FORMAT_G8R8_G8B8_UNORM: return "DXGI_FORMAT_G8R8_G8B8_UNORM";
	case DXGI_FORMAT_BC1_TYPELESS: return "DXGI_FORMAT_BC1_TYPELESS";
	case DXGI_FORMAT_BC1_UNORM: return "DXGI_FORMAT_BC1_UNORM";
	case DXGI_FORMAT_BC1_UNORM_SRGB: return "DXGI_FORMAT_BC1_UNORM_SRGB";
	case DXGI_FORMAT_BC2_TYPELESS: return "DXGI_FORMAT_BC2_TYPELESS";
	case DXGI_FORMAT_BC2_UNORM: return "DXGI_FORMAT_BC2_UNORM";
	case DXGI_FORMAT_BC2_UNORM_SRGB: return "DXGI_FORMAT_BC2_UNORM_SRGB";
	case DXGI_FORMAT_BC3_TYPELESS: return "DXGI_FORMAT_BC3_TYPELESS";
	case DXGI_FORMAT_BC3_UNORM: return "DXGI_FORMAT_BC3_UNORM";
	case DXGI_FORMAT_BC3_UNORM_SRGB: return "DXGI_FORMAT_BC3_UNORM_SRGB";
	case DXGI_FORMAT_BC4_TYPELESS: return "DXGI_FORMAT_BC4_TYPELESS";
	case DXGI_FORMAT_BC4_UNORM: return "DXGI_FORMAT_BC4_UNORM";
	case DXGI_FORMAT_BC4_SNORM: return "DXGI_FORMAT_BC4_SNORM";
	case DXGI_FORMAT_BC5_TYPELESS: return "DXGI_FORMAT_BC5_TYPELESS";
	case DXGI_FORMAT_BC5_UNORM: return "DXGI_FORMAT_BC5_UNORM";
	case DXGI_FORMAT_BC5_SNORM: return "DXGI_FORMAT_BC5_SNORM";
	case DXGI_FORMAT_B5G6R5_UNORM: return "DXGI_FORMAT_B5G6R5_UNORM";
	case DXGI_FORMAT_B5G5R5A1_UNORM: return "DXGI_FORMAT_B5G5R5A1_UNORM";
	case DXGI_FORMAT_B8G8R8A8_UNORM: return "DXGI_FORMAT_B8G8R8A8_UNORM";
	case DXGI_FORMAT_B8G8R8X8_UNORM: return "DXGI_FORMAT_B8G8R8X8_UNORM";
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: return "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM";
	case DXGI_FORMAT_B8G8R8A8_TYPELESS: return "DXGI_FORMAT_B8G8R8A8_TYPELESS";
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB";
	case DXGI_FORMAT_B8G8R8X8_TYPELESS: return "DXGI_FORMAT_B8G8R8X8_TYPELESS";
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB";
	case DXGI_FORMAT_BC6H_TYPELESS: return "DXGI_FORMAT_BC6H_TYPELESS";
	case DXGI_FORMAT_BC6H_UF16: return "DXGI_FORMAT_BC6H_UF16";
	case DXGI_FORMAT_BC6H_SF16: return "DXGI_FORMAT_BC6H_SF16";
	case DXGI_FORMAT_BC7_TYPELESS: return "DXGI_FORMAT_BC7_TYPELESS";
	case DXGI_FORMAT_BC7_UNORM: return "DXGI_FORMAT_BC7_UNORM";
	case DXGI_FORMAT_BC7_UNORM_SRGB: return "DXGI_FORMAT_BC7_UNORM_SRGB";
	case DXGI_FORMAT_AYUV: return "DXGI_FORMAT_AYUV";
	case DXGI_FORMAT_Y410: return "DXGI_FORMAT_Y410";
	case DXGI_FORMAT_Y416: return "DXGI_FORMAT_Y416";
	case DXGI_FORMAT_NV12: return "DXGI_FORMAT_NV12";
	case DXGI_FORMAT_P010: return "DXGI_FORMAT_P010";
	case DXGI_FORMAT_P016: return "DXGI_FORMAT_P016";
	case DXGI_FORMAT_420_OPAQUE: return "DXGI_FORMAT_420_OPAQUE";
	case DXGI_FORMAT_YUY2: return "DXGI_FORMAT_YUY2";
	case DXGI_FORMAT_Y210: return "DXGI_FORMAT_Y210";
	case DXGI_FORMAT_Y216: return "DXGI_FORMAT_Y216";
	case DXGI_FORMAT_NV11: return "DXGI_FORMAT_NV11";
	case DXGI_FORMAT_AI44: return "DXGI_FORMAT_AI44";
	case DXGI_FORMAT_IA44: return "DXGI_FORMAT_IA44";
	case DXGI_FORMAT_P8: return "DXGI_FORMAT_P8";
	case DXGI_FORMAT_A8P8: return "DXGI_FORMAT_A8P8";
	case DXGI_FORMAT_B4G4R4A4_UNORM: return "DXGI_FORMAT_B4G4R4A4_UNORM";
	case DXGI_FORMAT_P208: return "DXGI_FORMAT_P208";
	case DXGI_FORMAT_V208: return "DXGI_FORMAT_V208";
	case DXGI_FORMAT_V408: return "DXGI_FORMAT_V408";
	case DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE: return "DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE";
	case DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE: return "DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE";
	case DXGI_FORMAT_FORCE_UINT: return "DXGI_FORMAT_FORCE_UINT";
	default: return "DXGI_FORMAT_UNKNOWN";
	}
}

// See copyright and license notice in dxutils.h
bool DXUtils::GetParsedShaderData(const char* bytecode, size_t /*bytecodeLen*/, ParsedDXShaderData_t* outData)
{
	const DXBCHeader* fileHeader = reinterpret_cast<const DXBCHeader*>(bytecode);

	if (fileHeader->DXBCHeaderFourCC != DXBC_FOURCC_NAME)
		return false;

	if (!outData)
		return false;

	outData->numTextureResources = 0;
	outData->mtlTexSlotCount = 0;
	for (uint32_t i = 0; i < fileHeader->BlobCount; ++i)
	{
		const DXBCBlobHeader* blob = fileHeader->pBlob(i);

		if (blob->BlobFourCC == DXBC_FOURCC_RDEF_NAME)
		{
			outData->EnableFlag(SHDR_FOUND_RDEF);

			const RDefBlobHeader_t* rdef = reinterpret_cast<const RDefBlobHeader_t*>(blob->GetBlobData());
			Debug("Shader built by \"%s\"\n", rdef->compilerName());

			switch (rdef->ShaderType)
			{
			case PixelShader:
				outData->pakShaderType = 0; // eShaderType::Pixel
				break;
			case VertexShader:
				outData->pakShaderType = 1; // eShaderType::Vertex
				break;
			case GeometryShader:
				outData->pakShaderType = 2; // eShaderType::Geometry
				break;
			case HullShader:
				outData->pakShaderType = 3; // eShaderType::Hull
				break;
			case DomainShader:
				outData->pakShaderType = 4; // eShaderType::Domain
				break;
			case ComputeShader:
				outData->pakShaderType = 5; // eShaderType::Compute
				break;
			default:
				Error("Unknown shader type: %X\n", rdef->ShaderType);
				break;
			}

			for (uint32_t j = 0; j < rdef->BoundResourceCount; ++j)
			{
				const RDefResourceBinding_t* resource = rdef->resource(j);
				//printf("%s %s (%X)\n", resource->dimensionName(), resource->name(blob->GetBlobData()), resource->Flags);

				if (resource->Type == D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE)
				{
					outData->numTextureResources++;

					if (resource->BindPoint < 40)
						outData->mtlTexSlotCount = static_cast<uint8_t>(resource->BindPoint) + 1;
				}

				if (outData->numTextureResources > 0 && resource->Type != D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE)
					break;
			}
		}
	}

	return true;
}