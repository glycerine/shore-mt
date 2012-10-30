/*$Header: /p/shore/shore_cvs/src/oo7/Support.C,v 1.3 1995/03/24 23:58:23 nhall Exp $*/
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

#include <stdio.h>
#include <sys/time.h>

#ifdef _HPUX_SOURCE
#include <unistd.h>
#include "getrusage.h"
#endif
#include <sys/resource.h>

#include "oo7.h"
#include "globals.h"


void JoinDoNothing(int id1, int id2)
{
    if (debugMode) {
        printf("==> JoinDoNothing(id1 = %d, id2 = %d.)\n",
		               id1, id2);
    }
}


double ComputeWallClockTime(struct timeval *startWallTime, 
	           struct timeval *endWallTime)
{
    double seconds, useconds;


    seconds = (double) (endWallTime->tv_sec - startWallTime->tv_sec);
    useconds = (double) (endWallTime->tv_usec - startWallTime->tv_usec);

    if (useconds < 0.0) {
        useconds = 1000000.0 + useconds;
        seconds--;
    }
    return (seconds + useconds/1000000.0);
}

double ComputeUserTime(struct rusage *startUsage, 
	           struct rusage *endUsage)
{
    double seconds, useconds;

    seconds = (double) (endUsage->ru_utime.tv_sec -
			startUsage->ru_utime.tv_sec);

    useconds = (double) (endUsage->ru_utime.tv_usec -
			 startUsage->ru_utime.tv_usec);

    if (useconds < 0.0) {
        useconds = 1000000.0 + useconds;
        seconds--;
    }
    return (seconds + useconds/1000000.0);
}

double ComputeSystemTime(struct rusage *startUsage, 
	           struct rusage *endUsage)
{
    double seconds, useconds;

    seconds = (double) (endUsage->ru_stime.tv_sec -
			startUsage->ru_stime.tv_sec);

    useconds = (double) (endUsage->ru_stime.tv_usec -
			 startUsage->ru_stime.tv_usec);

    if (useconds < 0.0) {
        useconds = 1000000.0 + useconds;
        seconds--;
    }
    return (seconds + useconds/1000000.0);
}

void PrintOp(BenchmarkOp op)
{
    switch (op) {
        case Trav1:
            printf("Trav1");
	    break;
        case Trav2a:
            printf("Trav2a");
	    break;
        case Trav2b:
            printf("Trav2b");
	    break;
        case Trav2c:
            printf("Trav2c");
	    break;
        case Trav3a:
            printf("Trav3a");
	    break;
        case Trav3b:
            printf("Trav3b");
	    break;
        case Trav3c:
            printf("Trav3c");
	    break;
        case Trav4:
            printf("Trav4");
	    break;
        case Trav5do:
            printf("Trav5do");
	    break;
        case Trav5undo:
            printf("Trav5undo");
	    break;
        case Trav6:
            printf("Trav6");
	    break;
        case Trav7:
            printf("Trav7");
	    break;
        case Query1:
            printf("Query1");
	    break;
        case Query2:
            printf("Query2");
	    break;
        case Query3:
            printf("Query3");
	    break;
        case Query4:
            printf("Query4");
	    break;
        case Query5:
            printf("Query5");
	    break;
        case Query6:
            printf("Query6");
	    break;
        case Query7:
            printf("Query7");
	    break;
        case Query8:
            printf("Query8");
	    break;
        case Insert:
            printf("Insert");
	    break;
        case Delete:
            printf("Delete");
	    break;
        case Reorg1:
            printf("Reorg1");
	    break;
        case Reorg2:
            printf("Reorg2");
	    break;
        case MultiTrav1:
            printf("MultiTrav1");
	    break;
        case MultiTrav2:
            printf("MultiTrav2");
	    break;
        case MultiTrav3:
            printf("MultiTrav3");
	    break;
        case MultiTrav4:
            printf("MultiTrav4");
	    break;
        case MultiTrav5:
            printf("MultiTrav5");
	    break;
        case MultiTrav6:
            printf("MultiTrav6");
	    break;
    }
}
