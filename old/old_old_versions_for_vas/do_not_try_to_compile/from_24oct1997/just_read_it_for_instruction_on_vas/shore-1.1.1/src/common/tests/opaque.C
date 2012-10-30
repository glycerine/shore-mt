/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stream.h>
#include <basics.h>
#include <assert.h>
#include <debug.h>
#include <tid_t.h>

template ostream 
&operator<<(ostream &, const opaque_quantity<max_server_handle_len> &);

main()
{
    // test unaligned vectors if possible;

    char dummy[500];

    char *d = &dummy[3];

    server_handle_t *s = (server_handle_t *)d;

    *s = "abc";

    cout << "address of s = " << hex << (unsigned int) s << endl;
    cout << "value of s = " << *s << endl;
    cout << "length of s = " << s->length() << endl;
}
