#pragma once
#include "math/vector.h"
#include "math/color.h"
#include "math/vmatrix.h"
#include "dmarray.h"

// Structs in order for compile-time classification
// to work inside Dma_GetTypeForPod().
struct DmeHandle_t { int h; };
struct DmeSymbol_t { int s; };

struct DmeTime_s
{
	DmeTime_s() : tms(INT_MIN) {}
	explicit DmeTime_s(int tms) : tms(tms) {}
	explicit DmeTime_s(float sec) : tms(RoundSecondsToTMS(sec)) {}
	explicit DmeTime_s(double sec) : tms(RoundSecondsToTMS(sec)) {}

	static int RoundSecondsToTMS(float sec) { return (int)floor(10000.0f * sec + 0.5f); };
	static int RoundSecondsToTMS(double sec) { return (int)floor(10000.0f * sec + 0.5f); };

	int tms;
};

union DmAttributeValue_u
{
	DmeHandle_t element;
	int         intVal;
	float       floatVal;
	bool        boolVal;
	DmeSymbol_t stringSym;
	void*       ptr;
	DmeTime_s   timeVal;
	ColorB      color;
	Vector2     vec2;
	Vector3     vec3;
	Vector4     vec4;
	QAngle      ang;
	Quaternion  quat;
	VMatrix     mat;

	DmAttributeValue_u() { memset(this, 0, sizeof(*this)); }
};

enum DmAttributeType_e : int8_t
{
	AT_UNKNOWN = 0,

	AT_FIRST_VALUE_TYPE,

	AT_ELEMENT = AT_FIRST_VALUE_TYPE,
	AT_INT,
	AT_FLOAT,
	AT_BOOL,
	AT_STRING,
	AT_VOID,
	AT_TIME,
	AT_COLOR,
	AT_VECTOR2,
	AT_VECTOR3,
	AT_VECTOR4,
	AT_QANGLE,
	AT_QUATERNION,
	AT_VMATRIX,

	AT_FIRST_ARRAY_TYPE,

	AT_ELEMENT_ARRAY = AT_FIRST_ARRAY_TYPE,
	AT_INT_ARRAY,
	AT_FLOAT_ARRAY,
	AT_BOOL_ARRAY,
	AT_STRING_ARRAY,
	AT_VOID_ARRAY,
	AT_TIME_ARRAY,
	AT_COLOR_ARRAY,
	AT_VECTOR2_ARRAY,
	AT_VECTOR3_ARRAY,
	AT_VECTOR4_ARRAY,
	AT_QANGLE_ARRAY,
	AT_QUATERNION_ARRAY,
	AT_VMATRIX_ARRAY,

	AT_TYPE_COUNT,
	AT_TYPE_INVALID,
};

inline const char* Dma_TypeToString(const DmAttributeType_e type)
{
	switch (type)
	{
		case AT_UNKNOWN:              return "unknown";
		case AT_ELEMENT:              return "element";
		case AT_INT:                  return "int";
		case AT_FLOAT:                return "float";
		case AT_BOOL:                 return "bool";
		case AT_STRING:               return "string";
		case AT_VOID:                 return "void";
		case AT_TIME:                 return "time";
		case AT_COLOR:                return "color";
		case AT_VECTOR2:              return "vector2";
		case AT_VECTOR3:              return "vector3";
		case AT_VECTOR4:              return "vector4";
		case AT_QANGLE:               return "qangle";
		case AT_QUATERNION:           return "quaternion";
		case AT_VMATRIX:              return "vmatrix";
		case AT_ELEMENT_ARRAY:        return "element array";
		case AT_INT_ARRAY:            return "int array";
		case AT_FLOAT_ARRAY:          return "float array";
		case AT_BOOL_ARRAY:           return "bool array";
		case AT_STRING_ARRAY:         return "string array";
		case AT_VOID_ARRAY:           return "void array";
		case AT_TIME_ARRAY:           return "time array";
		case AT_COLOR_ARRAY:          return "color array";
		case AT_VECTOR2_ARRAY:        return "vector2 array";
		case AT_VECTOR3_ARRAY:        return "vector3 array";
		case AT_VECTOR4_ARRAY:        return "vector4 array";
		case AT_QANGLE_ARRAY:         return "qangle array";
		case AT_QUATERNION_ARRAY:     return "quaternion array";
		case AT_VMATRIX_ARRAY:        return "vmatrix array";
		default:                      return "invalid";
	}
}

