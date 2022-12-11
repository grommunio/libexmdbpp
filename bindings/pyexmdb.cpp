#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "queries.h"

namespace py = pybind11;

using namespace exmdbpp::constants;
using namespace exmdbpp::requests;
using namespace exmdbpp::queries;
using namespace exmdbpp::structures;

using release_gil = py::call_guard<py::gil_scoped_release>;


template<typename T, size_t N>
constexpr inline size_t arrsize(T(&)[N])
{return N;}

std::string hexstr(uint32_t value, uint8_t width)
{
	static const char* chars = "0123456789abcdef";
	std::string res("0x");
	res.reserve(width+2);
	for(int64_t nibble = 4*(width-1); nibble >= 0; nibble -= 4)
		res += chars[(value&(0xFULL<<nibble))>>nibble];
	return res;
}


std::string TaggedPropval_repr(const TaggedPropval& tp)
{return "TaggedPropval("+hexstr(tp.tag, 8)+", "+tp.toString()+")";}

std::string Folder_repr(const Folder& f)
{return "<Folder '"+f.displayName+"'>";}

std::string FolderList_repr(const FolderList& fl)
{return "<List of "+std::to_string(fl.folders.size())+" folder"+(fl.folders.size() == 1? "" : "s")+">";}

///////////////////////////////////////////////////////////////////////////////
// Wrappers for TaggedPropvals

static char* copyStr(const char* str)
{
	char* copy = new char[strlen(str)+1];
	strcpy(copy, str);
	return copy;
}

TaggedPropval TaggedPropval_init(uint32_t tag, const py::object& value) try
{
	switch(PropvalType::tagType(tag))
	{
	case PropvalType::BYTE:
		return TaggedPropval(tag, value.cast<uint8_t>());
	case PropvalType::SHORT:
		return TaggedPropval(tag, value.cast<uint16_t>());
	case PropvalType::LONG:
	case PropvalType::ERROR:
		return TaggedPropval(tag, value.cast<uint32_t>());
	case PropvalType::LONGLONG:
	case PropvalType::CURRENCY:
	case PropvalType::FILETIME:
		return TaggedPropval(tag, value.cast<uint64_t>());
	case PropvalType::FLOAT:
		return TaggedPropval(tag, value.cast<float>());
	case PropvalType::DOUBLE:
	case PropvalType::FLOATINGTIME:
		return TaggedPropval(tag, value.cast<double>());
	case PropvalType::STRING:
	case PropvalType::WSTRING:
		return TaggedPropval(tag, value.cast<std::string>());
	case PropvalType::BINARY: {
		std::string content = value.cast<std::string>();
		return TaggedPropval(tag, static_cast<const void*>(content.c_str()), uint32_t(content.size()));
	}
	}
	if(!py::isinstance<py::list>(value))
		throw py::type_error("Cannot store value of type "+std::string(py::str(value.get_type()))+" in "+TaggedPropval::typeName(tag&0xFFFF)+" tag.");
	py::list list = value.cast<py::list>();
	switch(PropvalType::tagType(tag))
	{
	case PropvalType::SHORT_ARRAY: {
		TaggedPropval tp(tag, static_cast<uint16_t*>(nullptr), uint32_t(list.size()));
		for(uint32_t i = 0; i < list.size(); ++i)
			tp.value.a16.first[i] = list[i].cast<uint16_t>();
		return tp;
	}
	case PropvalType::LONG_ARRAY: {
		TaggedPropval tp(tag, static_cast<uint32_t*>(nullptr), uint32_t(list.size()));
		for(uint32_t i = 0; i < list.size(); ++i)
			tp.value.a32.first[i] = list[i].cast<uint32_t>();
		return tp;
	}
	case PropvalType::LONGLONG_ARRAY:
	case PropvalType::CURRENCY_ARRAY: {
		TaggedPropval tp(tag, static_cast<uint64_t*>(nullptr), uint32_t(list.size()));
		for(uint32_t i = 0; i < list.size(); ++i)
			tp.value.a64.first[i] = list[i].cast<uint64_t>();
		return tp;
	}
	case PropvalType::FLOAT_ARRAY: {
		TaggedPropval tp(tag, static_cast<float*>(nullptr), uint32_t(list.size()));
		for(uint32_t i = 0; i < list.size(); ++i)
			tp.value.af.first[i] = list[i].cast<float>();
		return tp;
	}
	case PropvalType::DOUBLE_ARRAY:
	case PropvalType::FLOATINGTIME_ARRAY: {
		TaggedPropval tp(tag, static_cast<double*>(nullptr), uint32_t(list.size()));
		for(uint32_t i = 0; i < list.size(); ++i)
			tp.value.ad.first[i] = list[i].cast<double>();
		return tp;
	}
	case PropvalType::STRING_ARRAY:
	case PropvalType::WSTRING_ARRAY: {
		TaggedPropval tp(tag, static_cast<const char**>(nullptr), uint32_t(list.size()));
		for(uint32_t i = 0; i < list.size(); ++i)
			tp.value.astr.first[i] = copyStr(list[i].cast<std::string>().c_str());
		return tp;
	}
	}
	throw py::value_error("Unsupported tag type");
} catch (const py::cast_error&)
{
	std::string pytype = py::str(value.get_type());
	throw py::type_error("Cannot store value of type "+pytype+" in "+TaggedPropval::typeName(tag&0xFFFF)+" tag.");
}


