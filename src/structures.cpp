/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: 2020-2021 grommunio GmbH
 */
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "constants.h"
#include "exceptions.h"
#include "IOBufferImpl.h"
#include "structures.h"
#include "util.h"

using namespace exmdbpp::constants;

namespace exmdbpp::structures
{
void TaggedPropval::Value::zero()
{data.first = data.second = nullptr;}

TaggedPropval::Value& TaggedPropval::Value::operator=(const Value& v)
{
	data = v.data;
	return *this;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief      Deserialize PropTag from buffer
 *
 * @param      buff  Buffer containing serialized PropTag
 */
TaggedPropval::TaggedPropval(IOBuffer& buff) : value()
{
	tag = buff.pop<uint32_t>();
	type = tag == PropvalType::UNSPECIFIED? buff.pop<uint16_t>() : tag&0xFFFF;
	switch(type)
	{
	default:
		throw SerializationError("Deserialization of type "+std::to_string(type)+" is not supported.");
	case PropvalType::BYTE:
		buff >> value.u8; break;
	case PropvalType::SHORT:
		buff >> value.u16; break;
	case PropvalType::LONG:
	case PropvalType::ERROR:
		buff >> value.u32; break;
	case PropvalType::LONGLONG:
	case PropvalType::CURRENCY:
	case PropvalType::FILETIME:
		buff >> value.u64; break;
	case PropvalType::FLOAT:
		buff >> value.f; break;
	case PropvalType::DOUBLE:
	case PropvalType::FLOATINGTIME:
		buff >> value.d; break;
	case PropvalType::STRING:
	case PropvalType::WSTRING:
		value.str = copyStr(buff.pop<const char*>()); break;
	case PropvalType::BINARY:
		buff >> value.data; break;
	case PropvalType::SHORT_ARRAY:
		buff >> value.a16; break;
	case PropvalType::LONG_ARRAY:
		buff >> value.a32; break;
	case PropvalType::LONGLONG_ARRAY:
	case PropvalType::CURRENCY_ARRAY:
		buff >> value.a64; break;
	case PropvalType::FLOAT_ARRAY:
		buff >> value.af; break;
	case PropvalType::DOUBLE_ARRAY:
	case PropvalType::FLOATINGTIME_ARRAY:
		buff >> value.ad; break;
	case PropvalType::STRING_ARRAY:
	case PropvalType::WSTRING_ARRAY:
		buff >> value.astr;
		for(char*& str : value.astr)
		{
			const char* temp = buff.pop<const char*>();
			size_t len = strlen(temp);
			str = new char[len+1];
			strcpy(str, temp);
		}
		break;
	case PropvalType::BINARY_ARRAY:
		buff >> value.adata; break;
	}
}

/**
 * @brief      Initialize tagged property value (8 bit unsigned)
 *
 * @param      tag        Tag identifier
 * @param      val        Tag value
 *
 * @throw      std::invalid_argument     The tag type does not match 8 bit unsigned
 */
TaggedPropval::TaggedPropval(uint32_t tag, uint8_t val) : tag(tag), type(tag&0xFFFF)
{
	if(type != PropvalType::BYTE)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from uint8_t");
	value.u8 = val;
}

/**
 * @brief      Initialize tagged property value (16 bit unsigned)
 *
 * @param      tag        Tag identifier
 * @param      val        Tag value
 *
 * @throw      std::invalid_argument     The tag type does not match 16 bit unsigned
 */
TaggedPropval::TaggedPropval(uint32_t tag, uint16_t val) : tag(tag), type(tag&0xFFFF)
{
	if(type != PropvalType::SHORT)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from uint16_t");
	value.u16 = val;
}

/**
 * @brief      Initialize tagged property value (32 bit unsigned)
 *
 * @param      tag        Tag identifier
 * @param      val        Tag value
 *
 * @throw      std::invalid_argument     The tag type does not match 32 bit unsigned
 */
TaggedPropval::TaggedPropval(uint32_t tag, uint32_t val) : tag(tag), type(tag&0xFFFF)
{
	if(type != PropvalType::LONG && type != PropvalType::ERROR)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from uint32_t");
	value.u32 = val;
}

/**
 * @brief      Initialize tagged property value (64 bit unsigned)
 *
 * @param      tag        Tag identifier
 * @param      val        Tag value
 *
 * @throw      std::invalid_argument     The tag type does not match 64 bit unsigned
 */
TaggedPropval::TaggedPropval(uint32_t tag, uint64_t val) : tag(tag), type(tag&0xFFFF)
{
	if(type != PropvalType::LONGLONG && type != PropvalType::CURRENCY && type != PropvalType::FILETIME)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from uint64_t");
	value.u64 = val;
}

/**
 * @brief      Initialize tagged property value (32 bit floating point)
 *
 * @param      tag        Tag identifier
 * @param      val        Tag value
 *
 * @throw      std::invalid_argument     The tag type does not match 32 bit float
 */
TaggedPropval::TaggedPropval(uint32_t tag, float val) : tag(tag), type(tag&0xFFFF)
{
	if(type != PropvalType::FLOAT)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from float");
	value.f = val;
}

/**
 * @brief      Initialize tagged property value (64 bit floating point)
 *
 * @param      tag        Tag identifier
 * @param      val        Tag value
 *
 * @throw      std::invalid_argument     The tag type does not match 64 bit float
 */
TaggedPropval::TaggedPropval(uint32_t tag, double val) : tag(tag), type(tag&0xFFFF)
{
	if(type != PropvalType::DOUBLE && type != PropvalType::FLOATINGTIME)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from double");
	value.d = val;
}

/**
 * @brief      Initialize tagged property value (C string)
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match string
 */
TaggedPropval::TaggedPropval(uint32_t tag, const char* val, bool copy) : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::STRING && type != PropvalType::WSTRING)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from char*");
	if(copy)
		value.str = copyStr(val);
	else
		value.str = const_cast<char*>(val);
}

