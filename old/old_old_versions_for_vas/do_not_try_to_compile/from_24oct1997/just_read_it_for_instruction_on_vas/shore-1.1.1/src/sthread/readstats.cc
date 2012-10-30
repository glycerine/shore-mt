/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: readstats.cc,v 1.5 1997/06/15 02:26:22 solomon Exp $
 */
#define DISKRW_C

#include <copyright.h>
#include <stdio.h>
#include <iostream.h>
#include <fstream.h>
#include <w_statistics.h>
#include <strstream.h>
#include <w_signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif

#include <unistd.h>
#if defined(Sparc) || defined(Mips) || defined(I860)
	extern "C" int fsync(int);
#endif
#if defined(HPUX8) && !defined(_INCLUDE_XOPEN_SOURCE)
	extern "C" int fsync(int);
#endif

#include "diskstats.h"

main(int argc, char *argv[])
{

    int statfd = -1;
    const char *path=0;
    int flags;
    int mode = 0755;

    // get filename from argv
    if(argc < 2) {
	    cerr << "usage: " << argv[0] 
		    << " <filename> " << endl;
	    exit(1);
    }
    flags = O_RDONLY;
    path = argv[1];
    // open & read it into s, u
    if(path) {
	statfd = open(path, flags, mode);
	if(statfd<0) {
		perror("open");
		cerr << "path=" << path <<endl;
		exit(1);
	}
    }

    int cc;
    if((cc = read(statfd, &s, sizeof s)) != sizeof s ) {
	    perror("read s");
	    exit(1);
    }

    if((cc = read(statfd, &u, sizeof u)) != sizeof u ) {
	    perror("read u");
	    exit(1);
    }

    {
	U r;
	r.copy(s.reads, u);

	float secs = r.compute_time();

	if(secs == 0.0) {
		cout <<"No time transpired." <<endl;
		exit(0);
	}
	cout 
		<< "Non-zero rusages in "
		<< (float)secs << " seconds :" << endl;

	{
	    float usr = r.usertime() / (float)1000000;
	    float sys = r.systime() / (float)1000000;
	    float clk = r.clocktime() / (float)1000000;
	    cout 
		    << "secs( "
		    << (float)usr << "usr " 
		    << (float)sys << "sys " 
		    << (float)clk << "clk ) " 
		    << endl;
	}

	cout
		<< r << endl;
	cout << endl;


	r.copy(s.reads, u);
	cout << "READ CALLS       : " << s.reads << endl;
	cout << "READ CALLS/sec   : " << (float)s.reads/secs << endl;
	r.copy(s.bread, u);
	cout << "BYTES READ       : " << s.bread << endl;
	cout << "BYTES READ/sec   : "  << (float)s.bread/secs << endl;
	cout << "BYTES/CALL avg   : " << (int)(s.bread/s.reads) << endl;
	cout << endl;

	r.copy(s.discards, u);
	cout << "DISCARDED RBUF   : " << s.discards << endl;
	r.copy(s.skipreads, u);
	cout << "READS SKIPPED    : " << s.skipreads << endl;

	r.copy(s.writes, u);
	cout << "WRITE CALLS      : " << s.writes << endl;
	cout << "WRITE CALLS/sec  : " << (float)s.writes/secs << endl;

	r.copy(s.bwritten, u);
	cout << "BYTES WRITTEN    : " << s.bwritten << endl;
	cout << "BYTES WRITTEN/sec: "  << (float)s.bwritten/secs << endl;
	cout << "BYTES/CALL avg   : " << (int)(s.bwritten/s.writes) << endl;
	cout << endl;

	r.copy(s.fsyncs, u);
	cout << "FSYNC CALLS      : " << s.fsyncs << endl;
	cout << "FSYNC CALLS/sec  : " << (float)s.fsyncs/secs << endl;

	r.copy(s.fsyncs, u);
	cout << "FTRUNCS CALLS    : " << s.ftruncs << endl;

    }
    return 0;
}

