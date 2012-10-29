/*<std-header orig-src='shore'>

 $Id: hash2.cpp,v 1.30 2010/12/08 17:37:38 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_stream.h>
#include <cstddef>

#include <w.h>

const int htsz = 3;
const int nrecs = 20;

class int_key_t {
	int i;
public:
	int_key_t(int _i) : i(_i) {}
	~int_key_t() {}

	operator int () const { return i; }

	w_base_t::uint4_t hash() const { return w_base_t::uint4_t(i); }
};


struct elem_t {
    int         i;
    w_link_t        link;
};

int main()
{
    w_hash_t<elem_t, unsafe_list_dummy_lock_t, int_key_t> h(htsz, W_HASH_ARG(elem_t, i, link), unsafe_nolock);
    elem_t array[nrecs];

    int i;
    for (i = 0; i < nrecs; i++)  {
        array[i].i = i;
        h.push(&array[i]);
    }

    for (i = 0; i < nrecs; i++)  {
#if W_DEBUG_LEVEL>0
        elem_t* p = h.lookup(i);
        w_assert1(p);
        w_assert1(p->i == i);
#else
        (void) h.lookup(i);
#endif
    }

    {
        int flag[nrecs];
        for (i = 0; i < nrecs; i++) flag[i] = 0;
        w_hash_i<elem_t,unsafe_list_dummy_lock_t,int_key_t> iter(h);

        while (iter.next())  {
            i = iter.curr()->i;
            w_assert1(i >= 0 && i < nrecs);
            ++flag[i];
        }

        for (i = 0; i < nrecs; i++)  {
            w_assert1(flag[i] == 1);
        }
    }

    for (i = 0; i < nrecs; i++)  {
#if W_DEBUG_LEVEL>0
        elem_t* p = h.remove(i);
        w_assert1(p);
        w_assert1(p->i == i);
#else
        (void) h.remove(i);
#endif
    }

    for (i = 0; i < nrecs; i++)  {
        h.append(&array[i]);
    }

    for (i = 0; i < nrecs; i++)  {
        h.remove(&array[i]);
    }

    return 0;
}

#ifdef EXPLICIT_TEMPLATE
template class w_hash_t<elem_t, unsafe_list_dummy_lock_t, int_key_t>;
template class w_hash_i<elem_t, unsafe_list_dummy_lock_t, int_key_t>;
template class w_list_t<elem_t, unsafe_list_dummy_lock_t>;
template class w_list_i<elem_t, unsafe_list_dummy_lock_t>;
#endif

