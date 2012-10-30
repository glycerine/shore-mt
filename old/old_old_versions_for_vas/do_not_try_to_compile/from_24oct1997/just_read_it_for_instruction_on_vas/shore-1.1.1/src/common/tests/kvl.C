/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stream.h>
#include <w.h>
#include <basics.h>
#include <serial_t.h>
#include <assert.h>
#include <vec_t.h>
#include <kvl_t.h>
#include <zvec_t.h>
#include <debug.h>

main(int argc, const char *argv[])
{
    if(argc != 3 && argc != 4) {
	cerr << "Usage: " << argv[0]
		<< " <store-id: x.y> <key: string> [<elem: string >]"
		<< endl;
    } else {
	int v;
	int st;
	{
	    istrstream anon(argv[1],strlen(argv[1]));
	    char dot;
	    anon >> v;
	    anon >> dot;
	    anon >> st;
	}
	stid_t s(v,st);
	cvec_t key(argv[2], strlen(argv[2]));

	cvec_t elem;
	if(argc > 3) {
	    elem.put(argv[3], strlen(argv[3]));
	}
	kvl_t  kvl(s, key, elem);

	cout 
	    << "store: " << s 
	    << " key: " << key 
	    << " elem: " << elem 
	    << " kvl: " << kvl << endl;
    }
}

