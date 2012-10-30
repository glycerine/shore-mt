#ifndef  _stats
#define  _stats
/*$Header: /p/shore/shore_cvs/src/oo7/stats.h,v 1.6 1996/07/24 21:18:59 nhall Exp $*/
/********************************************************/
/*                                                      */
/*               OO7 Benchmark                          */
/*                                                      */
/*              COPYRIGHT (C) 1993                      */
/*                                                      */
/*                Michael J. Carey 		        */
/*                David J. DeWitt 		        */
/*                Jeffrey Naughton 		        */
/*               Madison, WI U.S.A.                     */
/*                                                      */
/*	         ALL RIGHTS RESERVED                    */
/*                                                      */
/********************************************************/

/*
 * stats.h
 *
 * Class definition for the stats class.
 * Member functions are defined in stats.c
 */

#include <stdio.h>
#include <sys/types.h>
#ifdef hpux
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <sys/resource.h>

/*
 * Stats objects are meant to be used as follows:
 *
 *        statsObj.Start();
 *        section of code for which stats are being gathered
 *        statsObj.Stop();
 *        examine gathered statistics
 *
 * Note that member functions marked as unimplmented return zero.
 * They are unimplemented because Mach has yet to support the 
 * gathering of the required statistics.
 * Mach will supposedly provide support in the near future, however.
 */


/*
 * Stats for Unix processes.
 */
class ProcessStats {

protected:
    struct timeval  time1;		/* gettimeofday() buffer */
    struct timeval  time2;		/* gettimeofday() buffer */
    struct rusage   rusage1;	/* getrusage() buffer */
    struct rusage   rusage2;	/* getrusage() buffer */

	int	smPagesRead1;
	int	smPagesRead2;
	int	smPagesWrite1;
	int	smPagesWrite2;

public:
	ProcessStats()	{smPagesRead1 = smPagesRead2 = smPagesWrite1 = smPagesWrite2 = 0; }
    void   Start();			/* start gathering stats  */
    void   Stop();			/* stop gathering stats  */
    float  RealTime();		/* elapsed real time in micro-seconds */
    float  UserTime();		/* elapsed user time in micro-seconds */
    float  SystemTime();	/* elapsed system time in micro-seconds */
    int    PageReclaims();	/* page reclaims */
    int    PageFaults();	/* page faults */
    int    Swaps();			/* swaps */
    int    PageIns();		/* page-ins */
    int    PageOuts();		/* page-outs */
    int    MaxRss();		/* max resident set */
    int    smPagesRead();		/* pages read from sm */
    int    smPagesWrite();		/* pages written to sm */
	void   PrintStatsHeader(FILE *f);
	void   PrintStats(FILE *f, char *prefix);
	void   PrintStatsAvg(FILE *f, double div, char *prefix);
};

/* 
 * C library externs.
 */
#if !defined(_INCLUDE_HPUX_SOURCE) && !defined(SOLARIS2)
extern "C" int    getrusage(int, struct rusage*);
extern "C" int    gettimeofday(struct timeval*, struct timezone*);
#endif

#endif /* _stats */