/**
 * @brief      Initialize tagged property value (binary)
 *
 * Serialization expects the first 4 bytes of the data to contain a little endian
 * unsigned 32 bit integer with length information about the following data.
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * The buffer length is automatically prepended to the data.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      len   Length of the buffer
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match binary
 */
TaggedPropval::TaggedPropval(uint32_t tag, const void* val, uint32_t len, bool copy) : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::BINARY)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from void*");
	if(copy)
		copyData(val, len);
	else
	{
		value.data.first = static_cast<uint8_t*>(const_cast<void*>(val));
		value.data.second = value.data.first+len;
	}
}


/**
 * @brief      Initialize tagged property value (C++ string)
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match string
 */
TaggedPropval::TaggedPropval(uint32_t tag, const std::string& val, bool copy) : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::STRING && type != PropvalType::WSTRING)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from string");
	if(val.size() > std::numeric_limits<uint32_t>::max())
		throw std::out_of_range("String exceeds length limit");
	if(copy)
		copyData(val.c_str(), uint32_t(val.size())+1);
	else
		value.str = const_cast<char*>(val.c_str());
}

/**
 * @brief      Initialize tagged property value (16 bit unsigned array)
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match string
 */
TaggedPropval::TaggedPropval(uint32_t tag, const uint16_t* val, uint32_t len, bool copy)
    : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::SHORT_ARRAY)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from 16 bit unsigned array");
	if(copy)
		copyData(val, len*sizeof(uint16_t), alignof(uint16_t));
	else
		value.a16 = VArray<uint16_t>(const_cast<uint16_t*>(val), len);
}

/**
 * @brief      Initialize tagged property value (32 bit unsigned array)
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match string
 */
TaggedPropval::TaggedPropval(uint32_t tag, const uint32_t* val, uint32_t len, bool copy)
    : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::LONG_ARRAY)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from 32 bit unsigned array");
	if(copy)
		copyData(val, len*sizeof(uint32_t), alignof(uint32_t));
	else
		value.a32 = VArray<uint32_t>(const_cast<uint32_t*>(val), len);
}

/**
 * @brief      Initialize tagged property value (64 bit unsigned array)
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match string
 */
TaggedPropval::TaggedPropval(uint32_t tag, const uint64_t* val, uint32_t len, bool copy)
    : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::LONGLONG_ARRAY && type != PropvalType::CURRENCY_ARRAY)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from 64 bit unsigned array");
	if(copy)
		copyData(val, len*sizeof(uint64_t), alignof(uint64_t));
	else
		value.a64 = VArray<uint64_t>(const_cast<uint64_t*>(val), len);
}

/**
 * @brief      Initialize tagged property value (32 bit float array)
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match string
 */
TaggedPropval::TaggedPropval(uint32_t tag, const float* val, uint32_t len, bool copy)
    : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::FLOAT_ARRAY)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from 32 bit float array");
	if(copy)
		copyData(val, len*sizeof(float), alignof(float));
	else
		value.af = VArray<float>(const_cast<float*>(val), len);
}

/**
 * @brief      Initialize tagged property value (64 bit float array)
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match string
 */
TaggedPropval::TaggedPropval(uint32_t tag, const double* val, uint32_t len, bool copy)
    : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::DOUBLE_ARRAY && type != PropvalType::FLOATINGTIME_ARRAY)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from 64 bit float array");
	if(copy)
		copyData(val, len*sizeof(double), alignof(double));
	else
		value.ad = VArray<double>(const_cast<double*>(val), len);
}

/**
 * @brief      Initialize tagged property value (string array)
 *
 * If copy is set to true, the data is copied to an internally managed buffer,
 * otherwise only the pointer to the data is stored.
 *
 * @param      tag   Tag identifier
 * @param      val   Tag value
 * @param      copy  Whether to copy data
 *
 * @throw      std::invalid_argument     The tag type does not match string
 */
