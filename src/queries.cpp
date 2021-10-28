/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: 2020 grommunio GmbH
 */

#include <algorithm>

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
{PropTag::FOLDERID, PropTag::DISPLAYNAME, PropTag::COMMENT, PropTag::CREATIONTIME, PropTag::CONTAINERCLASS};

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief      Retrieve public folder list
 *
 * Provides a higher level protocol implementation for retrieving
 * the public folders of a domain.
 *
 * @param      homedir  Home directory path of the domain
 * @param      proptags Tags to return
 *
 * @return     Table of tagged propvals. Can be converted to FolderList for easier access.
 */
ExmdbQueries::PropvalTable ExmdbQueries::getFolderList(const std::string& homedir, const std::vector<uint32_t>& proptags)
{
	uint64_t folderId = util::makeEidEx(1, PublicFid::IPMSUBTREE);
	auto lhtResponse = send<LoadHierarchyTableRequest>(homedir, folderId, "", 0);
	auto qtResponse = send<QueryTableRequest>(homedir, "", 0, lhtResponse.tableId, proptags, 0, lhtResponse.rowCount);
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
	else permissions[0] = PermissionData(PermissionData::MODIFY_ROW, {TaggedPropval(PropTag::SMTPADDRESS, existing.mail, false),
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
 * @brief      Remove member from member list
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
	Restriction ddFilter = Restriction::PROPERTY(Restriction::EQ, 0, TaggedPropval(PropTag::DISPLAYNAME, "devicedata"));

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
 * @brief       Initiate device resync
 *
 * Deletes the device sync folder, causing the device to re-sync.
 *
 * @param       homedir     Home directory path of the user
 * @param       folderName  Name of the folder containing sync data
 * @param       deviceId    Device ID string
 */
void ExmdbQueries::resyncDevice(const std::string& homedir, const std::string& folderName, const std::string& deviceId)
{
	uint64_t rootFolderId = util::makeEidEx(1, PublicFid::ROOT);
	auto syncFolder = send<GetFolderByNameRequest>(homedir, rootFolderId, folderName);
	auto deviceFolder = send<GetFolderByNameRequest>(homedir, syncFolder.folderId, deviceId);
	send<EmptyFolderRequest>(homedir, 0, "", deviceFolder.folderId, true, false, true, false);
}

}
