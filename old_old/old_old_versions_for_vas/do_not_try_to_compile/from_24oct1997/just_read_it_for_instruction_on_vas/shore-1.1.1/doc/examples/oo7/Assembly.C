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
// Assembly Methods
//
////////////////////////////////////////////////////////////////////////////

#include <libc.h>
#include <stdio.h>

#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "VarParams.h"
//#include "IntBtree.h"
//#include "StringBtree.h"
#include "baidlist.h"
#include "random.h"
#include "string.h"

extern char*	types[NumTypes];

extern BAIdList* private_cp;
extern BAIdList* shared_cp;

////////////////////////////////////////////////////////////////////////////
//
// ComplexAssembly Constructor
//
////////////////////////////////////////////////////////////////////////////

void
ComplexAssembly::init(long asId,
		      REF(Module) module,
		      REF(ComplexAssembly) parentAssembly,
		      long levelNo)
{
    // int partDate;
    long int i, nextId;
    REF(ComplexAssembly) subAssembly;   
    REF(BaseAssembly) baseAssembly;

    // MLM - 11/15/93
    oModule = 0;

    if (debugMode) {
        printf("ComplexAssembly::ComplexAssembly(asId = %d, levelNo = %d)\n",
                asId, levelNo);
    }

    // initialize the simple stuff
    id = asId;
    int typeNo = (int) (random() % NumTypes);
    strncpy(type, types[typeNo], (unsigned int)TypeSize);
    buildDate = MinAssmDate + (int) (random() % (MaxAssmDate-MinAssmDate+1));
    superAssembly = parentAssembly;
    assmType = Complex;

    // recursively create subassemblies for this complex assembly
    for (i = 0; i < NumAssmPerAssm; i++) 
    {
        if (levelNo < NumAssmLevels-1) 
	{
	    // create a complex assembly as the subassembly
            nextId = nextComplexAssemblyId++;
	    subAssembly = new (ModuleSet) ComplexAssembly;
	    subAssembly.update()->init(nextId,module,this,levelNo+1);

	    // subAssemblies.update()->add(subAssembly);
	    subAssemblies.add((REF(Assembly) &)subAssembly);
        } 
	else 
	{
	    // create a base assembly as the subassembly
	    // and insert its id into a hash index
            nextId = nextBaseAssemblyId++;
	    // baseAssembly = new (ModuleSet) BaseAssembly (nextId, module, this);
	    // baseAssembly = baseAssembly.new_persistent(ModuleSet);
	    baseAssembly = new (ModuleSet) BaseAssembly ;
	    baseAssembly.update()->init(nextId,module,this);
	    subAssemblies.add((REF(Assembly)&)baseAssembly);
	    // INDEX STUFF
	    W_IGNORE(tbl->BaseAssemblyIdx.insert(nextId, baseAssembly)); 
	    // add to index
        }
    }
}


////////////////////////////////////////////////////////////////////////////
//
// BaseAssembly Constructor
//
////////////////////////////////////////////////////////////////////////////

void
BaseAssembly::init(long asId,
		   REF(Module) 	module,
		   REF(ComplexAssembly) parentAssembly)
{
    REF(CompositePart)	cp;
    int 		compId;
    int 		i;
    int 		lowCompId;
    int 		compIdLimit;


    // MLM - 11/15/93
    oModule = 0;

    if (debugMode) {
        printf("BaseAssembly::BaseAssembly(asId = %d, levelNo = %d)\n",
                asId, NumAssmLevels);
    }
    // initialize the simple stuff
    id = asId;
    int typeNo = (int) (random() % NumTypes);
    strncpy(type, types[typeNo], (unsigned int)TypeSize);
    buildDate = MinAssmDate + (int) (random() % (MaxAssmDate-MinAssmDate+1));

    superAssembly = parentAssembly;
    superAssembly = parentAssembly;
    // componentsPriv = new (ModuleSet) Assoc(&ModuleSet);
    // componentsPriv = componentsPriv.new_persistent(ModuleSet);
    // componentsPriv = new (ModuleSet) Assoc;
    // componentsPriv.update()->init();

    // componentsShar = new (ModuleSet) Assoc(&ModuleSet);
    // componentsShar = componentsShar.new_persistent(ModuleSet);
    // componentsShar = new (ModuleSet) Assoc;
    // componentsShar.update()->init();

    assmType = Base;
    module.update()->allBases.add(this); // add the base assembly to the extent 
		// of all base assemblies in this module

    // get access to the design library containing composite parts
    lowCompId = (int)((module->id - 1) * NumCompPerModule + 1);
    compIdLimit = NumCompPerModule;

    // first select the private composite parts for this assembly
    for (i = 0; i < NumCompPerAssm; i++) 
    {
	compId = lowCompId + (int) (random() % compIdLimit);
	private_cp[compId].insert(this);  // keep track of which
		// composite parts this base assembly uses as private
    }

    // next select the shared composite parts for this assembly
    if (debugMode) { printf("}, Shared parts = { "); }
    for (i = 0; i < NumCompPerAssm; i++) 
    {
	compId = (int) (random() % TotalCompParts) + 1;
	shared_cp[compId].insert(this);  // keep track of which
		// composite parts this base assembly uses as shared
    }
}

//////////////////////////////////////////////////////////////////////////
// 
// DoNothing method for Assemblies 
// 
//////////////////////////////////////////////////////////////////////////

void Assembly::DoNothing () const
{
    if (id < 0) 
        printf("DoNothing: negative id.\n");
    if (debugMode) {
        printf("==> DoNothing(id = %d, type = %s, buildDate = %d)\n",
                               id, type, buildDate);
    }
}

// we don't yet have pure virual fct's...
AssemblyType
Assembly::mytype()  const { return Complex;}
// these used to be inline... (perhaps they should be put in a .h somewhere)
// but they are virtual. so just as well.
AssemblyType
BaseAssembly::mytype() const  { return Base;}
AssemblyType
ComplexAssembly::mytype() const  { return Complex;}
