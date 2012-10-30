/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *   $RCSfile: unix_stats.cc,v $
 *   $Revision: 1.7 $
 *   $Date: 1997/06/15 02:06:02 $
 */ 

#ifdef __GNUC__
#pragma implementation "unix_stats.h"
#endif

#include <stream.h>
#include <w_workaround.h>
#include "unix_stats.h"
#define MILLION 1000000

unix_stats::unix_stats()  { who = RUSAGE_SELF; }
unix_stats::unix_stats(int _who)  { who = _who; }

void
unix_stats::start() 
{
	iterations = 0; 
    /*
     * Save the current stats in buffer area 1.
     */
    gettimeofday(&time1, NULL);
    getrusage(who, &rusage1);

    return;
}

void
unix_stats::stop(int iter) 
{
    /*
     * Save the final stats in buffer area 2.
     */
    gettimeofday(&time2, NULL);
    getrusage(who, &rusage2);

	if(iter < 1) {
		iterations = 0;
	} else {
		iterations = iter;
	}

    return;
}

int 
unix_stats::clocktime()  const  // in microseconds
{
    return (MILLION * (time2.tv_sec - time1.tv_sec)) + 
           (time2.tv_usec - time1.tv_usec);
}

int 
unix_stats::usertime()  const // in microseconds
{
    return (MILLION * (rusage2.ru_utime.tv_sec - rusage1.ru_utime.tv_sec)) +
           (rusage2.ru_utime.tv_usec - rusage1.ru_utime.tv_usec);
}

int 
unix_stats::systime()  const // in microseconds
{
    return (MILLION * (rusage2.ru_stime.tv_sec - rusage1.ru_stime.tv_sec)) +
           (rusage2.ru_stime.tv_usec - rusage1.ru_stime.tv_usec);
}

int
unix_stats::page_reclaims()  const
{
    return rusage2.ru_minflt - rusage1.ru_minflt;
}

int
unix_stats::page_faults()  const
{
    return rusage2.ru_majflt - rusage1.ru_majflt;
}

int
unix_stats::swaps()  const
{
    return rusage2.ru_nswap - rusage1.ru_nswap;
}

int
unix_stats::vcsw()  const
{
    return rusage2.ru_nvcsw - rusage1.ru_nvcsw;
}

int
unix_stats::invcsw()  const
{
    return rusage2.ru_nivcsw - rusage1.ru_nivcsw;
}
int
unix_stats::inblock()  const
{
    return rusage2.ru_inblock - rusage1.ru_inblock;
}

int
unix_stats::oublock()  const
{
    return rusage2.ru_oublock - rusage1.ru_oublock;
}
int
unix_stats::rss()  const
{
    return rusage2.ru_idrss - rusage1.ru_idrss;
}
int
unix_stats::msgsent()  const
{
    return rusage2.ru_msgsnd - rusage1.ru_msgsnd;
}
int
unix_stats::msgrecv()  const
{
    return rusage2.ru_msgrcv - rusage1.ru_msgrcv;
}

ostream &unix_stats::print(ostream &o) const
{
	int   i;
	int clk,usr,sys;

	if(iterations== 0) {
		o << "error: unix_stats was never properly \"stop()\"-ed. ";
		return o;
	}
	clk=clocktime();
	usr=usertime();
	sys=systime();
#define FORM(u) ("%2.2f", ((((float)u)/iterations)/MILLION)) << #u <<" ";
	if(clk+usr+sys != 0) {
		o << "usec/iter for " << iterations << " iterations" << endl;
		// print in order that time(csh(1)) does:
		if(usr!= 0) { W_FORM(o)FORM(usr); }
		if(sys != 0) { W_FORM(o)FORM(sys); }
		if(clk!= 0) { W_FORM(o)FORM(clk); }
		if(clk!= 0) {
		    W_FORM(o)("%2.2f %%",(float)((float)(usr+sys)/clk)*100) << " ";
		}
		o << endl;
	}
	if( (i=page_faults()) != 0) {
		W_FORM(o)("%6d maj ",i);
	}
	if( (i=page_reclaims()) != 0) {
		W_FORM(o)("%6d min ",i);
	}
	if( (i=swaps()) != 0) {
		W_FORM(o)("%6d swp ",i);
	}
	if( (i=vcsw()) != 0) {
		W_FORM(o)("%6d csw",i);
	}
	if( (i=invcsw()) != 0) {
		W_FORM(o)("%6d inv",i);
	}
	if( (i=inblock()) != 0) {
		W_FORM(o)("%6d inblk",i);
	}
	if( (i=oublock()) != 0) {
		W_FORM(o)("%6d oublk",i);
	}
	// Rss is sort of meaningless
	// if( (i=rss()) != 0) {
	//	W_FORM(o)("%6d rss",i);
	//}
	if( (i=msgsent()) != 0) {
		W_FORM(o)("%6d snd",i);
	}
	if( (i=msgrecv()) != 0) {
		W_FORM(o)("%6d rcv",i);
	}
	return o;
}

ostream& operator<<( ostream& o, const unix_stats &s)
{
	return s.print(o);
}

int 
unix_stats::s_clocktime()  const  
{
    return time2.tv_sec - time1.tv_sec;
}

int 
unix_stats::s_usertime()  const 
{
    return rusage2.ru_utime.tv_sec - rusage1.ru_utime.tv_sec;
}

int 
unix_stats::s_systime()  const 
{
    return rusage2.ru_stime.tv_sec - rusage1.ru_stime.tv_sec;
}

int 
unix_stats::us_clocktime()  const  // in microseconds
{
    return (time2.tv_usec - time1.tv_usec);
}

int 
unix_stats::us_usertime()  const // in microseconds
{
    return (rusage2.ru_utime.tv_usec - rusage1.ru_utime.tv_usec);
}

int 
unix_stats::us_systime()  const // in microseconds
{
    return (rusage2.ru_stime.tv_usec - rusage1.ru_stime.tv_usec);
}

int 
unix_stats::signals()  const 
{
    return (rusage2.ru_nsignals- rusage1.ru_nsignals);
}


extern float compute_time(const struct timeval*, const struct timeval*);

float compute_time(const struct timeval* start_time, const struct timeval* end_time)
{
    double seconds, useconds;

    seconds = (double)(end_time->tv_sec - start_time->tv_sec);
    useconds = (double)(end_time->tv_usec - start_time->tv_usec);

    if (useconds < 0.0) {
        useconds = 1000000.0 + useconds;
        seconds--;
    }
    return (seconds + useconds/1000000.0);
}

float 
unix_stats::compute_time() const
{
    return ::compute_time(&time1, &time2);
}

