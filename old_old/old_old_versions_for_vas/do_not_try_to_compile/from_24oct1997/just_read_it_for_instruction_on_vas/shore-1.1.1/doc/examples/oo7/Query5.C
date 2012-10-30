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
// Query #5 - find all base assemblies B that "use" a composite
// part with a more recent build date than B's build date.
//
/////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
//
// Routine to do Query #5
//
//////////////////////////////////////////////////////////////////


int query5()
{
    REF(Module) modH;  
    REF(BaseAssembly) baseH;
    REF(CompositePart) compH;

    // scan all modules, scanning all of the base assemblies in each one,
    // scanning all of the composite parts that they use, looking for base
    // assemblies that use more recently dated composite parts

    int count = 0;
    int im;
    for (im=0; im<tbl->AllModules.get_size(); im++)
    {
	modH = tbl->AllModules.get_elt(im);
	// for each module iterate over its base assemblies
	// this assumes that we maintain a collection of base asemblies
	// in each module
	int i;
	for (i=0; i<modH->allBases.get_size(); i++)
	{
	    baseH = modH->allBases.get_elt(i);
            if (debugMode) {
                printf("In Query5, processing base assembly %d, buildDate = %d\n",
		            baseH->id, baseH->buildDate);
            }
	    int j;
	    for (j=0; j<baseH->componentsPriv.get_size(); j++)
	    {
		compH = baseH->componentsPriv.get_elt(j);
                if (debugMode) {
                    printf("            [Checking composite part %d, buildDate = %d]\n",
		                        compH->id, compH->buildDate);
                }
		if (compH->buildDate > baseH->buildDate) {
	            baseH->DoNothing();
		    count++;
	        }
	    }
	}
    }
    return count;
}
