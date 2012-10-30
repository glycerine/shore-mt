#ifdef PARSETS

#include <Slave.h>
#include <assert.h>
#include "stats.h"


#define CHECK_NULL(x)			\
if((x) == NULL){			\
    cerr << #x << " == NULL" << endl;	\
	cerr << "The database for this configuration is not present." << endl;\
    assert(0);				\
}

#ifndef NEWCOMMUNICATION
#define myNode()  mynode()
#endif

void slaveGenInit(char *args)
{
	// ProcessStats    totalTime;
	// ServerStats     totalSrvTime;
        // char*		purgeVar;
        char *vashost;


	// Parset check this 
	vashost = getenv(VASHOST);
	if(vashost == NULL)
	    vashost = DEF_VASHOST;


	if(oc.init(vashost)){
	    cerr << "can't initlialize object cache" << endl;
	    return ;
	}

	if (oc.begin_transaction(3)){
	    cerr << "SetParams: can't begin transaction" << endl;
	    return ;
	}

	SetSlaveParams(args);

//	printf("[%d] Commiting transaction \n", myNode());

       	if(oc.commit_transaction()){
	    cerr << "SetParams: can't commit transaction" << endl;
	    return ;
	}

//	printf("[%d] DONE setting params\n", myNode());
	return ;
}

// This call is going to change what it does based on whether we are creating the
// database or if we are going to 

void slaveInitGlobals(void)
{
	w_rc_t rc;

	rc = Shore::begin_transaction(3);
	if(rc){
	    cerr << "slaveInitGlobals: can't begin transaction: "
		 << rc << endl;
	    return;
	}

	printf("[%d] Initializing global variables\n");

	// BUGBUG
	// Change this call to create local pools, names of the local pools
	// is the pool name appended with the slave number, 
	rc = InitGlobals();
	if(rc){
	    cerr << "slaveInitGlobals: error in InitGlobals: " << rc
		 << endl;
	    return;
	}

       	rc = Shore::commit_transaction();
	if(rc){
	    cerr << "slaveInitGlobals: can't commit transaction: "
		 << rc << endl;
	    return ;
	}

	printf("[%d] Done initializing global variables\n");	
		
	return ;
}

void slaveBeginTransaction(void)
{

//	printf("[%d] slaveBeginTransaction\n", myNode());
//	cout << endl;

	if (oc.begin_transaction(3)){
	    cerr << "Begin: cannot begin transaction" << endl;
	    return ;
	}

//	printf("[%d]", myNode());
//	cout << "Out of Begin Transaction\n" << endl;

	return ;

}
#include <stdlib.h> 
extern double ComputeWallClockTime(struct timeval *startWallTime, 
	           struct timeval *endWallTime);
void slaveCommitTransaction(void)
{
  struct timeval startTime;
  struct timeval endTime;

//	printf("[%d] slaveCommitTransaction\n", myNode());
//	cout << endl;

	gettimeofday(&startTime, IGNOREZONE &ignoreZone);
      	if(oc.commit_transaction()){
	    cerr << "slaveCommitTransaction: can't commit transaction" << endl;
	    return ;
	}
	gettimeofday(&endTime, IGNOREZONE &ignoreZone);
//	cout << "[" << myNode() << "]";
//	cout << " Elapsed Time for Remote Commit at Remote Site "<< ComputeWallClockTime(&startTime, &endTime) << endl; 

//	printf("[%d]", myNode());
//	cout << "Out of Commit Transaction\n" << endl;

	return ;
}

int declusterCompositeParts(char * cpId)
{
  int i;
	// round robin decluster the composite parts

  // CAUTION THIS WILL WORK ONLY WITH MASTER AND NOT AT SLAVES??
  // BUGBUG
  

	i = ((*(int *)cpId) % NumNodes)+1;
	return i;

}

