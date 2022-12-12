#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <memory>
#include <variant>
#include <vector>

#include "IOBuffer.h"

namespace exmdbpp::structures
{

/**
 * @brief      Tagged property value
 */
class TaggedPropval
{
public:
	template<typename T>
	struct VArray : public std::pair<T*, T*>
	{
		using std::pair<T*, T*>::pair;
		constexpr VArray(T* ptr, uint32_t count) : std::pair<T*, T*>(ptr, ptr+count) {}

		constexpr uint32_t count() const
		{return this->second-this->first;}

		constexpr T* begin() const
		{return this->first;}

		constexpr T* end() const
		{return this->second;}
	};

	TaggedPropval() = default;
	explicit TaggedPropval(IOBuffer&);
	TaggedPropval(const TaggedPropval&);
	TaggedPropval(TaggedPropval&&) noexcept;
	~TaggedPropval();

	TaggedPropval(uint32_t, uint8_t);
	TaggedPropval(uint32_t, uint16_t);
	TaggedPropval(uint32_t, uint32_t);
	TaggedPropval(uint32_t, uint64_t);
	TaggedPropval(uint32_t, float);
	TaggedPropval(uint32_t, double);
	TaggedPropval(uint32_t, const char*, bool=true);
	TaggedPropval(uint32_t, const void*, uint32_t, bool=true);
	TaggedPropval(uint32_t, const std::string&, bool=true);
	TaggedPropval(uint32_t, const uint16_t*, uint32_t, bool=true);
	TaggedPropval(uint32_t, const uint32_t*, uint32_t, bool=true);
	TaggedPropval(uint32_t, const uint64_t*, uint32_t, bool=true);
	TaggedPropval(uint32_t, const float*, uint32_t, bool=true);
	TaggedPropval(uint32_t, const double*, uint32_t, bool=true);
	TaggedPropval(uint32_t, const char**, uint32_t, bool=true);

	TaggedPropval& operator=(const TaggedPropval&);
	TaggedPropval& operator=(TaggedPropval&&) noexcept;

	std::string printValue() const;
	std::string toString() const;
	uint32_t binaryLength() const;
	const void* binaryData() const;

	uint32_t count() const;

	const char* typeName() const;
	static const char* typeName(uint16_t);

	uint32_t tag = 0; ///< Tag identifier
	uint16_t type = 0; ///< Type of the tag (either derived from tag or explicitly specified if tag type is UNSPECIFIED)

	union Value
	{
		uint8_t u8;
		uint16_t u16;
		uint32_t u32;
		uint64_t u64;
		float f;
		double d;
		char* str;
		VArray<uint8_t> data;
		VArray<uint16_t> a16;
		VArray<uint32_t> a32;
		VArray<uint64_t> a64;
		VArray<float> af;
		VArray<double> ad;
		VArray<char*> astr;
		VArray<VArray<uint8_t>> adata;

		constexpr Value() : data(nullptr, nullptr) {}
		constexpr Value(const Value& v) : data(v.data) {}

		Value& operator=(const Value&);
		void zero();
	} value; ///< Data contained by the tag

private:
	bool owned = true; ///< Whether the memory stored in pointer values is owned (automatically deallocated in destructor)

	char* copyStr(const char*);
	void copyValue(const TaggedPropval&);
	void copyData(const void*, uint32_t, size_t=alignof(uint8_t));
	void free();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief      GUID class
 */
struct GUID
{
	GUID() = default;
	GUID(const std::string&);
	constexpr GUID(uint32_t, uint16_t, uint16_t, const std::array<uint8_t,2>&, const std::array<uint8_t, 6>&);

	uint32_t timeLow;
	uint16_t timeMid;
	uint16_t timeHighVersion;
	std::array<uint8_t, 2> clockSeq;
	std::array<uint8_t, 6> node;

	static constexpr GUID fromDomainId(uint32_t);
	static GUID fromString(const std::string&);

	static const GUID PSETID_GROMOX;
};

/**
 * @brief      XID with size information
 */
struct SizedXID
{
	SizedXID(uint8_t, const GUID&, uint64_t);

	void writeXID(IOBuffer&) const;