TaggedPropval::TaggedPropval(uint32_t tag, const char** val, uint32_t len, bool copy)
    : tag(tag), type(tag&0xFFFF), owned(copy)
{
	if(type != PropvalType::STRING_ARRAY && type != PropvalType::WSTRING_ARRAY)
		throw std::invalid_argument(std::string("Cannot construct ")+typeName()+" tag from string array");
	if(copy)
	{
		copyData(val, len*sizeof(char*), alignof(char*));
		if(val)
			for(char*& str : value.astr)
				str = copyStr(str);
	}
	else
		value.astr = VArray<char*>(const_cast<char**>(val), len);
}

/**
 * @brief      Copy constructor
 *
 * @param      tp    TaggedPropval to copy
 */
TaggedPropval::TaggedPropval(const TaggedPropval& tp) : tag(tp.tag), type(tp.type), owned(tp.owned)
{copyValue(tp);}

/**
 * @brief      Move constructor
 *
 * @param      tp    TaggedPropval to move data from
 */
TaggedPropval::TaggedPropval(TaggedPropval&& tp) noexcept : tag(tp.tag), type(tp.type), value(tp.value), owned(tp.owned)
{tp.value.zero();}


/**
 * @brief      Copy assignment operator
 *
 * @param      tp    TaggedPropval to copy
 *
 * @return     The result of the assignment
 */
TaggedPropval& TaggedPropval::operator=(const TaggedPropval& tp)
{
	if(&tp == this)
		return *this;
	free();
	tag = tp.tag;
	type = tp.type;
	owned = tp.owned;
	copyValue(tp);
	return *this;
}

/**
 * @brief      Move assignment operator
 *
 * @param      tp    TaggedPropval to move data from
 *
 * @return     The result of the assignment
 */
TaggedPropval& TaggedPropval::operator=(TaggedPropval&& tp) noexcept
{
	if(&tp == this)
		return *this;
	free();
	tag = tp.tag;
	type = tp.type;
	value = tp.value;
	owned = tp.owned;
	tp.value.zero();
	return *this;
}

/**
 * @brief      Destructor
 */
TaggedPropval::~TaggedPropval()
{free();}

static std::string hexData(const uint8_t* data, uint32_t len)
{
	static const char* digits = "0123456789ABCDEF";
	std::string str;
	str.reserve(len*2);
	for(const uint8_t* end = data+len; data < end; ++data)
	{
		str += digits[*data>>4];
		str += digits[*data&0xF];
	}
	return str;
}

/**
 * @brief      Generate string representation of contained value
 *
 * Pretty prints contained value into a string.
 *
 * @return     String representation of value, according to type
 */
std::string TaggedPropval::printValue() const
{
	std::string content;
	switch(type)
	{
	default:
		return toString();
	case PropvalType::FILETIME:
		time_t timestamp = util::nxTime(value.u64);
		content = ctime(&timestamp);
		content.pop_back(); break;
	}
	return content;
}

/**
 * @brief      Convert value to string
 *
 * Generates string representation of the contained value.
 * In contrast to printValue(), the value is not interpreted but converted according to its type
 * (i.e. timestamps are not converted into human readable format, etc).
 *
 * @return     String representation of value, according to type
 */
std::string TaggedPropval::toString() const
{
	std::string content;
	switch(type)
	{
	case PropvalType::BYTE:
		content = std::to_string(int(value.u8)); break;
	case PropvalType::SHORT:
		content = std::to_string(int(value.u16)); break;
	case PropvalType::LONG:
	case PropvalType::ERROR:
		content = std::to_string(value.u32); break;
	case PropvalType::LONGLONG:
	case PropvalType::CURRENCY:
	case PropvalType::FILETIME:
		content = std::to_string(value.u64); break;
	case PropvalType::FLOAT:
		content = std::to_string(value.f); break;
	case PropvalType::DOUBLE:
	case PropvalType::FLOATINGTIME:
		content = std::to_string(value.d); break;
	case PropvalType::STRING:
	case PropvalType::WSTRING:
		content = value.str; break;
	case PropvalType::BINARY:
		content = "["+std::to_string(count())+" bytes]"; break;
	case PropvalType::SHORT_ARRAY:
	case PropvalType::LONG_ARRAY:
	case PropvalType::LONGLONG_ARRAY:
	case PropvalType::CURRENCY_ARRAY:
	case PropvalType::FLOAT_ARRAY:
	case PropvalType::DOUBLE_ARRAY:
	case PropvalType::FLOATINGTIME_ARRAY:
	case PropvalType::STRING_ARRAY:
	case PropvalType::WSTRING_ARRAY:
	case PropvalType::BINARY_ARRAY:
		content = "["+std::to_string(count())+" elements]"; break;
	default:
		content = "[UNKNOWN]";
	}
	return content;
}

/**
 * @brief      Return length of the binary data
 *
 * The binary data contained by the TaggedPropval always has the length
 * information prepended to the actual data. This is a convenience function
 * to decode this length information.
 *
 * If the TaggedPropval is not of type BINARY or the buffer is not initialized,
 * 0 is returned.
 *
 * @return     Data length in bytes
 */
uint32_t TaggedPropval::binaryLength() const
{
	return type != PropvalType::BINARY || !value.data.first? 0 : uint32_t(value.data.second-value.data.first);
}

