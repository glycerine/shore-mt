/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: list1.cc,v 1.12 1997/06/17 17:49:46 solomon Exp $
 *
 *  Test list push, pop, append, chop.
 *
 */
#include <iostream.h>
#include <assert.h>
#include <stddef.h>


#include <w.h>

struct elem1_t {
    int 	i;
    w_link_t	link;
};

ostream& operator<<(ostream& o, const elem1_t& e)
{
    return o << e.i;
}

main()
{
    w_list_t<elem1_t> l(offsetof(elem1_t, link));

    elem1_t array[10];
    
    int i;
    for (i = 0; i < 10; i++)  {
	array[i].i = i;
	l.push(&array[i]);
    }

    cout << l << endl;

    for (i = 0; i < 10; i++)  {
	elem1_t* p = l.pop();
	assert(p);
	assert(p->i == 9 - i);
    }

    assert(l.pop() == 0);

    for (i = 0; i < 10; i++)  {
	l.append(&array[i]);
    }

    for (i = 0; i < 10; i++)  {
	elem1_t* p = l.chop();
	assert(p);
	assert(p->i == 9 - i);
    }
    assert(l.chop() == 0);

    return 0;
}

#ifdef __BORLANDC__
#pragma option -Jgd
#include <w_list.c>
typedef w_list_t<elem1_t> w_list_t_elem1_t_dummy;
#endif /*__BORLANDC__*/


#ifdef __GNUG__
// force instatiation of this function.
template ostream& operator<<(ostream& o, const w_list_t<elem1_t>& l);
#endif

#ifdef __GNUG__
template class w_list_t<elem1_t>;
template class w_list_i<elem1_t>;
#endif

