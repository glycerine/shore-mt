/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
// $Header: /p/shore/shore_cvs/src/examples/unixfile/chm.C,v 1.8 1996/07/19 16:33:13 nhall Exp $
//
// Simple test to change the mode of a registered
// object, and to demonstrate how to cope with
// an permissions error after changing mode.
//
// to use:  
//		chm path mode1 mode2
//
// The program creates a registered object named
// 		path
// with the permissions given as mode1.
// Then it attempts to update the object,
// change the mode to mode2, and
// update the object again.  Depending on the values
// given for mode1 and mode2, you the program will
// catch errors at different places (or not at all).
// If an error is caught, the transaction is aborted.
//
// If the transaction commits, and you want to remove
// the object, use the program "remove", in this directory.
//
// This program deals with errors and aborting three
// ways.  The functions method1, method2, and method3
// demonstrate the 3 mechanisms.
*/

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
    << " filename initial_mode new_mode" << endl;
    exit(1);
}

void 
my_error_handler(shrc rc) 
{
	cerr << "ERROR IN DEREFERENCE  : " << rc << endl;
	cerr << "To debug this, put a breakpoint in my_error_handler()" <<endl;
}


/////////////////////////////////////////////
// error-handling method 1: explicitly
// check access bits after create and chmod
/////////////////////////////////////////////
void
method1(const char *fname,
	mode_t mode,
	mode_t newmode
)
{
    shrc 	rc;
	SH_BEGIN_TRANSACTION(rc);
	//
	// macro does a setjump, longjmp if error
	// during tx

	// If that failed, or if a transaction aborted...
	if(rc){
		/////////////////////////////////
		// You get here if you explicitly
		// abort, or if the abort came from
		// an SH_DO macro invocation.
		/////////////////////////////////
		cerr << "Aborted : " << rc << endl;
		return;
	} else {
		// The main body of the transaction goes here.
		TxStatus x = Shore::get_txstatus();

		/////////////////////////////////
		// create an object, cache it
		/////////////////////////////////
		char 	*buf = "t\0";
		REF(SdlUnixFile) o; 		// const reference

		/////////////////////////////////////////
		// If you get an error (e.g., already
		// exists) you have difficulty tracking
		// it down.
		// 		o = new (fname, mode) SdlUnixFile;
		// prefereable to use the static form:
		////////////////////////////////////////
		rc = REF(SdlUnixFile)::new_persistent(fname, mode, o);

		if(rc || !o) {
			cerr << "Cannot create new SdlUnixFile. " << endl;

			// just for the sake of showing how to
			// compare for certain errors ...
			// and to show how to use perror
			if(rc == RC(EEXIST) ) {
				cerr << "You must remove " << fname 
					<< " because " << Shore::perror(rc) << endl;
			}

			SH_ABORT_TRANSACTION(rc);

		} else {
			int 	errno;
			WREF(SdlUnixFile) wr(o);// non-const reference
									// constructing the reference
									// does NOT inspect the object

			////////////////////////////////////////
			// check access if we want to discover
			// whether it's readable before we do
			// anything with it
			////////////////////////////////////////
			SH_DO(Shore::access(fname, W_OK, errno));
			if(errno) {
				SH_ABORT_TRANSACTION(RC(errno));
			}

			wr->UnixText.strcat(buf);
			/////////////////////////////////
			// since we checked the access first,
			// we should not get an error in the strcat
			/////////////////////////////////
			assert( !(rc = Shore::rc()));

			/////////////////////////////////
			// chmod to 2nd mode
			/////////////////////////////////
			SH_DO(Shore::chmod(fname, newmode));

			////////////////////////////////////////
			// check access again
			////////////////////////////////////////
			SH_DO(Shore::access(fname, W_OK, errno));
			if(errno) {
				SH_ABORT_TRANSACTION(RC(errno));
			}
			/////////////////////////////////
			// try to use it again
			/////////////////////////////////
			wr->UnixText.strcat(buf);

			/////////////////////////////////
			// again, since we checked the access first,
			// we should not get an error in the strcat
			/////////////////////////////////
			assert( !(rc = Shore::rc()));
		}
	}
	SH_DO(SH_COMMIT_TRANSACTION);
}

