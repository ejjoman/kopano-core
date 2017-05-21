/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <kopano/platform.h>
#include <kopano/lockhelper.hpp>
#include <kopano/memory.hpp>
#include "m4l.mapispi.h"
#include "m4l.mapiutil.h"
#include "m4l.mapix.h"
#include "m4l.debug.h"

#include <mapi.h>
#include <kopano/CommonUtil.h>
#include <kopano/Util.h>
#include <kopano/ECGuid.h>

#include <algorithm>
#include <kopano/mapi_ptr.h>

using namespace KCHL;

M4LMAPIGetSession::M4LMAPIGetSession(LPMAPISESSION new_session) {
	assert(new_session != NULL);
	session = new_session;
	session->AddRef();
}

M4LMAPIGetSession::~M4LMAPIGetSession() {
	session->Release();
}

HRESULT M4LMAPIGetSession::GetMAPISession(LPUNKNOWN *lppSession)
{
	HRESULT hr;
	hr = session->QueryInterface(IID_IMAPISession, (void**)lppSession);
	return hr;
}

// iunknown passthru
ULONG M4LMAPIGetSession::AddRef() {
    return M4LUnknown::AddRef();
}
ULONG M4LMAPIGetSession::Release() {
    return M4LUnknown::Release();
}
HRESULT M4LMAPIGetSession::QueryInterface(REFIID refiid, void **lpvoid) {
	HRESULT hr = hrSuccess;

	if (refiid == IID_IMAPIGetSession) {
		AddRef();
		*lpvoid = static_cast<IMAPIGetSession *>(this);
	} else if (refiid == IID_IUnknown) {
		AddRef();
		*lpvoid = static_cast<IUnknown *>(this);
    } else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	return hr;
}

M4LMAPISupport::M4LMAPISupport(LPMAPISESSION new_session, LPMAPIUID lpUid,
    SVCService *lpService) :
	session(new_session), service(lpService)
{
	if(lpUid) {
    	this->lpsProviderUID = new MAPIUID;
        memcpy(this->lpsProviderUID, lpUid, sizeof(MAPIUID));
		return;
	}
        this->lpsProviderUID = NULL;
}

M4LMAPISupport::~M4LMAPISupport() {
	delete lpsProviderUID;
	for (const auto &i : m_advises)
		MAPIFreeBuffer(i.second.lpKey);
}

HRESULT M4LMAPISupport::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR * lppMAPIError) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::GetMemAllocRoutines(LPALLOCATEBUFFER * lpAllocateBuffer, LPALLOCATEMORE * lpAllocateMore,
											LPFREEBUFFER * lpFreeBuffer) {
	HRESULT hr = hrSuccess;

	*lpAllocateBuffer = MAPIAllocateBuffer;
	*lpAllocateMore   = MAPIAllocateMore;
	*lpFreeBuffer     = MAPIFreeBuffer;
    return hr;
}

HRESULT M4LMAPISupport::Subscribe(LPNOTIFKEY lpKey, ULONG ulEventMask, ULONG ulFlags, LPMAPIADVISESINK lpAdviseSink,
								  ULONG * lpulConnection) {
	HRESULT hr = hrSuccess;
	LPNOTIFKEY lpNewKey = NULL;
	ulock_normal l_adv(m_advises_mutex, std::defer_lock_t());

	/* Copy key (this should prevent deletion of the key while it is still in the list */
	hr = MAPIAllocateBuffer(CbNewNOTIFKEY(sizeof(GUID)), (void **)&lpNewKey);
	if (hr != hrSuccess)
		goto exit;

	memcpy(lpNewKey, lpKey, sizeof(*lpKey));
	l_adv.lock();
	++m_connections;
	m_advises.insert(M4LSUPPORTADVISES::value_type(m_connections, M4LSUPPORTADVISE(lpNewKey, ulEventMask, ulFlags, lpAdviseSink)));
	*lpulConnection = m_connections;
	l_adv.unlock();
exit:
    return hr;
}