LOID  createCompositePart(char *poolName, char *id)
{
        REF(CompositePart) cp;
	LOID    cpOid;

//	cout << " In createCompositePart \n" << "ID=" << *(int *)id<< endl;

//	oc.begin_transaction(3);
	if (id == NULL){
	  cout << " Grave error , Id is null" << endl << flush;
	  exit(-4);
	}


	cp = new (CompPartSet) CompositePart;
	cp.update()->init((int)*((int *) id));

//	cout << " Get OID " << cpOid <<endl;
	cpOid = cp.get_loid(true);
//	cout << " While Creating " << cpOid <<endl;
//	cout << " While Creating Non Primary " << cp.get_loid() << endl;

//	oc.commit_transaction();
//	cout << "out of create compositepart" << endl;
	return cpOid;
 }

int RegisterFunctions(FunctionBank& fnBank)
{
  fnBank.RegisterFunction((fnPtrType) slaveGenInit);
  fnBank.RegisterFunction((fnPtrType) slaveInitGlobals);
  fnBank.RegisterFunction((fnPtrType) slaveBeginTransaction);
  fnBank.RegisterFunction((fnPtrType) slaveCommitTransaction);
  fnBank.RegisterFunction((fnPtrType) createCompositePart);
  fnBank.RegisterFunction((fnPtrType) declusterCompositeParts);
  fnBank.RegisterFunction((fnPtrType) slaveCreatePools);
  fnBank.RegisterFunction((fnPtrType) slaveOpenPools);
  fnBank.RegisterFunction((fnPtrType) traverse1);
  fnBank.RegisterFunction((fnPtrType) traverse1ww);
  fnBank.RegisterFunction((fnPtrType) traverse2a);
  fnBank.RegisterFunction((fnPtrType) traverse2b);
  fnBank.RegisterFunction((fnPtrType) traverse2c);
  fnBank.RegisterFunction((fnPtrType) traverse3a);
  fnBank.RegisterFunction((fnPtrType) traverse3b);
  fnBank.RegisterFunction((fnPtrType) traverse3c);
  fnBank.RegisterFunction((fnPtrType) traverse4);
  fnBank.RegisterFunction((fnPtrType) traverse5do);
  fnBank.RegisterFunction((fnPtrType) traverse5undo);
  fnBank.RegisterFunction((fnPtrType) traverse6);

 return 1;
}

void slaveCreatePools(void)
{
    char name[100];

    cerr << "Creating pools on node " << myNode() << endl;
    oc.begin_transaction(3);
    sprintf(name, "CompPartSet%d", myNode());
    cerr << "Creating Pool= " << name << "  On Node= " << myNode() << endl;
    REF(Pool)::create(name, 0644, CompPartSet);

    sprintf(name, "AtomicPartSet%d", myNode());
    cerr << "Creating Pool= " << name << "  On Node= " << myNode() << endl;
    REF(Pool)::create(name, 0644, AtomicPartSet);

    sprintf(name, "DocumentSet%d", myNode());
    cerr << "Creating Pool= " << name << "  On Node= " << myNode() << endl;
    REF(Pool)::create(name, 0644, DocumentSet);

    sprintf(name, "assocFile%d", myNode());
    cerr << "Creating Pool= " << name << "  On Node= " << myNode() << endl;
    REF(Pool)::create(name, 0644, assocFile);


    // sprintf(name, "ManualSet%d", myNode());
    // cerr << "Creating Pool= " << name << "  On Node= " << myNode() << endl;
    // REF(Pool)::create(name, 0644, ManualSet);

    // sprintf(name, "ModuleSet%d", myNode());
    // cerr << "Creating Pool= " << name << "  On Node= " << myNode() << endl;
    // REF(Pool)::create(name, 0644, ModuleSet);

    sprintf(name, "DummySetCol%d", myNode());
    cerr << "Creating Pool= " << name << "  On Node= " << myNode() << endl;
    REF(Pool)::create(name, 0644, DummySetCol);

    // create all the indices needed

    sprintf(name, "AtomicPartIdx%d", myNode());
    cerr << "Creating Index= " << name << "  On Node= " << myNode() << endl;
    AtomicPartIdx = new (name, 0644) IntBtIndex;
    AtomicPartIdx.update()->init();


    sprintf(name, "CompPartIdx%d", myNode());
    cerr << "Creating Index= " << name << "  On Node= " << myNode() << endl;
    CompPartIdx = new (name, 0644) IntBtIndex;
    CompPartIdx.update()->init();


    sprintf(name, "DocumentIdx%d", myNode());
    cerr << "Creating Index= " << name << "  On Node= " << myNode() << endl;
    DocumentIdx = new (name, 0644) StringBtIndex;
    DocumentIdx.update()->init();


    sprintf(name, "DocumentIdIdx%d", myNode());
    cerr << "Creating Index= " << name << "  On Node= " << myNode() << endl;
    DocumentIdIdx = new (name, 0644) IntBtIndex;
    DocumentIdIdx.update()->init();

//    sprintf(name, "ModuleIdx%d", myNode());
//    cerr << "Creating Index= " << name << "  On Node= " << myNode() << endl;
//    ModuleIdx = new (name, 0644) StringBtIndex;
//    ModuleIdx.update()->init();

//    sprintf(name, "BaseAssemblyIdx%d", myNode());
//    cerr << "Creating Index= " << name << "  On Node= " << myNode() << endl;
//    BaseAssemblyIdx = new (name, 0644) IntBtIndex;
//    BaseAssemblyIdx.update()->init();

//    sprintf(name, "AllModules%d", myNode());
//    cerr << "Creating AllModules= " << name << "  On Node= " << myNode() << endl;
//    AllModules = new (name, 0644) Assoc;
//    AllModules.update()->init();

    oc.commit_transaction();
    return;
}

