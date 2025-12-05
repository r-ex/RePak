#include "pch.h"
#include "DmxTools.h"
#include "public/dmelement.h"
#include "public/dmattribute.h"
#include "public/symboltable.h"

bool Dmx_ParseHdr(BinaryIO& bio, DmxHeader_s& hdr)
{
	char hdrStr[DmxHeader_s::MAX_HEADER_LENGTH];
	
	if (!bio.ParseToken(hdrStr, DMX_VERSION_STARTING_TOKEN, DMX_VERSION_ENDING_TOKEN, sizeof(hdrStr)))
	{
		Error("%s: failed to tokenize DMX header! (format should start with \"" DMX_VERSION_STARTING_TOKEN "\" and end with \"" DMX_VERSION_ENDING_TOKEN "\")", __FUNCTION__);
		return false;
	}

	const int scanCount = sscanf_s(hdrStr, "encoding %s %d format %s %d\n",
		hdr.encodingName, (uint32_t)sizeof(hdr.encodingName), &hdr.encodingVersion,
		hdr.formatName, (uint32_t)sizeof(hdr.formatName), &hdr.formatVersion);

	if (scanCount != DMX_HEADER_COMPONENT_COUNT)
	{
		Error("%s: failed to parse DMX header! (expected %d components, got %d)", __FUNCTION__, DMX_HEADER_COMPONENT_COUNT, scanCount);
		return false;
	}

	char c; // Consume the rest of the text so the cursor points past the header.
	while (bio.Get(c) && c != '\0');

	return true;
}

static bool Dmx_GetStringTable(DmContext_s& ctx, BinaryIO& bio, const int numStrings)
{
	char stack[2048];
	int iter;

	for (iter = 0; iter < numStrings; iter++)
	{
		size_t strLen;

		if (!bio.ReadString(stack, sizeof(stack), &strLen))
			break;

		ctx.symbolTable.AddString({ stack, strLen });
	}

	if (iter != numStrings)
	{
		Error("%s: failed to parse DMX string table! (expected %d strings, got %d)", __FUNCTION__, numStrings, iter);
		return false;
	}

	return true;
}

//static const char* Dme_GetString(BinaryIO& bio, const DmSymbolTable& table)
//{
//	const int symHnd = bio.Read<int>();
//	return table.GetString(symHnd).data();
//}

static bool Dme_ReadElem(BinaryIO& bio, DmElement_s& elem)
{
	elem.name = bio.Read<DmeSymbol_t>();
	elem.type = bio.Read<DmeSymbol_t>();

	return bio.Read(elem.id);
}

char* Dme_GuidToString(const DmObjectId_s& guid, DmObjectIdString_s out)
{
	static constexpr char hexDigits[] = "0123456789abcdef";

	const uint8_t* v = guid.value;
	int pos = 0;

	for (int i = 0; i < 16; ++i)
	{
		out[pos++] = hexDigits[v[i] >> 4];
		out[pos++] = hexDigits[v[i] & 0xF];

		if (i == 3 || i == 5 || i == 7 || i == 9)
			out[pos++] = '-';
	}

	out[pos] = '\0';
	return out;
}

static bool Dmx_DeserializeElements(DmContext_s& ctx, BinaryIO& bio)
{
	const int numElems = bio.Read<int>();

	if (!numElems)
		return true; // valve does this too.

	ctx.elementList.resize(numElems);

	for (int iter = 0; iter < numElems; iter++)
	{
		if (!Dme_ReadElem(bio, ctx.elementList[iter]))
			return false;
	}

	//printf("num elems: %d\n", numElems);
	return true;
}

static DmeSymbol_t Dma_ReadSymHnd(BinaryIO& bio)
{
	return bio.Read<DmeSymbol_t>();
}

static DmeHandle_t Dma_ReadElemIdx(BinaryIO& bio)
{
	const DmeHandle_t hnd = bio.Read<DmeHandle_t>();

	if (hnd.h == DM_ELEMENT_EXTERNAL)
		Error("%s: external element references are not supported!\n", __FUNCTION__);

	return hnd;
}

template <typename T>
static inline T Dma_DoRead(BinaryIO& bio)
{
	return bio.Read<T>();
}

template <>
inline DmeTime_s Dma_DoRead<DmeTime_s>(BinaryIO& bio)
{
	return DmeTime_s(Dma_DoRead<float>(bio));
}

template <typename T>
static void Dma_Deserialize(BinaryIO& bio, DmAttribute_s& attrib)
{
	Dma_SetValue<T>(attrib, Dma_DoRead<T>(bio));
}

static DmAttributeType_e s_lastType;

