/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef RESTART_S_H
#define RESTART_S_H

#ifdef __GNUG__
#pragma interface
#endif

struct dp_entry_t {
    bfpid_t      pid;
    lsn_t       rec_lsn;
    w_link_t    link;

    dp_entry_t() : rec_lsn(0, 0)                {};
    dp_entry_t(const lpid_t& p, const lsn_t& l)
    : pid(p), rec_lsn(l)                {};
};

#endif /*RESTART_S_H*/
