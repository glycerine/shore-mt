#ifdef PARSETS
#ifndef slave_h
#define slave_h
#include <oo7.h>
#include <TSet.h>
#include <ParSet.h>
#include <ParSets.h>
#include <globals.h>

// Header file for slave parameters

typedef struct A{
        char   dirname[80];
	char   poolname[80];
	int    NumAssmPerAssm;
	int    NumCompPerAssm;
	int    NumCompPerModule;
	int    NumAssmLevels;
	int    TotalModules;
	int    NumAtomicPerComp;
	int    NumConnPerAtomic;
	int    DocumentSize;
	int    ManualSize;
	int    TotalCompParts;
	int    TotalAtomicParts;
	int    CachePages;
	int    NumNodes;
}SlaveArgs;


extern void slaveGenInit(char *args);
extern void slaveInitGlobals(void);
extern void slaveBeginTransaction(void);
extern void slaveCommitTransaction(void);
extern void baseAssemblyAdd(int id, REF(CompositePart) cp); 
extern int declusterCompositeParts(char *arg);
extern void slaveCreatePools(void);
extern void slaveOpenPools(void);


extern int SetParams(char* configFileName, SlaveArgs &slArgs);
extern int SetSlaveParams(char *args);
extern w_rc_t InitGlobals();

extern int mynode(void);
extern long traverse1(REF(CompositePart) cp);
extern long traverse1ww(REF(CompositePart) cp);
extern long traverse2a(REF(CompositePart) cp);
extern long traverse2b(REF(CompositePart) cp);
extern long traverse2c(REF(CompositePart) cp);
extern long traverse3a(REF(CompositePart) cp);
extern long traverse3b(REF(CompositePart) cp);
extern long traverse3c(REF(CompositePart) cp);
extern long traverse4(REF(CompositePart) cp);
extern long traverse5do(REF(CompositePart) cp);
extern long traverse5undo(REF(CompositePart) cp);
extern long traverse6(REF(CompositePart) cp);

extern LOID createCompositePart(char *poolName, char *id);
extern PrimaryParSet <REF(CompositePart)> *compositeParSet;
extern int NumNodes;
#endif
#endif




