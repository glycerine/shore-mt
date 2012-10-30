/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sthread_stats.h,v 1.9 1996/11/20 23:40:46 bolo Exp $
 */
#ifndef STHREAD_STATS_H
#define STHREAD_STATS_H

class sthread_stats {
public:
#include "sthread_stats_struct.i"

    sthread_stats() {
	    // easier than remembering to add the inits 
	    // since we're changing the stats a lot
	    // during development
	    memset(this,'\0', sizeof(*this));
	}
    ~sthread_stats(){ }
    friend ostream &operator <<(ostream &o, const sthread_stats &s);
    void clear() {
	memset((void *)this, '\0', sizeof(*this));
    }
};

#ifdef STHREAD_C
ostream &
operator <<(ostream &o, const sthread_stats &s) {

	o << "STHREAD STATS: " << endl;
	o << "CTX switches: " << s.ctxsw << endl;
	o << "Idle thread:: " 
		<< s.idle_yield_return  << " yields, "
		// << s.selects  << " waits==selects() "
		<< endl;
	o << "Spins: " << s.spins << endl;

	o << "Latches, mutexes, semaphores: "  << endl
 	  << "\tlatch waits:        " << s.latch_wait << endl
 	  << "\tlatch wait time:    " << s.latch_time << endl
 	  << "\tmutex waits:        " << s.mutex_wait << endl
 	  << "\tcondition waits:    " << s.scond_wait << endl
 	  << "\tevent sem waits:    " << s.sevsem_wait << endl
	  ;

	o << "Selects:" << endl 
 	  << "\tselect():           " << s.selects << endl
	  << "\ttimed out:          " << s.idle << endl
	  << "\tinterrupted:        " << s.eintrs << endl
	  << "\tfound something:    " << s.selfound << endl
	  << "\tidle time in select:" << s.idle_time << endl
	  << endl << endl;

	o << "I/O:" << endl 
	    << "\tio_time: " << s.io_time
	    << "   ios: " << s.io 
	    << "   cc_io: " << s.ccio 
	    << "   iowaits: " << s.iowaits
	    << endl;
        o <<   "\tfastpath I/O: " << s.fastpath
            << "   slow writes: " << s.nofastwrite 
	    << endl;
        o <<   "\tlocal I/O: " << s.local_io << endl;

	o << "\tI/Os waiting the given #selects:\n\t";
	    if(s.zero>0) o << " zero: " << s.zero;
	    if(s.one>0)  o << " one: " << s.one ;
	    if(s.two>0)  o << " two: " << s.two ;
	    if(s.three>0)o << " three: " << s.three;
	    if(s.more>0)o << " >3: " << s.more;
	    if(s.wrapped>0)o << " wrapped: " << s.wrapped;
	    o << endl;

	return o;
}
#endif /*STHREAD_C*/

extern class sthread_stats SthreadStats;

#define STATS SthreadStats

#endif /* STHREAD_STATS_H */
