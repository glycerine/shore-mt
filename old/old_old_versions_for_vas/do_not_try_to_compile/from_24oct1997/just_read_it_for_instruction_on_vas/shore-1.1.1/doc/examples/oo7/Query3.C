/*$Header: /p/shore/shore_cvs/src/oo7/Query3.C,v 1.7 1995/03/24 23:58:10 nhall Exp $*/
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

#include <libc.h>
#include <stdio.h>

#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "BenchParams.h"

//////////////////////////////////////////////////////////////////
//
// Query #3 - 10% selection on AtomicParts via build date (the
// most recent 10% of AtomicParts.)   The actual percentage
// is determined by the constant Query3Percent.  This may
// be varied as we gain experience with the benchmark - the
// goal is to check out if the "optimizer" is smart enough
// to properly choose between B+ tree and scan. 
//
/////////////////////////////////////////////////////////////////


//extern persistent btree<int> BuildDateIndex;


//////////////////////////////////////////////////////////////////
//
// Routine to do Query #3
//
//////////////////////////////////////////////////////////////////


int query3()
{

    REF(AtomicPart) ap;
    // choose a range of dates with the appropriate selectivity
    int dateRange = (Query3Percent * (MaxAtomicDate - MinAtomicDate)) / 100;
    int lowerDate = MaxAtomicDate - dateRange;

    if (debugMode) {
        printf("    Running Query3 with dateRange = %d, lowerDate = %d\n",
		    dateRange, lowerDate);
    }

    // iterate over all atomic parts in the design library (since this
    // release of Objectivity/DB does not include B+ trees) looking for
    // atomic parts that were built recently enough to be of interest

    int count = 0;
#ifdef oldcode
    fprintf(stderr,"ERROR: SDL builddateindex not implemented\n");
	exit(1);
#endif
    //btree_retrieve<int> DateRetrieve(BuildDateIndex);
    //DateRetrieve.SetLB(lowerDate);
    index_iter<typeof(tbl->BuildDateIndex)>
	DateRetrieve(tbl->BuildDateIndex,lowerDate,MaxAtomicDate);
    // upper range is end of index?
	shrc rc;
    for (rc=DateRetrieve.next(); !rc&&!DateRetrieve.eof; rc=DateRetrieve.next())
    {  
	ap = DateRetrieve.cur_val; 
	if (debugMode) {
	    printf("    In Query1, partId = %d, buildDate = %d\n",
			ap->id, ap->buildDate);
	}

	// process qualifying parts by calling the null procedure
	if (ap->buildDate >= lowerDate) {
	    ap->DoNothing();
	    count++;
	}
	else // shouldn't happen
	{  
	    printf("error:    In Query2, partId = %d, buildDate = %d, indexval %d lowerDate %d\n",
			ap->id, ap->buildDate, 
			DateRetrieve.cur_key,lowerDate);
	}
    }
    return count;
}
