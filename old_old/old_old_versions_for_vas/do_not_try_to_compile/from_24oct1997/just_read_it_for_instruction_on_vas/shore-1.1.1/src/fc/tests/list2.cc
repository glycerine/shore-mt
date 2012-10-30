/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: list2.cc,v 1.12 1997/06/17 17:49:46 solomon Exp $
 *
 *  Test list iterator
 *
 */
#include <iostream.h>
#include <assert.h>
#include <stddef.h>

#include <w.h>

struct elem2_t {
    int 	i;
    w_link_t	link;
};

main()
{
    w_list_t<elem2_t> l(offsetof(elem2_t, link));

    elem2_t array[10];

    int i;
    for (i = 0; i < 10; i++)  {
	array[i].i = i;
	l.push(&array[i]);
    }

    {
	w_list_i<elem2_t> iter(l);
	for (i = 0; i < 10; i++)  {
	    elem2_t* p = iter.next();
	    assert(p);
	    assert(p->i == 9 - i);
	}
	assert(iter.next() == 0);

	w_list_const_i<elem2_t> const_iter(l);
	for (i = 0; i < 10; i++)  {
	    const elem2_t* p = const_iter.next();
	    assert(p);
	    assert(p->i == 9 - i);
	}
	assert(const_iter.next() == 0);
    }

    for (i = 0; i < 10; i++)  {
	elem2_t* p = l.pop();
	assert(p);
	assert(p->i == 9 - i);
    }

    assert(l.pop() == 0);

    for (i = 0; i < 10; i++)  {
	l.append(&array[i]);
    }

    {
	w_list_i<elem2_t> iter(l);
	for (i = 0; i < 10; i++)  {
	    elem2_t* p = iter.next();
	    assert(p);
	    assert(p->i == i);
	}
    }

    for (i = 0; i < 10; i++)  {
	elem2_t* p = l.chop();
	assert(p);
	assert(p->i == 9 - i);
    }
    assert(l.chop() == 0);

    return 0;
}

#ifdef __BORLANDC__
#pragma option -Jgd
#include <w_list.c>
typedef w_list_t<elem2_t> w_list_t_elem2_t_dummy;
#endif /*__BORLANDC__*/

#ifdef __GNUG__
template class w_list_t<elem2_t>;
template class w_list_i<elem2_t>;
template class w_list_const_i<elem2_t>;
#endif
