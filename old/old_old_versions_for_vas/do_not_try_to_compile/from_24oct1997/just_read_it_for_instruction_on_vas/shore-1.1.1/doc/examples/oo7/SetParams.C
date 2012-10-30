/*$Header: /p/shore/shore_cvs/src/oo7/SetParams.C,v 1.10 1995/04/14 22:30:19 schuh Exp $*/
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

#include <stdio.h>
#include "GenParams.h"
#include "oo7.h"


int NumAtomicPerComp;
int NumConnPerAtomic;
int DocumentSize;

int NumAssmPerAssm;
int NumCompPerAssm;

int NumCompPerModule;
int NumAssmLevels;
int TotalModules;

int TotalCompParts;
int TotalAtomicParts;

int CachePages;
int ManualSize;

#ifdef MULTIUSER
int MultiSleepTime;
#endif
//////////////////////////////////////////////////////////////////
// Set generation parameters for the benchmark.
//
// Assumes a configuration file "OO7.config" of the
// following format:
//
// NumAssmPerAssm     	n1
// NumCompPerAssm     	n2
// NumCompPerModule   	n3
// NumAssmLevels      	n4
// TotalModules       	n5
// NumAtomicPerComp	n6
// NumConnPerAtomic 	n7
// DocumentSize		n8
// ManualSize           n9
//
// where n1 through n8 are integers.  The order of
// parameters is critical!  (This is a dumb parser
// after all.) 
//////////////////////////////////////////////////////////////////

#ifdef PARSETS
#include <Slave.h>
int SetParams(char* configFileName, SlaveArgs &slaveArgs) {

	FILE	*configFile;
	char	dirname[80];
	char	poolname[80];
	shrc rc;

	configFile = fopen(configFileName, "r");
	if (!configFile) {
	  fprintf(stderr, "Couldn't open config file: %s\n",configFileName);
	  return(-1);
	}

	// Shore parameters
	fscanf(configFile, "%*s %s\n", dirname);

	// we cheat a little here to make the initial directory;
	// by convention the dirs use by oo7 are subdirectories
	// of /oo7 .
	rc = Shore::chdir("/oo7");
	if (rc)
	{
		if (rc.err_num() != SH_NotFound)
			rc.fatal(); //give up
		SH_DO(Shore::mkdir("/oo7",0755));
		SH_DO(Shore::chdir("/oo7",0775));
	}

	// now try to make the subdir.
	rc = Shore::chdir(dirname);
	if (rc)
	{
		if (rc.err_num() != SH_NotFound)
			rc.fatal(); //give up
		SH_DO(Shore::mkdir(dirname,0755));
		SH_DO(Shore::chdir(dirname));
	}

	strcpy(slaveArgs.dirname, dirname);

	fscanf(configFile, "%*s %s\n", poolname);
	// poolname is no longer used.

	strcpy(slaveArgs.poolname, poolname);

	// Get parameters.
	fscanf(configFile, "%*s %d\n", &NumAssmPerAssm);
	printf("NumAssmPerAssm = %d.\n", NumAssmPerAssm); 
	slaveArgs.NumAssmPerAssm = NumAssmPerAssm;

	fscanf(configFile, "%*s %d\n", &NumCompPerAssm); 
	printf("NumCompPerAssm = %d.\n", NumCompPerAssm); 
	slaveArgs.NumCompPerAssm = NumCompPerAssm;

	fscanf(configFile, "%*s %d\n", &NumCompPerModule); 
	printf("NumCompPerModule = %d.\n", NumCompPerModule); 
	slaveArgs.NumCompPerModule = NumCompPerModule;

	fscanf(configFile, "%*s %d\n", &NumAssmLevels); 
	printf("NumAssmLevels = %d.\n", NumAssmLevels); 
	slaveArgs.NumAssmLevels = NumAssmLevels;

	fscanf(configFile, "%*s %d\n", &TotalModules); 
	printf("TotalModules = %d.\n", TotalModules); 
	slaveArgs.TotalModules = TotalModules;

	fscanf(configFile, "%*s %d\n", &NumAtomicPerComp); 
	printf("NumAtomicPerComp = %d.\n", NumAtomicPerComp); 
	slaveArgs.NumAtomicPerComp = NumAtomicPerComp;

	fscanf(configFile, "%*s %d\n", &NumConnPerAtomic); 
	printf("NumConnPerAtomic = %d.\n", NumConnPerAtomic); 
	slaveArgs.NumConnPerAtomic = NumConnPerAtomic;

	fscanf(configFile, "%*s %d\n", &DocumentSize); 
	printf("DocumentSize = %d.\n", DocumentSize); 
	slaveArgs.DocumentSize = DocumentSize;


	fscanf(configFile, "%*s %d\n", &ManualSize); 
	printf("ManualSize = %d.\n", ManualSize); 
	slaveArgs.ManualSize = ManualSize;

	TotalCompParts = NumCompPerModule * TotalModules;
	printf("Setting TotalCompParts to %d.\n", TotalCompParts);
	slaveArgs.TotalCompParts = TotalCompParts;

	TotalAtomicParts = TotalCompParts * NumAtomicPerComp;
	printf("Setting TotalAtomicParts to %d.\n", TotalAtomicParts);
	slaveArgs.TotalAtomicParts = TotalAtomicParts;

	fscanf(configFile, "%*s %d\n", &CachePages); 
	printf("Cache Pages = %d.\n", CachePages); 
	slaveArgs.CachePages = CachePages;


	slaveArgs.NumNodes = NumNodes;
	return(1);
}

