/*$Header: /p/shore/shore_cvs/src/oo7/GenDB.C,v 1.35 1996/07/24 22:50:59 schuh Exp $*/
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
#include "getrusage.h"

//#include <ShoreApp.h>

//#include <E/collection.h>

/* new */
#include <stdlib.h> 

#ifdef __CCE__
#pragma expand
#endif
#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "VarParams.h"
// #include "IntBtree.h"
// #include "StringBtree.h"
#include "stats.h"
// #include "stats_server.h"
#include "baidlist.h"

int NoLogging = 1;
// this should be a command line flag.
// this should also be integerated into the E runtime.
// well maybe.

//////////////////////////////////////////////////////////////////
//
// Global Variables, etc., for Benchmark Database Generation
//
//////////////////////////////////////////////////////////////////



#ifdef PARSETS
#include <TSet.h>
#include <ParSets.h>
#include <Slave.h>

PrimaryParSet  <REF(CompositePart)>  *compositeParSet;
TSet <int>      CompNodes;
TSet <int>      ZeroNode;
TSet <int>      OneNode;
SlaveArgs       slArgs;
int             NumNodes;
LOID  objtype;
#else
extern SetParams(char* configFileName);

#endif

extern double ComputeWallClockTime(struct timeval *startWallTime, 
				   struct timeval *endWallTime);

extern double ComputeUserTime(struct rusage *startUsage,
			      struct rusage *endUsage);

extern double ComputeSystemTime(struct rusage *startUsage, 
				struct rusage *endUsage);




extern void createDummy(int);

// data structures to keep track of which composite parts each base
// assemblies uses.  This is done so that we can generate the base
// assemblies first and then the composite parts.  Why bother?
// In particular, if you did the composite parts first and the base
// assemblies, there would be no need for these data structures.
// This second approach worked fine for the small and medium databases
// but not for the large as the program seeked all over the disk.

// BAIdList* private_cp;
// BAIdList* shared_cp;
// these are declare in globals.[Ch] now.

//////////////////////////////////////////////////////////////////
//
// Do Benchmark Data Generation
//
//////////////////////////////////////////////////////////////////