/**
 * @brief      Return pointer to the binary data
 *
 * If the TaggedPropval is not of type BINARY or the buffer is not initialized,
 * nullptr is returned.
 *
 * @return
 */
const void* TaggedPropval::binaryData() const
{return type == PropvalType::BINARY? value.data.first : nullptr;}

/**
 * @brief      Copy string to new buffer
 *
 * @param      str   String to copy
 *
 * @return     Newly allocated string buffer
 */
char* TaggedPropval::copyStr(const char* str)
{
	char* copy = new char[strlen(str)+1];
	strcpy(copy, str);
	return copy;
}

/**
 * @brief      Copy data from another TaggedPropval
 *
 * This operation does not perform a type check and assumes that the types of
 * both TaggedPropvals are aligned.
 *
 * Previously held data must be freed manually before calling copyValue.
 *
 * @param      TaggedPropval to copy data from
 */
void TaggedPropval::copyValue(const TaggedPropval& tp)
{
	if(tp.value.data.first == nullptr || !tp.owned)
		value.data = tp.value.data;
	else if((type == PropvalType::STRING || type == PropvalType::WSTRING))
		value.str = copyStr(tp.value.str);
	else if(type == PropvalType::STRING_ARRAY || type == PropvalType::WSTRING_ARRAY)
	{
		copyData(tp.value.astr.first, tp.value.astr.count()*sizeof(char*), alignof(char*));
		for(char*& str : value.astr)
			str = copyStr(str);
	}
	else if(type == PropvalType::BINARY_ARRAY)
	{
		copyData(nullptr, tp.value.data.count(), alignof(VArray<uint8_t>));
		for(uint32_t i = 0; i < value.adata.count(); ++i)
		{
			uint32_t len = value.adata.first[i].count();
			value.adata.first[i] = VArray<uint8_t>(new uint8_t[len], len);
			memcpy(value.adata.first[i].first, tp.value.adata.first[i].first, len);
		}
	}
	else if(PropvalType::isArray(type))
		copyData(tp.value.data.first, tp.value.data.count(), alignof(std::max_align_t));
	else
		value = tp.value;
}

/**
 * @brief      Copy data to internal buffer
 *
 * @param      data  Data to copy
 * @param      len   Number of bytes
 */
void TaggedPropval::copyData(const void* data, uint32_t len, size_t alignment)
{
	value.data.first = new(std::align_val_t(alignment)) uint8_t[len];
	value.data.second = value.data.first+len;
	if(data)
		memcpy(value.data.first, data, len);
}

/**
 * @brief      Clean up managed buffer
 */
void TaggedPropval::free()
{
	if(!owned || value.data.first == nullptr)
		return;
	if(type == PropvalType::STRING_ARRAY || type == PropvalType::WSTRING_ARRAY)
		for(char* ptr : value.astr)
			delete[] ptr;
	else if(type == PropvalType::BINARY_ARRAY)
		for(auto bin : value.adata)
			delete[] bin.first;
	if(type == PropvalType::STRING || type == PropvalType::WSTRING)
		delete[] value.str;
	else if(PropvalType::isArray(type))
		delete[] value.data.first;
	value.zero();
}

/**
 * @brief      Return value count
 *
 * Return the number of elements of array tags,
 * the number of bytes in a binary tag and
 * 1 for non-array types.
 *
 * @return     Number of values
 */
uint32_t TaggedPropval::count() const
{
	if(!PropvalType::isArray(type))
		return 1;
	if(value.data.first == nullptr)
		return 0;
	switch(type)
	{
	case PropvalType::BINARY:
		return value.data.count();
	case PropvalType::SHORT_ARRAY:
		return value.a16.count();
	case PropvalType::LONG_ARRAY:
		return value.a32.count();
	case PropvalType::LONGLONG_ARRAY:
	case PropvalType::CURRENCY_ARRAY:
		return value.a64.count();
	case PropvalType::FLOAT_ARRAY:
		return value.af.count();
	case PropvalType::DOUBLE_ARRAY:
	case PropvalType::FLOATINGTIME_ARRAY:
		return value.ad.count();
	case PropvalType::STRING_ARRAY:
	case PropvalType::WSTRING_ARRAY:
		return value.astr.count();
	case PropvalType::BINARY_ARRAY:
		return value.adata.count();
	}
	return 0;
}

/**
 * @brief      String representation of tag type
 */
const char* TaggedPropval::typeName() const
{return typeName(type);}

/**
 * @brief      String representation of tag type
 */
