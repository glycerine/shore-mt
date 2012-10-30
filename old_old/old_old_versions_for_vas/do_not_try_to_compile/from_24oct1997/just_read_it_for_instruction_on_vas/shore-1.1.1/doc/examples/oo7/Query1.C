/*$Header: /p/shore/shore_cvs/src/oo7/Query1.C,v 1.8 1995/07/17 15:50:27 nhall Exp $*/
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
#include "VarParams.h"
//#include "IntBtree.h"
#include "random.h"

//////////////////////////////////////////////////////////////////
//
// Query #1 - randomly choose "Query1RepeatCnt" atomic parts by 
// lookup on their id field.  An index can be used for the
// actual lookup.  
//
/////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// Routine to do Query #1
//
//////////////////////////////////////////////////////////////////


int query1()
{
    int partId;
    REF(AtomicPart) atomH;
    REF(any) r;

    // set random seed so "hot" runs are truly hot
    srandom(1);

    // now randomly select parts via partId index and process them
    for (int i = 0; i < Query1RepeatCnt; i++) {

	// generate part id and lookup part
	bool found;
	partId = (int) (random() % TotalAtomicParts) + 1;
	if (tbl->AtomicPartIdx.find(partId,atomH,found) || !found ||atomH == 0){
            fprintf(stderr, "ERROR: Unable to find atomic part %d\n", partId);
	    cerr << __FILE__ << ", line " << __LINE__
		 << ", not an AtomicPart" << endl;
	    exit(1);
	}

        if (debugMode) {
            printf("    In Query1, partId = %d:\n", partId);
        }
	// process part by calling the null procedure
        atomH->DoNothing();
    }
    return Query1RepeatCnt;
}
