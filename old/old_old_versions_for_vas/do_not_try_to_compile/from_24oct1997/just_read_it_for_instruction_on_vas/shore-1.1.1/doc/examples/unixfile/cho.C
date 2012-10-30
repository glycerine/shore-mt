/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
// $Header: /p/shore/shore_cvs/src/examples/unixfile/cho.C,v 1.4 1995/04/24 19:30:32 zwilling Exp $
//
// Simple test to change the ownership of a registered
// object.
//
// to use:  
//		cho path uid gid
//
// The program creates a registered object named
// 		path
// and subsequently changes the ownership/group
// to the values given as uid, gid.  (You can't
// change ownership unless you're a privileged user,
// but this program tests that. You can change the
// group to a group of which you are a member.)
// If the chown() works, the program attempts to 
// update the object.
// If an error is caught, the transaction is aborted.
//
// If the transaction commits, and you want to remove
// the object, use the program "remove", in this directory.
//
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
    << " filename uid gid" << endl;
    exit(1);
}

void 
error_handler(shrc rc) 
{
	cerr << "ERROR : "<< rc << endl;
}

main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    char	*fname=0;
    mode_t 	mode = 0755;
    uid_t   uid = 0;
    gid_t   gid = 0;

    shrc 	rc;

    // Establish a connection with the vas and initialize the object
    // cache.
    SH_DO(Shore::init(argc, argv, argv[0], 0));
    (void) Shore::set_error_handler(error_handler);

    if(argc < 3){
	usage(cerr, progname);
    }

	if(argc>=3) {
		uid = strtol(argv[2],0,0);
		gid = strtol(argv[3],0,0);
		fname = argv[1];
		if(strlen(fname)<2){
			usage(cerr, progname);
		}
	}
	{

		SH_BEGIN_TRANSACTION(rc);

		// If that failed, or if a transaction aborted...
		if(rc){
			cerr << "Aborted : " << rc << endl;
			return 1;
		} else {

			/////////////////////////////////
			// create an object, cache it
			/////////////////////////////////
			char 	*buf = "t\0";
			REF(SdlUnixFile) o;

			o = new (fname, mode) SdlUnixFile;

			if(!o) {
				cerr << "Cannot create new SdlUnixFile." << endl;
			} else {
				WREF(SdlUnixFile) wrp = o;
				if(o) {

					/////////////////////////////////
					// chown it to group and owner
					// given on command line
					/////////////////////////////////

					SH_DO(Shore::chown(fname, uid, gid));

					/////////////////////////////////
					// try to use it again
					/////////////////////////////////
					if(wrp.valid()) {
						wrp->UnixText.strcat(buf);
					} else {
						int errno;
						SH_DO(Shore::access(fname, W_OK, errno));
						if(errno) {
							SH_ABORT_TRANSACTION(RC(errno));
						}
					}

					/////////////////////////////////
					// assuming it works, we should
					// get here.  If we can't read
					// the object, we should abort
					/////////////////////////////////

					cerr << "OK" << endl;
				} else {
					cerr << "Cannot create writable ref of SdlUnixFile." << endl;
				}
			}
		}
		SH_DO(SH_COMMIT_TRANSACTION);
	}
	return 0;
}
