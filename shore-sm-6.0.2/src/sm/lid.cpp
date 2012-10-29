/*<std-header orig-src='shore'>

 $Id: lid.cpp,v 1.156 2011/09/08 18:10:56 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#define SM_SOURCE
#define LID_C

#ifdef __GNUG__
#pragma implementation "lid.h"
#endif

#include <sm_int_4.h>
#include <btcursor.h>

#ifdef HAVE_UNAME
#        include <sys/utsname.h>
#endif

#include <unistd.h> 

#include <netdb.h>        /* XXX really should be included for all */
#include <arpa/inet.h>        /* XXX really should be included for all */
#include <w_hashing.h>    /* to hash on the time of day */

rc_t
lid_m::generate_new_volid(lvid_t& lvid, const char *name_in/*=NULL*/)
{
    FUNC(lid_m::_generate_new_volid);
    /*
     * For now the long volume ID will consists of
     * the machine network address and the current time-of-day.
     *
     * Since the time of day resolution is in seconds,
     * we protect this function with a mutex to guarantee we
     * don't generate duplicates.
     */
    const int    max_name = 100;
    char         name[max_name+1];

    // Mutex only for generating new volume ids.
    static queue_based_block_lock_t lidmgnrt_mutex;
    CRITICAL_SECTION(cs, lidmgnrt_mutex);

    /* NOTE to programmers: 
     * If you are using multiple cooperating servers and you want these
     * host names to be distinct, and if you do not have gethostbyname
     * or uname 
     * you've got to do something about the choice of a host name here.
     * You can use a server configuration argument from your config file
     * if you wish.  That value will override all here.
     *
     * For now we just use localhost.localdomain, and suppose that the
     * time of day will suffice to distinguish for long ids.   However
     */
    if(name_in == NULL) {
#if HAVE_UNAME
        struct        utsname uts;
        if (uname(&uts) == -1) return RC(eOS);
        strncpy(name, uts.nodename, max_name);

/* NOTE: config seems not to distinguish gethostbyname from gethostname */
#elif HAVE_GETHOSTBYNAME
        if (gethostname(name, max_name)) return RC(eOS);
#else
        strncpy(name, "localhost.localdomain", max_name);
#endif
    } else {
        strncpy(name, name_in, max_name);
    }

#if HAVE_GETHOSTBYNAME
    {
        struct hostent* hostinfo = gethostbyname(name);

        if (!hostinfo)
            W_FATAL(eINTERNAL);

        memcpy(&lvid.high, hostinfo->h_addr, sizeof(lvid.high));
    }
#else
    {
        struct addrinfo* res = NULL;
        struct addrinfo hints;
        memset (&hints, 0, sizeof (hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags |= AI_CANONNAME;

        int e=0;
        if( (e=getaddrinfo(name, NULL /*service*/, &hints, &res)) ) {
            const char *msg = gai_strerror(e);
            W_FATAL_MSG(eINTERNAL, << msg);
        }

        struct sockaddr_in *addr = ((struct sockaddr_in *) res->ai_addr);
        // min:
        unsigned smaller = sizeof(lvid.high);
        if(smaller >  sizeof(addr->sin_addr)) smaller = sizeof(addr->sin_addr);

        memcpy(&lvid.high, &addr->sin_addr, smaller);

        if(res) {
           freeaddrinfo(res);
        }
    }
#endif

    static w_hashing::uhash lvidhashfunc;

#if HAVE_GETTIMEOFDAY
    /* replacement for using seconds:  hash based on usec + seconds */
    {
        struct timeval res;
        long t = gettimeofday(&res, NULL);
        if(t) {
            W_FATAL_MSG(eINTERNAL, << strerror(t));
        }
        lvid.low = uint4_t(
                lvidhashfunc(res.tv_sec) + 
                lvidhashfunc(res.tv_usec));
    }
#else
    {
        /* XXXX generating ids fast enough can create a id time sequence
           that grows way faster than real time!  This could be a problem!
           Better time resolution than seconds does exist, might be worth
           using it.  */
        static long  last_time = 0;
        stime_t curr_time = stime_t::now();

        if (curr_time.secs() > last_time)
                last_time = curr_time.secs();
        else
                last_time++;

        lvid.low = uint4_t(lvidhashfunc(last_time));
    }
#endif

    DBG( << "lvid " << lvid );

    return RCOK;
}