const char* TaggedPropval::typeName(uint16_t type)
{
	switch(type)
	{
	case PropvalType::BYTE:
		return "BYTE";
	case PropvalType::SHORT:
		return "SHORT";
	case PropvalType::LONG:
		return "LONG";
	case PropvalType::ERROR:
		return "ERROR";
	case PropvalType::LONGLONG:
		return "LONGLONG";
	case PropvalType::CURRENCY:
		return "CURRENCY";
	case PropvalType::FILETIME:
		return "FILETIME";
	case PropvalType::FLOAT:
		return "FLOAT";
	case PropvalType::DOUBLE:
		return "DOUBLE";
	case PropvalType::FLOATINGTIME:
		return "FLOATINGTIME";
	case PropvalType::STRING:
		return "STRING";
	case PropvalType::WSTRING:
		return "WSTRING";
	case PropvalType::BINARY:
		return "BINARY";
	case PropvalType::SHORT_ARRAY:
		return "SHORT ARRAY";
	case PropvalType::LONG_ARRAY:
		return "LONG ARRAY";
	case PropvalType::LONGLONG_ARRAY:
		return "LONGLONG ARRAY";
	case PropvalType::CURRENCY_ARRAY:
		return "CURRENCY ARRAY";
	case PropvalType::FLOAT_ARRAY:
		return "FLOAT ARRAY";
	case PropvalType::DOUBLE_ARRAY:
		return "DOUBLE ARRAY";
	case PropvalType::FLOATINGTIME_ARRAY:
		return "FLOATINGTIME ARRAY";
	case PropvalType::STRING_ARRAY:
		return "STRING ARRAY";
	case PropvalType::WSTRING_ARRAY:
		return "WSTRING ARRAY";
	case PropvalType::BINARY_ARRAY:
		return "BINARY ARRAY";
	}
	return "UNKNOWN";
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const GUID GUID::PSETID_GROMOX{0x1DE937E2, 0x85C6, 0x40A1, {0xBD, 0x9D}, {0xA6, 0xE2, 0xB7, 0xB7, 0x87, 0xB1}};

/**
 * @brief      Initialize GUID from string
 *
 * The string must be in the format xxxxxxxx-xxxx-xxxx-xxxxxxxxxxxx, where
 * each x is a hexadecimal character.
 *
 * @param      str      String to parse
 *
 * @throws     std::invalid_argument      Input string parsing failed
 */
GUID::GUID(const std::string& str)
{
	if(sscanf(str.c_str(),"%08x-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
	          &timeLow, &timeMid, &timeHighVersion, &clockSeq[0], &clockSeq[1],
	          &node[0], &node[1], &node[2], &node[3], &node[4], &node[5]) != 11)
		throw std::invalid_argument("Failed to parse guid '"+str+"'");
}


/**
 * @brief      Initialize XID with size information
 *
 * @param      size     Size of the XID object
 * @param      guid     GUID object
 * @param      localId  Local ID value (see util::valueToGc)
 */
SizedXID::SizedXID(uint8_t size, const GUID& guid, uint64_t localId) : guid(guid), localId(localId), size(size)
{}

/**
 * @brief      Write XID to buffer
 *
 * @param      buff     Buffer to write XID to
 */
void SizedXID::writeXID(IOBuffer& buff) const
{
	if(size < 17 || size > 24)
		throw SerializationError("Invalid XID size: "+std::to_string(size));
	buff.push(guid);
	buff.push_raw(&localId, size-16);
}

const uint8_t PermissionData::ADD_ROW;
const uint8_t PermissionData::MODIFY_ROW;
const uint8_t PermissionData::REMOVE_ROW;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief      Construct new PermissionData object
 *
 * @param      flags     Operation flags
 * @param      propvals  List of TaggedPropvals to modify
 */
PermissionData::PermissionData(uint8_t flags, const std::vector<TaggedPropval>& propvals) : flags(flags), propvals(propvals)
{}


/**
 * @brief      Load PropertyProblem from buffer
 *
 * @param      buff     Buffer to read data from
 */
PropertyProblem::PropertyProblem(IOBuffer& buff)
    : index(buff.pop<uint16_t>()), proptag(buff.pop<uint32_t>()), err(buff.pop<uint32_t>())
{}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline Restriction::RChain::RChain(std::vector<Restriction>&& ress) : elements(std::move(ress)) {}
inline Restriction::RNot::RNot(Restriction&& res) : res(new Restriction(std::move(res))) {}
inline Restriction::RContent::RContent(uint32_t fl, uint32_t pt, TaggedPropval&& tp) : fuzzyLevel(fl), proptag(pt != 0? pt : tp.tag), propval(std::move(tp)) {}
inline Restriction::RProp::RProp(Op op, uint32_t pt, TaggedPropval&& tp) : op(op), proptag(pt != 0? pt : tp.tag), propval(std::move(tp)) {}
inline Restriction::RPropComp::RPropComp(Op op, uint32_t pt1, uint32_t pt2) : op(op), proptag1(pt1), proptag2(pt2) {}
inline Restriction::RBitMask::RBitMask(bool all, uint32_t pt, uint32_t mask): all(all), proptag(pt), mask(mask) {}
inline Restriction::RSize::RSize(Op op, uint32_t pt, uint32_t size) : op(op), proptag(pt), size(size) {}
inline Restriction::RExist::RExist(uint32_t pt) : proptag(pt) {}
inline Restriction::RSubObj::RSubObj(uint32_t so, Restriction&& res) : subobject(so), res(new Restriction(std::move(res))) {}
inline Restriction::RComment::RComment(std::vector<TaggedPropval>&& tvs, Restriction&& res) : propvals(std::move(tvs)), res(res.res.index() == size_t(Type::iXNULL)? new Restriction(std::move(res)) : nullptr) {}
inline Restriction::RCount::RCount(uint32_t count, Restriction&& res) : count(count), subres(new Restriction(std::move(res))) {}

template<Restriction::Type I, typename... Args>
inline Restriction Restriction::create(Args&&... args)
{
	Restriction r;
	r.res.emplace<size_t(I)>(std::forward<Args>(args)...);
	return r;
}

/**
 * @brief       Create a new AND restriction chain
 *
 * Resulting restriction matches iff all sub restrictions match.
 *
 * @param       ress    Vector of sub restrictions
 *
 * @return      New AND restriction
 */
Restriction Restriction::AND(std::vector<Restriction>&& ress)
{return create<Type::AND>(std::move(ress));}

/**
 * @brief       Create a new OR restriction chain
 *
 * Resulting restriction matches iff at least on sub restriction matches.
 *
 * @param       ress    Vector of sub restrictions
 *
 * @return      New OR restriction
 */
Restriction Restriction::OR(std::vector<Restriction>&& ress)
{return create<Type::OR>(std::move(ress));}

/**
 * @brief       Create a new NOT restriction
 *
 * Resulting restriction matches iff sub restriction does not match
 *
 * @param       res     Sub restriction
 *
 * @return      New NOT restriction
 */
Restriction Restriction::NOT(Restriction&& res)
{return create<Type::NOT>(std::move(res));}

/**
 * @brief       Create a new CONTENT restriction
 *
 * Resulting restriction matches iff the string is contained in the property. Can only be applied to UNICODE proptags.
 *
 * `fuzzyLevel` can be one of FL_FULLSTRING, FL_SUBSTRING or FL_PREFIX,
 * optionally combined with one or more of FL_IGNORECASE, FL_IGNORENOSPACE or
 * FL_LOOSE.
 *
 * @param       fuzzyLevel  How precise the match must be
 * @param       proptag     Tag to match (or 0 to derive automatically from `propval`)
 * @param       propval     Propval to match against
 *
 * @return      New CONTENT restriction
 */
Restriction Restriction::CONTENT(uint32_t fuzzyLevel, uint32_t proptag, TaggedPropval&& propval)
{return create<Type::CONTENT>(fuzzyLevel, proptag, std::move(propval));}

/**
 * @brief       Create a new PROPERTY restriction
 *
 * Resulting restriction matches iff the property exists and matches (according
 * to the operator) the provided property.
 *
 * @param       op          Operator to apply
 * @param       proptag     Tag to match (or 0 to derive automatically from `propval`)
 * @param       propval     Propval to match against
 *
 * @return      New PROPERTY restriction
 */
Restriction Restriction::PROPERTY(Op op, uint32_t proptag, TaggedPropval&& propval)
{return create<Type::PROPERTY>(op, proptag, std::move(propval));}

/**
 * @brief       Create a new PROPCOMP restriction
 *
 * Resulting restriction matches iff the first tag matches (according to the
 * operator) the second tag.
 *
 * @param       op          Operator to apply
 * @param       proptag1    First operand
 * @param       proptag2    Second operand
 *
 * @return      New PROPCOMP restriction
 */
Restriction Restriction::PROPCOMP(Op op, uint32_t proptag1, uint32_t proptag2)
{return create<Type::PROPCOMP>(op, proptag1, proptag2);}

/**
 * @brief       Create a new BITMASK restriction
 *
 * Resulting restriction matches iff the bitmask overlaps with at least 1
 * (all = true) or no (all = false) bits of the target property.
 *
 * Can only be applied to LONG properties.
 *
 * @param       all         Whether the bitmask must match
 * @param       proptag     Tag to match
 * @param       mask        Bitmask to apply
 *
 * @return      New CONTENT restriction
 */
Restriction Restriction::BITMASK(bool all, uint32_t proptag, uint32_t mask)
{return create<Type::BITMASK>(all, proptag, mask);}

/**
 * @brief       Create a new SIZE restriction
 *
 * Resulting restriction matches iff the size of the proptag matches (according
 * to the operator) the specified size
 *
 * @param       op          Operator to apply
 * @param       proptag     Tag to match
 * @param       size        Memory size of tag value in bytes
 *
 * @return      New SIZE restriction
 */
Restriction Restriction::SIZE(Op op, uint32_t proptag, uint32_t size)
{return create<Type::SIZE>(op, proptag, size);}

/**
 * @brief       Create a new EXIST restriction
 *
 * Resulting restriction matches iff the proptag exists.
 *
 * @param       proptag     Tag to check
 *
 * @return      New EXIST restriction
 */
Restriction Restriction::EXIST(uint32_t proptag)
{return create<Type::EXIST>(proptag);}

/**
 * @brief       Create a new SUBOBJECT restriction
 *
 * Apply restriction to a specific subobject. Possible subobjects are
 * `MESSAGERECIPIENTS` and `MESSAGEATTACHMENTS` properties.
 *
 * @param       res         Restriction to apply
 *
 * @return      New SUBOBJECT restriction
 */
Restriction Restriction::SUBOBJECT(uint32_t subobject, Restriction&& res)
{return create<Type::SUBRES>(subobject, std::move(res));}

/**
 * @brief       Create a new COMMENT restriction
 *
 * Restriction with arbitrary (unused) metadata.
 *
 * Matches iff the sub-restriction matches.
 *
 * @param       propvals    Properties acting as comments
 * @param       res         Restriction to apply
 *
 * @return      New COMMENT restriction
 */
Restriction Restriction::COMMENT(std::vector<TaggedPropval>&& propvals, Restriction&& res)
{return create<Type::COMMENT>(std::move(propvals), std::move(res));}

/**
 * @brief       Create a new COUNT restriction
 *
 * Resulting restriction matches iff sub-restriction matches, but only at most
 * `count` times.
 *
 * @param       count       Maximum number of matches
 * @param       subres      Sub-restriction to count
 *
 * @return      New SIZE restriction
 */
Restriction Restriction::COUNT(uint32_t count, Restriction&& subres)
{return create<Type::COUNT>(count, std::move(subres));}

/**
 * @brief       Create a new NULL restriction
 *
 * The NULL restriction is only a virtual construct and is not serialized and
 * sent to the server.
 *
 * @return      New NULL restriction
 */
Restriction Restriction::XNULL()
{return Restriction();}

/**
 * @brief       Serialize Restriction into IOBuffer
 *
 * @param       buff    Buffer to write serialized data to
 */
void Restriction::serialize(IOBuffer& buff) const
{
	Type type = Type(res.index());
	if(type == Type::iXNULL)
		return;
	buff.push(uint8_t(type));
	switch(type)
	{
	case Type::AND:
	case Type::OR: {
		const std::vector<Restriction>* ress = type == Type::AND? &std::get<size_t(Type::AND)>(res).elements : &std::get<size_t(Type::OR)>(res).elements;
		if(ress->size() > std::numeric_limits<uint32_t>::max())
			throw SerializationError("Too many sub-restrictions ("+std::to_string(ress->size())+")");
		buff.push(uint32_t(ress->size()));
		for(const Restriction& r : *ress)
			buff.push(r);
		return;
	}
	case Type::NOT:
		return buff.push(*std::get<size_t(Type::NOT)>(res).res);
	case Type::CONTENT: {
		const RContent& r = std::get<size_t(Type::CONTENT)>(res);
		return buff.push(r.fuzzyLevel, r.proptag, r.propval);
	}
	case Type::PROPERTY: {
		const RProp& r = std::get<size_t(Type::PROPERTY)>(res);
		return buff.push(uint8_t(r.op), r.proptag, r.propval);
	}
	case Type::PROPCOMP: {
		const RPropComp& r = std::get<size_t(Type::PROPCOMP)>(res);
		return buff.push(uint8_t(r.op), r.proptag1, r.proptag2);
	}
	case Type::BITMASK: {
		const RBitMask& r = std::get<size_t(Type::BITMASK)>(res);
		return buff.push(uint8_t(!r.all), r.proptag, r.mask);
	}
	case Type::SIZE: {
		const RSize& r = std::get<size_t(Type::SIZE)>(res);
		return buff.push(uint8_t(r.op), r.proptag, r.size);
	}
	case Type::EXIST: {
		const RExist& r = std::get<size_t(Type::EXIST)>(res);
		return buff.push(r.proptag);
	}
	case Type::SUBRES: {
		const RSubObj& r = std::get<size_t(Type::SUBRES)>(res);
		return buff.push(r.subobject, *r.res);
	}
	case Type::COMMENT: {
		const RComment& r = std::get<size_t(Type::COMMENT)>(res);
		if(r.propvals.size() == 0 || r.propvals.size() > 255)
			throw SerializationError("Invalid COMMENT restriction propval count "+std::to_string(r.propvals.size()));
		buff.push(uint8_t(r.propvals.size()));
		for(const TaggedPropval& tp : r.propvals)
			buff.push(tp);
		return r.res? buff.push(uint8_t(1), *r.res) : buff.push(uint8_t(0));
	}
	case Type::COUNT: {
		const RCount& r = std::get<size_t(Type::COUNT)>(res);
		return buff.push(r.count, *r.subres);
	}
	default:
		throw SerializationError("Invalid restriction type "+std::to_string(uint8_t(type)));
	}
}

/**
 * @brief       Check whether the restriction is non-empty
 */
Restriction::operator bool() const
{return res.index() != size_t(Type::iXNULL);}


}