HRESULT M4LMAPISupport::Unsubscribe(ULONG ulConnection) {
	HRESULT hr = hrSuccess;
	M4LSUPPORTADVISES::iterator i;
	scoped_lock l_adv(m_advises_mutex);

	i = m_advises.find(ulConnection);
	if (i != m_advises.cend()) {
		MAPIFreeBuffer(i->second.lpKey);
		m_advises.erase(i);
	} else
		hr = MAPI_E_NOT_FOUND;

    return hr;
}

HRESULT M4LMAPISupport::Notify(LPNOTIFKEY lpKey, ULONG cNotification, LPNOTIFICATION lpNotifications, ULONG * lpulFlags) {
	HRESULT hr = hrSuccess;
	KCHL::object_ptr<IMAPIAdviseSink> lpAdviseSink;
	ulock_normal l_adv(m_advises_mutex);

	auto iter = find_if(m_advises.cbegin(), m_advises.cend(), findKey(lpKey));
	if (iter == m_advises.cend()) {
		l_adv.unlock();
		/* Should this be reported as error? */
		goto exit;
	}
	lpAdviseSink.reset(iter->second.lpAdviseSink);
	l_adv.unlock();
	hr = lpAdviseSink->OnNotify(cNotification, lpNotifications);

exit:
    return hr;
}

HRESULT M4LMAPISupport::ModifyStatusRow(ULONG cValues, LPSPropValue lpColumnVals, ULONG ulFlags) {
	auto hr = static_cast<M4LMAPISession *>(this->session)->setStatusRow(cValues, lpColumnVals);
    return hr;
}

HRESULT M4LMAPISupport::OpenProfileSection(const MAPIUID *lpUid, ULONG ulFlags,
    IProfSect **lppProfileObj)
{
	if (lpUid == NULL)
		lpUid = lpsProviderUID;
        
	HRESULT hr = session->OpenProfileSection(lpUid, NULL, ulFlags, lppProfileObj);
	return hr;
}

HRESULT M4LMAPISupport::RegisterPreprocessor(LPMAPIUID lpMuid, LPTSTR lpszAdrType, LPTSTR lpszDLLName, LPSTR lpszPreprocess,
											 LPSTR lpszRemovePreprocessInfo, ULONG ulFlags) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::NewUID(LPMAPIUID lpMuid) {
    HRESULT hr = CoCreateGuid((GUID*)lpMuid);
	return hr;
}

HRESULT M4LMAPISupport::MakeInvalid(ULONG ulFlags, LPVOID lpObject, ULONG ulRefCount, ULONG cMethods) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::SpoolerYield(ULONG ulFlags) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::SpoolerNotify(ULONG ulFlags, LPVOID lpvData) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags,
									 ULONG * lpcbEntryID, LPENTRYID * lppEntryID) {
	// although it's called EC... the return value is HRESULT :)
	HRESULT hr = ECCreateOneOff(lpszName, lpszAdrType, lpszAddress, ulFlags, lpcbEntryID, lppEntryID);
	return hr;
}

HRESULT M4LMAPISupport::SetProviderUID(LPMAPIUID lpProviderID, ULONG ulFlags) {
    return hrSuccess;
}

HRESULT M4LMAPISupport::CompareEntryIDs(ULONG cbEntry1, LPENTRYID lpEntry1, ULONG cbEntry2, LPENTRYID lpEntry2,
										ULONG ulCompareFlags, ULONG * lpulResult) {
	HRESULT hr = hrSuccess;

	if (cbEntry1 != cbEntry2)
		*lpulResult = FALSE;
	else if (!lpEntry1 || !lpEntry2)
		hr = MAPI_E_INVALID_ENTRYID;
	else if (memcmp(lpEntry1, lpEntry2, cbEntry1) != 0)
		*lpulResult = FALSE;
	else
		*lpulResult = TRUE;
	return hr;
}

HRESULT M4LMAPISupport::OpenTemplateID(ULONG cbTemplateID, LPENTRYID lpTemplateID, ULONG ulTemplateFlags, LPMAPIPROP lpMAPIPropData,
									   LPCIID lpInterface, LPMAPIPROP * lppMAPIPropNew, LPMAPIPROP lpMAPIPropSibling) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulOpenFlags, ULONG * lpulObjType,
								  LPUNKNOWN * lppUnk) {
	HRESULT hr = session->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulOpenFlags, lpulObjType, lppUnk);
	return hr;
}