py::object TaggedPropval_getValue(const TaggedPropval& tp)
{
	switch(tp.type)
	{
	case PropvalType::BYTE:
		return py::cast(tp.value.u8);
	case PropvalType::SHORT:
		return py::cast(tp.value.u16);
	case PropvalType::LONG:
	case PropvalType::ERROR:
		return py::cast(tp.value.u32);
	case PropvalType::LONGLONG:
	case PropvalType::CURRENCY:
	case PropvalType::FILETIME:
		return py::cast(tp.value.u64);
	case PropvalType::FLOAT:
		return py::cast(tp.value.f);
	case PropvalType::DOUBLE:
	case PropvalType::FLOATINGTIME:
		return py::cast(tp.value.d);
	case PropvalType::STRING:
	case PropvalType::WSTRING:
		return py::cast(tp.value.str);
	case PropvalType::BINARY:
		return py::object(py::bytes(reinterpret_cast<const char*>(tp.binaryData()), tp.binaryLength()));
	case PropvalType::SHORT_ARRAY: {
		py::list list(tp.count());
		for(uint32_t i = 0; i < tp.count(); ++i)
			list[i] = py::cast(tp.value.a16.first[i]);
		return list;
	}
	case PropvalType::LONG_ARRAY: {
		py::list list(tp.count());
		for(uint32_t i = 0; i < tp.count(); ++i)
			list[i] = py::cast(tp.value.a32.first[i]);
		return list;
	}
	case PropvalType::LONGLONG_ARRAY:
	case PropvalType::CURRENCY_ARRAY: {
		py::list list(tp.count());
		for(uint32_t i = 0; i < tp.count(); ++i)
			list[i] = py::cast(tp.value.a64.first[i]);
		return list;
	}
	case PropvalType::FLOAT_ARRAY: {
		py::list list(tp.count());
		for(uint32_t i = 0; i < tp.count(); ++i)
			list[i] = py::cast(tp.value.af.first[i]);
		return list;
	}
	case PropvalType::DOUBLE_ARRAY:
	case PropvalType::FLOATINGTIME_ARRAY: {
		py::list list(tp.count());
		for(uint32_t i = 0; i < tp.count(); ++i)
			list[i] = py::cast(tp.value.ad.first[i]);
		return list;
	}
	case PropvalType::STRING_ARRAY:
	case PropvalType::WSTRING_ARRAY: {
		py::list list(tp.count());
		for(uint32_t i = 0; i < tp.count(); ++i)
			list[i] = py::cast(tp.value.astr.first[i]);
		return list;
	}
	case PropvalType::BINARY_ARRAY: {
		py::list list(tp.count());
		for(uint32_t i = 0; i < tp.count(); ++i)
			list[i] = py::bytes(reinterpret_cast<const char*>(tp.binaryData()), tp.binaryLength());
		return list;
	}
	}
	return py::none();
}

