/*$Header: /p/shore/shore_cvs/src/oo7/Bench.C,v 1.35 1996/07/24 22:50:56 schuh Exp $*/
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
#include <stdlib.h>
#include "getrusage.h"
#include "random.h"

#include "ShoreApp.h"

#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "VarParams.h"
#include "BenchParams.h"
//#include "IntBtree.h" 
//#include "StringBtree.h"
#include "stats.h"
//#include "stats_server.h"
#include "baidlist.h"

extern int RealWork; // set to one to make atomicpart::DoNothing
		// actually do some work
extern int WorkAmount;   // controls how much work DoNothings do

#ifdef PARSETS
#include <Slave.h>
#else
extern void SetParams(char* configFileName);
#endif

extern int  traverse7();
extern int  query1();
extern int  query2();
extern int  query3();
extern int  query4();
extern int  query5();
extern int  query6();
extern int  query7();
extern int  query8();
extern void insert1();
extern void delete1();
extern int  reorg1();
extern int  reorg2();


extern double ComputeWallClockTime(struct timeval *startWallTime, 
			           struct timeval *endWallTime);

extern double ComputeUserTime(struct rusage *startUsage, 
	        	      struct rusage *endUsage);

extern double ComputeSystemTime(struct rusage *startUsage, 
	 	                struct rusage *endUsage);



//////////////////////////////////////////////////////////////////
//
// Global Variables, etc., for Benchmarking ODB Operations
//
//////////////////////////////////////////////////////////////////




#ifdef PARSETS
PrimaryParSet  <REF(CompositePart)>  *compositeParSet;
TSet <int>  CompNodes;
SlaveArgs slArgs;
int NumNodes;
int declusterCompositeParts(char * cpId);
#endif

static char *usage1 = " <configFileName> <count> <op> <xacts> [-debug yes/no]"; 
static char *usage2 = 
"Currently supported <op>s: t1, t1ww, t2a, t2b, t2c, t3a, t3b, t3c\n\
\t t4, t5do, t5undo, t6, t7, t8, t9, q1, q2, q3, q4, q5, q6, q7, q8\n\
\t i, d, r1, r2 wu";
static char *usage3 = "count = number of times to repeat op.";
static char *usage4 =
"<xacts> = [single|one|many]\n\
	(all repetitions done in one or many distinct transactions)";

static option_t *count_opt=0;
static option_t *op_opt=0;
static option_t *xact_opt=0;

