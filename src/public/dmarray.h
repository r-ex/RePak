#pragma once

struct DmAttribute_s;

//template<typename T>
//struct DmArrayValue_s
//{
//	std::vector<T> data;
//};
//
//template<typename T>
//class DmArray
//{
//public:
//	DmArray(DmAttribute_s* attr)
//		: m_attr(attr)
//	{
//		assert(attr != nullptr);
//		//assert(attr->type == GetExpectedType());
//		if (!attr->value.ptr)
//		{
//			// Allocate.
//			attr->value.ptr = new DmArrayValue_s<T>();
//		}
//		m_vec = static_cast<DmArrayValue_s<T>*>(attr->value.ptr);
//	}
//
//	void AddToTail(const T& val) { m_vec->data.push_back(val); }
//	void RemoveAll() { m_vec->data.clear(); }
//	void EnsureCapacity(const size_t count) { m_vec->data.reserve(count); }
//	size_t Count() const { return m_vec->data.size(); }
//
//	T& operator[](size_t i) { return m_vec->data[i]; }
//	const T& operator[](size_t i) const { return m_vec->data[i]; }
//
//	auto begin() { return m_vec->data.begin(); }
//	auto end() { return m_vec->data.end(); }
//
//private:
//	DmAttribute_s* m_attr;
//	DmArrayValue_s<T>* m_vec;
//
//	///static DmAttributeType_e GetExpectedType();
//};

//template<> inline DmAttributeType_e DmArray<int>::GetExpectedType() { return AT_INT_ARRAY; }
//template<> inline DmAttributeType_e DmArray<float>::GetExpectedType() { return AT_FLOAT_ARRAY; }
//template<> inline DmAttributeType_e DmArray<DmeSymbol_t>::GetExpectedType() { return AT_STRING_ARRAY; }