void TaggedPropval_setValue(TaggedPropval& tp, const py::object& value) try
{
	switch(tp.type)
	{
	case PropvalType::BYTE:
		tp.value.u8 = value.cast<uint8_t>(); return;
	case PropvalType::SHORT:
		tp.value.u16 = value.cast<uint16_t>(); return;
	case PropvalType::LONG:
	case PropvalType::ERROR:
		tp.value.u32 = value.cast<uint32_t>(); return;
	case PropvalType::LONGLONG:
	case PropvalType::CURRENCY:
	case PropvalType::FILETIME:
		tp.value.u64 = value.cast<uint64_t>(); return;
	case PropvalType::FLOAT:
		tp.value.f = value.cast<float>(); return;
	case PropvalType::DOUBLE:
	case PropvalType::FLOATINGTIME:
		tp.value.d = value.cast<double>(); return;
	case PropvalType::STRING:
	case PropvalType::WSTRING:
	case PropvalType::BINARY:
	case PropvalType::SHORT_ARRAY:
	case PropvalType::LONG_ARRAY:
	case PropvalType::LONGLONG_ARRAY:
	case PropvalType::CURRENCY_ARRAY:
	case PropvalType::FLOAT_ARRAY:
	case PropvalType::DOUBLE_ARRAY:
	case PropvalType::FLOATINGTIME_ARRAY:
	case PropvalType::STRING_ARRAY:
	case PropvalType::WSTRING_ARRAY:
	//case PropvalType::BINARY_ARRAY: //No write support yet
		tp = TaggedPropval_init(tp.tag, value); return; //Do not bother to modify in-place, just make a new one
	}
	throw py::type_error("Tag type not supported for writing");
} catch(const py::cast_error&)
{
	std::string pytype = py::str(value.get_type());
	throw py::type_error("Cannot store value of type "+pytype+" in "+tp.typeName()+" tag.");
}

/**
 * @brief      Wrapper for Restriction constructors
 *
 * Workaround because binding to functions with rvalue references does not
 * work properly.
 */
struct pyRestriction
{
	static Restriction CONTENT(uint32_t fuzzyLevel, uint32_t proptag, TaggedPropval propval)
	{return Restriction::CONTENT(fuzzyLevel, proptag, std::move(propval));}

	static Restriction COUNT(uint32_t count, Restriction restriction)
	{return Restriction::COUNT(count, std::move(restriction));}

	static Restriction NOT(Restriction restriction)
	{return Restriction::NOT(std::move(restriction));}

	static Restriction PROPERTY(Restriction::Op op, uint32_t proptag, TaggedPropval propval)
	{return Restriction::PROPERTY(op, proptag, std::move(propval));}

	static Restriction SUBOBJECT(uint32_t subobject, Restriction restriction)
	{return Restriction::SUBOBJECT(subobject, std::move(restriction));}

	static uint32_t FL_FULLSTRING;

	static std::function<uint32_t(const py::object)> getv(uint32_t v)
	{return [v](const py::object&){return v;};}
};

///////////////////////////////////////////////////////////////////////////////
// Module definition

