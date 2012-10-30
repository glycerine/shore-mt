/*$Header: /p/shore/shore_cvs/src/oo7/Insert.C,v 1.12 1995/07/17 15:50:22 nhall Exp $*/
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
// Insert - Create NumNewCompParts new composite parts (which
// means also creating a set of AtomicParts for each.)  Add
// a reference to each new composite part from a randomly chosen
// base assembly (a different random choice of base assembly for
// each new composite part.)
//
//////////////////////////////////////////////////////////////////

extern int	nextBaseAssemblyId;

//////////////////////////////////////////////////////////////////
//
// Routine to do Insert
//
//////////////////////////////////////////////////////////////////

void insert1()
{
    // now create the desired number of new composite parts, adding each
    // one as a (private) composite part that a randomly selected base
    // assembly now wishes to use

    long int 		compId;
    long int 		assmId;
    REF(CompositePart)	compH;
    REF(BaseAssembly)  	assmH;

    if (debugMode) {
	printf("In Insert, nextCompositeId = %d:\n", nextCompositeId);
	printf("In Insert, nextAtomicId = %d:\n", nextAtomicId);
	printf("In Insert, nextBaseAssemblyId = %d:\n", nextBaseAssemblyId);
    }

    for (int i = 0; i < NumNewCompParts; i++) {

	// add a new composite part to the design library
        if (debugMode) {
	    printf("In Insert, making composite part %d:\n", nextCompositeId);
	}
  printf("In Insert, making composite part %d:\n", nextCompositeId);
        compId = nextCompositeId++;
	// compH = new (CompPartSet) CompositePart(compId);
	// compH = compH.new_persistent(CompPartSet);
	compH = new (CompPartSet) CompositePart;
	compH.update()->init(compId);

	W_COERCE(tbl->CompPartIdx.insert(compId, compH));
	// add composite part to its index

	// randomly select a base assembly that should use it and figure
	// out which module it resides in
        int baseAssembliesPerModule = (nextBaseAssemblyId-1)/TotalModules;
	assmId = (int) (random() % (nextBaseAssemblyId-1)) + 1;

	// now locate the appropriate base assembly object within its module
	bool found;
	if( tbl->BaseAssemblyIdx.find(assmId,assmH,found) || !found  || assmH == 0)
	{
            fprintf(stderr, "ERROR: Can't find base assembly %d\n", assmId);
	    cerr << __FILE__ << ", line " << __LINE__
		 << ": not a BaseAssembly" << endl;
	    _exit(1);
	}

        // finally, add the newly created composite part as a privately used
        // member of the randomly selected base assembly

	// first add this assembly to the list of assemblies in which
	// this composite part is used as a private member
	compH.update()->usedInPriv.add(assmH);
	// then add the composite part compH to the list of private parts used
	// in this assembly
	assmH.update()->componentsPriv.add(compH);

        if (debugMode) {
	    printf("[just made it be used by base assembly %d]\n", assmId);
        }
    }
}

