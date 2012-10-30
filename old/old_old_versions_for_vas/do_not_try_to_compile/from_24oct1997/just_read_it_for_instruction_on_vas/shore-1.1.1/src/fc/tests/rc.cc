/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: rc.cc,v 1.15 1997/06/17 17:49:48 solomon Exp $
 *  Test rc facility
 *
 */
#include <iostream.h>
#include <assert.h>
#include <stddef.h>
#include <w.h>

w_rc_t testing()
{
    w_rc_t rc = RC(fcOS);
	RC_AUGMENT(rc);
	RC_PUSH(rc, fcINTERNAL);
	RC_AUGMENT(rc);
	RC_PUSH(rc, fcFULL);
	RC_AUGMENT(rc);
	RC_PUSH(rc, fcEMPTY);
	RC_AUGMENT(rc);
	RC_PUSH(rc, fcNOTFOUND);
	RC_AUGMENT(rc);
    if (rc)  {; }

    return rc;
}

w_rc_t testing_ok()
{
    return RCOK;
}

main()
{
    cout << "Expect two 'error not checked' message" << endl;
#ifdef CHEAP_RC
	cout << "(This won't happen -- we have cheap RCs)" <<endl;
#endif
    {
        w_rc_t rc = testing();
    }

    {
	testing_ok();
    }

    if(testing_ok() != RCOK) {
	cout << "FAILURE: This should never happen!" << endl;
    }

    cout << "Expect 3 forms of the string of errors" << endl;
    {
        w_rc_t rc = testing();
	{
	    //////////////////////////////////////////////////// 
	    // this gets you to the integer values, one at a time
	    //////////////////////////////////////////////////// 
	    cout << "*************************************" << endl;
	    w_rc_i iter(rc);
	    cout << endl << "1 : List of error numbers:" << endl;
	    for(w_base_t::int4_t e = iter.next_errnum();
		    e!=0; e = iter.next_errnum()) {
		cout << "error = " << e << endl;
	    }
	    cout << "End list of error numbers:" << endl;
	}
	{
	    //////////////////////////////////////////////////// 
	    // this gets you to the w_error_t structures, one
	    // at a time.  If you print each one, though, you
	    // get it PLUS everything attached to it
	    //////////////////////////////////////////////////// 
	    w_rc_i iter(rc);
	    cout << "*************************************" << endl;
	    cout << endl << "2 : List of w_error_t:" << endl;
	    for(w_error_t *e = iter.next();
		    e; 
		    e = iter.next()) {
		cout << "error = " << *e << endl;
	    }
	    cout << "End list of w_error_t:" << endl;
	}
	{
	    cout << "*************************************" << endl;
	    cout << endl << "3 : print the rc:" << endl;
	    cout << "error = " << rc << endl;
	    cout << "End print the rc:" << endl;
	}
    }
    return 0;
}
       
