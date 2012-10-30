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
// Query #6 - find all assemblies (base or complex) B that "use" 
// (directly or transitively ) a composite part with a more recent 
// build date than B's build date.
//
/////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// Recursive Workhorse Routine for Query #6
//
//////////////////////////////////////////////////////////////////


int checkMaxCompDate(REF(Assembly) assmH, int& count)
{
    REF(Assembly) subAssmH;
    REF(BaseAssembly) baseH;  
    REF(ComplexAssembly) compH;
    REF(CompositePart) cpH;
    int rsltDate;

    // check to see if this assembly uses a more recently dated composite
    // part, and process it if so - handling complex and base assemblies
    // differently since the former have subassemblies while the latter
    // directly use composite parts

    int maxSubDate = -1;

    if (assmH->assmType == Complex)
    {
        // complex assembly case - check its subassemblies recursively

        if (debugMode) {
            printf("In Query6:  complex assembly %d, buildDate = %d\n",
                    assmH->id, assmH->buildDate);
        }
	compH =  (REF(ComplexAssembly) &)assmH;
	int i;
	for (i=0; i< compH->subAssemblies.get_size(); i++)
	{
	    subAssmH = compH->subAssemblies.get_elt(i);
	    rsltDate = checkMaxCompDate(subAssmH, count);
	    if (rsltDate > maxSubDate) maxSubDate = rsltDate;
	}
    } 
    else 
    {
        // base assembly case - check its composite part build dates
        if (debugMode) {
            printf("In Query6:  base assembly %d, buildDate = %d\n",
                    assmH->id, assmH->buildDate);
            printf("    composite part dates = { ");
        }
	baseH =  (REF(BaseAssembly) &) assmH;
	int i;
	for (i=0; i< baseH->componentsPriv.get_size(); i++)
	{
	    cpH = baseH->componentsPriv.get_elt(i);
            if (debugMode) { printf("%d ",cpH->buildDate); }
	    if (cpH->buildDate > maxSubDate) maxSubDate = (int)cpH->buildDate;
        }
        if (debugMode) { printf("}\n"); }
    }

    // see if the maximum build date of composite parts used by this assembly
    // is greater than the assembly's build date, and process the assembly if
    // this is found to be the case;  also, return the maximum build date for
    // this subassembly

    if (maxSubDate > assmH->buildDate) {
        assmH->DoNothing();
	count++;
    }
    return(maxSubDate);
}


//////////////////////////////////////////////////////////////////
//
// Top-Level Routine to do Query #6
//
//////////////////////////////////////////////////////////////////


int query6()
{
    REF(Module) modH;  
    // scan modules, visiting all of the assemblies in each one, checking
    // to see if they use (either directly or indirectly) a more recently
    // dated composite part;  the visits are done by recursively traversing
    // the assemblies, checking them on the way back up (to avoid lots of
    // repeated work)

    int count = 0;
    int im;
    for (im=0; im< tbl->AllModules.get_size(); im++)
    {
	modH = tbl->AllModules.get_elt(im);
	int rslt = checkMaxCompDate((REF(Assembly) &)(modH->designRoot),
					count);
    }
    return count;
}