template <typename T>
static void Dma_ReadArray(BinaryIO& bio, DmAttribute_s& attrib)
{
	DmArray<T> arr(&attrib);
	const int numElems = bio.Read<int>();

	if (!numElems)
		return;

	arr.RemoveAll();
	arr.EnsureCapacity(numElems);

	for (int i = 0; i < numElems; i++)
	{
		const T rd = Dma_DoRead<T>(bio);
		arr.AddToTail(rd);
	}
}

static void Dma_DeserializeAny(BinaryIO& bio, DmAttribute_s& attrib, const DmAttributeType_e type)
{
	switch (type)
	{
	case AT_INT:
		Dma_Deserialize<int>(bio, attrib);
		break;
	case AT_FLOAT:
		Dma_Deserialize<float>(bio, attrib);
		break;
	case AT_BOOL:
		Dma_Deserialize<bool>(bio, attrib);
		break;
//	case AT_STRING:
		//Dma_Deserialize<>(bio, attrib); // todo
//		break;
//	case AT_VOID:
		//Dma_Deserialize<>(bio, attrib); // todo
//		break;
	case AT_TIME:
		Dma_Deserialize<DmeTime_s>(bio, attrib);
		break;
	case AT_COLOR:
		Dma_Deserialize<ColorB>(bio, attrib);
		break;
	case AT_VECTOR2:
		Dma_Deserialize<Vector2>(bio, attrib);
		break;
	case AT_VECTOR3:
		Dma_Deserialize<Vector3>(bio, attrib);
		break;
	case AT_VECTOR4:
		Dma_Deserialize<Vector4>(bio, attrib);
		break;
	case AT_QANGLE:
		Dma_Deserialize<QAngle>(bio, attrib);
		break;
	case AT_QUATERNION:
		Dma_Deserialize<Quaternion>(bio, attrib);
		break;
	case AT_VMATRIX:
		Dma_Deserialize<VMatrix>(bio, attrib);
		break;
	case AT_INT_ARRAY:
		Dma_ReadArray<int>(bio, attrib);
		break;
	case AT_FLOAT_ARRAY:
		Dma_ReadArray<float>(bio, attrib);
		break;
	case AT_BOOL_ARRAY:
		Dma_ReadArray<bool>(bio, attrib);
		break;
//	case AT_STRING_ARRAY:
		//Unserialize<CUtlString>(buf, this->m_pData);
//		break;
//	case AT_VOID_ARRAY:
		//Unserialize<CUtlBinaryBlock>(buf, this->m_pData);
//		break;
	case AT_TIME_ARRAY:
		Dma_ReadArray<DmeTime_s>(bio, attrib);
		break;
	case AT_COLOR_ARRAY:
		Dma_ReadArray<ColorB>(bio, attrib);
		break;
	case AT_VECTOR2_ARRAY:
		Dma_ReadArray<Vector2>(bio, attrib);
		break;
	case AT_VECTOR3_ARRAY:
		Dma_ReadArray<Vector3>(bio, attrib);
		break;
	case AT_VECTOR4_ARRAY:
		Dma_ReadArray<Vector4>(bio, attrib);
		break;
	case AT_QANGLE_ARRAY:
		Dma_ReadArray<QAngle>(bio, attrib);
		break;
	case AT_QUATERNION_ARRAY:
		Dma_ReadArray<Quaternion>(bio, attrib);
		break;
	case AT_VMATRIX_ARRAY:
		Dma_ReadArray<VMatrix>(bio, attrib);
		break;

	default:
		Error("%s: unhandled attribute type %d @ %zd!\n", __FUNCTION__, type, bio.TellGet());
	}

	s_lastType = type;
}

static bool Dma_DeserializeAttribute(const DmContext_s& /*ctx*/, BinaryIO& bio, DmAttribute_s& attrib, const DmAttributeType_e type)
{
	switch (type)
	{
	case AT_ELEMENT:
		Dma_SetValue(attrib, Dma_ReadElemIdx(bio));
		break;
	case AT_ELEMENT_ARRAY:
		Dma_ReadArray<DmeHandle_t>(bio, attrib);
		break;
	case AT_STRING:
		Dma_SetValue(attrib, Dma_ReadSymHnd(bio));
		break;
	case AT_STRING_ARRAY:
		Dma_ReadArray<DmeSymbol_t>(bio, attrib);
		break;

	default:
		Dma_DeserializeAny(bio, attrib, type);
	}

	return true;
}

static bool Dmx_DeserializeAttributes(const DmContext_s& ctx, DmElement_s& elem, BinaryIO& bio)
{
	const int numAttribs = bio.Read<int>();
	elem.attr.resize(numAttribs);

	for (int i = 0; i < numAttribs; i++)
	{
		DmAttribute_s& attrib = elem.attr[i];
		attrib.name = bio.Read<DmeSymbol_t>();

		//printf("%s: %s\n",__FUNCTION__, ctx.symbolTable.GetString(attrib.name.s).data());
		const DmAttributeType_e type = bio.Read<DmAttributeType_e>();
		Dma_DeserializeAttribute(ctx, bio, attrib, type);
	}

	//printf("numAttribs: %d\n", numAttribs);
	return true;
}

