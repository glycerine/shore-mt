/*$Header: /p/shore/shore_cvs/src/oo7/globals.C,v 1.16 1995/07/17 15:50:34 nhall Exp $*/
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

#include "oo7.h"
/********************************************************/
#include "getrusage.h"
#include "GenParams.h"

#define oo7_DECLARE_GLOBALS 1
#include "globals.h"

REF(Pool) global_pool; // duh.

bool chain_tx = true;

char    *types[NumTypes] = {
		"type000", "type001", "type002", "type003", "type004",
		"type005", "type006", "type007", "type008", "type009"
	};

#define CHECK_NULL(x)			\
if((x) == NULL){			\
    cerr << #x << " == NULL" << endl;	\
	cerr << "The database for this configuration is not present." << endl;\
    assert(0);				\
}
#ifdef PARSETS
w_rc_t InitGlobals()
{
    W_DO(REF(Pool)::lookup("ManualSet", ManualSet));
    W_DO(REF(Pool)::lookup("ModuleSet", ModuleSet));

    CHECK_NULL(ManualSet)
    CHECK_NULL(ModuleSet)

    W_DO(REF(IntBtIndex)::   lookup("BaseAssemblyIdx", BaseAssemblyIdx));
    W_DO(REF(StringBtIndex)::lookup("ModuleIdx", ModuleIdx));

    CHECK_NULL(BaseAssemblyIdx)
    CHECK_NULL(ModuleIdx)

    return RCOK;
}


#else
w_rc_t InitGlobals()
{
    W_DO(REF(Pool)::lookup("CompPartSet", CompPartSet));
    W_DO(REF(Pool)::lookup("AtomicPartSet", AtomicPartSet));
    W_DO(REF(Pool)::lookup("DocumentSet", DocumentSet));
    W_DO(REF(Pool)::lookup("ManualSet", ManualSet));
    W_DO(REF(Pool)::lookup("ModuleSet", ModuleSet));
    W_DO(REF(Pool)::lookup("DummySetCol", DummySetCol));
    W_DO(REF(Pool)::lookup("assocFile", assocFile));

    CHECK_NULL(CompPartSet)
    CHECK_NULL(AtomicPartSet)
    CHECK_NULL(DocumentSet)
    CHECK_NULL(ManualSet)
    CHECK_NULL(ModuleSet)
    CHECK_NULL(DummySetCol)
    CHECK_NULL(assocFile)

    W_DO(REF(oo7Indices)::   lookup("tbl", tbl));

    CHECK_NULL(tbl)

    return RCOK;
}

#endif

void
oo7Indices::init()
// this shoul probably be someplace better, but I don't feel like making a
// new file
{
	W_COERCE(AtomicPartIdx.init(BTree));
	W_COERCE(CompPartIdx.init(BTree));
	W_COERCE(DocumentIdx.init(BTree));
	W_COERCE(DocumentIdIdx.init(BTree));
	W_COERCE(BaseAssemblyIdx.init(BTree));
	W_COERCE(ModuleIdx.init(BTree));
	W_COERCE(BuildDateIndex.init(BTree));
}
