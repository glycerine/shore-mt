/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <iostream.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>
#include <w_statistics.h>
#include "test_stat.h"

/* the code is here: */

// define the output operator
#include "test_stat_op.i"

// the strings:
const char *test_stat ::stat_names[] = {
#include "test_stat_msg.i"
};

void
test_stat::inc() 
{
	i++;
	j++;
	k+=1.0;
	l += 1;
	v ++;
	compute();
}