HRESULT M4LMAPISupport::GetOneOffTable(ULONG ulFlags, LPMAPITABLE * lppTable) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::Address(ULONG * lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST * lppAdrList) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::Details(ULONG * lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID,
								LPENTRYID lpEntryID, LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext, LPTSTR lpszButtonText,
								ULONG ulFlags) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::NewEntry(ULONG ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, LPENTRYID lpEIDContainer, ULONG cbEIDNewEntryTpl,
								 LPENTRYID lpEIDNewEntryTpl, ULONG * lpcbEIDNewEntry, LPENTRYID * lppEIDNewEntry) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::DoConfigPropsheet(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszTitle, LPMAPITABLE lpDisplayTable,
										  LPMAPIPROP lpCOnfigData, ULONG ulTopPage) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::CopyMessages(LPCIID lpSrcInterface, LPVOID lpSrcFolder, LPENTRYLIST lpMsgList, LPCIID lpDestInterface,
									 LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags) {
	HRESULT hr = hrSuccess;
	LPMAPIFOLDER lpSource = NULL;
	LPMAPIFOLDER lpDest = NULL;
	ULONG ulObjType;
	KCHL::memory_ptr<ENTRYLIST> lpDeleteEntries;
	bool bPartial = false;
	ULONG i;

	if (!lpSrcInterface || !lpSrcFolder || !lpDestFolder || !lpMsgList) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (*lpSrcInterface != IID_IMAPIFolder || (lpDestInterface != NULL && *lpDestInterface != IID_IMAPIFolder)) {
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
		goto exit;
	}

	lpSource = (LPMAPIFOLDER)lpSrcFolder;
	lpDest = (LPMAPIFOLDER)lpDestFolder;
	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), &~lpDeleteEntries);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(sizeof(SBinary)*lpMsgList->cValues, lpDeleteEntries, (void**)&lpDeleteEntries->lpbin);
	if (hr != hrSuccess)
		goto exit;

	lpDeleteEntries->cValues = 0;

	for (i = 0; i < lpMsgList->cValues; ++i) {
		object_ptr<IMessage> lpSrcMessage, lpDestMessage;

		hr = lpSource->OpenEntry(lpMsgList->lpbin[i].cb,
		     reinterpret_cast<ENTRYID *>(lpMsgList->lpbin[i].lpb),
		     &IID_IMessage, 0, &ulObjType, &~lpSrcMessage);
		if (hr != hrSuccess) {
			// partial, or error to calling client?
			bPartial = true;
			goto next_item;
		}
		hr = lpDest->CreateMessage(&IID_IMessage, MAPI_MODIFY, &~lpDestMessage);
		if (hr != hrSuccess) {
			bPartial = true;
			goto next_item;
		}

		hr = this->DoCopyTo(&IID_IMessage, lpSrcMessage, 0, NULL, NULL, ulUIParam, lpProgress, &IID_IMessage, lpDestMessage, 0, NULL);
		if (FAILED(hr)) {
			goto exit;
		} else if (hr != hrSuccess) {
			bPartial = true;
			goto next_item;
		}

		hr = lpDestMessage->SaveChanges(0);
		if (hr != hrSuccess) {
			bPartial = true;
		} else if (ulFlags & MAPI_MOVE) {
			lpDeleteEntries->lpbin[lpDeleteEntries->cValues].cb = lpMsgList->lpbin[i].cb;
			lpDeleteEntries->lpbin[lpDeleteEntries->cValues].lpb = lpMsgList->lpbin[i].lpb;
			++lpDeleteEntries->cValues;
		}

next_item:
		;
	}

	if ((ulFlags & MAPI_MOVE) && lpDeleteEntries->cValues > 0) {
		if (lpSource->DeleteMessages(lpDeleteEntries, 0, NULL, 0) != hrSuccess)
			bPartial = true;
	}
	
	if (bPartial)
		hr = MAPI_W_PARTIAL_COMPLETION;

exit:
	return hr;
}

