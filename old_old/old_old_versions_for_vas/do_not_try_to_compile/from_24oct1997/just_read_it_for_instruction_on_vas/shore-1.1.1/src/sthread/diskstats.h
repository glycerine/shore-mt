/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef __DISKSTATS_H__
#define __DISKSTATS_H__

#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif

#include <string.h>


struct S {
	int writes;
	int bwritten;
	int reads;
	int skipreads;
	int bread;
	int fsyncs;
	int ftruncs;
	int discards;

	S(): writes(0), bwritten(0),
		reads(0), bread(0),
		fsyncs(0), discards(0) {}
};

class U : public unix_stats {
public:
	// j is the iteratons
	void copy(int j, const U &u) {
		*this = u;
		if(j < 1) {
			iterations = 0;
		} else {
			iterations = j;
		}
	}
};

struct S s;
class  U u;
#endif __DISKSTATS_H__
