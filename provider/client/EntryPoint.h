/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright 2005 - 2016 Zarafa and its licensors
 */
#ifndef ENTRYPOINT_H
#define ENTRYPOINT_H

#include <kopano/zcdefs.h>
#include <mapispi.h>
#include "ProviderUtil.h"

extern "C" {

extern _kc_export HRESULT MSGServiceEntry(HINSTANCE, IMalloc *, IMAPISupport *, ULONG ui_param, ULONG se_flags, ULONG ctx, ULONG nprops, const SPropValue *props, IProviderAdmin *, MAPIERROR **);

}

class WSTransport;
extern HRESULT InitializeProvider(IProviderAdmin *, IProfSect *, const sGlobalProfileProps &, ULONG *eid_size, ENTRYID **store_eid);

// Global values
extern ECMapProvider g_mapProviders;
extern KC::tstring g_strProductName;

#endif // ENTRYPOINT_H
