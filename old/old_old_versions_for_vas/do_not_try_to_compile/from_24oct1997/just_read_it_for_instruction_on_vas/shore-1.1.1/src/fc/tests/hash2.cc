/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: hash2.cc,v 1.12 1997/06/17 17:49:45 solomon Exp $
 *
 *  Test hashtable
 *
 */
#include <iostream.h>
#include <assert.h>
#include <stddef.h>

#include <w.h>

const htsz = 3;
const nrecs = 20;

struct elem_t {
    int 	i;
    w_link_t	link;
};

main()
{
    w_hash_t<elem_t, int> h(htsz, offsetof(elem_t, i), 
			       offsetof(elem_t, link));
    elem_t array[nrecs];

    int i;
    for (i = 0; i < nrecs; i++)  {
	array[i].i = i;
	h.push(&array[i]);
    }

    for (i = 0; i < nrecs; i++)  {
	elem_t* p = h.lookup(i);
	assert(p);
	assert(p->i == i);
    }

    {
	int flag[nrecs];
	for (i = 0; i < nrecs; i++) flag[i] = 0;
	w_hash_i<elem_t,int> iter(h);

	while (iter.next())  {
	    i = iter.curr()->i;
	    assert(i >= 0 && i < nrecs);
	    ++flag[i];
	}

	for (i = 0; i < nrecs; i++)  {
	    assert(flag[i] == 1);
	}
    }

    for (i = 0; i < nrecs; i++)  {
	elem_t* p = h.remove(i);
	assert(p);
	assert(p->i == i);
    }

    for (i = 0; i < nrecs; i++)  {
	h.append(&array[i]);
    }

    for (i = 0; i < nrecs; i++)  {
	h.remove(&array[i]);
    }

    return 0;
}

#ifdef __BORLANDC__
#pragma option -Jgd
#include <w_list.c>
#include <w_hash.c>
typedef w_list_t<elem_t> w_list_t_element_t_dummy;
typedef w_hash_t<elem_t, int> w_hash_t_element_t_dummy;
#endif /*__BORLANDC__*/

#ifdef __GNUG__
template class w_hash_t<elem_t, int>;
template class w_hash_i<elem_t, int>;
template class w_list_t<elem_t>;
template class w_list_i<elem_t>;
#endif

