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


////////////////////////////////////////////////////////////////////////////
//
// Module Methods
//
////////////////////////////////////////////////////////////////////////////

#include <libc.h>
#include <stdio.h>

#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "VarParams.h"
#include "random.h"
#include "string.h"

extern char*	types[NumTypes];

////////////////////////////////////////////////////////////////////////////
//
// Module Constructor
//
////////////////////////////////////////////////////////////////////////////

void
Module::init(long modId)
{
    w_rc_t rc;
    if (debugMode) {
        printf("Module::Module(modId = %d)\n", modId);
    }
        printf("Module::Module(modId = %d)\n", modId);

    // initialize the simple stuff
    id = modId;
    int typeNo = (int) (random() % NumTypes);
    strncpy(type, types[typeNo], (int)TypeSize);
    buildDate = MinModuleDate + 
	(int) (random() % (MaxModuleDate-MinModuleDate+1));
    // allBases = new (ModuleSet) Assoc(&ModuleSet);
    // allBases = allBases.new_persistent(ModuleSet);
    // allBases = new (ModuleSet) Assoc;
    // allBases.update()->init();

    // man = new (ManualSet) Manual(modId, this);
    rc = man.new_persistent(ManualSet, man);
    if(rc){
	cerr << "Failed to create ManualSet: " << rc << endl;
	exit(1);
    }
    man.update()->init(modId,this);

    // now create the assemblies for the module
    if (NumAssmLevels > 1) {
        int assmId = nextComplexAssemblyId++;
        // designRoot = new (ModuleSet) ComplexAssembly (assmId, this, NULL, 1);
	// designRoot = designRoot.new_persistent(ModuleSet);
        designRoot = new (ModuleSet) ComplexAssembly ;
	designRoot.update()->init(assmId,this,NULL,1);
    }
    // MLM - 11/15/93
    else
	designRoot = 0;
}

////////////////////////////////////////////////////////////////////////////
//
// Module Destructor
//
////////////////////////////////////////////////////////////////////////////

void
Module::Delete()
{
    if (debugMode) {
	printf("Module::~Module(id = %d)\n", id);
   }
   // remove module from extent of all modules
   tbl.update()->AllModules.del (this);
}

////////////////////////////////////////////////////////////////////////////
//
// Module scanManual method
//
////////////////////////////////////////////////////////////////////////////

long Module::scanManual() const
{
    return (man->searchText('I'));
}

////////////////////////////////////////////////////////////////////////////
//
// Module firstLast method
//
////////////////////////////////////////////////////////////////////////////

long Module::firstLast() const
{
    return (man->firstLast());
}