template <class T>
constexpr DmAttributeType_e Dma_GetTypeForPod()
{
	if constexpr (std::is_same_v<T, DmeHandle_t>)
		return AT_ELEMENT;
	else if constexpr (std::is_same_v<T, DmeHandle_t*>)
		return AT_ELEMENT_ARRAY;
	else if constexpr (std::is_same_v<T, int>)
		return AT_INT;
	else if constexpr (std::is_same_v<T, int*>)
		return AT_INT_ARRAY;
	else if constexpr (std::is_same_v<T, float>)
		return AT_FLOAT;
	else if constexpr (std::is_same_v<T, float*>)
		return AT_FLOAT_ARRAY;
	else if constexpr (std::is_same_v<T, bool>)
		return AT_BOOL;
	else if constexpr (std::is_same_v<T, bool*>)
		return AT_BOOL_ARRAY;
	else if constexpr (std::is_same_v<T, void>)
		return AT_VOID;
	else if constexpr (std::is_same_v<T, void*>)
		return AT_VOID_ARRAY;
	else if constexpr (std::is_same_v<T, DmeSymbol_t>)
		return AT_STRING;
	else if constexpr (std::is_same_v<T, DmeSymbol_t*>)
		return AT_STRING_ARRAY;
	else if constexpr (std::is_same_v<T, DmeTime_s>)
		return AT_TIME;
	else if constexpr (std::is_same_v<T, DmeTime_s*>)
		return AT_TIME_ARRAY;
	else if constexpr (std::is_same_v<T, ColorB>)
		return AT_COLOR;
	else if constexpr (std::is_same_v<T, ColorB*>)
		return AT_COLOR_ARRAY;
	else if constexpr (std::is_same_v<T, Vector2>)
		return AT_VECTOR2;
	else if constexpr (std::is_same_v<T, Vector2*>)
		return AT_VECTOR2_ARRAY;
	else if constexpr (std::is_same_v<T, Vector3>)
		return AT_VECTOR3;
	else if constexpr (std::is_same_v<T, Vector3*>)
		return AT_VECTOR3_ARRAY;
	else if constexpr (std::is_same_v<T, Vector4>)
		return AT_VECTOR4;
	else if constexpr (std::is_same_v<T, Vector4*>)
		return AT_VECTOR4_ARRAY;
	else if constexpr (std::is_same_v<T, QAngle>)
		return AT_QANGLE;
	else if constexpr (std::is_same_v<T, QAngle*>)
		return AT_QANGLE_ARRAY;
	else if constexpr (std::is_same_v<T, Quaternion>)
		return AT_QUATERNION;
	else if constexpr (std::is_same_v<T, Quaternion*>)
		return AT_QUATERNION_ARRAY;
	else if constexpr (std::is_same_v<T, VMatrix>)
		return AT_VMATRIX;
	else if constexpr (std::is_same_v<T, VMatrix*>)
		return AT_VMATRIX_ARRAY;
	else
		static_assert(std::is_same_v<T, void>, "Cannot classify data type; unsupported.");
}

struct DmAttribute_s
{
	DmAttributeValue_u value;
	DmAttributeType_e type = AT_UNKNOWN;
	DmeSymbol_t name{ -1 };
};

