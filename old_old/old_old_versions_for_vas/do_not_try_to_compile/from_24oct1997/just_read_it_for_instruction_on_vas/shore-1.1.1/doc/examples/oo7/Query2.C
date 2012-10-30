/*$Header: /p/shore/shore_cvs/src/oo7/Query2.C,v 1.7 1995/03/24 23:58:08 nhall Exp $*/
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
// Query #2 - 1% selection on AtomicParts via build date (the
// most recent 1% of AtomicParts.)  A B+ tree index is used
//
/////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// Routine to do Query #2
//
//////////////////////////////////////////////////////////////////


int query2()
{
    REF(AtomicPart) ap;

    // choose a range of dates with the appropriate selectivity
    int dateRange = (Query2Percent * (MaxAtomicDate - MinAtomicDate)) / 100;
    int lowerDate = MaxAtomicDate - dateRange;

    if (debugMode) {
        printf("    Running Query2 with dateRange = %d, lowerDate = %d\n",
		    dateRange, lowerDate);
    }

    int count = 0;

#ifdef oldcode
    fprintf(stderr,"ERROR: SDL builddateindex not implemented\n");
	exit(1);
#endif

    // btree_retrieve<int> DateRetrieve(BuildDateIndex,lowerDate,);
    // DateRetrieve.SetLB(lowerDate);
    // upper range is end of index?
    index_iter<typeof(tbl->BuildDateIndex)> 
	DateRetrieve(tbl->BuildDateIndex,lowerDate,MaxAtomicDate);
	shrc rc;
    for (rc=DateRetrieve.next(); !rc&&!DateRetrieve.eof; rc=DateRetrieve.next())
    {  
	ap = DateRetrieve.cur_val; 
	if (debugMode) {
	    printf("    In Query2, partId = %d, buildDate = %d\n",
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

