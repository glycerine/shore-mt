/*$Header: /p/shore/shore_cvs/src/oo7/Query8.C,v 1.8 1997/10/24 14:44:27 solomon Exp $*/
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
#include <string.h>
#include <stdio.h>

#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "BenchParams.h"
#include "VarParams.h"
//#include "IntBtree.h"

//////////////////////////////////////////////////////////////////
//
// Query #8 --- value join between Documents and AtomicParts
// on Document.id and AtomicPart.docId.  This is a key - foreign
// key join.  Algorithm is nested loops with index on document id field
//
// The main drawback is that we have no semantics whatsoever for
// this query.
//
//////////////////////////////////////////////////////////////////

extern void JoinDoNothing(int x, int y);


//////////////////////////////////////////////////////////////////
//
// Routine to do Query #8
//
//////////////////////////////////////////////////////////////////


int query8()
{
    REF(AtomicPart) ap;
    REF(Document) doc;
    int	apId;

    int count = 0;
    PoolScan AtomScan(AtomicPartSet);
    // should check error return ...
    while ( AtomScan.next((REF(any)&)ap,true) == 0)
    {
	apId = ap->id;
	bool found;
    	if (!tbl->DocumentIdIdx.find(apId,doc,found) && found && doc != NULL)
	{
		// found a matching pair of tuples
                if (debugMode) 
        	        printf("In Query8, atomic part %d matches doc %d.\n",
			            ap->id, doc->id);

	        JoinDoNothing(ap->id, doc->id);
	        count++;
    	}
    }
    return count;
}
