/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: cq1.cc,v 1.9 1997/06/17 17:49:43 solomon Exp $
 *
 *  Test ascending/descending list & list iterator
 *
 */
#include <iostream.h>
#include <assert.h>
#include <stddef.h>

#define W_INCL_CIRCULAR_QUEUE
#include <w.h>

#ifdef __GNUG__
template class w_cirqueue_t<int>;
#endif


main()
{
    int qbuf[10];
    w_cirqueue_t<int> q(qbuf, 10);

    w_assert1(q.is_empty());

    int i;
    for (i = 0; i < 10; i++)  {
	W_COERCE( q.put(i) );
    }

    w_assert1(q.is_full());

    w_assert1( q.put(i) );

    for (i = 0; i < 10; i++)  {
	int j;
	W_COERCE( q.get(j));
	w_assert1(j == i);
    }

    w_assert1(q.is_empty());
    w_assert1( q.get() );

    for (i = 0; i < 3; i++)  {
	W_COERCE( q.put(i) );
    }

    for (i = 0; i < 2; i++)  {
	W_COERCE( q.put(i) );
    }

    for (i = 0; i < 3; i++)  {
	W_COERCE( q.get());
    }

    w_assert1(! q.is_empty());
    w_assert1(q.cardinality() == 2);

    for (i = 2; i < 10; i++)  {
	W_COERCE(q.put(i));
    }

    w_assert1(q.is_full());
    w_assert1( q.put(i) );

    for (i = 0; i < 10; i++)  {
	int j;
	W_COERCE( q.get(j));
	w_assert1(j == i);
    }

    w_assert1( q.get() );

    return 0;
}
       