int SetSlaveParams(char* args) {

	FILE	*configFile;
	struct SlaveArgs *slArgs;

	printf("In set slave params\n");
	slArgs = (SlaveArgs *)args;
	
	if(oc.chdir(slArgs->dirname)){
	    cerr << slArgs->dirname << ": can't change to directory" << endl;
	    exit(1);
	}


	// Get parameters.
	NumAssmPerAssm = slArgs->NumAssmPerAssm; 	
	printf("NumAssmPerAssm = %d.\n", NumAssmPerAssm); 

	NumCompPerAssm = slArgs->NumCompPerAssm;
	printf("NumCompPerAssm = %d.\n", NumCompPerAssm); 

	NumCompPerModule = slArgs->NumCompPerModule;
	printf("NumCompPerModule = %d.\n", NumCompPerModule); 

	NumAssmLevels = slArgs->NumAssmLevels;
	printf("NumAssmLevels = %d.\n", NumAssmLevels); 

	TotalModules = slArgs->TotalModules;
	printf("TotalModules = %d.\n", TotalModules); 

	NumAtomicPerComp = slArgs->NumAtomicPerComp;
	printf("NumAtomicPerComp = %d.\n", NumAtomicPerComp); 

	NumConnPerAtomic = slArgs->NumConnPerAtomic;
	printf("NumConnPerAtomic = %d.\n", NumConnPerAtomic); 

	DocumentSize = slArgs->DocumentSize;
	printf("DocumentSize = %d.\n", DocumentSize); 

	ManualSize = slArgs->ManualSize;
	printf("ManualSize = %d.\n", ManualSize); 

	TotalCompParts = slArgs->TotalCompParts;
	printf("Setting TotalCompParts to %d.\n", TotalCompParts);

	TotalAtomicParts = slArgs->TotalAtomicParts;
	printf("Setting TotalAtomicParts to %d.\n", TotalAtomicParts);

	CachePages = slArgs->CachePages;
	printf("Cache Pages = %d.\n", CachePages); 

	NumNodes = slArgs->NumNodes;
	printf("NumNodes = %d.\n", NumNodes); 
	return(1);
}

#else
int SetParams(char* configFileName) {

	FILE	*configFile;
	char	dirname[80];
	char	poolname[80];
	w_rc_t rc;

	configFile = fopen(configFileName, "r");
	if (!configFile) {
	  fprintf(stderr, "Couldn't open config file: %s\n",configFileName);
	  return(-1);
	}

	// we cheat a little here to make the initial directory;
	// by convention the dirs use by oo7 are subdirectories
	// of /oo7 .
	rc = Shore::chdir("/oo7");
	if (rc)
	{
		if (rc.err_num() != SH_NotFound)
			rc.fatal(); //give up
		SH_DO(Shore::mkdir("/oo7",0755));
		SH_DO(Shore::chdir("/oo7"));
	}

	// Shore parameters
	fscanf(configFile, "%*s %s\n", dirname);
	// now try to make the subdir.
	rc = Shore::chdir(dirname);
	if (rc)
	{
		if (rc.err_num() != SH_NotFound)
			rc.fatal(); //give up
		SH_DO(Shore::mkdir(dirname,0755));
		SH_DO(Shore::chdir(dirname));
	}

	fscanf(configFile, "%*s %s\n", poolname);
	// poolname is no longer used.

	// Get parameters.
	fscanf(configFile, "%*s %d\n", &NumAssmPerAssm);
	printf("NumAssmPerAssm = %d.\n", NumAssmPerAssm); 

	fscanf(configFile, "%*s %d\n", &NumCompPerAssm); 
	printf("NumCompPerAssm = %d.\n", NumCompPerAssm); 

	fscanf(configFile, "%*s %d\n", &NumCompPerModule); 
	printf("NumCompPerModule = %d.\n", NumCompPerModule); 

	fscanf(configFile, "%*s %d\n", &NumAssmLevels); 
	printf("NumAssmLevels = %d.\n", NumAssmLevels); 

	fscanf(configFile, "%*s %d\n", &TotalModules); 
	printf("TotalModules = %d.\n", TotalModules); 

	fscanf(configFile, "%*s %d\n", &NumAtomicPerComp); 
	printf("NumAtomicPerComp = %d.\n", NumAtomicPerComp); 

	fscanf(configFile, "%*s %d\n", &NumConnPerAtomic); 
	printf("NumConnPerAtomic = %d.\n", NumConnPerAtomic); 

	fscanf(configFile, "%*s %d\n", &DocumentSize); 
	printf("DocumentSize = %d.\n", DocumentSize); 

	fscanf(configFile, "%*s %d\n", &ManualSize); 
	printf("ManualSize = %d.\n", ManualSize); 
#ifdef MULTIUSER
	fscanf(configFile, "%*s %d\n", &MultiSleepTime); 
	printf("ManualSize = %d.\n", MultiSleepTime); 
#endif
	TotalCompParts = NumCompPerModule * TotalModules;
	printf("Setting TotalCompParts to %d.\n", TotalCompParts);

	TotalAtomicParts = TotalCompParts * NumAtomicPerComp;
	printf("Setting TotalAtomicParts to %d.\n", TotalAtomicParts);

	fscanf(configFile, "%*s %d\n", &CachePages); 
	printf("Cache Pages = %d.\n", CachePages); 


	return(1);
}
#endif
