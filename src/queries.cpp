/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: 2020 grommunio GmbH
 */

#include <algorithm>
#include <unordered_set>

#include "queries.h"
#include "util.h"
#include "constants.h"
#include "IOBufferImpl.h"

using namespace exmdbpp::constants;
using namespace exmdbpp::requests;
using namespace exmdbpp::structures;


namespace exmdbpp::queries
{

const std::vector<uint32_t> ExmdbQueries::defaultFolderProps =
{PropTag::FOLDERID, PropTag::DISPLAYNAME, PropTag::COMMENT, PropTag::CREATIONTIME, PropTag::CONTAINERCLASS,
 PropTag::PARENTFOLDERID};

const uint32_t ExmdbQueries::ownerRights = 0x000007fb;

/**
 * @brief      Load propvals into predefined fields
 *
 * @param      propvals     List of TaggedPropvals
 */
Folder::Folder(const std::vector<TaggedPropval>& propvals)
{init(propvals);}


/**
 * @brief      Folder response into predefined fields
 *
 * @param      response     Response to convert
 */
Folder::Folder(const PropvalResponse& response)
{init(response.propvals);}

/**
 * @brief     Initialize from tagged propval array
 *
 * @param      propvals     List of TaggedPropvals
 */
void Folder::init(const std::vector<structures::TaggedPropval>& propvals)
{
	for(const TaggedPropval& tp : propvals)
	{
		switch(tp.tag)
		{
		case PropTag::FOLDERID:
			folderId = tp.value.u64; break;
		case PropTag::DISPLAYNAME:
			displayName = tp.value.str; break;
		case PropTag::COMMENT:
			comment = tp.value.str; break;
		case PropTag::CREATIONTIME:
			creationTime = tp.value.u64; break;
		case PropTag::CONTAINERCLASS:
			container = tp.value.str; break;
		case PropTag::PARENTFOLDERID:
			parentId = tp.value.u64; break;
		}
	}
}

/**
 * @brief      Interpret query table response as folder list
 *
 * @param      response  Response to convert
 */
FolderList::FolderList(const Response_t<QueryTableRequest>& response) : FolderList(response.entries)
{}

/**
 * @brief      Interpret propval table as folder list
 *
 * @param      table     Table of propvals to interpret
 */
FolderList::FolderList(const std::vector<std::vector<structures::TaggedPropval>>& table)
{
	folders.reserve(table.size());
	for(auto& entry : table)
		 folders.emplace_back(entry);
}

/**
 * @brief      Interpret query table response as folder member list
 *
 * @param      table     Table to convert
 */
FolderMemberList::FolderMemberList(const std::vector<std::vector<structures::TaggedPropval>>& table)
{
	members.reserve(table.size());
	for(auto& entry : table)
	{
		Member member;
		for(const TaggedPropval& tp : entry)
		{
			switch(tp.tag)
			{
			case PropTag::MEMBERID:
				member.id = tp.value.u64; break;
			case PropTag::MEMBERNAME:
				member.name = tp.value.str; break;
			case PropTag::MEMBERRIGHTS:
				member.rights= tp.value.u32; break;
			case PropTag::SMTPADDRESS:
				member.mail = tp.value.str; break;
			}
		}
		members.emplace_back(std::move(member));
	}
}

/**
 * @brief      Interpret query table response as folder member list
 *
 * @param      response  Response to convert
 */
FolderMemberList::FolderMemberList(const Response_t<QueryTableRequest>& response) : FolderMemberList(response.entries)
{}

/**
 * @brief      Check whether the member is a special member
 *
 * Special members do not represent actual users but are used as internal placeholders.
 *
 * @return     true if the member is special, false otherwise
 */
bool FolderMemberList::Member::special() const
{return id == 0 || id == 0xFFFFFFFFFFFFFFFF;}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief      Find folder with given name
 *
 * @param      homedir      Home directory path of the store
 * @param      name         Name to search for
 * @param      parent       Parent folder ID
 * @param      recursive    Whether to search sub-folders
 * @param      fuzzyLevel   FuzzyLevel to use for matching
 * @param      proptags     Tags to return
 *
 * @return     Search results (table of tagged propvals, can be converted to FolderList for easier access)
 */
ExmdbQueries::PropvalTable ExmdbQueries::findFolder(const std::string& homedir, const std::string& name, uint64_t parent,
                                                    bool recursive, uint32_t fuzzyLevel, const std::vector<uint32_t>& proptags)
{
	parent = parent? parent: util::makeEidEx(1, PrivateFid::ROOT);
	Restriction res = Restriction::CONTENT(fuzzyLevel, 0, TaggedPropval(PropTag::DISPLAYNAME, name));
	auto lhtResponse = send<LoadHierarchyTableRequest>(homedir, parent, "", recursive? TableFlags::DEPTH : 0, res);
	auto qtResponse = send<QueryTableRequest>(homedir, "", 0, lhtResponse.tableId, proptags, 0, lhtResponse.rowCount);
	send<UnloadTableRequest>(homedir, lhtResponse.tableId);
	return std::move(qtResponse.entries);
}

/**
 * @brief      Retrieve public folder list
 *
 * Calls listFolders on the IPMSUBTREE folder.
 *
 * @deprecated This function will be removed. Use listFolders() instead.
 *
 * @param      homedir  Home directory path of the domain
 * @param      proptags Tags to return
 * @param      offset       Number of results to skip
 * @param      limit        Limit number of results
 *
 * @return     Table of tagged propvals. Can be converted to FolderList for easier access.
 */
ExmdbQueries::PropvalTable ExmdbQueries::getFolderList(const std::string& homedir, const std::vector<uint32_t>& proptags,
                                                       uint32_t offset, uint32_t limit)
{return listFolders(homedir, util::makeEidEx(1, PublicFid::IPMSUBTREE), false, proptags, offset, limit);}

/**
 * @brief      List sub-folders of a given folder
 *
 * If both limit and offset are 0 (default), all results are returned.
 *
 * @param      homedir      Home directory path of the domain
 * @param      parent       ID of the parent folder
 * @param      recursive    Recursively list sub-folders
 * @param      proptags     Tags to return
 * @param      offset       Number of results to skip
 * @param      limit        Limit number of results
 *
 * @return     Table of tagged propvals. Can be converted to FolderList for easier access.
 */
ExmdbQueries::PropvalTable ExmdbQueries::listFolders(const std::string& homedir, uint64_t parent, bool recursive,
                                                     const std::vector<uint32_t>& proptags, uint32_t offset, uint32_t limit,
                                                     const structures::Restriction& restriction)
{
	auto lhtResponse = send<LoadHierarchyTableRequest>(homedir, parent, "", recursive? TableFlags::DEPTH : 0, restriction);
	limit = offset || limit || lhtResponse.rowCount < limit? limit : lhtResponse.rowCount;
	auto qtResponse = send<QueryTableRequest>(homedir, "", 0, lhtResponse.tableId, proptags, offset, limit);
	send<UnloadTableRequest>(homedir, lhtResponse.tableId);
	return std::move(qtResponse.entries);
}

/**
 * @brief      Create a public folder
 *
 * @param      homedir     Home directory path of the domain
 * @param      domainId    Domain ID
 * @param      folderName  Name of the new folder
 * @param      container   Folder container class
 * @param      comment     Comment to attach
 *
 * @return     ID of the folder or 0 on error
 */
uint64_t ExmdbQueries::createFolder(const std::string& homedir, uint32_t domainId,
                                    const std::string& folderName, const std::string& container, const std::string& comment)
{
	auto acResponse = send<AllocateCnRequest>(homedir);
	std::vector<TaggedPropval> propvals;
	uint64_t now = util::ntTime();
	SizedXID xid(22, GUID::fromDomainId(domainId), util::valueToGc(be64toh(acResponse.changeNum)));
	IOBuffer tmpbuff;
	propvals.reserve(10);
	tmpbuff.reserve(128);
	propvals.emplace_back(PropTag::PARENTFOLDERID, util::makeEidEx(1, PublicFid::IPMSUBTREE));
	propvals.emplace_back(PropTag::FOLDERTYPE, FolderType::GENERIC);
	propvals.emplace_back(PropTag::DISPLAYNAME, folderName, false);
	propvals.emplace_back(PropTag::COMMENT, comment, false);
	propvals.emplace_back(PropTag::CREATIONTIME, now);
	propvals.emplace_back(PropTag::LASTMODIFICATIONTIME, now);
	propvals.emplace_back(PropTag::CHANGENUMBER, acResponse.changeNum);

	xid.writeXID(tmpbuff);
	propvals.emplace_back(PropTag::CHANGEKEY, tmpbuff.data(), uint32_t(tmpbuff.size()));

	size_t offset = tmpbuff.size();
	tmpbuff.push(xid);
	propvals.emplace_back(PropTag::PREDECESSORCHANGELIST, tmpbuff.data()+offset, uint32_t(tmpbuff.size()-offset));
	if(!container.empty())
		propvals.emplace_back(PropTag::CONTAINERCLASS, container);
	return send<CreateFolderByPropertiesRequest>(homedir, 0, propvals).folderId;
}

/**
 * @brief      Delete folder
 *
 * Folders can only deleted when empty. Clearing the folder contents can be
 * done implicitely by setting `empty` to true.
 *
 * @param      homedir   Home directory path of the domain
 * @param      folderId  Id of the folder to delete
 * @param      clear     Clear folder contents before deletion
 *
 * @return     true if successful, false otherwise
 */
bool ExmdbQueries::deleteFolder(const std::string& homedir, uint64_t folderId, bool clear)
{
	if(clear)
		send<EmptyFolderRequest>(homedir, 0, "", folderId, true, true, true, true);
	return send<DeleteFolderRequest>(homedir, 0, folderId, true).success;
}

/**
 * @brief      Get list of folder members
 *
 * @param      homedir   Home directory path of the domain
 * @param      folderId  ID of the folder
 *
 * @return     Table containing the member information. Can be converted to FolderMemberList for easier access.
 */
ExmdbQueries::PropvalTable ExmdbQueries::getFolderMemberList(const std::string& homedir, uint64_t folderId)
{
	auto lptResponse = send<LoadPermissionTableRequest>(homedir, folderId, 0);
	uint32_t proptags[] = {PropTag::MEMBERID, PropTag::SMTPADDRESS, PropTag::MEMBERNAME, PropTag::MEMBERRIGHTS};
	auto qtResponse = send<QueryTableRequest>(homedir, "", 0, lptResponse.tableId, proptags, 0, lptResponse.rowCount);
	send<UnloadTableRequest>(homedir, lptResponse.tableId);
	return qtResponse.entries;
}


uint32_t ExmdbQueries::setFolderMember(const std::string& homedir, uint64_t folderId, const FolderMemberList::Member& existing,
                                       uint32_t rights, bool remove)
{
	uint32_t modified = remove? existing.rights & ~rights : existing.rights | rights;
	if(modified == existing.rights)
		return modified;
	PermissionData permissions[1];
	if(modified == 0)
		permissions[0] = PermissionData(PermissionData::REMOVE_ROW, {TaggedPropval(PropTag::MEMBERID, existing.id)});
	else if(!existing.id)
		permissions[0] = PermissionData(PermissionData::ADD_ROW, {TaggedPropval(PropTag::SMTPADDRESS, existing.mail, false),
		                            TaggedPropval(PropTag::MEMBERRIGHTS, modified)});
	else
		permissions[0] = PermissionData(PermissionData::MODIFY_ROW, {TaggedPropval(PropTag::SMTPADDRESS, existing.mail, false),
		                            TaggedPropval(PropTag::MEMBERRIGHTS, modified),
		                            TaggedPropval(PropTag::MEMBERID, existing.id)});
	send<UpdateFolderPermissionRequest>(homedir, folderId, false, permissions);
	return modified;
}

/**
 * @brief      Add user to folder member list
 *
 * @param      homedir   Home directory path of the domain
 * @param      folderId  ID of the folder
 * @param      username  Username to add to list
 * @param      rights    Bitmask of member rights
 * @param      remove    Remove rights instead of adding them
 *
 * @return     New rights value
 */
uint32_t ExmdbQueries::setFolderMember(const std::string& homedir, uint64_t folderId, const std::string& username,
                                       uint32_t rights, bool remove)
{
	FolderMemberList members = getFolderMemberList(homedir, folderId);
	auto it = std::find_if(members.members.begin(), members.members.end(),
	                       [&username](const FolderMemberList::Member& m){return m.mail == username;});
	FolderMemberList::Member existing = it == members.members.end()? FolderMemberList::Member() : *it;
	existing.mail = username;
	return setFolderMember(homedir, folderId, existing, rights, remove);
}

/**
 * @brief      Set folder member rights for existing user
 *
 * @param      homedir   Home directory path of the domain
 * @param      folderId  ID of the folder
 * @param      ID        ID of the member
 * @param      rights    Bitmask of member rights
 * @param      remove    Remove rights instead of adding them
 *
 * @return     New rights value
 */
uint32_t ExmdbQueries::setFolderMember(const std::string& homedir, uint64_t folderId, uint64_t ID,
                                       uint32_t rights, bool remove)
{
	FolderMemberList members = getFolderMemberList(homedir, folderId);
	auto it = std::find_if(members.members.begin(), members.members.end(),
	                       [ID](const FolderMemberList::Member& m){return m.id == ID;});
	if(it == members.members.end())
		return 0;
	return setFolderMember(homedir, folderId, *it, rights, remove);
}

/**
 * @brief      Set folder member rights for a list of users
 *
 * Rights are added for each member in the list and removed for each member not
 * in the list.
 *
 * @param      homedir   Home directory path of the domain
 * @param      folderId  ID of the folder
 * @param      usernames List of mail addresses to modify
 * @param      rights    Bitmask of member rights
 *
 * @return     Number of members modified
 */
size_t ExmdbQueries::setFolderMembers(const std::string& homedir, uint64_t folderId,
                                        const std::vector<std::string>& usernames, uint32_t rights)
{
	std::unordered_set<std::string> requested(usernames.begin(), usernames.end());
	FolderMemberList members = getFolderMemberList(homedir, folderId);
	std::vector<PermissionData> permissions;
	permissions.reserve(members.members.size()+usernames.size()); //Usually too much, but definitely enough.
	for(const auto& member : members.members)
	{
		if(member.special())
			continue;
		bool contained = requested.erase(member.mail);
		uint32_t newrights = contained? member.rights | rights : member.rights & ~rights;
		if(newrights == member.rights)
			continue;
		permissions.emplace_back(PermissionData(newrights? PermissionData::MODIFY_ROW : PermissionData::REMOVE_ROW,
		                                        {TaggedPropval(PropTag::MEMBERID, member.id),
		                                         TaggedPropval(PropTag::MEMBERRIGHTS, newrights)}));
	}
	for(auto& username : requested)
		permissions.emplace_back(PermissionData(PermissionData(PermissionData::ADD_ROW,
		                                                       {TaggedPropval(PropTag::SMTPADDRESS, username),
		                                                        TaggedPropval(PropTag::MEMBERRIGHTS, rights)})));
	if(permissions.size())
		send<UpdateFolderPermissionRequest>(homedir, folderId, false, permissions);
	return permissions.size();
}

/**
 * @brief      Modify store properties
 *
 * @param      homedir   Home directory path of the domain
 * @param      cpid      Unknown purpose
 * @param      propvals  PropertyValues to modify
 *
 * @return     List of problems encountered
 */
ExmdbQueries::ProblemList ExmdbQueries::setStoreProperties(const std::string& homedir, uint32_t cpid,
                                                           const std::vector<TaggedPropval>& propvals)
{return send<SetStorePropertiesRequest>(homedir, cpid, propvals).problems;}

/**
 * @brief      Unload the users store
 *
 * @param      homedir   Home directory path of the user
 */
void ExmdbQueries::unloadStore(const std::string& homedir)
{send<UnloadStoreRequest>(homedir);}


/**
 * @brief      Modify folder properties
 *
 * @param      homedir   Home directory path of the domain
 * @param      cpid      Unknown purpose
 * @param      folderId  ID of the folder to modify
 * @param      propvals  PropertyValues to modify
 *
 * @return     List of problems encountered
 */
ExmdbQueries::ProblemList ExmdbQueries::setFolderProperties(const std::string& homedir, uint32_t cpid, uint64_t folderId,
                                                            const std::vector<TaggedPropval>& propvals)
{return send<SetFolderPropertiesRequest>(homedir, cpid, folderId, propvals).problems;}

/**
 * @brief      Get folder properties
 *
 * @param      homedir   Home directory path of the domain
 * @param      cpid      Unknown purpose
 * @param      folderId  ID of the folder to get
 * @param      propvals  PropertyValues to get
 *
 * @return     List of properties
 */
ExmdbQueries::PropvalList ExmdbQueries::getFolderProperties(const std::string& homedir, uint32_t cpid, uint64_t folderId,
                                                            const std::vector<uint32_t>& proptags)
{return send<GetFolderPropertiesRequest>(homedir, cpid, folderId, proptags).propvals;}

/**
 * @brief      Get store properties
 *
 * @param      homedir   Home directory path of the domain
 * @param      cpid      Unknown purpose
 * @param      propvals  PropertyValues to modify
 *
 * @return     List of properties
 */
ExmdbQueries::PropvalList ExmdbQueries::getStoreProperties(const std::string& homedir, uint32_t cpid,
                                                           const std::vector<uint32_t>& proptags)
{return send<GetStorePropertiesRequest>(homedir, cpid, proptags).propvals;}

/**
 * @brief      Get all store proptags
 *
 * @param      homedir   Home directory path of the domain
 *
 * @return     List prop tags
 */
ExmdbQueries::ProptagList ExmdbQueries::getAllStoreProperties(const std::string& homedir)
{return send<GetAllStorePropertiesRequest>(homedir).proptags;}

/**
 * @brief      Remove store properties
 *
 * @param      homedir   Home directory path of the domain
 * @param      proptags  Properties to remove
 */
void ExmdbQueries::removeStoreProperties(const std::string& homedir, const std::vector<uint32_t>& proptags)
{send<RemoveStorePropertiesRequest>(homedir, proptags);}


/**
 * @brief       Get grommunio-sync state for user
 *
 * @param       homedir     Home directory path of the user
 * @param       folderName  Name of the folder containing sync data
 *
 * @return      Map of devices and their state
 */
ExmdbQueries::SyncData ExmdbQueries::getSyncData(const std::string& homedir, const std::string& folderName)
{
	uint64_t parentFolderID = util::makeEidEx(1, PublicFid::ROOT);
	uint32_t fidTags[] = {PropTag::FOLDERID, PropTag::DISPLAYNAME};
	uint32_t bodyTag[] = {PropTag::BODY};
	uint32_t midTag[] = {PropTag::MID};
	Restriction ddFilter =
	        Restriction::AND({Restriction::PROPERTY(Restriction::EQ, 0, TaggedPropval(PropTag::DISPLAYNAME, "devicedata")),
	                          Restriction::PROPERTY(Restriction::EQ, 0, TaggedPropval(PropTag::MESSAGECLASS, "IPM.Note.GrommunioState"))});

	SyncData data;

	auto folder = send<GetFolderByNameRequest>(homedir, parentFolderID, folderName);
	auto subfolders = send<LoadHierarchyTableRequest>(homedir, folder.folderId, "", 0);
	data.reserve(subfolders.rowCount);
	auto subfolderIDs = send<QueryTableRequest>(homedir, "", 0, subfolders.tableId, fidTags, 0, subfolders.rowCount);
	send<UnloadTableRequest>(homedir, subfolders.tableId);
	for(const auto& subfolder: subfolderIDs.entries)
	{
		if(subfolder.size() != 2 || subfolder[0].tag != PropTag::FOLDERID || subfolder[1].tag != PropTag::DISPLAYNAME)
			continue;
		auto content = send<LoadContentTableRequest>(homedir, 0, subfolder[0].value.u64, "", 2, ddFilter);
		auto table = send<QueryTableRequest>(homedir, "", 0, content.tableId, midTag, 0, content.rowCount);
		send<UnloadTableRequest>(homedir, content.tableId);
		if(table.entries.empty())
			continue;
		auto& msgobject = table.entries[0];
		if(msgobject.size() != 1 || msgobject[0].tag != PropTag::MID)
			continue;
		auto message = send<GetMessagePropertiesRequest>(homedir, "", 0, msgobject[0].value.u64, bodyTag);
		if(message.propvals.size() != 1 || message.propvals[0].tag != PropTag::BODY)
			continue;
		data.emplace(subfolder[1].value.str, message.propvals[0].value.str);
	}
	return data;
}

/**
 * @brief       Remove device
 *
 * Deletes the device sync folder.
 * If the device is still active, this results in a re-sync of the device.
 *
 * @param       homedir     Home directory path of the user
 * @param       folderName  Name of the folder containing sync data
 * @param       deviceId    Device ID string
 */
void ExmdbQueries::removeDevice(const std::string& homedir, const std::string& folderName, const std::string& deviceId)
{
	uint64_t rootFolderId = util::makeEidEx(1, PublicFid::ROOT);
	auto syncFolder = send<GetFolderByNameRequest>(homedir, rootFolderId, folderName);
	auto deviceFolder = send<GetFolderByNameRequest>(homedir, syncFolder.folderId, deviceId);
	send<EmptyFolderRequest>(homedir, 0, "", deviceFolder.folderId, true, false, true, false);
	send<DeleteFolderRequest>(homedir, 0, deviceFolder.folderId, true);
}

/**
 * @brief        Resync device
 *
 * Deletes all sync states but keeps the device data.
 *
 * @param       homedir     Home directory path of the user
 * @param       folderName  Name of the folder containing sync data
 * @param       deviceId    Device ID string
 * @param       userId      (MySQL) ID of the user
 *
 * @return      true if deletion was successful, false if only part of the states was deleted
 */
bool ExmdbQueries::resyncDevice(const std::string& homedir, const std::string& folderName, const std::string& deviceId,
                                uint32_t userId)
{
	static const Restriction noDD = Restriction::PROPERTY(Restriction::NE, 0, TaggedPropval(PropTag::DISPLAYNAME, "devicedata"));
	static const uint32_t midTag[] = {PropTag::MID};
	uint64_t rootFolderId = util::makeEidEx(1, PublicFid::ROOT);
	auto syncFolder = send<GetFolderByNameRequest>(homedir, rootFolderId, folderName);
	auto deviceFolder = send<GetFolderByNameRequest>(homedir, syncFolder.folderId, deviceId);
	auto content = send<LoadContentTableRequest>(homedir, 0, deviceFolder.folderId, "", 2, noDD);
	auto table = send<QueryTableRequest>(homedir, "", 0, content.tableId, midTag, 0, content.rowCount);
	send<UnloadTableRequest>(homedir, content.tableId);
	std::vector<uint64_t> mids;
	mids.reserve(table.entries.size());
	for(const auto& row : table.entries)
		for(const auto& tag : row)
			if(tag.tag == PropTag::MID)
				mids.emplace_back(tag.value.u64);
	return !send<DeleteMessagesRequest>(homedir, userId, 0, "", deviceFolder.folderId, mids, true).partial;
}

}
