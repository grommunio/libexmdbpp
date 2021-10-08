#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "queries.h"

namespace py = pybind11;

using namespace exmdbpp::requests;
using namespace exmdbpp::queries;
using namespace exmdbpp::structures;


template<typename T, size_t N>
constexpr inline size_t arrsize(T(&)[N])
{return N;}

std::string hexstr(uint32_t value, uint8_t width)
{
	static const char* chars = "0123456789ABCDEF";
	std::string res("0x");
	res.resize(width+2);
	for(int64_t nibble = 4*(width-1); nibble >= 0; nibble -= 4)
		res += chars[(value&(0xF<<nibble))>>nibble];
	return res;
}


std::string TaggedPropval_repr(const TaggedPropval& tp)
{return "TaggedPropval("+hexstr(tp.tag, 8)+", "+tp.toString()+")";}

std::string Folder_repr(const Folder& f)
{return "<Folder '"+f.displayName+"'>";}

std::string FolderList_repr(const FolderList& fl)
{return "<List of "+std::to_string(fl.folders.size())+" folder"+(fl.folders.size() == 1? "" : "s")+">";}


PYBIND11_MODULE(pyexmdb, m)
{
	m.doc() = "libexmdbpp Python bindings";

	py::class_<ExmdbQueries>(m, "ExmdbQueries", "Main exmdb client interface")
	    .def_readonly_static("defaultFolderProps", &ExmdbQueries::defaultFolderProps)
	    .def_readonly_static("ownerRights", &ExmdbQueries::ownerRights)
	    .def(py::init<const std::string&, const std::string&, const std::string&, bool>(),
	         py::arg("host"), py::arg("port"), py::arg("homedir"), py::arg("isPrivate"))
	    .def("createFolder", &ExmdbQueries::createFolder,
	         py::arg("homedir"), py::arg("domainId"), py::arg("folderName"), py::arg("container"), py::arg("comment"))
	    .def("deleteFolder", &ExmdbQueries::deleteFolder,
	        py::arg("homedir"), py::arg("folderId"))
	    .def("deleteFolderMember", &ExmdbQueries::deleteFolderMember,
	         py::arg("homedir"), py::arg("folderId"), py::arg("memberId"))
	    .def("getAllStoreProperties", &ExmdbQueries::getAllStoreProperties,
	         py::arg("homedir"))
	    .def("getFolderList", &ExmdbQueries::getFolderList,
	         py::arg("homedir"), py::arg("properties") = ExmdbQueries::defaultFolderProps)
	    .def("getFolderMemberList", &ExmdbQueries::getFolderMemberList,
	         py::arg("homedir"), py::arg("folderId"))
	    .def("getFolderMemberList", &ExmdbQueries::getFolderMemberList,
	         py::arg("homedir"), py::arg("folderId"))
	    .def("getFolderProperties", &ExmdbQueries::getFolderProperties,
	         py::arg("homedir"), py::arg("cpid"), py::arg("folderId"), py::arg("proptags") = ExmdbQueries::defaultFolderProps)
	    .def("getStoreProperties", &ExmdbQueries::getStoreProperties,
	         py::arg("homedir"), py::arg("cpid"), py::arg("proptags"))
	    .def("getSyncData", &ExmdbQueries::getSyncData,
	         py::arg("homedir"), py::arg("folderName"))
	    .def("removeStoreProperties", &ExmdbQueries::removeStoreProperties,
	         py::arg("homedir"), py::arg("proptags"))
	    .def("resyncDevice", &ExmdbQueries::resyncDevice,
	         py::arg("homedir"), py::arg("folderName"), py::arg("deviceId"))
	    .def("setFolderMember", &ExmdbQueries::setFolderMember,
	         py::arg("homedir"), py::arg("folderId"), py::arg("username"), py::arg("rights"), py::arg("ID")=0)
	    .def("setFolderProperties", &ExmdbQueries::setFolderProperties,
	         py::arg("homedir"), py::arg("cpid"), py::arg("folderId"), py::arg("propvals"))
	    .def("setStoreProperties", &ExmdbQueries::setStoreProperties,
	         py::arg("homedir"), py::arg("cpid"), py::arg("propvals"))
	    .def("unloadStore", &ExmdbQueries::unloadStore,
	         py::arg("homedir"));

	py::class_<Folder>(m, "Folder")
	        .def(py::init())
	        .def(py::init<ExmdbQueries::PropvalList>(), py::arg("propvalList"))
	        .def_readwrite("folderId", &Folder::folderId)
	        .def_readwrite("displayName", &Folder::displayName)
	        .def_readwrite("comment", &Folder::comment)
	        .def_readwrite("creationTime", &Folder::creationTime)
	        .def_readwrite("container", &Folder::container)
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
	        .def_readonly("name", &FolderMemberList::Member::name)
	        .def_readonly("rights", &FolderMemberList::Member::rights);

	py::class_<PropertyProblem>(m, "PropertyProblem")
	        .def_readwrite("err", &PropertyProblem::err)
	        .def_readwrite("index", &PropertyProblem::index)
	        .def_readwrite("proptag", &PropertyProblem::proptag);

	py::class_<ProptagResponse>(m, "ProptagResponse")
	    .def_readonly("proptags", &ProptagResponse::proptags);

	py::class_<TableResponse>(m, "TableResponse", "Response to a query table request")
	    .def_readonly("entries", &TableResponse::entries);

	py::class_<TaggedPropval>(m, "TaggedPropval")
	        .def(py::init<uint32_t, uint64_t>(),
	             py::arg("tag"), py::arg("value"))
	        .def(py::init<uint32_t, const std::string&>(),
	             py::arg("tag"), py::arg("value"))
	        .def("toString", &TaggedPropval::toString)
	        .def_readwrite("tag", &TaggedPropval::tag)
	        .def_readwrite("type", &TaggedPropval::type)
	        .def_readwrite("value", &TaggedPropval::value)
	        .def("__repr__", &TaggedPropval_repr);

	py::class_<TaggedPropval::Value>(m, "_TaggedPropval_Value")
	        .def_readwrite("u8", &TaggedPropval::Value::u8)
	        .def_readwrite("u16", &TaggedPropval::Value::u16)
	        .def_readwrite("u32", &TaggedPropval::Value::u32)
	        .def_readwrite("u64", &TaggedPropval::Value::f)
	        .def_readwrite("d", &TaggedPropval::Value::d);

	py::register_exception<exmdbpp::ExmdbError>(m, "ExmdbError", PyExc_RuntimeError);
}