w_rc_t gendb()
{
    // int 		partDate;
    long int 		id /*, typeNo, docLen */ ;
    char 		moduleName[40];
    REF(Module)		moduleH;
    REF(CompositePart)	cp;
	//reinitialize these globals.
	nextAtomicId=1;
	nextCompositeId=1;
	nextComplexAssemblyId=1;
	nextBaseAssemblyId=1;
	nextModuleId=1;


#ifdef PARSETS
#define MAXCOMPIDS  50 
#define HALFCOMPIDS 25

    int  *idList= new int [MAXCOMPIDS * NumNodes];
    LOID *OIDList = new LOID[MAXCOMPIDS * NumNodes];
    int idCount;

    cout << "CREATING POOLS" << endl << flush;

    // create the sets (pools, for now)
    // W_DO(REF(Pool)::create("CompPartSet",   0644, CompPartSet));
    // W_DO(REF(Pool)::create("AtomicPartSet", 0644, AtomicPartSet));
    // W_DO(REF(Pool)::create("DocumentSet",   0644, DocumentSet));
    W_DO(REF(Pool)::create("ManualSet",     0644, ManualSet));
    W_DO(REF(Pool)::create("ModuleSet",     0644, ModuleSet));
    W_DO(REF(Pool)::create("assocFile",     0644, assocFile));
    // W_DO(REF(Pool)::create("DummySetCol",   0644, DummySetCol));

    ModuleIdx = new ("ModuleIdx", 0644) StringBtIndex;
    ModuleIdx.update()->init();

    // BaseAssemblyIdx = new (ModuleSet) IntBtIndex;
    // BaseAssemblyIdx = BaseAssemblyIdx.new_persistent(ModuleSet);
    BaseAssemblyIdx = new ("BaseAssemblyIdx", 0644) IntBtIndex;
    BaseAssemblyIdx.update()->init();

#else

    // create the sets (pools, for now)
    W_DO(REF(Pool)::create("CompPartSet",   0644, CompPartSet));
    W_DO(REF(Pool)::create("AtomicPartSet", 0644, AtomicPartSet));
    W_DO(REF(Pool)::create("DocumentSet",   0644, DocumentSet));
    W_DO(REF(Pool)::create("ManualSet",     0644, ManualSet));
    W_DO(REF(Pool)::create("ModuleSet",     0644, ModuleSet));
    W_DO(REF(Pool)::create("DummySetCol",   0644, DummySetCol));
    W_DO(REF(Pool)::create("assocFile",     0644, assocFile));

    // create all the indices needed
    tbl =	new("tbl",	0644) oo7Indices;
    tbl.update()->init();


#endif

    printf("TotalCompParts = %d\n",TotalCompParts);

    shared_cp = new BAIdList[TotalCompParts+1];
    private_cp = new BAIdList[TotalCompParts+1];

    // First generate the desired number of modules
    while (nextModuleId <= TotalModules) 
    {
	long int id = nextModuleId++;

#ifdef USE_MODULE_INDEX
	sprintf(moduleName, "Module %08d", id);
	// moduleH = new (ModuleSet) Module (id);
	// moduleH = moduleH.new_persistent(ModuleSet);
	moduleH = new (ModuleSet) Module ;
	moduleH.update()->init(id);

	// add module to index
	tbl->ModuleIdx.insert(moduleName, moduleH);
#else
	sprintf(moduleName, "Module%d", id);
	moduleH = new (moduleName, 0644) Module;
	moduleH.update()->init(id);
#endif
	tbl.update()->AllModules.add(moduleH); // add module to extent
	// E_CommitTransaction();
	// E_BeginTransaction();
	// if (NoLogging)
	// sm_SetLogLevel(LOG_SPACE, 0, 0);
      }

    printf("generated all the modules, now generating the comp parts\n");
    // now generate the composite parts

#ifdef PARSETS

    if(Shore::commit_transaction()){
	cerr << "gendb: can't commit transaction" << endl;
	exit(-4);
    }


    gettimeofday(&startTime, IGNOREZONE &ignoreZone);
    printf("Elapsed Time for Local Pool Creation= %f\n",
	   ComputeWallClockTime(&startWallTime, &startTime));

    SlaveRPC(CompNodes, (char *)slaveCreatePools, NULL, -1);	
    gettimeofday(&endTime, IGNOREZONE &ignoreZone);

    printf("Elapsed Time for Remote Pool Creation= %f\n",
	   ComputeWallClockTime(&startTime, &endTime));

    gettimeofday(&startTime, IGNOREZONE &ignoreZone);
#ifdef NEWCOMMUNICATION
    myParSetServer->CreateParSet("oo7db", "CompositePart", ParSet::kPrimary, 
		 objtype, (char *)createCompositePart, 4, CompNodes, 
		  ParSet::kUserDef, (char *)declusterCompositeParts);
#else
    CreateParSet("oo7db", "CompositePart", ParSet::kPrimary, 
		 objtype, 4, (char *)createCompositePart, CompNodes, 
		  ParSet::kUserDef, (char *)declusterCompositeParts);
#endif

    compositeParSet = new PrimaryParSet <REF(CompositePart)>("oo7db", "CompositePart");
    gettimeofday(&endTime, IGNOREZONE &ignoreZone);
	
    printf("Elapsed Time for Remote Parset Creation= %f\n",
	   ComputeWallClockTime(&startTime, &endTime));

    W_DO(Shore::begin_transaction(3));

    SlaveRPC(CompNodes, (char *)slaveBeginTransaction, NULL, -1);

    idCount = 0;
    while (nextCompositeId <= TotalCompParts)   {
        LOID  cpOid;

	id = nextCompositeId++;

	// cp = new (CompPartSet) CompositePart(id);
	// cp = cp.new_persistent(CompPartSet);

	// add composite part to its index
	// CompPartIdx.update()->insert(id, cp);
	// dts 3-11-93 AllCompParts is not used, so don't maintain it.
	// (used in Reorg code though, but that's not used currently)
	// AllCompParts.add(cp);  // add composite part to its extent 

	idList[idCount++] = id;
	if ((idCount % HALFCOMPIDS) == 0){
	    printf("progress : nextCompositeId=%d, idCount=%d maxcomp=%d\n", 
		   nextCompositeId, idCount, MAXCOMPIDS);

	    if (idCount == (MAXCOMPIDS * NumNodes)) {

	        printf("Commiting %d and MAXCOMP=%d\n", idCount, MAXCOMPIDS);

		gettimeofday(&startTime, IGNOREZONE &ignoreZone);
	        compositeParSet->New(10, (char *)idList, idCount, OIDList);
		gettimeofday(&endTime, IGNOREZONE &ignoreZone);
		printf("Elapsed Time for Batch New= %f\n",
		       ComputeWallClockTime(&startTime, &endTime));

		gettimeofday(&startTime, IGNOREZONE &ignoreZone);
		for (int i=0; i< idCount; i++){
		  cp = OIDList[i];
		  baseAssemblyAdd(idList[i], cp);
		}
		gettimeofday(&endTime, IGNOREZONE &ignoreZone);
		printf("Elapsed Time for Adding to BaseAssembly= ");
		printf("%f, Objects=%d\n",
		       ComputeWallClockTime(&startTime, &endTime), idCount);

		gettimeofday(&startTime, IGNOREZONE &ignoreZone);
		SlaveRPC(CompNodes, (char *)slaveCommitTransaction, NULL, -1);
		gettimeofday(&endTime, IGNOREZONE &ignoreZone);
		printf("Elapsed Time for Remote Commit= %f\n",
		       ComputeWallClockTime(&startTime, &endTime));

		gettimeofday(&startTime, IGNOREZONE &ignoreZone);

		if(chain_tx) {
		    W_DO(Shore::chain_transaction());
		} else {
		    W_DO(Shore::commit_transaction());
		}

		gettimeofday(&endTime, IGNOREZONE &ignoreZone);
		printf("Elapsed Time for Local Commit= %f\n",
		       ComputeWallClockTime(&startTime, &endTime));

		if(!chain_tx) {
		    W_DO(Shore::begin_transaction(3));
		}
		SlaveRPC(CompNodes, (char *)slaveBeginTransaction, NULL, -1);
		// if (NoLogging)
		// sm_SetLogLevel(LOG_SPACE, 0, 0);
		printf("intermediate commmit done\n");
	        idCount = 0;
	      }
	  }
      }

    if (idCount > 0){
     printf("Commiting LAST %d and MAXCOMP=%d\n", idCount, MAXCOMPIDS);
      gettimeofday(&startTime, IGNOREZONE &ignoreZone);
      compositeParSet->New(10, (char *)idList, idCount, OIDList);
      gettimeofday(&endTime, IGNOREZONE &ignoreZone);
      printf("Elapsed Time for Batch New= %f\n",
	       ComputeWallClockTime(&startTime, &endTime));

     gettimeofday(&startTime, IGNOREZONE &ignoreZone);
      for (int i=0; i< idCount; i++){
	cp = OIDList[i];
	baseAssemblyAdd(idList[i], cp);
      }

     gettimeofday(&endTime, IGNOREZONE &ignoreZone);
     printf("Elapsed Time for Adding to BaseAssemble= %f, Objects=%d\n",
	       ComputeWallClockTime(&startTime, &endTime), idCount);


	gettimeofday(&startTime, IGNOREZONE &ignoreZone);
	SlaveRPC(CompNodes, (char *)slaveCommitTransaction, NULL, -1);
	gettimeofday(&endTime, IGNOREZONE &ignoreZone);
	printf("Elapsed Time for Remote Commit= %f\n",
	       ComputeWallClockTime(&startTime, &endTime));

	gettimeofday(&startTime, IGNOREZONE &ignoreZone);
	if(chain_tx) {
	    W_DO(Shore::chain_transaction());
	} else {
	    W_DO(Shore::commit_transaction());
	}
	gettimeofday(&endTime, IGNOREZONE &ignoreZone);
	printf("Elapsed Time for Local Commit= %f\n",
	       ComputeWallClockTime(&startTime, &endTime));

	if(!chain_tx) {
	    W_DO(Shore::begin_transaction(3));
	}
	SlaveRPC(CompNodes, (char *)slaveBeginTransaction, NULL, -1);
   }

#else

    gettimeofday(&startTime, IGNOREZONE &ignoreZone);
    while (nextCompositeId <= TotalCompParts) 
    {
	id = nextCompositeId++;
	// cp = new (CompPartSet) CompositePart(id);
	// cp = cp.new_persistent(CompPartSet);
	cp = new (CompPartSet) CompositePart;
	cp.update()->init(id);

	W_COERCE(tbl->CompPartIdx.insert(id, cp));  // add composite part to its index
	// dts 3-11-93 AllCompParts is not used, so don't maintain it.
	// (used in Reorg code though, but that's not used currently)
	// AllCompParts.add(cp);  // add composite part to its extent 

	if ((nextCompositeId % 25)==0)
	{
	    printf("progress : nextCompositeId=%d\n", nextCompositeId);
	    if ( 0 && (nextCompositeId%50)==0)
	    {
	        gettimeofday(&endTime, IGNOREZONE &ignoreZone);
		printf("Elapsed Time for Local Creation = %f\n",
		       ComputeWallClockTime(&startTime, &endTime));

	        gettimeofday(&startTime, IGNOREZONE &ignoreZone);
		if(chain_tx) {
		    W_DO(Shore::chain_transaction());
		} else {
		    W_DO(Shore::commit_transaction());
		}
	        gettimeofday(&endTime, IGNOREZONE &ignoreZone);
		printf("Elapsed Time for Local Commit = %f\n",
		       ComputeWallClockTime(&startTime, &endTime));

	        gettimeofday(&startTime, IGNOREZONE &ignoreZone);
		if(!chain_tx) {
		    W_DO(Shore::begin_transaction(3));
		}
		// if (NoLogging)
		// sm_SetLogLevel(LOG_SPACE, 0, 0);
		printf("intermediate commmit done\n");
	    }
	}
    }

#endif

    // E_CommitTransaction();

    // Print out some useful information about what happened

    printf("=== DONE CREATING DATABASE, TOTALS WERE ===\n");
    printf("    # atomic parts\t\t%d\n", nextAtomicId-1);
    printf("    # composite parts\t\t%d\n", nextCompositeId-1);
    printf("    # complex assemblies\t%d\n", nextComplexAssemblyId-1);
    printf("    # base assemblies\t\t%d\n", nextBaseAssemblyId-1);
    printf("    # modules\t\t\t%d\n", nextModuleId-1);
    printf("    # overFlowCnt\t\t\t%d\n", overFlowCnt);

    //////////////////////////////////////////////////////////////////
    //
    // Shutdown DB
    //
    //////////////////////////////////////////////////////////////////

    // compute and report wall clock time
    gettimeofday(&endWallTime, IGNOREZONE &ignoreZone);
    printf("Wall-Clock time to generate database: %f seconds.\n", 
	   ComputeWallClockTime(&startWallTime, &endWallTime));

    getrusage(RUSAGE_SELF, &endUsage);
    fprintf(stdout, "(CPU: %f seconds.)\n", 
	    ComputeUserTime(&startUsage, &endUsage) +
	    ComputeSystemTime(&startUsage, &endUsage));

    return RCOK;
}

