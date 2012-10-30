/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <w.h>

class fnelem_t {
public:
    double d;	// use double to verify alignment
    W_FASTNEW_CLASS_DECL;
private:
};

W_FASTNEW_STATIC_DECL(fnelem_t, 4);

main()
{
    fnelem_t* p;

    int i;
    for (i = 0; i < 10; i++)  {
	p = new fnelem_t;
	p->d = i;
	delete p;
    }

    fnelem_t* np[10];
    for (i = 0; i < 10; i++)  {
	np[i] = new fnelem_t;
    }

    for (i = 1; i < 10; i+= 2)  {
	delete np[i];
	np[i] = 0;
    }

    for (i = 0; i < 10; i+=2)  {
	delete np[i];
	np[i] = 0;
    }

    fnelem_t* pa = new fnelem_t[10];
    for (i = 0; i < 10; i++)  {
	pa[i].d = i;
    }
    delete[] pa;

    return 0;
}
    