#define PRINT_ARRAY(fmt, accessor)                                      \
    {                                                                   \
        auto* arr = static_cast<DmArrayValue_s<decltype(accessor) >*>(attr.value.ptr); \
        printf("{ ");                                                   \
        for (size_t i = 0; i < arr->data.size(); ++i)                   \
        {                                                               \
            printf(fmt, arr->data[i]);                                  \
            if (i + 1 < arr->data.size()) printf(", ");                 \
        }                                                               \
        printf(" }");                                                   \
    }

static void Dma_Print(const DmContext_s& ctx, const DmAttribute_s& attr)
{
	printf("\t%s(%s) = ", ctx.symbolTable.GetString(attr.name.s).data(), Dma_TypeToString(attr.type));

	switch (attr.type)
	{
	case AT_ELEMENT:
		printf("#%d (--> %s)", attr.value.element.h, ctx.symbolTable.GetString(ctx.elementList[attr.value.element.h].name.s).data());
		break;
	case AT_INT:
		printf("%d", attr.value.intVal);
		break;
	case AT_FLOAT:
		printf("%f", attr.value.floatVal);
		break;
	case AT_BOOL:
		printf(attr.value.boolVal ? "true" : "false");
		break;
	case AT_STRING:
		printf("%s", ctx.symbolTable.GetString(attr.value.stringSym.s).data());
		break;
	case AT_VOID:
		printf("ptr: %p", attr.value.ptr);
		break;
	case AT_TIME:
		printf("%d tms (%.4f sec)", attr.value.timeVal.tms, attr.value.timeVal.tms / 10000.0f);
		break;
	case AT_COLOR:
		printf("Color(%u, %u, %u, %u)", attr.value.color.r, attr.value.color.g,
			attr.value.color.b, attr.value.color.a);
		break;
	case AT_VECTOR2:
		printf("Vector2(%.3f, %.3f)", attr.value.vec2.x, attr.value.vec2.y);
		break;
	case AT_VECTOR3:
		printf("Vector3(%.3f, %.3f, %.3f)",
			attr.value.vec3.x, attr.value.vec3.y, attr.value.vec3.z);
		break;
	case AT_VECTOR4:
		printf("Vector4(%.3f, %.3f, %.3f, %.3f)",
			attr.value.vec4.x, attr.value.vec4.y, attr.value.vec4.z, attr.value.vec4.w);
		break;
	case AT_QANGLE:
		printf("QAngle(%.3f, %.3f, %.3f)",
			attr.value.ang.x, attr.value.ang.y, attr.value.ang.z);
		break;
	case AT_QUATERNION:
		printf("Quaternion(%.3f, %.3f, %.3f, %.3f)",
			attr.value.quat.x, attr.value.quat.y, attr.value.quat.z, attr.value.quat.w);
		break;
	case AT_VMATRIX:
		printf("VMatrix[\n");
		for (int i = 0; i < 4; ++i)
		{
			printf("    %.3f %.3f %.3f %.3f\n",
				attr.value.mat.m[i][0],
				attr.value.mat.m[i][1],
				attr.value.mat.m[i][2],
				attr.value.mat.m[i][3]);
		}
		printf("]");
		break;
	case AT_ELEMENT_ARRAY:
		if (attr.value.ptr)
		{
			auto* arr = static_cast<DmArrayValue_s<DmeHandle_t>*>(attr.value.ptr);
			printf("{ ");
			for (size_t i = 0; i < arr->data.size(); ++i)
			{
				const auto& v = arr->data[i];
				printf("#%d (--> %s)", v.h, ctx.symbolTable.GetString(ctx.elementList[v.h].name.s).data());
				if (i + 1 < arr->data.size()) printf(", ");
			}
			printf(" }");
		}
		else printf("{}");
		break;
	case AT_INT_ARRAY:
		if (attr.value.ptr) { PRINT_ARRAY("%d", int()); }
		else printf("{}");
		break;
	case AT_FLOAT_ARRAY:
		if (attr.value.ptr) { PRINT_ARRAY("%f", float()); }
		else printf("{}");
		break;
	case AT_BOOL_ARRAY:
		if (attr.value.ptr)
		{
			auto* arr = static_cast<DmArrayValue_s<bool>*>(attr.value.ptr);
			printf("{ ");
			for (size_t i = 0; i < arr->data.size(); ++i)
			{
				printf(arr->data[i] ? "true" : "false");
				if (i + 1 < arr->data.size()) printf(", ");
			}
			printf(" }");
		}
		else printf("{}");
		break;
	case AT_VECTOR2_ARRAY:
		if (attr.value.ptr)
		{
			auto* arr = static_cast<DmArrayValue_s<Vector2>*>(attr.value.ptr);
			printf("{ ");
			for (size_t i = 0; i < arr->data.size(); ++i)
			{
				const auto& v = arr->data[i];
				printf("(%.2f, %.2f)", v.x, v.y);
				if (i + 1 < arr->data.size()) printf(", ");
			}
			printf(" }");
		}
		else printf("{}");
		break;
	case AT_VECTOR3_ARRAY:
		if (attr.value.ptr)
		{
			auto* arr = static_cast<DmArrayValue_s<Vector3>*>(attr.value.ptr);
			printf("{ ");
			for (size_t i = 0; i < arr->data.size(); ++i)
			{
				const auto& v = arr->data[i];
				printf("(%.2f, %.2f, %.2f)", v.x, v.y, v.z);
				if (i + 1 < arr->data.size()) printf(", ");
			}
			printf(" }");
		}
		else printf("{}");
		break;
	case AT_VECTOR4_ARRAY:
		if (attr.value.ptr)
		{
			auto* arr = static_cast<DmArrayValue_s<Vector4>*>(attr.value.ptr);
			printf("{ ");
			for (size_t i = 0; i < arr->data.size(); ++i)
			{
				const auto& v = arr->data[i];
				printf("(%.2f, %.2f, %.2f, %.2f)", v.x, v.y, v.z, v.w);
				if (i + 1 < arr->data.size()) printf(", ");
			}
			printf(" }");
		}
		else printf("{}");
		break;
	case AT_QANGLE_ARRAY:
		if (attr.value.ptr)
		{
			auto* arr = static_cast<DmArrayValue_s<QAngle>*>(attr.value.ptr);
			printf("{ ");
			for (size_t i = 0; i < arr->data.size(); ++i)
			{
				const auto& v = arr->data[i];
				printf("(%.2f, %.2f, %.2f)", v.x, v.y, v.z);
				if (i + 1 < arr->data.size()) printf(", ");
			}
			printf(" }");
		}
		else printf("{}");
		break;
	case AT_QUATERNION_ARRAY:
		if (attr.value.ptr)
		{
			auto* arr = static_cast<DmArrayValue_s<Quaternion>*>(attr.value.ptr);
			printf("{ ");
			for (size_t i = 0; i < arr->data.size(); ++i)
			{
				const auto& v = arr->data[i];
				printf("(%.2f, %.2f, %.2f, %.2f)", v.x, v.y, v.z, v.w);
				if (i + 1 < arr->data.size()) printf(", ");
			}
			printf(" }");
		}
		else printf("{}");
		break;
	case AT_STRING_ARRAY:
		if (attr.value.ptr)
		{
			auto* arr = static_cast<DmArrayValue_s<DmeSymbol_t>*>(attr.value.ptr);
			printf("{ ");
			for (size_t i = 0; i < arr->data.size(); ++i)
			{
				printf("%s", ctx.symbolTable.GetString(arr->data[i].s).data());
				if (i + 1 < arr->data.size()) printf(", ");
			}
			printf(" }");
		}
		else printf("{}");
		break;

	default:
		printf("<unknown>");
		break;
	}

	printf("\n");
}

