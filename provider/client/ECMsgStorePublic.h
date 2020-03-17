/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright 2005 - 2016 Zarafa and its licensors
 */
#pragma once
#include <mapidefs.h>
#include <mapispi.h>
#include <edkmdb.h>
#include "ECMsgStore.h"
#include "ClientUtil.h"
#include <kopano/ECMemTable.h>
#include <kopano/Util.h>
#include <kopano/memory.hpp>
#include <kopano/zcdefs.h>

class ECMsgStorePublic KC_FINAL_OPG : public ECMsgStore {
protected:
	ECMsgStorePublic(const char *profile, IMAPISupport *, WSTransport *, BOOL modify, unsigned int profile_flags, BOOL offline_store);
public:
	static HRESULT GetPropHandler(unsigned int tag, void *prov, unsigned int flags, SPropValue *, ECGenericProp *lpParam, void *base);
	static HRESULT SetPropHandler(unsigned int tag, void *prov, const SPropValue *, ECGenericProp *);
	static HRESULT Create(const char *profile, IMAPISupport *, WSTransport *, BOOL modify, unsigned int profile_flags, BOOL offline_store, ECMsgStore **);
	virtual HRESULT QueryInterface(const IID &, void **) override;
	virtual HRESULT OpenEntry(unsigned int eid_size, const ENTRYID *eid, const IID *intf, unsigned int flags, unsigned int *obj_type, IUnknown **) override;
	virtual HRESULT SetEntryId(unsigned int eid_size, const ENTRYID *eid) override;
	HRESULT InitEntryIDs();
	HRESULT GetPublicEntryId(enumPublicEntryID ePublicEntryID, void *lpBase, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
	HRESULT ComparePublicEntryId(enumPublicEntryID, ULONG eid_size, const ENTRYID *eid, ULONG *result);
	KC::ECMemTable *GetIPMSubTree();
	virtual HRESULT Advise(ULONG eid_size, const ENTRYID *, ULONG evt_mask, IMAPIAdviseSink *, ULONG *conn) override;

protected:
	KC::memory_ptr<ENTRYID> m_lpIPMSubTreeID, m_lpIPMFavoritesID;
	KC::memory_ptr<ENTRYID> m_lpIPMPublicFoldersID;
	ULONG m_cIPMSubTreeID = 0, m_cIPMFavoritesID = 0;
	ULONG m_cIPMPublicFoldersID = 0;
	KC::object_ptr<KC::ECMemTable> m_lpIPMSubTree; /* Built-in IPM subtree */
	KC::object_ptr<IMsgStore> m_lpDefaultMsgStore;

	HRESULT BuildIPMSubTree();
	// entryid : level
	ALLOC_WRAP_FRIEND;
};
