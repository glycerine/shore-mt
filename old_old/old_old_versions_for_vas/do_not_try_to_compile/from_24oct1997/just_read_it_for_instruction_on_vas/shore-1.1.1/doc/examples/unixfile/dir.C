/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


#include <stream.h>
#include <iostream.h>
#include <iomanip.h>
#include <string.h>
#include <ShoreApp.h>
#include <ShoreStats.h>
#include <sdl_UnixFile.h>

#define MODE 0755

#define CHECK_RC(msg)\
	if(rc!=RCOK) {\
	    cout << msg << " failed : " << rc << endl;\
	    exit(1);\
	}

main(int argc, char *argv[])
{
	w_rc_t	rc;
	int bufSize = 40;
	char name[bufSize];

	SH_DO(Shore::init(argc, argv, argv[0], "dir.rc"));
	SH_BEGIN_TRANSACTION(rc);

	CHECK_RC("begin");
	rc = Shore::getcwd(name, bufSize);
	CHECK_RC("getcwd");

	cout << "The current directory is " << name << endl;

	if ((rc = Shore::chdir("dir")) == RCOK) {	// see if "/dir" exists
	    rc = Shore::getcwd(name, bufSize);
	    CHECK_RC("getcwd");

	    cout <<"Thanks to chdir, the current dir. is now " << name <<endl;
	} else {
	    if ((rc = Shore::mkdir("dir", MODE)) == RCOK) {
		if ((rc = Shore::chdir("dir")) == RCOK) {
		    rc = Shore::getcwd(name, bufSize);
		    CHECK_RC("getcwd");
		    cout <<"Thanks to mkdir, we are now in " << name << endl;
		} else {
		    cerr << "ERROR: mkdir worked but chdir failed!: " << rc << endl;
		}
	    } else {
		cerr << "mkdir failed! :" << rc << endl;
		rc = Shore::getcwd(name, bufSize);
		CHECK_RC("getcwd");
		cout << "The current directory is " << name << endl;
	    }
	}		
	SH_DO(SH_COMMIT_TRANSACTION);
}