	GUID guid;
	uint64_t localId;
	uint8_t size;
};

/**
 * @brief      Permission data struct
 */
struct PermissionData
{
	PermissionData() = default;
	PermissionData(uint8_t, const std::vector<TaggedPropval>&);

	uint8_t flags;
	std::vector<TaggedPropval> propvals;

	static const uint8_t ADD_ROW = 0x01;
	static const uint8_t MODIFY_ROW = 0x02;
	static const uint8_t REMOVE_ROW = 0x04;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct PropertyName
{
	PropertyName(const GUID&, uint32_t lid);
	PropertyName(const GUID&, const std::string&);


	uint8_t kind;
	GUID guid;
	uint32_t lid = 0;
	std::string name;

	static const uint8_t ID = 0;
	static const uint8_t NAME = 1;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief      Problem that occurred while setting store properties
 */
struct PropertyProblem
{
	PropertyProblem() = default;
	explicit PropertyProblem(IOBuffer&);

	uint16_t index;  ///< Index in the PropTag array
	uint32_t proptag; ///< PropTag that caused the error
	uint32_t err; ///< Exchange error code
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief      Restriction for filtered table loading
 */
class Restriction
{
public:
	enum Op : uint8_t
	{
		LT = 0x00,  ///< Less-than comparison
		LE,         ///< Less-or-equal comparison
		GT,         ///< Greater-than comparison
		GE,         ///< Greater-or-equal comparison
		EQ,         ///< Equal comparison
		NE,         ///< Not-equal comparison
		RE,         ///< Regex match (?), not implemented
		MEMBER = 0x64, ///< Unknown, not implemented
	};

	static const uint32_t FL_FULLSTRING     = 0; ///< Perform full string match
	static const uint32_t FL_SUBSTRING      = 1; ///< Perform substring match
	static const uint32_t FL_PREFIX         = 2; ///< Perform prefix match
	static const uint32_t FL_IGNORECASE     = 1 << 16; ///< Ignore case
	static const uint32_t FL_IGNORENONSPACE = 1 << 17; ///< Seems to be unused
	static const uint32_t FL_LOOSE          = 1 << 18; ///< Do not require exact match (?) (equivalent to FL_IGNORECASE)

	static Restriction AND(std::vector<Restriction>&&);
	static Restriction OR(std::vector<Restriction>&&);
	static Restriction NOT(Restriction&&);
	static Restriction CONTENT(uint32_t, uint32_t, TaggedPropval&&);
	static Restriction PROPERTY(Op, uint32_t, TaggedPropval&&);
	static Restriction PROPCOMP(Op, uint32_t, uint32_t);
	static Restriction BITMASK(bool, uint32_t, uint32_t);
	static Restriction SIZE(Op, uint32_t, uint32_t);
	static Restriction EXIST(uint32_t);
	static Restriction SUBOBJECT(uint32_t, Restriction&&);
	static Restriction COMMENT(std::vector<TaggedPropval>&&, Restriction&&=Restriction());
	static Restriction COUNT(uint32_t, Restriction&&);
	static Restriction XNULL();

	constexpr Restriction() noexcept {} //should be '= default', workaround for g++/clang bug

	void serialize(IOBuffer&) const;

	operator bool() const;

private:
	enum class Type : uint8_t {
		AND = 0x00,
		OR = 0x01,
		NOT = 0x02,
		CONTENT = 0x03,
		PROPERTY = 0x04,
		PROPCOMP = 0x05,
		BITMASK = 0x06,
		SIZE = 0x07,
		EXIST = 0x08,
		SUBRES= 0x09,
		COMMENT = 0x0a,
		COUNT = 0x0b,
		iXNULL = 0x0c,
		XNULL = 0xff,
	};

	template<typename T>
	struct acu_ptr : public std::unique_ptr<T> //Auto-copy unique_ptr
	{
		using std::unique_ptr<T>::unique_ptr;
		inline acu_ptr(const acu_ptr<T>& other) {if(other) this->reset(new T(*other));}
		inline acu_ptr<T>& operator=(const acu_ptr<T>& other) {this->reset(other? new T(*other): nullptr); return *this;}
	};


	struct RChain
	{
		explicit RChain(std::vector<Restriction>&&);
		std::vector<Restriction> elements;
	};

	struct RNot
	{
		explicit RNot(Restriction&&);
		acu_ptr<Restriction> res;
	};

	struct RContent
	{
		RContent(uint32_t, uint32_t, TaggedPropval&&);
		uint32_t fuzzyLevel;
		uint32_t proptag;
		TaggedPropval propval;
	};

	struct RProp
	{
		RProp(Op, uint32_t, TaggedPropval&&);
		Op op;
		uint32_t proptag;
		TaggedPropval propval;
	};

	struct RPropComp
	{
		RPropComp(Op, uint32_t, uint32_t);
		Op op;
		uint32_t proptag1;
		uint32_t proptag2;
	};

	struct RBitMask
	{
		RBitMask(bool, uint32_t, uint32_t);
		bool all;
		uint32_t proptag;
		uint32_t mask;
	};

	struct RSize
	{
		RSize(Op, uint32_t, uint32_t);
		Op op;
		uint32_t proptag;
		uint32_t size;
	};

	struct RExist
	{
		explicit RExist(uint32_t);
		uint32_t proptag;
	};

	struct RSubObj
	{
		RSubObj(uint32_t, Restriction&&);
		uint32_t subobject;
		acu_ptr<Restriction> res;
	};

	struct RComment
	{
		explicit RComment(std::vector<TaggedPropval>&&, Restriction&&=Restriction::XNULL());
		std::vector<TaggedPropval> propvals;
		acu_ptr<Restriction> res;
	};

	struct RCount
	{
		RCount(uint32_t, Restriction&&);
		uint32_t count;
		acu_ptr<Restriction> subres;
	};

	///////////////////////////////////////////////////////////////////////////////

	template<Type I, typename... Args>
	static Restriction create(Args&&...);

	std::variant<RChain, RChain, RNot, RContent, RProp, RPropComp, RBitMask, RSize, RExist, RSubObj, RComment, RCount, std::monostate> res = std::monostate();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MessageContent
{
	struct AttachmentContent
	{
		AttachmentContent() = default;
		explicit AttachmentContent(IOBuffer&);

		std::vector<TaggedPropval> propvals;
		std::unique_ptr<MessageContent> embedded;
	};

	MessageContent() = default;
	explicit MessageContent(IOBuffer&);

	std::vector<TaggedPropval> propvals;
	std::vector<std::vector<TaggedPropval>> recipients;
	std::vector<AttachmentContent> attachments;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr GUID::GUID(uint32_t timeLow, uint16_t timeMid, uint16_t timeHighVersion, const std::array<uint8_t, 2>& clockSeq,
                     const std::array<uint8_t, 6>& node) :
                     timeLow(timeLow), timeMid(timeMid), timeHighVersion(timeHighVersion), clockSeq(clockSeq), node(node)
{}

/**
 * @brief      Create GUID from domain ID
 *
 * @param      domainId  Domain ID
 *
 * @return     Initialized GUID object
 */
constexpr GUID GUID::fromDomainId(uint32_t domainId)
{return GUID(domainId, 0x0afb, 0x7df6, {0x91, 0x92}, {0x49, 0x88, 0x6a, 0xa7, 0x38, 0xce});}


inline PropertyName::PropertyName(const GUID& guid, uint32_t lid) : kind(PropertyName::ID), guid(guid), lid(lid) {}
inline PropertyName::PropertyName(const GUID& guid, const std::string& name) : kind(PropertyName::NAME), guid(guid), name(name) {}

}

namespace exmdbpp
{

//Serialization declarations for structures

template<> void IOBuffer::Serialize<structures::TaggedPropval>::push(IOBuffer&, const structures::TaggedPropval&);
template<> void IOBuffer::Serialize<structures::PermissionData>::push(IOBuffer&, const structures::PermissionData&);
template<> void IOBuffer::Serialize<structures::GUID>::push(IOBuffer&, const structures::GUID&);
template<> void IOBuffer::Serialize<structures::SizedXID>::push(IOBuffer&, const structures::SizedXID&);
template<> void IOBuffer::Serialize<structures::Restriction>::push(IOBuffer&, const structures::Restriction&);
template<> void IOBuffer::Serialize<structures::PropertyName>::push(IOBuffer&, const structures::PropertyName&);

}