#undef PRINT_ARRAY

bool Dmx_DeserializeBinary(DmContext_s& ctx, BinaryIO& bio)
{
	const int numStrings = bio.Read<int>();

	if (!Dmx_GetStringTable(ctx, bio, numStrings))
		return false;

	if (!Dmx_DeserializeElements(ctx, bio))
		return false;

	//size_t j = 0;
	//for (auto& t : ctx.elementList)
	//{
	//	printf("----------------------- dmElem #%zd\n", j++);
	//	printf("%s(%s)\n", ctx.symbolTable.GetString(t.type.s).data(), ctx.symbolTable.GetString(t.name.s).data());

	//	DmObjectIdString_s idStr;
	//	Dme_GuidToString(t.id, idStr);

	//	printf("{%s}\n", idStr);
	//}

	for (size_t i = 0; i < ctx.elementList.size(); i++)
	{
		if (!Dmx_DeserializeAttributes(ctx, ctx.elementList[i], bio))
			return false;
	}

	size_t elemIdx = 0;
	for (auto& t : ctx.elementList)
	{
		printf("%s(%s) #%zu\n", ctx.symbolTable.GetString(t.type.s).data(), ctx.symbolTable.GetString(t.name.s).data(), elemIdx++);
		printf("{\n");

		for (auto& a : t.attr)
		{
			Dma_Print(ctx, a);
		}

		printf("}\n");
	}

	return true;
}