void slaveOpenPools(void)
{
    char name[100];

    cerr << "Opening pools on node " << myNode() << endl;

    oc.begin_transaction(3);
    sprintf(name, "CompPartSet%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;  
    CompPartSet     = REF(Pool)::lookup(name);

    sprintf(name, "AtomicPartSet%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;
    AtomicPartSet   = REF(Pool)::lookup(name);

    sprintf(name, "DocumentSet%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;
    DocumentSet       = REF(Pool)::lookup(name);

    sprintf(name, "assocFile%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;
    assocFile       = REF(Pool)::lookup(name);

//    sprintf(name, "ManualSet%d", myNode());
//    ManualSet       = REF(Pool)::lookup(name);
//    sprintf(name, "ModuleSet%d", myNode());
//    ModuleSet       = REF(Pool)::lookup(name);

    sprintf(name, "DummySetCol%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;
    DummySetCol     = REF(Pool)::lookup(name);

    CHECK_NULL(CompPartSet)
    CHECK_NULL(AtomicPartSet)
    CHECK_NULL(DocumentSet)
//    CHECK_NULL(ManualSet)
//    CHECK_NULL(ModuleSet)
    CHECK_NULL(DummySetCol)
    CHECK_NULL(assocFile)

    sprintf(name, "AtomicPartIdx%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;
    AtomicPartIdx   = REF(IntBtIndex)::   lookup(name);

    sprintf(name, "CompPartIdx%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;
    CompPartIdx     = REF(IntBtIndex)::   lookup(name);

    sprintf(name, "DocumentIdx%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;
    DocumentIdx     = REF(StringBtIndex)::lookup(name);

    sprintf(name, "DocumentIdIdx%d", myNode());
    cerr << "Searching Pool= " << name << "  On Node= " << myNode() << endl;
    DocumentIdIdx   = REF(IntBtIndex)::   lookup(name);

//    sprintf(name, "BaseAssemblyIdx%d", myNode());
//    BaseAssemblyIdx = REF(IntBtIndex)::   lookup(name);
//    sprintf(name, "ModuleIdx%d", myNode());
//    ModuleIdx       = REF(StringBtIndex)::lookup(name);
//    sprintf(name, "AllModules%d", myNode());
//    AllModules      = REF(Assoc)::        lookup(name);

    CHECK_NULL(AtomicPartIdx)
    CHECK_NULL(CompPartIdx)
    CHECK_NULL(DocumentIdx)
    CHECK_NULL(DocumentIdIdx)
//    CHECK_NULL(BaseAssemblyIdx)
//    CHECK_NULL(ModuleIdx)
//    CHECK_NULL(AllModules)
     oc.commit_transaction();
     return;
  }

#endif




