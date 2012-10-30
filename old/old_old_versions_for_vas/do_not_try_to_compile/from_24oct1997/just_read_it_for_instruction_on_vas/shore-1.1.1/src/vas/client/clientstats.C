/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/clientstats.C,v 1.2 1995/04/24 19:43:25 zwilling Exp $
 */

#include <copyright.h>
#include <w_statistics.h>
#include <clientstats.h>
#include <memory.h>

#include  "clientstats_op.i"

const char *
clientstats::stat_names[] = {
#include  "clientstats_msg.i"
};

