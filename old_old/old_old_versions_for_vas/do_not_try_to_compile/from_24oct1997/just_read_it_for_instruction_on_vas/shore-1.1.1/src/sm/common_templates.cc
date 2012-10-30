/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: common_templates.cc,v 1.2 1997/06/15 03:14:28 solomon Exp $
 */

#include "sm_int_0.h"

/*
 * Instantiations of templates that are used in MORE THAN ONE
 * file.  This is to reduce circularities in the physical dependence
 * graph.
 */

#ifdef __GNUG__
/*
 * vol.c, sort.c rtree.c, btree_bl.c, btree_p.c
 */
template class w_auto_delete_array_t<char>; 


/*
 * bf.c btree_impl.c, chkpt.c
 */
template class w_auto_delete_array_t<lpid_t>;

#endif