template <typename T>
T Dma_GetValue(const DmAttribute_s& attrib)
{
	constexpr DmAttributeType_e t = Dma_GetTypeForPod<T>();

#ifndef NDEBUG
	if (attrib.type != t)
		Error("%s: requested type does not match stored type", __FUNCTION__);
#endif
	if constexpr (t == AT_ELEMENT)
		return attrib.value.element;
	else if constexpr (t == AT_INT)
		return attrib.value.intVal;
	else if constexpr (t == AT_FLOAT)
		return attrib.value.floatVal;
	else if constexpr (t == AT_BOOL)
		return attrib.value.boolVal;
	else if constexpr (t == AT_STRING)
		return attrib.value.stringSym;
	else if constexpr (t == AT_VOID)
		return attrib.value.ptr;
	else if constexpr (t == AT_TIME)
		return attrib.value.timeVal;
	else if constexpr (t == AT_COLOR)
		return attrib.value.color;
	else if constexpr (t == AT_VECTOR2)
		return attrib.value.vec2;
	else if constexpr (t == AT_VECTOR3)
		return attrib.value.vec3;
	else if constexpr (t == AT_VECTOR4)
		return attrib.value.vec4;
	else if constexpr (t == AT_QANGLE)
		return attrib.value.ang;
	else if constexpr (t == AT_QUATERNION)
		return attrib.value.quat;
	else if constexpr (t == AT_VMATRIX)
		return attrib.value.mat;
	else
		static_assert(std::is_same_v<T, void>, "Unsupported attribute type");
}

template <typename T>
void Dma_SetValue(DmAttribute_s& attrib, const T& v)
{
	constexpr DmAttributeType_e t = Dma_GetTypeForPod<T>();
	attrib.type = t;

	if constexpr (t == AT_ELEMENT)
		attrib.value.element = v;
	else if constexpr (t == AT_INT)
		attrib.value.intVal = v;
	else if constexpr (t == AT_FLOAT)
		attrib.value.floatVal = v;
	else if constexpr (t == AT_BOOL)
		attrib.value.boolVal = v;
	else if constexpr (t == AT_STRING)
		attrib.value.stringSym = v;
	else if constexpr (t == AT_VOID)
		attrib.value.ptr = v;
	else if constexpr (t == AT_TIME)
		attrib.value.timeVal = v;
	else if constexpr (t == AT_COLOR)
		attrib.value.color = v;
	else if constexpr (t == AT_VECTOR2)
		attrib.value.vec2 = v;
	else if constexpr (t == AT_VECTOR3)
		attrib.value.vec3 = v;
	else if constexpr (t == AT_VECTOR4)
		attrib.value.vec4 = v;
	else if constexpr (t == AT_QANGLE)
		attrib.value.ang = v;
	else if constexpr (t == AT_QUATERNION)
		attrib.value.quat = v;
	else if constexpr (t == AT_VMATRIX)
		attrib.value.mat = v;
	else
		printf("Unsupported attribute type!\n");
		//static_assert(std::is_same_v<T, void>, "Unsupported attribute type!");
}

inline int Dma_NumComponents(const DmAttributeType_e type)
{
	switch (type)
	{
	case AT_BOOL:
	case AT_INT:
	case AT_FLOAT:
	case AT_TIME:
		return 1;

	case AT_VECTOR2:
		return 2;

	case AT_VECTOR3:
	case AT_QANGLE:
		return 3;

	case AT_COLOR:
	case AT_VECTOR4:
	case AT_QUATERNION:
		return 4;

	case AT_VMATRIX:
		return 16;

	case AT_ELEMENT:
	case AT_STRING:
	case AT_VOID:
	default:
		return 0;
	}
}

template<typename T>
struct DmArrayValue_s
{
	std::vector<T> data;
};

template<typename T>
class DmArray
{
public:
	DmArray(DmAttribute_s* attr)
		: m_attr(attr)
	{
		assert(attr != nullptr);
		//assert(attr->type == GetExpectedType());
		if (!attr->value.ptr)
		{
			// Allocate.
			attr->value.ptr = new DmArrayValue_s<T>();
		}
		attr->type = Dma_GetTypeForPod<T*>();
		m_vec = static_cast<DmArrayValue_s<T>*>(attr->value.ptr);
	}

	void AddToTail(const T& val) { m_vec->data.push_back(val); }
	void RemoveAll() { m_vec->data.clear(); }
	void EnsureCapacity(const size_t count) { m_vec->data.reserve(count); }
	size_t Count() const { return m_vec->data.size(); }

	T& operator[](size_t i) { return m_vec->data[i]; }
	const T& operator[](size_t i) const { return m_vec->data[i]; }

	auto begin() { return m_vec->data.begin(); }
	auto end() { return m_vec->data.end(); }

private:
	DmAttribute_s* m_attr;
	DmArrayValue_s<T>* m_vec;

	///static DmAttributeType_e GetExpectedType();
};
