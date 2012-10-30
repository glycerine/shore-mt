/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __test_stat_h__
#define __test_stat_h__

#include "test_stat_def.i"
class test_stat {
	/* add the stats */
#include "test_stat_struct.i"

public:
	test_stat() : 
		i((int)300),j((unsigned)0x333),k((float)1.2345),
		l(655666), v(0xffffffff),
		sum(0.0) { }

	void inc();
	void compute() {
		sum = (float)(i + j + k + l + v);
	}
};

#endif /* __test_stat_h__ */
