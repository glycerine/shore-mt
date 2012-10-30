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
#include "BenchParams.h"
#include "GenParams.h"
#include "globals.h"
#include "PartIdSet.h"

//////////////////////////////////////////////////////////////////
//
// Traverse  - DFS traverse module.  Upon reaching a base assembly,
// visit all referenced composite parts.  At each composite part,
// take an action that depends on which traversal variant has been
// requested.
//
///////////////////////////////////////////////////////////////////
extern void PrintOp(BenchmarkOp op);

//////////////////////////////////////////////////////////////////
//
// Module Method for Traversal 
//
//////////////////////////////////////////////////////////////////


long Module::traverse(BenchmarkOp op) const
{
    if (debugMode) {
        printf("Module::traverse(id = %d, op = ", id);
	PrintOp(op);
	printf(")\n");
    }

    // now traverse the assembly hierarchy
    return designRoot->traverse(op);
}


//////////////////////////////////////////////////////////////////
//
// ComplexAssembly Method for Traversal
//
//////////////////////////////////////////////////////////////////


long ComplexAssembly::traverse(BenchmarkOp op) const
{
    REF(Assembly) assm;

    if (debugMode) {
        printf("\tComplexAssembly::traverse(id = %d, op = ", id);
	PrintOp(op);
	printf(")\n");
    }

    // traverse each of the assembly's subassemblies
    int count = 0;
    int i;
    for ( i = 0; i<subAssemblies.get_size(); i++)
    {
	assm = subAssemblies.get_elt(i);
	count += (int)(assm->traverse(op));
    }
    return count;
}


//////////////////////////////////////////////////////////////////
//
// BaseAssembly Method for Traversal 
//
//////////////////////////////////////////////////////////////////


long BaseAssembly::traverse(BenchmarkOp op) const
{
    REF(CompositePart) cp;

    if (debugMode) {
        printf("\t\tBaseAssembly::traverse(id = %d, op = ", id);
	PrintOp(op);
	printf(")\n");
    }

    int count = 0;
    // establish iterator of private composite parts
    int i = 0;
    for (i=0; i<componentsPriv.get_size(); i++)
    {
	cp = componentsPriv.get_elt(i);
        count += (int)(cp->traverse(op));
    }
    return count;
}
// again, we (temporarily) lack pure virtuals..
long Assembly::traverse(BenchmarkOp) const { return 0;};


//////////////////////////////////////////////////////////////////
//
// CompositePart Method for Traversal
//
//////////////////////////////////////////////////////////////////


long CompositePart::traverse(BenchmarkOp op) const
{
    if (debugMode) {
        printf("\t\t\tCompositePart::traverse(id = %d, op = ", id);
	PrintOp(op);
	printf(")\n");
    }
    // do DFS of the composite part's atomic part graph
    if ((op >= Trav1) && (op <= Trav3c)) {

        // do parameterized DFS of atomic part graph
        PartIdSet visitedIds;
        return rootPart->traverse(op, visitedIds);

    } else if (op == Trav4) {

        // search document text for a certain character
        return documentation->searchText('I');

    } else if (op == Trav5do) {

        // conditionally change initial part of document text
        return documentation.update()->replaceText("I am", "This is");

    } else if (op == Trav5undo) {

        // conditionally change back initial part of document text
        return documentation.update()->replaceText("This is", "I am");

    } else if (op == Trav6) {
	
	// visit the root part only (it knows how to handle this)
        PartIdSet visitedIds;
        return rootPart->traverse(op, visitedIds);

    } else {

	// composite parts don't respond to other traversals
	printf("*** CompositePart::PANIC -- illegal traversal!!! ***\n");
	return 0;
    }
}


//////////////////////////////////////////////////////////////////
//
// AtomicPart Method for Traversal
//
//////////////////////////////////////////////////////////////////


long AtomicPart::traverse(BenchmarkOp op, PartIdSet& visitedIds) const
{
    REF(AtomicPart) ap;
    REF(Connection) cn;

    if (debugMode) {
        printf("\t\t\tAtomicPart::traverse(id = %d, op = ", id);
	PrintOp(op);
	printf(")\n");
    }

    int i;
    int count=0; // was 1 in Version 1. Why???

    switch (op) {

	case Trav1:

	    // just examine the part
            count += 1;
            DoNothing();
	    break;

        case Trav2a:

	    // swap X and Y if first part
	    if (visitedIds.empty()) { 
                // swapXY(); 
		// update hack
		REF(AtomicPart)(this).update()->swapXY();
                count += 1;
            }
	    break;

        case Trav2b:

	    // swap X and Y
            // swapXY();
	    // update hack
	    REF(AtomicPart)(this).update()->swapXY();
            count += 1;
	    break;

        case Trav2c:

	    // swap X and Y repeatedly
            for (i = 0; i < UpdateRepeatCnt; i++) { 
               // swapXY(); 
	       REF(AtomicPart)(this).update()->swapXY();
               count += 1;
            }
	    break;

        case Trav3a:

	    // toggle date if first part
	    if (visitedIds.empty()) { 
                // toggleDate(); 
		// update hack
                REF(AtomicPart)(this).update()->toggleDate(); 
                count += 1;
            }
	    break;

        case Trav3b:

	    // toggle date
            //toggleDate();
            REF(AtomicPart)(this).update()->toggleDate(); 
            count += 1;
	    break;

        case Trav3c:

	    // toggle date repeatedly
            for (i = 0; i < UpdateRepeatCnt; i++) { 
               // toggleDate();
               REF(AtomicPart)(this).update()->toggleDate(); 
               count += 1;
            }
	    break;

	case Trav6:

	    // examine only the root part
            count += 1;
            DoNothing();
	    return count;

        default:

	    // atomic parts don't respond to other traversals
	    printf("*** AtomicPart::PANIC -- illegal traversal!!! ***\n");

    }
    // now, record the fact that we've visited this part
    visitedIds.insert((int)id);

    // finally, continue with a DFS of the atomic parts graph
    // establish iterator over connected parts
    for ( i = 0; i<to.get_size(); i++)
    {
	cn = to.get_elt(i);
	ap = cn->to;  // get the associated atomic part
        if (!visitedIds.contains((int)(ap->id))) {
	    count += (int)(ap->traverse(op, visitedIds));
        }
    }
    return count;
}
