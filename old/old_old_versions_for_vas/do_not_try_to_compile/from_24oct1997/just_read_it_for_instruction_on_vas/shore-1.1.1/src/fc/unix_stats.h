/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __STATS_H__
#define __STATS_H__
/*
 *   $RCSfile: unix_stats.h,v $  
 *   $Revision: 1.10 $  
 *   $Date: 1996/10/28 17:51:53 $      
 */ 
/*
 * stats.h
 *
 * Class definition for the stats class.
 * Member functions are defined in stats.c
 */
#ifndef  _stats
#define  _stats

#ifdef __GNUG__
#pragma interface
#endif

#include <stream.h>
#include <iostream.h>

/*
 * SGI machines differentiate between SysV and BSD
 * get time of day sys calls. Make sure it uses the
 * BSD sys calls by defining _BSD_TIME. Check /usr/include/sys/time.h
 */

#if defined(Irix)
#define _BSD_TIME
#endif

#include <sys/time.h>
#include <sys/resource.h>


#ifdef HPUX8
#include <sys/syscall.h>
#define getrusage(a, b)  syscall(SYS_GETRUSAGE, a, b)

extern "C" syscall(int, int, void*);
#endif /* HPUX8 */

/*
 * unix_stats objects are meant to be used as follows:
 *
 *        s.start();
 *        section of code for which stats are being gathered
 *        s.Stop();
 *        examine gathered unix_stats
 */

#if defined(SOLARIS2) 
#undef __STATS_H__
#undef  _stats
#include <solaris_stats.h>
#else /* not SOLARIS2 */
#ifdef __GNUC__
#pragma interface
#endif



/*
 * Stats for Unix processes.
 */
class unix_stats {
protected:
    struct timeval  time1;		/* gettimeofday() buffer */
    struct timeval  time2;		/* gettimeofday() buffer */
    struct rusage   rusage1;	/* getrusage() buffer */
    struct rusage   rusage2;	/* getrusage() buffer */
    int	iterations;
    int who;

public:
    unix_stats();
    unix_stats(int who); // if other than self

    float compute_time() const;

    void   start();			/* start gathering stats  */
	// you can start and then stop multiple times-- don't need to continue
    void   stop(int iter=1);			/* stop gathering stats  */
    int    clocktime() const;		/* elapsed real time in micro-seconds */
    int    usertime() const;		/* elapsed user time in micro-seconds */
    int    systime() const;	/* elapsed system time in micro-seconds */
    /* variants */
    int    s_clocktime() const;		/* diff of seconds only */
    int    s_usertime() const;		
    int    s_systime() const;	
    int    us_clocktime() const;	/* diff of microseconds only */
    int    us_usertime() const;		
    int    us_systime() const;	

    int    page_reclaims() const;	/* page reclaims */
    int    page_faults() const;	/* page faults */
    int    swaps() const;			/* swaps */
    int    inblock() const;		/* page-ins */
    int    oublock() const;		/* page-outs */
    int    rss() const;			/* resident-set size */
    int    vcsw() const;			/* voluntary context swtch */
    int    invcsw() const;			/* involuntary context swtch */
    int    msgsent() const;			/* socket messages sent */
    int    msgrecv() const;			/* socket messages recvd */
    int    signals() const;			/* signals dispatched */

    ostream &print(ostream &) const;
};
extern ostream& operator<<(ostream&, const unix_stats &s);
#endif

#ifndef getrusage
extern "C" int    getrusage(int, struct rusage*);
#endif

#if !defined(SOLARIS2) && !defined(AIX41) && !defined(AIX32)
extern "C" int    gettimeofday(struct timeval*, struct timezone*);
#endif

float compute_time(const struct timeval *start_time, const struct timeval *end_time);

#endif /*_stats*/
#endif 