///////////////////////////////////////////////////////////////////////////
// ParseCommandLine parses the original shell call to "bench", determining 
// which operation to run, how many times to run it, and whether the
// individual runs are distinct transactions or are lumped together
// into one large transaction.
//////////////////////////////////////////////////////////////////////////
void ParseCommandLine(int argc, 
		      char**argv, 
		      int& opIndex,
		      int& repeatCount,
			  BenchmarkOp& whichOp,
			  bool& manyXACTS,
			  char **configfile
) 
{

	//
	// TODO: make it consistent re: command line overrides options
	// file
	//
	printf("Call was: ");
	for (int foo = 0; foo < argc; foo++) {
		printf("%s  ", argv[foo]);
	}
	printf("\n");

	if(argc>5) {
	    fprintf(stderr, "Usage: %s %s\n", argv[0], usage1);
	    fprintf(stderr, "%s\n", usage2);
	    fprintf(stderr, "%s\n", usage3);
	    fprintf(stderr, "%s\n", usage4);
	    exit(1);
	}


	opIndex = 3;

	// fix so we actually read command line parms.
	if (argc>= 4)
		xact_opt = 0;
	if  (argc >= 3)
		op_opt = 0;
	if  (argc >= 2)
		count_opt = 0;

	argc--;
	if (strcmp(argv[argc], "-d") == 0) {
		debugMode = 1;
		argc--;
	} else {
		if(debug_opt) {
			if(option_t::str_to_bool(debug_opt->value(),debugMode)) {
				debugMode = 0;
			}
		} else {
			debugMode = 0;
		}
	}

	if (strcmp(argv[4], "many") == 0) {
		manyXACTS = 1;
	} else {
		if(xact_opt) {
			if(option_t::str_to_bool(xact_opt->value(),manyXACTS)) {
				manyXACTS = 0;
			}
		} else {
			manyXACTS = 0;
		}
		manyXACTS = 0;
	}
	// ignore opIndex
	// 
	const char *op;
	if(op_opt) {
		if(op_opt->value()) {
			op =  op_opt->value();
		}
	} else {
		op = argv[opIndex];
	}
	if (strcmp(op, "t1") == 0) {
	    whichOp = Trav1;
	} else if (strcmp(op, "t1ww") == 0) {
	    whichOp = Trav1WW;
	    if (debugMode) {
			WorkAmount = atoi(argv[argc]);
		}else {
			WorkAmount = atoi(argv[argc]);
		}
		printf("WorkAmount = %d\n", WorkAmount);
	} else if (strcmp(op, "t2a") == 0) {
	    whichOp = Trav2a;
	} else if (strcmp(op, "t2b") == 0) {
	    whichOp = Trav2b;
	} else if (strcmp(op, "t2c") == 0) {
	    whichOp = Trav2c;
	} else if (strcmp(op, "t3a") == 0) {
	    whichOp = Trav3a;
	} else if (strcmp(op, "t3b") == 0) {
	    whichOp = Trav3b;
	} else if (strcmp(op, "t3c") == 0) {
	    whichOp = Trav3c;
	} else if (strcmp(op, "t4") == 0) {
	    whichOp = Trav4;
	} else if (strcmp(op, "t5do") == 0) {
	    whichOp = Trav5do;
	} else if (strcmp(op, "t5undo") == 0) {
	    whichOp = Trav5undo;
	} else if (strcmp(op, "t6") == 0) {
	    whichOp = Trav6;
	} else if (strcmp(op, "t7") == 0) {
	    whichOp = Trav7;
	} else if (strcmp(op, "t8") == 0) {
	    whichOp = Trav8;
	} else if (strcmp(op, "t9") == 0) {
	    whichOp = Trav9;
	} else if (strcmp(op, "t10") == 0) {
	    whichOp = Trav10;
	} else if (strcmp(op, "q1") == 0) {
	    whichOp = Query1;
	} else if (strcmp(op, "q2") == 0) {
	    whichOp = Query2;
	} else if (strcmp(op, "q3") == 0) {
	    whichOp = Query3;
	} else if (strcmp(op, "q4") == 0) {
	    whichOp = Query4;
	} else if (strcmp(op, "q5") == 0) {
	    whichOp = Query5;
	} else if (strcmp(op, "q6") == 0) {
	    whichOp = Query6;
	} else if (strcmp(argv[opIndex], "q7") == 0) {
	    whichOp = Query7;
	} else if (strcmp(argv[opIndex], "q8") == 0) {
	    whichOp = Query8;
	} else if (strcmp(argv[opIndex], "i") == 0) {
	    whichOp = Insert;
	} else if (strcmp(argv[opIndex], "d") == 0) {
	    whichOp = Delete;
	} else if (strcmp(argv[opIndex], "r1") == 0) {
	    whichOp = Reorg1;
	} else if (strcmp(argv[opIndex], "r2") == 0) {
	    whichOp = Reorg2;
// NEW
	} else if (strcmp(argv[opIndex], "wu") == 0) {
	    whichOp = WarmUpdate;
	} else {
	
	    fprintf(stderr, "ERROR: Illegal OO7 operation specified.\n");
		fprintf(stderr, "Usage: %s %s\n", argv[0], usage1);
	    exit(1);
	}
	// TODO: fix so option does not override command-line
	if(count_opt) {
		repeatCount = (int) strtol(count_opt->value(), 0, 0);
	} else {
		if ((repeatCount = atoi(argv[2])) <= 0) {
			fprintf(stderr, "Usage: %s %s", argv[0], usage1);
			exit(1);
		}
		argc--;
	} 

	if(repeatCount < 1) // or any other nastiness
	{
	    fprintf(stderr, "Bad value for count;\n", usage3);
	    fprintf(stderr, "%s\n", usage3);
	    exit(1);
	}

	if (argc < 2) {
		if(configfile_opt) {
			*configfile = (char *)configfile_opt->value();
		} else {
			 fprintf(stderr, "Usage: %s %s\n", argv[0], usage1);
			 exit(1);
		}
	} else {
		*configfile = argv[1];
	}
}                

