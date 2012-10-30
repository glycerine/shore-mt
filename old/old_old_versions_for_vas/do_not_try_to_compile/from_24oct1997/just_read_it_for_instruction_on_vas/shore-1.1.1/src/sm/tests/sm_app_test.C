/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stream.h>
#include <iostream.h>
#include <stddef.h>
#include <limits.h>
#include "sm_app.h"
#include "option.h"
#include "debug.h"

// Test that sm_app.h and fast_new are usable in applications

class C {
public:
    C() {d = 0; i = 0;}
    C(double d_, int i_);
    friend ostream& operator<<(ostream&, const C& c);

    double 	d;
    int 	i;
};

C::C(double d_, int i_)
{
    d = d_;
    i = i_;
}

ostream& operator<<(ostream& o, const C& c)
{
    return o << c.d << " " << c.i;
}

main()
{
    int i;
    double d;
    C* cp;

	DBG(<<"app_test: main");

    for (i = 0, d = 0; i < 100; i++, d++) {
		cp = new C(d, i);
		cout << "cp[" << i << "] " << *cp << endl;
		delete cp;
    }

    cout << "test new/delete array" << endl;
    C* ac = new C[10];
    delete [] ac;

    cout << "max small rec size " << ssm_constants::max_small_rec << endl;
    cout << "lg rec data size " << ssm_constants::lg_rec_page_space << endl;

	cout << "ERROR CODES:\n"; 
	(void) w_error_t::print(cout);

    vec_t v1;
    char* s = "ssss";
    v1.put(s, strlen(s));
    vec_t v2(s, strlen(s));
    v2.put(v1);
    v2.put(v1);
}
option_group_t t(2);

#ifdef __GNUC__
typedef w_auto_delete_array_t<char> gcc_kludge_1;
typedef w_list_i<option_t> 			gcc_kludge_0;

#endif /* __GNUC__*/

