/*$Header: /p/shore/shore_cvs/src/oo7/Delete.C,v 1.12 1995/07/17 15:50:19 nhall Exp $*/
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
//#include "IntBtree.h"
//#include "StringBtree.h"

//////////////////////////////////////////////////////////////////
//
// Delete - Delete NumNewCompParts randomly chosen composite parts. 
//
//////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////
//
// Routine to do Delete
//
//////////////////////////////////////////////////////////////////


void delete1()
{
    // now delete the desired number of new composite parts
    long int 	compId;
    REF(CompositePart) compH;
    //REF(CompositePart) compH2;
    REF(any) r1, r2;

    for (int i = 0; i < NumNewCompParts; i++) 
    {
        compId = nextCompositeId++;
        if (debugMode) {
	    printf("In Delete, deleting composite part %d\n", compId);
	}
	printf("In Delete, deleting composite part %d\n", compId);
	bool found;
	if(!tbl->CompPartIdx.find(compId,compH,found) && found && (compH != NULL)) 
	{
	    W_COERCE(tbl->CompPartIdx.remove(compId,compH));
	    compH.update()->Delete();
	    //compH.destroy();
	}
	else
            fprintf(stderr, "ERROR: Unable to locate composite part %d\n", compId);
    }
}