extern w_rc_t InitGlobals(void);

#include "oo7options.h"

#include "sdl_fct.h"
int Bench(int argc,char **argv);
sdl_fct bfct("bench",Bench);
int Bench(int argc,char **argv)
{
	REF(Module)  moduleH;
	REF(any) r;
	char moduleName[40];
	// ProcessStats    totalTime;
	// ServerStats     totalSrvTime;
	//char*	purgeVar;
	char  resultText[200];  // buffer to hold result of operation for
                                // printing outside of timing region.
	char *configfile;
	int opIndex = 2;
	int repeatCount = 1;
	BenchmarkOp whichOp = Trav1;
	bool manyXACTS = 0;

	w_rc_t rc;

#ifdef PARSETS

	LOID objtype;

	int NumNodes;

	//reinitialize some globals.
	nextAtomicId=0; 
	nextCompositeId=0;
	nextComplexAssemblyId=0;
	nextBaseAssemblyId=0;
	nextModuleId = TotalModules;
	initParSets(argc, argv);

	if (argc < 6){
	    fprintf(stderr, "Usage: %s %s\n", argv[0], usage1);
	    fprintf(stderr, "%s\n", usage3);
	    fprintf(stderr, "%s\n", usage4);

	    exit(1);
	}

	sscanf(argv[5], "%d", &NumNodes);
	printf("NUMNODES = %d\n", NumNodes);

	for (int j=0; j< NumNodes; j++)
	  CompNodes.Add(j+1);

#endif

	rc = initialize(argc, argv, usage1);
	if(rc) {
	    return 1;
	}

	rc = Shore::begin_transaction(3);
	if(rc){
	    cerr << "can't begin transaction: " << rc << endl;
	    return 1;
	}

	// initialize parameters for benchmark.
	ParseCommandLine(argc, argv, opIndex, repeatCount, whichOp, manyXACTS,
		&configfile);

#ifdef PARSETS
	SetParams(argv[1], slArgs);
#else
	SetParams(configfile);
#endif
	rc = InitGlobals();
	if(rc){
	    cerr << "Error in InitGlobals: " << rc << endl;
	    exit(1);
	}

	nextAtomicId  = TotalAtomicParts + 1;
	nextCompositeId = TotalCompParts + 1;

	rc = Shore::commit_transaction();
	if(rc){
	    cerr << "can't commit transaction: " << rc << endl;
	    return 1;
	}

#ifdef PARSETS


	SlaveRPC(CompNodes, (char *) slaveGenInit, (char *)&slArgs, sizeof(SlaveArgs));	
	SlaveRPC(CompNodes, (char *)slaveOpenPools, NULL, -1);	

#ifdef NEWCOMMUNICATION
	myParSetServer->CreateParSet("oo7db", "CompositePart", ParSet::kPrimary, objtype,(char *)createCompositePart, 4, CompNodes, ParSet::kUserDef, 
		     (char *)declusterCompositeParts);
#else
	CreateParSet("oo7db", "CompositePart", ParSet::kPrimary, objtype,
		     4, (char *)createCompositePart, CompNodes, 
		     ParSet::kUserDef, (char *)declusterCompositeParts);

#endif

	compositeParSet = new PrimaryParSet <REF(CompositePart)>("oo7db", "CompositePart");
#endif

	// Compute structural info needed by the update operations,
        // since these operations need to know which id's should
        // be used next.

	int baseCnt = NumAssmPerAssm;
	int complexCnt = 1;	for (int i = 1; i < NumAssmLevels-1; i++) {
            baseCnt = baseCnt * NumAssmPerAssm;
            complexCnt += complexCnt * NumAssmPerAssm;
	}
	nextBaseAssemblyId = TotalModules*baseCnt + 1;
	nextComplexAssemblyId = TotalModules*complexCnt + 1;
	nextAtomicId = TotalAtomicParts + 1;
	nextCompositeId = TotalCompParts + 1;


	// needed for insert and delete tests
	shared_cp = new BAIdList[TotalCompParts+NumNewCompParts+1];
	private_cp = new BAIdList[TotalCompParts+NumNewCompParts+1];


	// See if debug mode is desired, see which operation to run,
	// and how many times to run it.



	// totalTime.Start();
	// totalSrvTime.Start();

	enum {do_commit, do_chain, do_nothing, do_begin } choice=do_begin;

        // Actually run the darn thing.
	for (int iter = 0; iter < repeatCount; iter++) 
	{
	    //////////////////////////////////////////////////////////////////
	    // Run an OO7 Benchmark Operation
	    //
	    //////////////////////////////////////////////////////////////////

	    printf("RUNNING OO7 BENCHMARK OPERATION %s, iteration = %d.\n", 
	           argv[opIndex], iter);

  	    // get wall clock time
            gettimeofday(&startWallTime, IGNOREZONE &ignoreTimeZone);

	    // get starting usage values.
	    getrusage(RUSAGE_SELF, &startUsage);

	    // Start a new transaction if either this is the first iteration
	    // of a multioperation transaction or we we are running each
	    // operate as a separate transaction

#ifdef PARSETS
	    if(choice == do_begin) {
		W_COERCE(Shore::begin_transaction(3));
		SlaveRPC(CompNodes, (char *)slaveBeginTransaction, NULL, -1);
	    }

#else
	    if(choice == do_begin) {
		// E_BeginTransaction();
		W_COERCE(Shore::begin_transaction(3));
	    }
#endif

            // set random seed so "hot" runs are truly hot
            srandom(1);

	    // Use random module for the operation
//            int moduleId = (int) (random() % TotalModules) + 1;
	for (int moduleId = 1 ; moduleId <= TotalModules; moduleId++){
			
#ifdef USE_MODULE_INDEX
            sprintf(moduleName, "Module %08d", moduleId);
//	    printf("moduleName=%s\n",moduleName);
	    moduleH =  tbl->ModuleIdx.find(moduleName);
#else
	    sprintf(moduleName,"Module%d", moduleId);
	    rc = REF(Module)::lookup(moduleName, moduleH);
	    if(rc){
		cerr << "Can't find module " << moduleName << ": "
		     << rc << endl;
		return 1;
	    }
#endif
	    printf("Traversing Module= %s\n", moduleName);
	    if (moduleH == NULL)
	    {
	        fprintf(stderr, "ERROR: Unable to access %s.\n", moduleName);
		// E_AbortTransaction();
		W_COERCE(Shore::abort_transaction());
	        exit(1);
	    }

	    // Perform the requested operation on the chosen module
	    long count = 0;
	    int docCount = 0;
	    int charCount = 0;
	    int replaceCount = 0;

	    switch (whichOp) {
	        case Trav1:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 1 DFS visited %d atomic parts.\n",
			             count);
  		    break;
		 case Trav1WW:
                    RealWork = 1;
                    whichOp = Trav1;  // so traverse methods don't complain
                    count = moduleH->traverse(whichOp);
                    whichOp = Trav1WW;  // for next (hot) traversal
                    sprintf(resultText, "Traversal 1WW DFS visited %d atomic parts.\n",
                                     count);
                    break;
	        case Trav2a:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 2A swapped %d pairs of (X,Y) coordinates.\n",
 			         count);
		    break;
	        case Trav2b:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 2B swapped %d pairs of (X,Y) coordinates.\n",
			             count);
		    break;
	        case Trav2c:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 2C swapped %d pairs of (X,Y) coordinates.\n",
			             count);
		    break;
	        case Trav3a:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 3A toggled %d dates.\n",
			             count);
		    break;
	        case Trav3b:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 3B toggled %d dates.\n",
			             count);
		    break;
	        case Trav3c:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 3C toggled %d dates.\n",
			            count);
		    break;
	        case Trav4:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 4: %d instances of the character found\n",
			             count);
		    break;
	        case Trav5do:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 5(DO): %d string replacements performed\n",
			             count);
		    break;
	        case Trav5undo:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 5(UNDO): %d string replacements performed\n",
			         count);
		    break;
	        case Trav6:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 6: visited %d atomic part roots.\n",
			             count);
		    break;
	        case Trav7:
	            count = traverse7();
		    sprintf(resultText, "Traversal 7: found %d assemblies using rand om atomic part.\n", 
			count);
		    break;
	        case Trav8:
	            count = moduleH->scanManual();
		    sprintf(resultText, "Traversal 8: found %d occurrences of character in manual.\n", 
			count);
		    break;
	        case Trav9:
	            count = moduleH->firstLast();
		    sprintf(resultText, "Traversal 9: match was %d.\n", 
			count);
		    break;

                case Trav10:
                    // run traversal #1 on every module.
                    count = 0;
                    whichOp = Trav1;  // so object methods don't complain
                    for (moduleId = 1; moduleId <= TotalModules; moduleId++) {
                        sprintf(moduleName, "Module %08d", moduleId);
			bool found;
	  	        shrc rc =tbl->ModuleIdx.find(moduleName,moduleH,found);
   	                if (rc || !found ||moduleH == NULL) {
                                fprintf(stderr,
                                        "ERROR: t10 Unable to access %s.\n",
                                         moduleName);
				W_COERCE(Shore::abort_transaction());
                                exit(1);
                        }
                        count += moduleH->traverse(whichOp);
                    }
                    sprintf(resultText,
                           "Traversal 10 visited %d atomic parts in %d modules.\\n",
                                     count, TotalModules);
                    whichOp = Trav10;  // for next time around
                    break;           

	        case Query1:
	            count = query1();
	            sprintf(resultText, "Query one retrieved %d atomic parts.\n",
			             count);
		    break;
	        case Query2:
	            count = query2();
	            sprintf(resultText, "Query two retrieved %d qualifying atomic parts.\n",
			         count);
		    break;
	        case Query3:
	            count = query3();
	            sprintf(resultText, "Query three retrieved %d qualifying atomic parts.\n",
			         count);
		    break;
	        case Query4:
	            count = query4();
	            sprintf(resultText, "Query four retrieved %d (document, base assembly) pairs.\n",
			         count);
		    break;
	        case Query5:
	            count = query5();
	            sprintf(resultText, "Query five retrieved %d out-of-date base assemblies.\n",
			             count);
		    break;
	        case Query6:
	            count = query6();
	            sprintf(resultText, "Query six retrieved %d out-of-date assemblies.\n",
			         count);
		    break;
	        case Query7:
	            count = query7();
		    sprintf(resultText, "Query seven iterated through %d atomic part s.\n",
			             count);
		    break;
	        case Query8:
	            count = query8();
		    sprintf(resultText, "Query eight found %d atomic part/document m atches.\n",
			 count);
		    break;
	        case Insert:
	            insert1();
	            sprintf(resultText, "Inserted %d composite parts (a total of %d atomic parts.)\n",
		      NumNewCompParts, NumNewCompParts*NumAtomicPerComp);
		    break;
	        case Delete:
	            delete1();
	            sprintf(resultText, "Deleted %d composite parts (a total of %d atomic parts.)\n",
	             NumNewCompParts, NumNewCompParts*NumAtomicPerComp);
		    break;

		 case Reorg1:
		     count = reorg1();
		     sprintf(resultText, "Reorg1 replaced %d atomic parts.\n", 
			count);
		     break;

	    	 case Reorg2:
		     count = reorg2();
		     sprintf(resultText, "Reorg2 replaced %d atomic parts.\n", 
			count);
		     break;
