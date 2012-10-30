/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
// $Header: /p/shore/shore_cvs/src/examples/unixfile/uf.C,v 1.10 1996/07/19 18:48:21 nhall Exp $
//
// Simple test to create a "Unix file".
// Simplest way to use:  
//		uf <filename>
// 		(filename is required)
//
// Default file size is 1 byte. To make a bigger file, add the size
// argument:
//		uf <filename> <size>
//
// To remove the object, use the program "remove"  in this
// directory.
*/

/* defaults: */
#define DEFAULT_OBJ_SIZE 1

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
    os << "Usage: " << progname << 
	" filename [size(default==" << DEFAULT_OBJ_SIZE <<")] " << endl;
    exit(1);
}

int
main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    char 	*fname=0;
    int 	objsize = DEFAULT_OBJ_SIZE;
    shrc 	rc;

    if(argc >3 || argc < 2){
	usage(cerr, progname);
    }
    if(argc==3) {
	objsize = atoi(argv[2]);
	if(objsize<=0) {
	    usage(cerr, progname);
	}
    }
    fname = argv[1];
    if(strlen(fname)<2){
	usage(cerr, progname);
    }
    cerr << "Object size is " << objsize << endl;

    //
    // Establish a connection with the vas and initialize the object
    // cache.
    SH_DO(Shore::init(argc, argv));

    SH_BEGIN_TRANSACTION(rc);
    // If that failed, or if a transaction aborted...
    if(rc){
	cerr << rc << endl;
	return 1;
    } else {
	// The main body of the transaction goes here.
	TxStatus x = Shore::get_txstatus();

	SH_DO(Shore::chdir("/"));

	// create a Unix file object
	{ 
	    char 	buf[objsize+1];
	    int i=0;

	    for(i=0; i<objsize; i++) {
		buf[i]= 't';
	    }
	    buf[i] = '\0';
	    REF(SdlUnixFile) o;
	    o = new (fname, 0644) SdlUnixFile;
	    if(!o) {
		cerr << "Cannot create new SdlUnixFile." << endl;
	    } else {
		o.update()->UnixText.strcat(buf);
	    }
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }
    return 0;
}
