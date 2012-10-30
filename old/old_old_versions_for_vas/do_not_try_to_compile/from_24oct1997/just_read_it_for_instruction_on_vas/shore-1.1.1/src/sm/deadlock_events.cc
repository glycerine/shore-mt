/* --------------------------------------------------------------- */
/* -- Copyright (c) 1997 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: deadlock_events.cc,v 1.5 1997/06/15 03:14:29 solomon Exp $
 */

#define SM_SOURCE
#include <sm_int_1.h>

template class w_list_t<GtidElem>;
W_FASTNEW_STATIC_DECL(GtidElem, 64);

void
DeadlockEventPrinter::LocalDeadlockDetected(XctWaitsForLockList& waitsForList, const xct_t* current, const xct_t* victim)
{
    out << "LOCAL DEADLOCK DETECTED:\n"
	<< " CYCLE:\n";
    bool isFirstElem = true;
    while (XctWaitsForLockElem* elem = waitsForList.pop())  {
        char tidBuffer[80];
        ostrstream tidStream(tidBuffer, 80);
        tidStream << elem->xct->tid() << ends;
        out.form("  %7s%15s waits for ", isFirstElem ? "" : "held by", tidBuffer);
        out << elem->lockName << '\n';
        isFirstElem = false;
        delete elem;
    }
    out	<< " XCT WHICH CAUSED CYCLE:  " << current->tid() << '\n'
	<< " SELECTED VICTIM:         " << victim->tid() << '\n';
}

void
DeadlockEventPrinter::KillingGlobalXct(const xct_t* xct, const lockid_t& lockid)
{
    out << "GLOBAL DEADLOCK DETECTED:\n"
	<< " KILLING local xct " << xct->tid() << " (global id " << *xct->gtid()
	<< " was holding lock " << lockid << '\n';
}

void
DeadlockEventPrinter::GlobalDeadlockDetected(GtidList& list)
{
    out << "GLOBAL DEADLOCK DETECTED:\n"
	<< " PARTICIPANT LIST:\n";
    while (GtidElem* gtidElem = list.pop())  {
	out << "  " << gtidElem->gtid << '\n';
	delete gtidElem;
    }
}

void
DeadlockEventPrinter::GlobalDeadlockVictimSelected(const gtid_t& gtid)
{
    out << "GLOBAL DEADLOCK DETECTED:\n"
	<< " SELECTING VICTIM: " << gtid << '\n';
}