// NEW
	        case WarmUpdate:
		    // first do the t1 traversal to warm the cache
	            count = moduleH->traverse(Trav1);
		    // then call T2 to do the update
	            count = moduleH->traverse(Trav2a);
	            sprintf(resultText, 
			"Warm update swapped %d pairs of (X,Y) coordinates.\n",
 			         count);
		     break;
	        default:
	            fprintf(stderr, "Sorry, that operation isn't available yet.\n");
		    // E_AbortTransaction();
		    W_COERCE(Shore::abort_transaction());
	            exit(1);
	    }
		printf("Visited=%d\n", count);
	}
	{ 
#ifdef PARSETS

	    if ((iter == repeatCount-1) || manyXACTS){
		printf("Calling commit transaction\n");
		SlaveRPC(CompNodes, (char *)slaveCommitTransaction, NULL, -1);
		choice = do_commit;
	    }
#else
	    // Commit the current transaction if 
	    // we are running the last iteration 
	    // or running a multitransaction test and not chaining
	    // Chain the tx if we are chaining and not on
	    // the last iteration

	    if (iter == repeatCount-1) {
		choice=do_commit;
		// commit 
	    } else if(manyXACTS) {
		// not last iteration, multi tx test
		if(chain_tx) {
		    choice=do_chain;
		} else {
		    choice=do_commit;
		}
	    } else choice=do_nothing;
#endif
	    if(choice==do_commit) {
		//E_CommitTransaction();
		W_COERCE(Shore::commit_transaction());
		choice = do_begin;
	    } else if (choice==do_chain) {
		W_COERCE(Shore::chain_transaction());
		choice = do_nothing;
	    } 
	}

            // compute and report wall clock time
            gettimeofday(&endWallTime, IGNOREZONE &ignoreTimeZone);
	    printf("SHORE, operation= %s, iteration= %d, elapsedTime= %f seconds\n",
               argv[opIndex], iter,
               ComputeWallClockTime(&startWallTime, &endWallTime));
            if (iter == 1) startWarmTime = startWallTime;

            // Compute and report CPU time.
	    getrusage(RUSAGE_SELF, &endUsage);
            fprintf(stdout, resultText);
	    fprintf(stdout, "CPU time: %f seconds.\n", 
	                ComputeUserTime(&startUsage, &endUsage) +
			ComputeSystemTime(&startUsage, &endUsage));
	    fprintf(stdout, "(%f seconds user, %f seconds system.)\n", 
	                ComputeUserTime(&startUsage, &endUsage),
			ComputeSystemTime(&startUsage, &endUsage));

	    if ((repeatCount > 2) && (iter == repeatCount-2)) 
	    {
	       // compute average hot time for 2nd through n-1 th iterations
               printf("SHORE, operation=%s, average hot elapsedTime=%f seconds\n",
	       	  argv[opIndex], 
	          ComputeWallClockTime(&startWarmTime, &endWallTime)/(repeatCount-2)); 
	    }
	  }

	//////////////////////////////////////////////////////////////////
	//
	// Shutdown 
	//
	//////////////////////////////////////////////////////////////////

#ifdef PARSETS
	cleanupParSets();
#endif
	// totalTime.Stop();
	// totalSrvTime.Stop();
	// fprintf(stdout, "Total stats (client,server):\n");
	// totalTime.PrintStatsHeader(stdout);
	// totalTime.PrintStats(stdout, "TotalCli");
	// totalSrvTime.PrintStats(stdout, "TotalSrv");

	// Exit
	W_COERCE(Shore::exit());
	return(0);
}

