/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * This file contains implementations of various Shore SM utilities
 * that are commonly used by both the VAS (application and server sides)
 * and the SSM
 */

#ifdef __GNUG__
#pragma implementation "sm_s.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream.h>
#include <stddef.h>
#include <limits.h>
#include "sm_app.h"

const stid_t stid_t::null;
const rid_t rid_t::null;

const lpid_t lpid_t::bof;
const lpid_t lpid_t::eof;
const lpid_t lpid_t::null;

const lsn_t lsn_t::null(0, 0);
const lsn_t lsn_t::max(lsn_t::hwm, lsn_t::hwm);

int pull_in_sm_export()
{
    return eNOERROR;
}

