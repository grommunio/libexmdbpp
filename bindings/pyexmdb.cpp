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
	case PropvalType::BINARY:
		std::string content = value.cast<std::string>();
		return TaggedPropval(tag, static_cast<const void*>(content.c_str()), uint32_t(content.size()));
	}
	throw py::value_error("Usupported tag type");
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
		tp = TaggedPropval(tp.tag, value.cast<std::string>()); return;
	case PropvalType::BINARY:
		std::string content = value.cast<std::string>();
		tp = TaggedPropval(tp.tag, static_cast<const void*>(content.c_str()), uint32_t(content.size()));
	}
	throw py::type_error("Tag type not supported");
} catch(const py::cast_error&)
{
	std::string pytype = py::str(value.get_type());
	throw py::type_error("Cannot store value of type "+pytype+" in "+tp.typeName()+" tag.");
}

///////////////////////////////////////////////////////////////////////////////
// Module definition

PYBIND11_MODULE(pyexmdb, m)
{
	m.doc() = "libexmdbpp Python bindings";

	py::class_<ExmdbQueries>(m, "ExmdbQueries", "Main exmdb client interface")
	    .def_readonly_static("defaultFolderProps", &ExmdbQueries::defaultFolderProps)
	    .def_readonly_static("ownerRights", &ExmdbQueries::ownerRights)
	    .def_readonly_static("AUTO_RECONNECT", &ExmdbQueries::AUTO_RECONNECT)
	    .def(py::init<const std::string&, const std::string&, const std::string&, bool, uint8_t>(),
	         py::arg("host"), py::arg("port"), py::arg("homedir"), py::arg("isPrivate"), py::arg("flags")=0)
	    .def("createFolder", &ExmdbQueries::createFolder, release_gil(),
	         py::arg("homedir"), py::arg("domainId"), py::arg("folderName"), py::arg("container"), py::arg("comment"))
	    .def("deleteFolder", &ExmdbQueries::deleteFolder, release_gil(),
	        py::arg("homedir"), py::arg("folderId"), py::arg("clear")=false)
	    .def("findFolder", &ExmdbQueries::findFolder, release_gil(),
	         py::arg("homedir"), py::arg("name"), py::arg("folderId")=0, py::arg("recursive")=true, py::arg("fuzzyLevel")=0,
	         py::arg("proptags") = ExmdbQueries::defaultFolderProps)
	    .def("getAllStoreProperties", &ExmdbQueries::getAllStoreProperties,  release_gil(),
	         py::arg("homedir"))
	    .def("getFolderList", &ExmdbQueries::getFolderList, release_gil(),
	         py::arg("homedir"), py::arg("properties") = ExmdbQueries::defaultFolderProps)
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
	         py::arg("proptags") = ExmdbQueries::defaultFolderProps)
	    .def("removeStoreProperties", &ExmdbQueries::removeStoreProperties, release_gil(),
	         py::arg("homedir"), py::arg("proptags"))
	    .def("removeDevice", &ExmdbQueries::removeDevice, release_gil(),
	         py::arg("homedir"), py::arg("folderName"), py::arg("deviceId"))
	    .def("resyncDevice", &ExmdbQueries::resyncDevice, release_gil(),
	         py::arg("homedir"), py::arg("folderName"), py::arg("deviceId"), py::arg("userId"))
	    .def("setFolderMember",
	         py::overload_cast<const std::string&, uint64_t, uint64_t, uint32_t, bool>(&ExmdbQueries::setFolderMember),
	         py::arg("homedir"), py::arg("folderId"), py::arg("ID"), py::arg("rights"), py::arg("remove")=false,
	         release_gil())
	    .def("setFolderMember",
	         py::overload_cast<const std::string&, uint64_t, const std::string&, uint32_t, bool>(&ExmdbQueries::setFolderMember),
	         py::arg("homedir"), py::arg("folderId"), py::arg("username"), py::arg("rights"), py::arg("remove")=false,
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
	        .def(py::init<ExmdbQueries::PropvalList>(), py::arg("propvalList"))
	        .def_readwrite("folderId", &Folder::folderId)
	        .def_readwrite("displayName", &Folder::displayName)
	        .def_readwrite("comment", &Folder::comment)
	        .def_readwrite("creationTime", &Folder::creationTime)
	        .def_readwrite("container", &Folder::container)
	        .def_readwrite("parentId", &Folder::parentId)
	        .def("__repr__", &Folder_repr);

	py::class_<FolderList>(m, "FolderList")
	        .def(py::init<const ExmdbQueries::PropvalTable&>())
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
