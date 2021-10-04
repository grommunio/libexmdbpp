#pragma once

#include <unordered_map>
#include <vector>

#include "requests.h"
#include "ExmdbClient.h"

namespace exmdbpp
{


/**
 * @brief   Higher level implementation of multi-request queries
 */
namespace queries
{

/**
 * @brief   Convenience struct for public folder
 */
struct Folder
{
	Folder() = default;
	Folder(const std::vector<structures::TaggedPropval>&);
	Folder(const requests::PropvalResponse&);

	uint64_t folderId = 0;
	std::string displayName;
	std::string comment;
	uint64_t creationTime = 0;
	std::string container;
private:
	void init(const std::vector<structures::TaggedPropval>&);
};

/**
 * @brief      Response interpreter for ExmdbQueries::getFolderList
 *
 * Utility class providing a more structured access to data returned by
 * ExmdbQueries::getFolderList.
 */
struct FolderList
{
	FolderList(const requests::Response_t<requests::QueryTableRequest>&);
	FolderList(const std::vector<std::vector<structures::TaggedPropval>>&);

	std::vector<Folder> folders;
};


/**
 * @brief      Response interpreter for ExmdbQueries::getFolderOwnerList
 *
 * Utility class providing a more structured access to data returned by
 * ExmdbQueries::getPublicFolderOwnerList.
 */
struct FolderMemberList
{
	struct Member
	{
		uint64_t id = 0;
		std::string name;
		uint32_t rights = 0;
	};

	FolderMemberList(const requests::Response_t<requests::QueryTableRequest>&);
	FolderMemberList(const std::vector<std::vector<structures::TaggedPropval>>&);

	std::vector<Member> members;
};

/**
 * @brief      ExmdbClient extension providing useful queries
 *
 * ExmdbQueries can be used as a substitute for ExmdbClient and provides
 * implementations of frequently used queries (i.e. requests with fixed
 * default values or queries consisting of multiple requests).
 */
class ExmdbQueries final: public ExmdbClient
{
public:
	using ProblemList = std::vector<structures::PropertyProblem>; ///< List of problems
	using PropvalList = std::vector<structures::TaggedPropval>; ///< List of tagged propvals
	using PropvalTable = std::vector<PropvalList>; ///< Table of tagged propvals
	using SyncData = std::unordered_map<std::string, std::string>;
	using ProptagList = std::vector<uint32_t>; ///< List of tags

	using ExmdbClient::ExmdbClient;

	static const std::vector<uint32_t> defaultFolderProps; ///< Default properties when querying folders
	static const uint32_t ownerRights; ///< Default rights for folder owners

	uint64_t createFolder(const std::string&, uint32_t, const std::string&, const std::string&, const std::string&);
	bool deleteFolder(const std::string&, uint64_t);
	void deleteFolderMember(const std::string&, uint64_t, uint64_t);
	ProptagList getAllStoreProperties(const std::string&);
	PropvalTable getFolderList(const std::string&, const std::vector<uint32_t>& = defaultFolderProps);
	PropvalTable getFolderMemberList(const std::string&, uint64_t);
	PropvalList getFolderProperties(const std::string&, uint32_t, uint64_t, const std::vector<uint32_t>& = defaultFolderProps);
	void resyncDevice(const std::string&, const std::string&, const std::string&);
	SyncData getSyncData(const std::string&, const std::string&);
	PropvalList getStoreProperties(const std::string&, uint32_t, const std::vector<uint32_t>&);
	void removeStoreProperties(const std::string&, const std::vector<uint32_t>&);
	void setFolderMember(const std::string&, uint64_t, const std::string&, uint32_t, bool);
	ProblemList setFolderProperties(const std::string&, uint32_t, uint64_t, const std::vector<structures::TaggedPropval>&);
	ProblemList setStoreProperties(const std::string&, uint32_t, const std::vector<structures::TaggedPropval>&);
	void unloadStore(const std::string&);
};

}

}