HRESULT M4LMAPISupport::CopyFolder(LPCIID lpSrcInterface, LPVOID lpSrcFolder, ULONG cbEntryID, LPENTRYID lpEntryID,
								   LPCIID lpDestInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam,
								   LPMAPIPROGRESS lpProgress, ULONG ulFlags) {
	HRESULT hr = hrSuccess;
	object_ptr<IMAPIFolder> lpSource, lpDest, lpFolder, lpSubFolder;
	KCHL::memory_ptr<SPropValue> lpSourceName;
	ULONG ulObjType  = 0;
	ULONG ulFolderFlags = 0;
	static constexpr const SizedSPropTagArray (1, sExcludeProps) = {1, {PR_DISPLAY_NAME_A}};

	if (!lpSrcInterface || !lpSrcFolder || cbEntryID == 0 || !lpEntryID || !lpDestFolder) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (*lpSrcInterface != IID_IMAPIFolder) {
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
		goto exit;
	}
	hr = ((LPUNKNOWN)lpSrcFolder)->QueryInterface(IID_IMAPIFolder, &~lpSource);
	if (hr != hrSuccess)
		goto exit;

	// lpDestInterface == NULL or IID_IMAPIFolder compatible
	hr = ((LPUNKNOWN)lpDestFolder)->QueryInterface(IID_IMAPIFolder, &~lpDest);
	if (hr != hrSuccess)
		goto exit;
	hr = lpSource->OpenEntry(cbEntryID, lpEntryID, &IID_IMAPIFolder, 0, &ulObjType, &~lpFolder);
	if (hr != hrSuccess)
		goto exit;

	if (!lpszNewFolderName) {
		hr = HrGetOneProp(lpFolder, PR_DISPLAY_NAME_W, &~lpSourceName);
		if (hr != hrSuccess)
			goto exit;

		ulFolderFlags |= MAPI_UNICODE;
		lpszNewFolderName = (LPTSTR)lpSourceName->Value.lpszW;
	} else
		ulFolderFlags |= (ulFlags & MAPI_UNICODE);

	hr = lpDest->CreateFolder(FOLDER_GENERIC, lpszNewFolderName, nullptr, &IID_IMAPIFolder, ulFolderFlags, &~lpSubFolder);
	if (hr != hrSuccess)
		goto exit;
	hr = this->DoCopyTo(&IID_IMAPIFolder, lpFolder, 0, NULL, sExcludeProps,
	     ulUIParam, lpProgress, &IID_IMAPIFolder, lpSubFolder, ulFlags, NULL);
	if (hr != hrSuccess)
		goto exit;

	if (ulFlags & MAPI_MOVE)
		lpSource->DeleteFolder(cbEntryID, (LPENTRYID)lpEntryID, 0, NULL, DEL_FOLDERS | DEL_MESSAGES);

exit:
	return hr;
}

HRESULT M4LMAPISupport::DoCopyTo(LPCIID lpSrcInterface, LPVOID lpSrcObj,
    ULONG ciidExclude, LPCIID rgiidExclude, const SPropTagArray *lpExcludeProps,
    ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface,
    void *lpDestObj, ULONG ulFlags, SPropProblemArray **lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = Util::DoCopyTo(lpSrcInterface, lpSrcObj, ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpDestInterface, lpDestObj, ulFlags, lppProblems);
	return hr;
}

HRESULT M4LMAPISupport::DoCopyProps(LPCIID lpSrcInterface, void *lpSrcObj,
    const SPropTagArray *lpIncludeProps, ULONG ulUIParam,
    LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, void *lpDestObj,
    ULONG ulFlags, SPropProblemArray **lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = Util::DoCopyProps(lpSrcInterface, lpSrcObj, lpIncludeProps, ulUIParam, lpProgress, lpDestInterface, lpDestObj, ulFlags, lppProblems);
    return hr;
}

HRESULT M4LMAPISupport::DoProgressDialog(ULONG ulUIParam, ULONG ulFlags, LPMAPIPROGRESS * lppProgress) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::ReadReceipt(ULONG ulFlags, LPMESSAGE lpReadMessage, LPMESSAGE * lppEmptyMessage) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::PrepareSubmit(LPMESSAGE lpMessage, ULONG * lpulFlags) {
	// for every recipient,
	// if mapi_submitted flag, clear flag, responsibility == false
	// else set recipient type P1, responsibility == true
    return MAPI_E_NO_SUPPORT;
}

