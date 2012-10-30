/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: shmc_stats.h,v 1.4 1995/04/24 19:42:17 zwilling Exp $
 */

#ifndef __SHMC_STATS_H__
#define __SHMC_STATS_H__

class shmc_stats {
public:
#include "shmc_stats_struct.i"

    shmc_stats(): alarm(0), notify(0), kicks(0), falarm(0), fnotify(0),
				  found(0), lastsig(0), spins(0) {}

    ~shmc_stats(){ }

    friend ostream &operator <<(ostream &o, const shmc_stats &s);

	void clear() {
		memset((void *)this, '\0', sizeof(*this));
	}
};

extern class shmc_stats ShmcStats;

#if defined(STHREAD_C) || defined(DISKRW_C)

class shmc_stats ShmcStats; 

ostream &operator 
<<(ostream &o, const shmc_stats &s) {
	if(s.kicks + s.notify>0){
#ifdef STHREAD_C
		o << "Server ";
#endif
#ifdef DISKRW_C
		o << "Diskrw ";
#endif
		o << "Signal stats: \n" 
			<< " spins = " << s.spins
			<< " ALRMs = " << s.alarm
#ifndef PIPE_NOTIFY
			<< " USR1<--server= " << s.notify
			<< " USR2-->server= " << s.kicks
#else /* PIPE_NOTIFY */
			<< " PREAD= " << s.notify
			<< " PWRITE= " << s.kicks
#endif /* PIPE_NOTIFY */
			<< endl;
		o << "Found msg due to:" 
			<< " ALRMs= " << s.falarm
#ifndef PIPE_NOTIFY
			<< " USR1= " << s.fnotify
#else /* PIPE_NOTIFY */
			<< " PREAD= " << s.fnotify
#endif /* PIPE_NOTIFY */
			<< " found= " << s.found
			<< endl;
	}
	return o;
}
#endif /* STHREAD_C or DISKRW_C*/

#endif /* __SHMC_STATS_H__ */