// for SDL, a dummy routing.
extern void GenGlobals();

void abort()
{
    cout << "in abort" << endl;
    exit(0);
}
#ifdef PARSETS
static char *usage1= "<configFileName> <nodes> [-debug yes/no]";
#else
static char *usage1= "<configFileName> [-debug yes/no]";
#endif

#include "oo7options.h"
#include "sdl_fct.h"
int GenDB(int argc,char **argv);
sdl_fct gen_fct("gendb",GenDB);
int GenDB(int argc,char **argv)
{
	// ProcessStats    totalTime;
	// ServerStats     totalSrvTime;
        // char*		purgeVar;

	char *vashost;
	w_rc_t rc;

#ifdef PARSETS

	NumNodes = 1;

	// TODO
	initParSets(argc, argv);
	if (argc < 3){
	      fprintf(stderr, "Usage: %s %s\n", argv[0], usage1);
	      exit(1);
	}

	sscanf(argv[2], "%d", &NumNodes);
	printf("NUMNODES = %d\n", NumNodes);

	for (int i = 0; i< NumNodes; i++)
	  CompNodes.Add(i+1);

#endif

	rc = initialize(argc, argv, usage1);
	if(rc) {
	    return 1;
	}

	rc = Shore::begin_transaction(3);
	if(rc){
	    cerr << "can't begin transaction:" << rc << endl;
	    return 1;
	}

#ifdef PARSETS
	SetParams(configfile, slArgs);
#else
	SetParams(configfile);
#endif

	if(chain_tx) {
	    rc = Shore::chain_transaction();
	} else {
	    rc = Shore::commit_transaction();
	}
	if(rc){
	    cerr << "SetParams: can't commit transaction:" << rc << endl;
	    return 1;
	}

#ifdef PARSETS
	SlaveRPC(CompNodes, (char *) slaveGenInit, (char *)&slArgs, sizeof(SlaveArgs));	
#endif

	// get wall clock time
	gettimeofday(&startWallTime, IGNOREZONE &ignoreZone);

	// get starting usage values.
	getrusage(RUSAGE_SELF, &startUsage);

	// totalTime.Start();
	// totalSrvTime.Start();
	// E_BeginTransaction();
	// if (NoLogging)
	    // sm_SetLogLevel(LOG_SPACE, 0, 0);


	if(!chain_tx) {
		rc = Shore::begin_transaction(3);
		if(rc){
			cerr << "can not begin transaction:" << rc << endl;
			exit(1);
		}
	}

	// The stuff that used to be in GenGlobals is now in gendb()
	// GenGlobals();
	// generate the database
	rc = gendb();
	if(rc){
	    cerr << "gendb failed: " << rc << endl;
	    exit(1);
	}

	rc = Shore::commit_transaction();
	if(rc){
	    cerr << "can not commit transaction:" << rc << endl;
	    exit(1);
	}

#ifdef PARSETS
	SlaveRPC(CompNodes, (char *)slaveCommitTransaction, NULL, -1);
	cleanupParSets();
#endif
	// totalTime.Stop();
	// totalSrvTime.Stop();
	// fprintf(stdout, "Total stats (client,server):\n");
	// totalTime.PrintStatsHeader(stdout);
	// totalTime.PrintStats(stdout, "TotalCli");
	// totalSrvTime.PrintStats(stdout, "TotalSrv");

	W_COERCE(Shore::exit());
}





