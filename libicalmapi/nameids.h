/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright 2005 - 2016 Zarafa and its licensors
 */
#pragma once
#include <kopano/zcdefs.h>
#include <mapidefs.h>
#include <kopano/namedprops.h>
#include <kopano/mapiguidext.h>
#include <mapiguid.h>

namespace KC {

/* MAPINAMEID strings are in UCS-2LE, so cannot be static initialized in linux
   We first define all strings, so we can use the nmStringNames[] lookup array
 */
enum eIDNamedProps {
	PROP_KEYWORDS = 0,
// End of String names
	PROP_MEETINGLOCATION, PROP_GOID, PROP_ISRECURRING, PROP_CLEANID, PROP_OWNERCRITICALCHANGE, PROP_ATTENDEECRITICALCHANGE, PROP_OLDSTART, PROP_ISEXCEPTION, PROP_RECURSTARTTIME,
	PROP_RECURENDTIME, PROP_MOZGEN, PROP_MOZLASTACK, PROP_MOZ_SNOOZE_SUFFIX, PROP_MOZSENDINVITE , PROP_APPTTSREF, PROP_FLDID, PROP_SENDASICAL, PROP_APPTSEQNR, PROP_APPTSEQTIME,
	PROP_BUSYSTATUS, PROP_APPTAUXFLAGS, PROP_LOCATION, PROP_LABEL, PROP_APPTSTARTWHOLE, PROP_APPTENDWHOLE, PROP_APPTDURATION,
	PROP_ALLDAYEVENT, PROP_RECURRENCESTATE, PROP_MEETINGSTATUS, PROP_RESPONSESTATUS, PROP_RECURRING, PROP_INTENDEDBUSYSTATUS,
	PROP_RECURRINGBASE, PROP_REQUESTSENT, PROP_APPTREPLYNAME, PROP_RECURRENCETYPE, PROP_RECURRENCEPATTERN, PROP_TIMEZONEDATA, PROP_TIMEZONE,
	PROP_RECURRENCE_START, PROP_RECURRENCE_END, PROP_ALLATTENDEESSTRING, PROP_TOATTENDEESSTRING, PROP_CCATTENDEESSTRING, PROP_NETMEETINGTYPE,
	PROP_NETMEETINGSERVER, PROP_NETMEETINGORGANIZERALIAS, PROP_NETMEETINGAUTOSTART, PROP_AUTOSTARTWHEN, PROP_CONFERENCESERVERALLOWEXTERNAL, PROP_NETMEETINGDOCPATHNAME,
	PROP_NETSHOWURL, PROP_CONVERENCESERVERPASSWORD, PROP_APPTREPLYTIME, PROP_COUNTERPROPOSAL, PROP_PROPOSEDSTART, PROP_PROPOSEDEND,
	PROP_REMINDERMINUTESBEFORESTART, PROP_REMINDERTIME, PROP_REMINDERSET, PROP_PRIVATE,
	PROP_NOAGING, PROP_SIDEEFFECT, PROP_REMOTESTATUS, PROP_COMMONSTART, PROP_COMMONEND, PROP_COMMONASSIGN,
	PROP_CONTACTS, PROP_OUTLOOKINTERNALVERSION, PROP_OUTLOOKVERSION, PROP_REMINDERNEXTTIME, PROP_HIDE_ATTACH,
	PROP_TASK_STATUS, PROP_TASK_COMPLETE, PROP_TASK_PERCENTCOMPLETE, PROP_TASK_STARTDATE, PROP_TASK_DUEDATE,
	PROP_TASK_RECURRSTATE, PROP_TASK_ISRECURRING, PROP_TASK_COMPLETED_DATE,
// End if ID names
	SIZE_NAMEDPROPS
};

/* call this function to get the IDs from the listed namedprops above */
extern KC_EXPORT HRESULT HrLookupNames(IMAPIProp *propobj, SPropTagArray **namedprops);

} /* namespace */
