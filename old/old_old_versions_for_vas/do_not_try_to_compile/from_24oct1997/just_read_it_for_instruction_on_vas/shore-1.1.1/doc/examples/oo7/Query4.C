/*$Header: /p/shore/shore_cvs/src/oo7/Query4.C,v 1.12 1995/07/17 15:50:29 nhall Exp $*/
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
#include "random.h"

//////////////////////////////////////////////////////////////////
//
// Query #4 - randomly choose "Query4RepeatCnt" document objects by 
// lookup on their title field.  An index can be used for the
// actual lookup.  
//
/////////////////////////////////////////////////////////////////

static char	title[TitleSize];

//////////////////////////////////////////////////////////////////
//
// Routine to do Query #4
//
//////////////////////////////////////////////////////////////////


int  query4()
{
    REF(Document)	  docH;
    REF(BaseAssembly) ba;

    // now randomly select a document via its title index 
    // and trace a path to the base assemblies that use 
    // its associated composite part (and then
    // repeat this process the desired number of times)

    // set random seed so "hot" runs are truly hot
    srandom(1);

    int count = 0;
    for (int i = 0; i < Query4RepeatCnt; i++) 
    {
	// generate random document title and lookup document
	int compPartId = (int) (random() % TotalCompParts) + 1;
	bool found;
        sprintf(title, "Composite Part %08d", compPartId);
	if (tbl->DocumentIdx.find(title,docH,found) || !found  || docH == 0){
            fprintf(stderr, "ERROR: Unable to find document called \"%s\"\n",
	            title);
	    cerr << __FILE__ << ", line " << __LINE__
		 << ": not a Document" << endl;
	    exit(1);
	}

        if (debugMode) {
            printf("    In Query4, document title = \"%s\"\n", title);
        }

	// now walk back up the path(s) to the associated base assemblies
	// (based on private uses of the composite part, at least for now)
	int i;
	for (i= 0; i<docH->part->usedInPriv.get_size(); i++)
	{
	    ba = docH->part->usedInPriv.get_elt(i);
	    ba->DoNothing();
	    count++;
	}
    }
    return count;
}