PYBIND11_MODULE(pyexmdb, m)
{
	m.doc() = "libexmdbpp Python bindings";

	py::class_<Restriction> restriction (m, "Restriction");
	restriction.def_static("NULL", &Restriction::XNULL)
	           .def_static("AND", &Restriction::AND, py::arg("restrictions"))
	           .def_static("BITMASK", &Restriction::BITMASK, py::arg("all"), py::arg("proptag"), py::arg("mask"))
	           .def_static("CONTENT", &pyRestriction::CONTENT, py::arg("fuzzyLevel"), py::arg("proptag"), py::arg("propval"))
	           .def_static("COUNT", &pyRestriction::COUNT, py::arg("count"), py::arg("restriction"))
	           .def_static("EXIST", &Restriction::EXIST, py::arg("proptag"))
	           .def_static("NOT", &pyRestriction::NOT, py::arg("restriction"))
	           .def_static("OR", &Restriction::OR, py::arg("restrictions"))
	           .def_static("PROPCOMP", &Restriction::PROPCOMP, py::arg("op"), py::arg("proptag1"), py::arg("proptag2"))
	           .def_static("PROPERTY", &pyRestriction::PROPERTY, py::arg("op"), py::arg("proptag"), py::arg("propval"))
	           .def_static("SIZE", &Restriction::SIZE, py::arg("op"), py::arg("proptag"), py::arg("size"))
	           .def_static("SUBOBJECT", &pyRestriction::SUBOBJECT, py::arg("subobject"), py::arg("res"))
	           .def_property_readonly_static("FL_FULLSTRING", pyRestriction::getv(Restriction::FL_FULLSTRING))
	           .def_property_readonly_static("FL_SUBSTRING", pyRestriction::getv(Restriction::FL_SUBSTRING))
	           .def_property_readonly_static("FL_PREFIX", pyRestriction::getv(Restriction::FL_PREFIX))
	           .def_property_readonly_static("FL_IGNORECASE", pyRestriction::getv(Restriction::FL_IGNORECASE))
	           .def_property_readonly_static("FL_IGNORE_NONSPACE", pyRestriction::getv(Restriction::FL_IGNORENONSPACE))
	           .def_property_readonly_static("FL_LOOSE", pyRestriction::getv(Restriction::FL_LOOSE));

	py::enum_<Restriction::Op>(restriction, "Op")
	        .value("LT", Restriction::LT)
	        .value("LE", Restriction::LE)
	        .value("GT", Restriction::GT)
	        .value("GE", Restriction::GE)
	        .value("EQ", Restriction::EQ)
	        .value("NE", Restriction::NE);

	py::class_<ExmdbQueries> exmdbQueries(m, "ExmdbQueries", "Main exmdb client interface");

	py::enum_<ExmdbQueries::PermissionMode>(exmdbQueries, "PermissionMode")
	        .value("ADD", ExmdbQueries::PermissionMode::ADD)
	        .value("REMOVE", ExmdbQueries::PermissionMode::REMOVE)
	        .value("SET", ExmdbQueries::PermissionMode::SET)
	        .export_values();

	exmdbQueries.def_readonly_static("defaultFolderProps", &ExmdbQueries::defaultFolderProps)
	    .def_readonly_static("ownerRights", &ExmdbQueries::ownerRights)
	    .def_readonly_static("AUTO_RECONNECT", &ExmdbQueries::AUTO_RECONNECT)
	    .def(py::init<const std::string&, const std::string&, const std::string&, bool, uint8_t>(),
	         py::arg("host"), py::arg("port"), py::arg("homedir"), py::arg("isPrivate"), py::arg("flags")=0)
	    .def("createFolder", &ExmdbQueries::createFolder, release_gil(),
	         py::arg("homedir"), py::arg("domainId"), py::arg("folderName"), py::arg("container"), py::arg("comment"),
	         py::arg("parentId")=0)
	    .def("deleteFolder", &ExmdbQueries::deleteFolder, release_gil(),
	        py::arg("homedir"), py::arg("folderId"), py::arg("clear")=false)
	    .def("findFolder", &ExmdbQueries::findFolder, release_gil(),
	         py::arg("homedir"), py::arg("name"), py::arg("folderId")=0, py::arg("recursive")=true, py::arg("fuzzyLevel")=0,
	         py::arg("proptags") = ExmdbQueries::defaultFolderProps)
	    .def("getAllStoreProperties", &ExmdbQueries::getAllStoreProperties,  release_gil(),
	         py::arg("homedir"))
	    .def("getFolderMemberList", &ExmdbQueries::getFolderMemberList, release_gil(),
	         py::arg("homedir"), py::arg("folderId"))
	    .def("getFolderProperties", &ExmdbQueries::getFolderProperties, release_gil(),
	         py::arg("homedir"), py::arg("cpid"), py::arg("folderId"), py::arg("proptags") = ExmdbQueries::defaultFolderProps)
	    .def("getStoreProperties", &ExmdbQueries::getStoreProperties, release_gil(),
	         py::arg("homedir"), py::arg("cpid"), py::arg("proptags"))
	    .def("getSyncData", &ExmdbQueries::getSyncData, release_gil(),
	         py::arg("homedir"), py::arg("folderName"))
	    .def("listFolders", &ExmdbQueries::listFolders, release_gil(),
	         py::arg("homedir"), py::arg("folderId"), py::arg("recursive")=false,
	         py::arg("proptags") = ExmdbQueries::defaultFolderProps, py::arg("offset")=0, py::arg("limit")=0,
	         py::arg("restriction")=Restriction::XNULL())
	    .def("removeStoreProperties", &ExmdbQueries::removeStoreProperties, release_gil(),
	         py::arg("homedir"), py::arg("proptags"))
	    .def("removeDevice", &ExmdbQueries::removeDevice, release_gil(),
	         py::arg("homedir"), py::arg("folderName"), py::arg("deviceId"))
	    .def("removeSyncStates", &ExmdbQueries::removeSyncStates, release_gil(),
	         py::arg("homedir"), py::arg("folderName"))
	    .def("resolveNamedProperties", &ExmdbQueries::resolveNamedProperties, release_gil(),
	         py::arg("homedir"), py::arg("create"), py::arg("propnames"))
	    .def("resyncDevice", &ExmdbQueries::resyncDevice, release_gil(),
	         py::arg("homedir"), py::arg("folderName"), py::arg("deviceId"), py::arg("userId"))
	    .def("setFolderMember",
	         py::overload_cast<const std::string&, uint64_t, uint64_t, uint32_t, ExmdbQueries::PermissionMode>(&ExmdbQueries::setFolderMember),
	         py::arg("homedir"), py::arg("folderId"), py::arg("ID"), py::arg("rights"), py::arg("mode")=ExmdbQueries::ADD,
	         release_gil())
	    .def("setFolderMember",
	         py::overload_cast<const std::string&, uint64_t, const std::string&, uint32_t, ExmdbQueries::PermissionMode>(&ExmdbQueries::setFolderMember),
	         py::arg("homedir"), py::arg("folderId"), py::arg("username"), py::arg("rights"), py::arg("mode")=ExmdbQueries::ADD,
	         release_gil())
	    .def("setFolderMembers", &ExmdbQueries::setFolderMembers, release_gil(),
	         py::arg("homedir"), py::arg("folderId"), py::arg("usernames"), py::arg("rights"))
	    .def("setFolderProperties", &ExmdbQueries::setFolderProperties, release_gil(),
	         py::arg("homedir"), py::arg("cpid"), py::arg("folderId"), py::arg("propvals"))
	    .def("setStoreProperties", &ExmdbQueries::setStoreProperties, release_gil(),
	         py::arg("homedir"), py::arg("cpid"), py::arg("propvals"))
	    .def("unloadStore", &ExmdbQueries::unloadStore, release_gil(),
	         py::arg("homedir"));

	py::class_<Folder>(m, "Folder")
	        .def(py::init())
	        .def(py::init<ExmdbQueries::PropvalList, uint32_t>(), py::arg("propvalList"), py::arg("syncToMobileTag")=0)
	        .def_readwrite("folderId", &Folder::folderId)
	        .def_readwrite("displayName", &Folder::displayName)
	        .def_readwrite("comment", &Folder::comment)
	        .def_readwrite("creationTime", &Folder::creationTime)
	        .def_readwrite("container", &Folder::container)
	        .def_readwrite("parentId", &Folder::parentId)
	        .def_readwrite("syncToMobile", &Folder::syncToMobile)
	        .def("__repr__", &Folder_repr);

	py::class_<FolderList>(m, "FolderList")
	        .def(py::init<const ExmdbQueries::PropvalTable&, uint32_t>(), py::arg("table"), py::arg("syncToMobileTag")=0)
	        .def_readonly("folders", &FolderList::folders)
	        .def("__repr__", &FolderList_repr);

	py::class_<FolderMemberList>(m, "FolderMemberList")
	        .def(py::init<const ExmdbQueries::PropvalTable&>())
	        .def_readonly("members", &FolderMemberList::members);

	py::class_<FolderMemberList::Member>(m, "FolderMember")
	        .def_readonly("id", &FolderMemberList::Member::id)
	        .def_readonly("mail", &FolderMemberList::Member::mail)
	        .def_readonly("name", &FolderMemberList::Member::name)
	        .def_readonly("rights", &FolderMemberList::Member::rights)
	        .def_property_readonly("special", &FolderMemberList::Member::special);

	py::class_<GUID>(m, "GUID")
	        .def_readonly_static("PSETID_GROMOX", &GUID::PSETID_GROMOX);

	py::class_<PropertyName>(m, "PropertyName")
	        .def(py::init<const GUID&, uint32_t>())
	        .def(py::init<const GUID&, const std::string&>());

	py::class_<PropertyProblem>(m, "PropertyProblem")
	        .def_readwrite("err", &PropertyProblem::err)
	        .def_readwrite("index", &PropertyProblem::index)
	        .def_readwrite("proptag", &PropertyProblem::proptag);

	py::class_<ProptagResponse>(m, "ProptagResponse")
	    .def_readonly("proptags", &ProptagResponse::proptags);

	py::class_<TableResponse>(m, "TableResponse", "Response to a query table request")
	    .def_readonly("entries", &TableResponse::entries);

	py::class_<TaggedPropval>(m, "TaggedPropval")
	        .def(py::init(&TaggedPropval_init),
	             py::arg("tag"), py::arg("value"))
	        .def("toString", &TaggedPropval::toString)
	        .def_readonly("tag", &TaggedPropval::tag)
	        .def_readonly("type", &TaggedPropval::type)
	        .def_property("val", TaggedPropval_getValue, TaggedPropval_setValue)
	        .def("__repr__", &TaggedPropval_repr);

	auto exmdbError = py::register_exception<exmdbpp::ExmdbError>(m, "ExmdbError", PyExc_RuntimeError);
	py::register_exception<exmdbpp::ConnectionError>(m, "ConnectionError", exmdbError.ptr());
	py::register_exception<exmdbpp::ExmdbProtocolError>(m, "ExmdbProtocolError", exmdbError.ptr());
	py::register_exception<exmdbpp::SerializationError>(m, "SerializationError", exmdbError.ptr());
}