/** 
 * Should perform the following tasks:
 *
 * 1 Expand certain personal distribution lists to their component recipients.
 * 2 Replace all display names that have been changed with the original names.
 * 3 Mark any duplicate entries.
 * 4 Resolve all one-off addresses. 
 * 5 Check whether the message needs preprocessing and, if it does,
 *   set the flag pointed to by lpulFlags to NEEDS_PREPROCESSING.
 * 
 * Currently we only do step 1. The rest is done by the spooler and inetmapi.
 *
 * @param[in] lpMessage The message to process
 * @param[out] lpulFlags Return flags for actions required by the caller.
 * 
 * @return MAPI Error code
 */
HRESULT M4LMAPISupport::ExpandRecips(LPMESSAGE lpMessage, ULONG * lpulFlags) {
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrRecipientTable;
	SRowSetPtr ptrRow;
	AddrBookPtr ptrAddrBook;
	std::set<std::vector<unsigned char> > setFilter;
	SPropTagArrayPtr ptrColumns;

	hr = session->OpenAddressBook(0, NULL, AB_NO_DIALOG, &~ptrAddrBook);
	if (hr != hrSuccess)
		goto exit;
	hr = lpMessage->GetRecipientTable(fMapiUnicode | MAPI_DEFERRED_ERRORS, &~ptrRecipientTable);
	if (hr != hrSuccess)
		goto exit;
	hr = ptrRecipientTable->QueryColumns(TBL_ALL_COLUMNS, &~ptrColumns);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrRecipientTable->SetColumns(ptrColumns, 0);
	if (hr != hrSuccess)
		goto exit;

	while (true) {
		const SPropValue *lpAddrType = NULL;
		const SPropValue *lpDLEntryID = NULL;
		ULONG ulObjType;
		DistListPtr ptrDistList;
		MAPITablePtr ptrMemberTable;
		SRowSetPtr ptrMembers;

		hr = ptrRecipientTable->QueryRows(1, 0L, &ptrRow);
		if (hr != hrSuccess)
			goto exit;

		if (ptrRow.size() == 0)
			break;

		lpAddrType = PCpropFindProp(ptrRow[0].lpProps, ptrRow[0].cValues, PR_ADDRTYPE);
		if (!lpAddrType)
			continue;

		if (_tcscmp(lpAddrType->Value.LPSZ, _T("MAPIPDL")))
			continue;

		lpDLEntryID = PCpropFindProp(ptrRow[0].lpProps, ptrRow[0].cValues, PR_ENTRYID);
		if (!lpDLEntryID)
			continue;

		if (setFilter.find(std::vector<unsigned char>(lpDLEntryID->Value.bin.lpb, lpDLEntryID->Value.bin.lpb + lpDLEntryID->Value.bin.cb)) != setFilter.end()) {
			// already expanded this group so continue without opening
			hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE, (LPADRLIST)ptrRow.get());
			if (hr != hrSuccess)
				goto exit;
			continue;
		}
		setFilter.insert(std::vector<unsigned char>(lpDLEntryID->Value.bin.lpb, lpDLEntryID->Value.bin.lpb + lpDLEntryID->Value.bin.cb));
		hr = ptrAddrBook->OpenEntry(lpDLEntryID->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpDLEntryID->Value.bin.lpb), NULL, 0, &ulObjType, &~ptrDistList);
		if (hr != hrSuccess)
			continue;

		// remove MAPIPDL entry when distlist is opened
		hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE, (LPADRLIST)ptrRow.get());
		if (hr != hrSuccess)
			goto exit;
		hr = ptrDistList->GetContentsTable(fMapiUnicode, &~ptrMemberTable);
		if (hr != hrSuccess)
			continue;

		// Same columns as recipient table
		hr = ptrMemberTable->SetColumns(ptrColumns, 0);
		if (hr != hrSuccess)
			goto exit;

		// Get all recipients in distlist, and add to message.
		// If another distlist is here, it will expand in the next loop.
		hr = ptrMemberTable->QueryRows(-1, fMapiUnicode, &ptrMembers);
		if (hr != hrSuccess)
			continue;

		// find all unknown properties in the rows, reference-copy those from the original recipient
		// ModifyRecipients() will actually copy the data
		for (ULONG c = 0; c < ptrMembers.size(); ++c) {
			for (ULONG i = 0; i < ptrMembers[c].cValues; ++i) {
				if (PROP_TYPE(ptrMembers[c].lpProps[i].ulPropTag) != PT_ERROR)
					continue;

				// prop is unknown, find prop in recip, and copy value
				auto lpRecipProp = PCpropFindProp(ptrRow[0].lpProps, ptrRow[0].cValues, CHANGE_PROP_TYPE(ptrMembers[c].lpProps[i].ulPropTag, PT_UNSPECIFIED));
				if (lpRecipProp)
					ptrMembers[c].lpProps[i] = *lpRecipProp;
				// else: leave property unknown
			}
		}

		hr = lpMessage->ModifyRecipients(MODRECIP_ADD, (LPADRLIST)ptrMembers.get());
		if (hr != hrSuccess)
			goto exit;
	}
	hr = hrSuccess;

	// Return 0 (no spooler needed, no preprocessing needed)
	if(lpulFlags)
		*lpulFlags = 0;

