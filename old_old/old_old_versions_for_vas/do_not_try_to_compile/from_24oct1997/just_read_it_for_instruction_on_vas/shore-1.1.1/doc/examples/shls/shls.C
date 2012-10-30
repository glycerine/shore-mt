/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// shls.C
//

#include <iostream.h>
#include <string.h>
#include <ShoreApp.h>

void do_ls(const char *parthname);
int print_dir(const char *pathname);

int main(int argc, char *argv[]) {
    char *progname = argv[0];
    option_group_t *options;
    int i;
    bool iflag;
    shrc rc;

    // Establish a connection with the vas and initialize the object
    // cache.
    SH_DO(Shore::init(argc, argv, argv[0], getenv("SHORE_RC")));

    // Begin a transaction.
    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if (rc) {
	cerr << rc << endl;
	return 1;
    }

    // The main body of the transaction goes here.
    else {
	// With no args, do "shls /".
	if(argc < 2)
	    do_ls("/");

	// Otherwise, list each arg separately.
	else for(i = 1; i < argc; ++i)
		do_ls(argv[i]);

	// Commit the transaction.
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    return 0;
}

void do_ls(const char *path) {
    OStat osp;
    shrc rc;

    // Stat the object.
    rc = Shore::stat(path, &osp);
    if(rc){
	if(rc.err_num() == SH_NotFound)
	    cerr << path << ": No such file or directory" << endl;
	else
	    cerr << path << ": " << rc << endl;
	return;
    }

    // If it's a directory, then list its contents.
    if (osp.type_loid == ReservedOid::_Directory)
	cout << "Total " << print_dir(path) << endl;
    // For everything else, we just list the name (and possibly the oid).
    else
	cout << osp.loid << " " << path << endl;
}

int print_dir(const char *pathname) {
    DirEntry entry;
    int count = 0;

    // Open a scan over the pool given by "pathname."
    DirScan scan(pathname);

    // Make sure the scan was successfully opened.
    if (scan.rc() != RCOK) {
	cout << "Error scanning directory " << pathname << ": "
	     << scan.rc() << endl;
	return 0;
    }

    // Scan until end-of-scan or an error is encountered.
    for(count = 0; scan.next(&entry) == RCOK; ++count)
	cout << entry.name << '\t' << entry.loid << endl;

    cout << pathname << " has " << count << " objects." << endl;

    // Check for errors.
    if(scan.rc().err_num() != SH_EndOfScan){
	cout << "Error scanning directory " << pathname << ": "
	     << scan.rc() << endl;
	return 0;
    }

    // The destructor will close the scan object.
    return count;
}
