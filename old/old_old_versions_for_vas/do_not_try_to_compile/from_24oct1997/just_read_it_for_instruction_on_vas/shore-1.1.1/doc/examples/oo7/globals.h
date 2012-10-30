/*$Header: /p/shore/shore_cvs/src/oo7/globals.h,v 1.11 1996/07/23 19:33:29 nhall Exp $*/
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

#define CHAIN_TX chain_tx

// make a definition for extern keyword, so the REF's declared
// here  appear extern except in globals.C.
#ifdef oo7_DECLARE_GLOBALS
#define Extern
#else
#define Extern extern
#endif

// collection to hold composite parts, atomic parts, and connections
// extern persistent collection <dbvoid> CompPartSet;
Extern REF(Pool) CompPartSet;

// collection for atomic parts, no longer allocated from CompPartSet
Extern REF(Pool) AtomicPartSet;

// collection to hold documents
Extern REF(Pool) DocumentSet;

// collection to hold manuals
Extern REF(Pool) ManualSet;

// collection to hold modules and their assemblies
Extern REF(Pool) ModuleSet;

// index mapping atomic part ids to atomic parts
// extern persistent dbclass IntBtIndex*  AtomicPartIdx; 
// index mapping composite part ids to composite parts
// index mapping document titles to documents
// index mapping document ids to documents
// index mapping assembly ids to base assemblies
// index mapping module names to modules
Extern REF(oo7Indices) tbl;



// AllDocuments is an extent of all documents
// extern persistent dbclass Assoc  AllDocuments;

// AllCompParts is an extent of all composite parts
// extern persistent dbclass Assoc  AllCompParts;

// DummySet is an extent used to hold dummy objects
Extern REF(Pool) DummySetCol;
// extern persistent dbclass Assoc DummySet;


// pool for associations; really want clustering here but...
Extern REF(Pool) assocFile;

extern bool	chain_tx;


Extern int	nextAtomicId;
Extern int	nextCompositeId;
Extern int	nextComplexAssemblyId;
Extern int	nextBaseAssemblyId;
Extern int	nextModuleId;

Extern struct timeval startTime;
Extern struct timeval endTime;
Extern struct timezone ignoreZone;

#ifdef SOLARIS2
#define IGNOREZONE (void *)
#else 
#define IGNOREZONE 
#endif

Extern struct timeval startWallTime;
Extern struct timeval endWallTime;
Extern struct timezone ignoreTimeZone;
Extern struct timeval startWarmTime;

Extern struct rusage startUsage, endUsage;

Extern int overFlowCnt;

// here only to keep the make happy. used in gendb
Extern class BAIdList* private_cp;
Extern class BAIdList* shared_cp;




extern char    *types[];


// For Shore: environment variable to indicate host where vas is running
#define VASHOST		"VASHOST"
#define DEF_VASHOST	"localhost"
#define text Text
// attribute can't be named text in sdl, so redefine the name text as Text.
#undef Extern

#include "oo7options.h"