exit:
	return hr;
}

HRESULT M4LMAPISupport::UpdatePAB(ULONG ulFlags, LPMESSAGE lpMessage) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::DoSentMail(ULONG ulFlags, LPMESSAGE lpMessage) {
	HRESULT hr = ::DoSentMail(session, NULL, ulFlags, object_ptr<IMessage>(lpMessage)); // from CommonUtil
	return hr;
}

HRESULT M4LMAPISupport::OpenAddressBook(LPCIID lpInterface, ULONG ulFlags, LPADRBOOK * lppAdrBook) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::Preprocess(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::CompleteMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::StoreLogoffTransports(ULONG * lpulFlags) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::StatusRecips(LPMESSAGE lpMessage, LPADRLIST lpRecipList) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::WrapStoreEntryID(ULONG cbOrigEntry, LPENTRYID lpOrigEntry, ULONG * lpcbWrappedEntry,
										 LPENTRYID * lppWrappedEntry) {
	// get the dll name from SVCService
	HRESULT hr = hrSuccess;
	const SPropValue *lpDLLName = NULL;

	if (!service) {
		// addressbook provider doesn't have the SVCService object
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	lpDLLName = service->GetProp(PR_SERVICE_DLL_NAME_A);
	if (!lpDLLName || !lpDLLName->Value.lpszA) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// call mapiutil version
	hr = ::WrapStoreEntryID(0, (TCHAR*)lpDLLName->Value.lpszA, cbOrigEntry, lpOrigEntry, lpcbWrappedEntry, lppWrappedEntry);

exit:	
	return hr;
}

HRESULT M4LMAPISupport::ModifyProfile(ULONG ulFlags) {
	HRESULT hr = hrSuccess;
    return hr;
}

HRESULT M4LMAPISupport::IStorageFromStream(LPUNKNOWN lpUnkIn, LPCIID lpInterface, ULONG ulFlags, LPSTORAGE * lppStorageOut) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISupport::GetSvcConfigSupportObj(ULONG ulFlags, LPMAPISUP * lppSvcSupport) {
    return MAPI_E_NO_SUPPORT;
}

// iunknown passthru
ULONG M4LMAPISupport::AddRef() {
    return M4LUnknown::AddRef();
}
ULONG M4LMAPISupport::Release() {
    return M4LUnknown::Release();
}
HRESULT M4LMAPISupport::QueryInterface(REFIID refiid, void **lpvoid) {
	HRESULT hr = hrSuccess;

	if (refiid == IID_IMAPISup) {
		AddRef();
		*lpvoid = static_cast<IMAPISupport *>(this);
	} else if (refiid == IID_IUnknown) {
		AddRef();
		*lpvoid = static_cast<IUnknown *>(this);
	} else if (refiid == IID_IMAPIGetSession) {
		IMAPIGetSession *lpGetSession = new M4LMAPIGetSession(session);
		lpGetSession->AddRef();
		*lpvoid = lpGetSession;
    } else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	return hr;
}
