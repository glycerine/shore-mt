/*$Header: /p/shore/shore_cvs/src/oo7/Traverse7.C,v 1.12 1995/07/17 15:50:33 nhall Exp $*/
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
#include "GenParams.h"
#include "VarParams.h"
#include "PartIdSet.h"
#include "globals.h"
#include "random.h"

/////////////////////////////////////////////////////////////////
//
// Traversal #7.  Pick a random atomic part; traverse from the 
// part to its containing composite part P.  From P, traverse
// backwards up to all base assemblies referencing P; for each
// such base assembly, traverse upward through the assembly
// hierarchy until the root is reached.  This traversal coupled
// with traversal #1 ensures
// that all pointers are bi-directional.
//
/////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//
// Routine to initiate Traversal #7
//
//////////////////////////////////////////////////////////////////


int traverse7()
{
    int 	partId;
    REF(AtomicPart) atomH;

    // get access to the design library containing atomic parts

    // prepare to select a random atomic part via the id index
    // now randomly select a part and traverse upwards from there

    // set random seed so "hot" runs are truly hot
    srandom(1);

    partId = (int) (random() % TotalAtomicParts) + 1;
    bool found;
    if (tbl->AtomicPartIdx.find(partId,atomH,found) || !found || atomH==0) {
	fprintf(stderr, "ERROR: Unable to find atomic part %d\n", partId);
	exit(1);
    }

    if (debugMode) {
        printf("  In Traverse7, starting at atomic part %d\n", partId);
    }
    return atomH->partOf->traverse7();
}



//////////////////////////////////////////////////////////////////
//
// CompositePart Method for Traversal #7
//
//////////////////////////////////////////////////////////////////


long CompositePart::traverse7() const
{
    REF(BaseAssembly) ba; 
    PartIdSet visitedBaseIds;
    PartIdSet visitedComplexIds;

    if (debugMode) {
        printf("\tCompositePart::traverse7(id = %d)\n", id);
    }

    // search up the design hierarchy (along the private path)
    int count = 0;

    // establish iterator of private base assemblies
    int i;
    for (i=0; i<usedInPriv.get_size(); i++)
    {
	ba = usedInPriv.get_elt(i);
        if (!visitedBaseIds.contains((int)(ba->id))) {
            visitedBaseIds.insert((int)(ba->id));
	    count += (int)(ba->traverse7(visitedComplexIds));
	}
    }
    return count;
}


//////////////////////////////////////////////////////////////////
//
// BaseAssembly Method for Traversal #7
//
//////////////////////////////////////////////////////////////////


long BaseAssembly::traverse7(PartIdSet& visitedComplexIds) const
{
    if (debugMode) {
        printf("\t\tBaseAssembly::traverse7(id = %d)\n", id);
    }

    // visit this assembly and move on up the design hierarchy
    int count = 1;

    DoNothing();
    if (!visitedComplexIds.contains((int)(superAssembly->id))) {
        count += (int)(superAssembly->traverse7(visitedComplexIds));
    }
    return count;
}


//////////////////////////////////////////////////////////////////
//
// ComplexAssembly Method for Traversal #7
//
//////////////////////////////////////////////////////////////////


long ComplexAssembly::traverse7(PartIdSet& visitedComplexIds) const
{
    if (debugMode) {
        printf("\t\t\tComplexAssembly::traverse7(id = %d)\n", id);
    }

    // process this assembly and move on up the design hierarchy
    int count = 1;
    visitedComplexIds.insert((int)id);
    DoNothing();
    if (superAssembly != NULL) {
	if (!visitedComplexIds.contains((int)(superAssembly->id))) {
            count += (int)(superAssembly->traverse7(visitedComplexIds));
        }
    }
    return count;
}

// we lack pure virtuals...
long Assembly::traverse7(PartIdSet&) const
{ return 0;}

