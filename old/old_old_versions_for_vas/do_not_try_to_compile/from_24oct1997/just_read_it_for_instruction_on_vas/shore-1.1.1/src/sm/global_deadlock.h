/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: global_deadlock.h,v 1.5 1997/05/19 19:47:14 nhall Exp $
 */

#ifndef GLOBAL_DEADLOCK_H
#define GLOBAL_DEADLOCK_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef LOCK_X_H
#include <lock_x.h>
#endif

class GlobalDeadlockClient  {
    public:
	virtual rc_t	GlobalXctLockWait(lock_request_t* req, const char * blockname) = 0;
	virtual rc_t	UnblockGlobalXct(const gtid_t& gtid) = 0;
	virtual rc_t	SendWaitForGraph() = 0;
	virtual NORET	~GlobalDeadlockClient() {};
};


#ifdef COMMENTED_OUT
class GtidElem  {
    public:
	gtid_t			    gtid;
	w_link_t		    _link;

	NORET			    GtidElem(gtid_t g)
					: gtid(g)
					{};
	NORET			    ~GtidElem()
					{
					    if (_link.member_of() != NULL)
						_link.detach();
					};
	static uint4		    link_offset()
					{
					    return offsetof(GtidElem, _link);
					};
	W_FASTNEW_CLASS_DECL;
};

typedef w_list_t<GtidElem> GtidList;
typedef w_list_i<GtidElem> GtidListIter;

class XctWaitsForLockElem
{
    public:
	const xct_t*	xct;
	lockid_t	lockName;
	w_link_t	_link;

			XctWaitsForLockElem(const xct_t* theXct, const lockid_t& name);
			~XctWaitsForLockElem();
	static uint4	link_offset();

	W_FASTNEW_CLASS_DECL;
};

inline
XctWaitsForLockElem::XctWaitsForLockElem(const xct_t* theXct, const lockid_t& name)
:
    xct(theXct),
    lockName(name)
{
}

inline
XctWaitsForLockElem::~XctWaitsForLockElem()
{
    if (_link.member_of() != 0)  {
	_link.detach();
    }
}

inline uint4
XctWaitsForLockElem::link_offset()
{
    return offsetof(XctWaitsForLockElem, _link);
}

typedef w_list_t<XctWaitsForLockElem> XctWaitsForLockList;
typedef w_list_i<XctWaitsForLockElem> XctWaitsForLockListIter;


class DeadlockEventCallback
{
    public:
	virtual void LocalDeadlockDetected(
				XctWaitsForLockList&	waitsForList,
				const xct_t*		current,
				const xct_t*		victim) = 0;
	virtual void KillingGlobalXct(const xct_t* xct, const lockid_t& lockid) = 0;
	virtual void GlobalDeadlockDetected(GtidList& list) = 0;
	virtual void GlobalDeadlockVictimSelected(const gtid_t& gtid) = 0;
	virtual ~DeadlockEventCallback() {};
};
#endif


#endif