/////////////////////////////////////////////
// error-handling method 2: check error code
// after attempt to use the object.  
/////////////////////////////////////////////
void
method2(const char *fname,
	mode_t mode,
	mode_t newmode
)
{
    shrc 	rc;
	SH_BEGIN_TRANSACTION(rc);
	if(rc){
		/////////////////////////////////
		// You get here if you explicitly
		// abort, or if the abort came from
		// an SH_DO macro invocation.
		/////////////////////////////////
		cerr << "Aborted : " << rc << endl;
		return;
	} else {
		// The main body of the transaction goes here.
		TxStatus x = Shore::get_txstatus();

		/////////////////////////////////
		// create an object, cache it
		/////////////////////////////////
		char 	*buf = "t\0";
		REF(SdlUnixFile) o; 		// const reference

		/////////////////////////////////////////
		// If you get an error (e.g., already
		// exists) you have difficulty tracking
		// it down.
		// o = new (fname, mode) SdlUnixFile;
		// prefereable to use the static form:
		////////////////////////////////////////
		rc = REF(SdlUnixFile)::new_persistent(fname, mode, o);

		if(rc || !o) {
			cerr << "Cannot create new SdlUnixFile. " << endl;
			if(rc == RC(EEXIST) ) {
				cerr << "You must remove " << fname 
					<< " because " << Shore::perror(rc) << endl;
			}
			SH_ABORT_TRANSACTION(rc);
		} else {
			WREF(SdlUnixFile) wr(o);// non-const reference
									// constructing the reference
									// does NOT inspect the object

			wr->UnixText.strcat(buf);
			/////////////////////////////////
			//  See if we got an error during 
			//  the strcat
			/////////////////////////////////
			rc = Shore::rc();
			if(!rc) {
				/////////////////////////////////
				// chmod to newmode
				/////////////////////////////////

				rc = Shore::chmod(fname, newmode);
				if(!rc) {
					/////////////////////////////////
					// try to use it again
					/////////////////////////////////
					wr->UnixText.strcat(buf);
					rc = Shore::rc();
				}
			}
			if(rc) {
				SH_ABORT_TRANSACTION(rc);
			}
		}
	}
	SH_DO(SH_COMMIT_TRANSACTION);
}

/////////////////////////////////////////////
// error-handling method 3: use SH_DO
// SH_DO causes the program to exit if upon abort
/////////////////////////////////////////////
void
method3(const char *fname,
	mode_t mode,
	mode_t newmode
)
{
    shrc 	rc;
	SH_BEGIN_TRANSACTION(rc);
	if(rc){
		/////////////////////////////////
		// You get here if you explicitly
		// abort, or if the abort came from
		// an SH_DO macro invocation.
		/////////////////////////////////
		cerr << "Aborted : " << rc << endl;
		return;
	} else {
		// The main body of the transaction goes here.
		TxStatus x = Shore::get_txstatus();

		/////////////////////////////////
		// create an object, cache it
		/////////////////////////////////
		char 	*buf = "t\0";
		REF(SdlUnixFile) o; 		// const reference

		/////////////////////////////////////////
		// If you get an error (e.g., already
		// exists) you have difficulty tracking
		// it down.
		// o = new (fname, mode) SdlUnixFile;
		// prefereable to use the static form:
		////////////////////////////////////////
		rc = REF(SdlUnixFile)::new_persistent(fname, mode, o);
		if(rc || !o) {
			cerr << "Cannot create new SdlUnixFile. " <<  endl;
			if(rc == RC(EEXIST) ) {
				cerr << "You must remove " << fname 
					<< " because " << Shore::perror(rc) << endl;
			}
			SH_ABORT_TRANSACTION(rc);
		} else {
			WREF(SdlUnixFile) wr(o);// non-const reference
									// constructing the reference
									// does NOT inspect the object

			wr->UnixText.strcat(buf);
			/////////////////////////////////
			//  See if we got an error during 
			//  the strcat
			/////////////////////////////////
			SH_DO(Shore::rc());

			/////////////////////////////////
			// chmod to newmode
			/////////////////////////////////
			SH_DO(Shore::chmod(fname, newmode));

			/////////////////////////////////
			// try to use it again
			/////////////////////////////////
			wr->UnixText.strcat(buf);
			SH_DO(Shore::rc());

		}
	}
	SH_DO(SH_COMMIT_TRANSACTION);
}

main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    char	*fname=0;
    mode_t 	mode = 0;
    mode_t 	newmode = 0;

    shrc 	rc;

    // Establish a connection with the vas and initialize the object
    // cache.
    SH_DO(Shore::init(argc, argv, argv[0], 0));
    (void) Shore::set_error_handler(my_error_handler);

    if(argc < 3){
	usage(cerr, progname);
    }

	if(argc>=3) {
		mode = strtol(argv[2],0,0);
		newmode = strtol(argv[3],0,0);
		fname = argv[1];
		if(strlen(fname)<2){
			usage(cerr, progname);
		}
	}
	cerr << endl << "METHOD # 1 (check access)" <<endl;
	method1(fname,mode,newmode);

	cerr << endl << "METHOD # 2 (check rc)" <<endl;
	method2(fname,mode,newmode);

	cerr << endl << "METHOD # 3 (SH_DO)" <<endl;
	method3(fname,mode,newmode);
}