namespace exmdbpp
{

using namespace structures;

/**
 * @brief      De-/Serialization specialization for TaggedPropval::VArray
 */
template<typename T>
struct IOBuffer::Serialize<TaggedPropval::VArray<T>>
{
	/**
	 * @brief      Serialize VArray to buffer
	 *
	 * @param      buff  Buffer to write data to
	 * @param      pv    TaggedPropval to serialize
	 */
	static void push(IOBuffer& buff, const TaggedPropval::VArray<T>& va)
	{
		buff << va.count();
		if constexpr(sizeof(T) == 1)
		    return buff.push_raw(va.first, va.count());
		for(T v : va)
			buff << v;
	}

	/**
	 * @brief      Read VArray from buffer
	 *
	 * @param      buff  Buffer to write data to
	 * @param      pv    TaggedPropval to serialize
	 */
	static void pop(IOBuffer& buff, TaggedPropval::VArray<T>& va)
	{
		uint32_t count = buff.pop<uint32_t>();
		va.first = new T[count];
		va.second = va.first+count;
		if constexpr(sizeof(T) == 1)
		    memcpy(va.first, buff.pop_raw(count), count);
		else if constexpr(!std::is_pointer_v<T>)
		    for(T& v : va)
		        buff >> v;
	}
};

/**
 * @brief      Serialize tagged propval to buffer
 *
 * @param      buff  Buffer to write data to
 * @param      pv    TaggedPropval to serialize
 */
template<>
void IOBuffer::Serialize<TaggedPropval>::push(IOBuffer& buff, const TaggedPropval& pv)
{
	buff << pv.tag;
	if(PropvalType::tagType(pv.tag) == PropvalType::UNSPECIFIED)
		buff << pv.type;
	switch(pv.type)
	{
	default:
		throw SerializationError("Serialization of type "+std::to_string(pv.type)+" is not supported.");
	case PropvalType::BYTE:
		buff << pv.value.u8; break;
	case PropvalType::SHORT:
		buff << pv.value.u16; break;
	case PropvalType::LONG:
	case PropvalType::ERROR:
		buff << pv.value.u32; break;
	case PropvalType::LONGLONG:
	case PropvalType::CURRENCY:
	case PropvalType::FILETIME:
		buff << pv.value.u64; break;
	case PropvalType::FLOAT:
		buff << pv.value.f; break;
	case PropvalType::DOUBLE:
	case PropvalType::FLOATINGTIME:
		buff << pv.value.d; break;
	case PropvalType::STRING:
	case PropvalType::WSTRING:
		buff << pv.value.str; break;
	case PropvalType::BINARY:
		buff << pv.value.data; break;
	case PropvalType::SHORT_ARRAY:
		buff << pv.value.a16; break;
	case PropvalType::LONG_ARRAY:
		buff << pv.value.a32; break;
	case PropvalType::LONGLONG_ARRAY:
	case PropvalType::CURRENCY_ARRAY:
		buff << pv.value.a64; break;
	case PropvalType::FLOAT_ARRAY:
		buff << pv.value.af; break;
	case PropvalType::DOUBLE_ARRAY:
	case PropvalType::FLOATINGTIME_ARRAY:
		buff << pv.value.ad; break;
	case PropvalType::WSTRING_ARRAY:
	case PropvalType::STRING_ARRAY:
		buff << pv.value.astr; break;
	case PropvalType::BINARY_ARRAY:
		buff << pv.value.adata; break;
	}
}

/**
 * @brief      Serialize GUID to buffer
 *
 * @param      buff  Buffer to write data to
 * @param      guid  GUID to serialize
 */
template<>
void IOBuffer::Serialize<GUID>::push(IOBuffer& buff, const GUID& guid)
{buff.push(guid.timeLow, guid.timeMid, guid.timeHighVersion, guid.clockSeq, guid.node);}

/**
 * @brief      Serialize XID with size information to buffer
 *
 * @param      buff  Buffer to write data to
 * @param      sXID  SizedXID to serialize
 */
template<>
void IOBuffer::Serialize<SizedXID>::push(IOBuffer& buff, const SizedXID& sXID)
{
	if(sXID.size < 17 || sXID.size > 24)
		throw SerializationError("Invalid XID size: "+std::to_string(sXID.size));
	buff.push(sXID.size);
	sXID.writeXID(buff);
}

/**
 * @brief      Serialize PermissionData to buffer
 *
 * @param      buff  Buffer to write data to
 */
template<>
void IOBuffer::Serialize<PermissionData>::push(IOBuffer& buff, const PermissionData& pd)
{
	buff.push(pd.flags, uint16_t(pd.propvals.size()));
	for(auto& propval : pd.propvals)
		buff.push(propval);
}

template<>
void IOBuffer::Serialize<Restriction>::push(IOBuffer& buff, const Restriction& res)
{res.serialize(buff);}

template<>
void IOBuffer::Serialize<PropertyName>::push(IOBuffer& buff, const PropertyName& pn)
{
	buff.push(pn.kind, pn.guid);
	if(pn.kind == PropertyName::ID)
		return buff.push(pn.lid);
	if(pn.kind != PropertyName::NAME)
		return;
	if(pn.name.size() > std::numeric_limits<uint8_t>::max()-1)
		throw std::range_error("Cannot serialize named property: Name too long ("+std::to_string(pn.name.size())+
	                           "vs 254 chars)");
	buff.push(uint8_t(pn.name.size()+1), pn.name);
}

}
