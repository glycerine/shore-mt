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
//		ufstat [<size>] -l
// to print local stats before and after creating the unix file.
//		ufstat [<size>] -r
// to print remote stats before and after creating the unix file.
//		ufstat [<size>] -r -l
// to print both remote and local stats before and after creating 
//	the unix file.
//
//
*/

/* defaults: */
#define DEFAULT_OBJ_SIZE 1
bool	test_local=false;
bool	test_remote=false;

#include <stream.h>
#include <iostream.h>
#include <iomanip.h>
#include <string.h>
#include <ShoreApp.h>
#include <ShoreStats.h>
#include <sdl_UnixFile.h>

void
usage(ostream &os, char *progname)
{
    os << "Usage: " << progname 
	<< " [-l|-nol] [-r|-nor]" << endl;
    os << "-l/-nol control gathering of local(client) stats " << endl;
    os << "\tand printing of # voluntary context switches" << endl;  
    os << "-r/-nor control gathering of remote(server) stats " <<endl;
    os << "\tand printing of # log bytes generated" << endl;  
    os << "\t[size]" << endl;  
    exit(1);
}



int
main(int argc, char *argv[])
{
    shrc 	rc;
    char 	*fname = 0;
    char 	*buf = 0;
    {
	w_statistics_t	remotestats;
	w_statistics_t	localstats;
	w_statistics_t	*saved_local=0;
	w_statistics_t	*saved_remote=0;

	char 	*progname = argv[0];
	int 	objsize = DEFAULT_OBJ_SIZE;
	option_group_t *options;
	int 	i;

	if(argc >4 || argc < 1){
	    usage(cerr, progname);
	}
	if(argc>=1) {
	    for(int j = 1; j < argc; j++) {
		if(*argv[j] == '-') {
		    if(strcmp(argv[j],"-l")==0) {
			    test_local=true;

		    } else if(strcmp(argv[j],"-nol")==0) {
			    test_local=false;

		    } else if(strcmp(argv[j],"-r")==0) {
			    test_remote=true;

		    } else if(strcmp(argv[j],"-nor")==0) {
			    test_remote=false;
		    }
		} else {
		    objsize = atoi(argv[j]);
		    if(objsize<=0) {
			usage(cerr, progname);
		    }
		}
	    }
	}
	pid_t p = getpid();
	cerr << "Object size = " << objsize << endl;
	buf = new char[objsize + 50];
	ostrstream obuf(buf,objsize);

	obuf << "tmp" << p << "tmp" << p << ends;
	fname = buf;
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
		cout << "For module " << localstats.module(UNIX_ru_nvcsw) << endl;
		cout << ::form("\t%-30.30s %10.10d", 
			localstats.string(UNIX_ru_nvcsw),
			localstats.int_val(UNIX_ru_nvcsw)) << endl;
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
	    TxStatus x = Shore::get_txstatus();

	    SH_DO(Shore::chdir("/"));

	    //////////////////////////////////
	    // create a Unix file object
	    //////////////////////////////////
	    {
		REF(SdlUnixFile) o;
		o = new (fname, 0644) SdlUnixFile;
		if(!o) {
		    cerr << "Cannot create new SdlUnixFile." << endl;
		} else {
		    cerr << "Prior: length=" << o->UnixText.length() << endl;
		    o.update()->UnixText.memcpy(buf, objsize);
		    cerr << "After: length=" << o->UnixText.length() << endl;
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
		cout << "For module " << localstats.module(UNIX_ru_nvcsw) << endl;
		cout << ::form("\t%-30.30s %10.10d", 
			localstats.string(UNIX_ru_nvcsw),
			localstats.int_val(UNIX_ru_nvcsw)) << endl;

		// Now, print the differences: need writable version
		// of the current local

		w_statistics_t *cur = localstats.copy_brief();

		*cur -= *saved_local;

		cout << "DIFFS LOCAL:" << endl;
		cout << "For module " << cur->module(UNIX_ru_nvcsw) << endl;
		cout << ::form("\t%-30.30s %10.10d", 
			cur->string(UNIX_ru_nvcsw),
			cur->int_val(UNIX_ru_nvcsw)) << endl;
		delete cur;
	    }
	    if(saved_remote) delete saved_remote;
	    if(saved_local) delete saved_local;
	}

    }
	exit(1);

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

    if(buf) delete[] buf;
    return 0;
}
