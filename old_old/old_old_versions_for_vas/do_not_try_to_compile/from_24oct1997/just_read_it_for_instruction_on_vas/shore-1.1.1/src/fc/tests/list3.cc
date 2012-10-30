/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: list3.cc,v 1.13 1997/06/17 17:49:47 solomon Exp $
 *
 *  Test ascending/descending list & list iterator
 *
 */
#include <iostream.h>
#include <assert.h>
#include <stddef.h>

#define W_INCL_LIST_EX
#include <w.h>

struct elem3_t {
    int 	i;
    w_link_t	link;
};

typedef w_ascend_list_t<elem3_t, int>  elem_ascend_list_t;
typedef w_descend_list_t<elem3_t, int>  elem_descend_list_t;

main()
{
    elem3_t array[10];
    elem3_t* p;

    int i;
    for (i = 0; i < 10; i++)
	array[i].i = i;

    {
	elem_ascend_list_t u(offsetof(elem3_t, i), 
			     offsetof(elem3_t, link));
	for (i = 0; i < 10; i += 2)   {
	    u.put_in_order(&array[9 - i]);	// insert 9, 7, 5, 3, 1
	}

	for (i = 0; i < 10; i += 2)  {
	    u.put_in_order(&array[i]);	// insert 0, 2, 4, 6, 8
	}

	for (i = 0; i < 10; i++)  {
	    p = u.search(i);
	    w_assert1(p && p->i == i);
	}

	{
	    w_list_i<elem3_t> iter(u);
	    for (i = 0; i < 10; i++)  {
		p = iter.next();
		assert(p && p->i == i);
	    }
	    assert(iter.next() == 0);
	}

	p = u.first();
	assert(p && p->i == 0);

	p = u.last();
	assert(p && p->i == 9);

	for (i = 0; i < 10; i++)  {
	    p = u.first();
	    assert(p && p->i == i);
	    p = u.pop();
	    assert(p && p->i == i);
	}

	p = u.pop();
	assert(!p);
    }

    {
	elem_descend_list_t d(offsetof(elem3_t, i), 
			      offsetof(elem3_t, link));

	for (i = 0; i < 10; i += 2)  {
	    d.put_in_order(&array[9 - i]);	// insert 9, 7, 5, 3, 1
	}
    
	for (i = 0; i < 10; i += 2)  {
	    d.put_in_order(&array[i]);	// insert 0, 2, 4, 6, 8
	}

	for (i = 0; i < 10; i++)  {
	    p = d.search(i);
	    w_assert1(p == &array[i]);
	}

	{
	    w_list_i<elem3_t> iter(d);
	    for (i = 0; i < 10; i++)  {
		p = iter.next();
		assert(p && p->i == 9 - i);
	    }
	    assert(iter.next() == 0);
	}

	p = d.first();
	assert(p && p->i == 9);

	p = d.last();
	assert(p && p->i == 0);

	for (i = 0; i < 10; i++)  {
	    p = d.first();
	    assert(p && p->i == 9 - i);
	    p = d.pop();
	    assert(p && p->i == 9 - i);
	}

	p = d.pop();
	assert(!p);
    }

    return 0;
}

#ifdef __BORLANDC__
#pragma option -Jgd
#include <w_list.c>
typedef w_list_t<elem3_t> w_list_t_elem3_t_dummy;
#endif /*__BORLANDC__*/

#ifdef __GNUG__
template class w_ascend_list_t<elem3_t, int>;
template class w_descend_list_t<elem3_t, int>;
template class w_keyed_list_t<elem3_t, int>;
template class w_list_t<elem3_t>;
template class w_list_i<elem3_t>;
#endif

