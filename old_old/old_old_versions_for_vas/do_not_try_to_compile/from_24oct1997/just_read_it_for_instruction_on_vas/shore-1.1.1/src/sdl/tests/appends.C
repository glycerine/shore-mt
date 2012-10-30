/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
// This program demonstrates various ways to use the
// statistics.   It is a copy of the "uf" program to 
// create a Unix file object, with statistics gathered
// before and after the act of creating the file.
//
//
// do:
//		appends [<size>] 
//
*/
#define MODULE_CODE

/* defaults: */
#define DEFAULT_NCATS 1
// default is no stats -- for the purpose
// of automatic testing
bool	test_local=false;
bool	test_remote=false;

#pragma implementation

#include <stream.h>
#include <iostream.h>
#include <iomanip.h>
#include <string.h>
#include <ShoreConfig.h>
#include <ShoreApp.h>
#include <ShoreStats.h>
#include "appends.h"

void
usage(ostream &/*os*/, char */*progname*/)
{
    exit(1);
}



int
main(int argc, char *argv[])
{
    w_statistics_t	remotestats;
    w_statistics_t	localstats;
    w_statistics_t	*saved_local=0;
    w_statistics_t	*saved_remote=0;

    char 	*progname = argv[0];
    int 	ncats = DEFAULT_NCATS;
    shrc 	rc;

    if(argc >2 || argc < 1){
	usage(cerr, progname);
    }
    if(argc>=2) {
	ncats = atoi(argv[1]);
	if(ncats<=0) {
	    usage(cerr, progname);
	}
    }
    pid_t p = getpid();
    char big[8192];
    for (size_t k=0; k<sizeof(big); k++) {
	big[k]='b';
    }
    big[sizeof(big)]='\0';

    char buf[5];
    ostrstream obuf(buf,sizeof(buf));

    obuf << "tmp" << p << "tmp" << p << ends;
    char *fname = buf;
    //
    // Establish a connection with the vas and initialize 
	// the object  cache.
	//
    SH_DO(Shore::init(argc, argv));

    {
	//
	// get server and client stats
	//
	if(test_remote) {
	    SH_DO(Shore::gather_stats(remotestats,true));
	}
	if(test_local) {
	    SH_DO(Shore::gather_stats(localstats));
	}
	if(test_remote) {
	    cout << "INITIAL REMOTE:" << endl;
	    cout << "For module " 
		    << remotestats.module(SM_log_bytes_generated) << endl;
	    cout << ::form("\t%-30.30s %10.10d", 
		    remotestats.string(SM_log_bytes_generated),
		    remotestats.int_val(SM_log_bytes_generated)) << endl;
	}
	if (test_local) {
	    cout << "INITIAL LOCAL:" << endl;
	    cout << localstats << endl;
	}

	// save copies of stats for later diffing
	if(test_remote) {
	    saved_remote = remotestats.copy_brief();
	}
	if(test_local) {
	    saved_local = localstats.copy_brief();
	}
    }

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	// The main body of the transaction goes here.

	SH_DO(Shore::chdir("/"));

	//////////////////////////////////
	// create a Unix file object
	//////////////////////////////////
	{
	    REF(myobj) o;
	    SH_DO( REF(myobj)::new_persistent (fname, 0644, o) ) ;
	    if(!o) {
		cerr << "Cannot create new myobj." << endl;
	    } else {
		int j;
		for(j = 0; j < ncats; j++) {
		    o.update()->seq.append_elt(30);
		    o.update()->array[j] = j;
		}
		for(j = 0; j < 300 ; j++) {
		    o.update()->sarray[j].set("abcde");
		}
	    }
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    {
	// For remote stats we must gather again.
	if(test_remote) {
	    SH_DO(Shore::gather_stats(remotestats,true));

	    cout << "CURRENT REMOTE:" << endl;
		cout << "For module " << 
		    remotestats.module(SM_log_bytes_generated) << endl;
	    cout << ::form("\t%-30.30s %10.10d", 
		    remotestats.string(SM_log_bytes_generated),
		    remotestats.int_val(SM_log_bytes_generated)) << endl;

	    remotestats -= *saved_remote;

	    cout << "DIFFS REMOTE:" << endl;
		cout << "For module " << 
		    remotestats.module(SM_log_bytes_generated) << endl;
	    cout << ::form("\t%-30.30s %10.10d", 
		    remotestats.string(SM_log_bytes_generated),
		    remotestats.int_val(SM_log_bytes_generated)) << endl;
	}
	if(test_local) {
	    // Don't have to gather local stats again 
	    cout << "CURRENT LOCAL:" << endl;
	    cout << localstats << endl;

	}
	if(saved_remote) delete saved_remote;
	if(saved_local) delete saved_local;
    }

    // 
    // Another transaction to destroy the file...
    //
    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	SH_DO(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }
    return 0;
}
