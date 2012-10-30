/*$Header: /p/shore/shore_cvs/src/oo7/Query7.C,v 1.7 1997/10/24 14:44:26 solomon Exp $*/
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

//////////////////////////////////////////////////////////////////
//
// Query #7 --- iterate through all atomic parts.  Checks scan
// speed.  Some duplication with other queries, especially in
// systems that lack indices and force the other queries to be
// done by iterating through sets.
//
//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
//
// Routine to do Query #7
//
//////////////////////////////////////////////////////////////////


int query7()
{
    REF(AtomicPart) ap;
    int count = 0;
    PoolScan AtomScan(AtomicPartSet);
    // should check error codes here...
    while ( AtomScan.next((REF(any)&)ap,true)== 0)
    {
        if (debugMode) {
            printf("    In Query7, partId = %d:\n", ap->id);
        }
	ap->DoNothing();
        count++;
    }
    return count;
}
