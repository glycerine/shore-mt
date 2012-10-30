/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5, 6  Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <iostream.h>
#include <assert.h>
#include <stddef.h>

#include <w.h>

#include <w_vector.h>

typedef w_vector_t<float> floatvec;
template class w_vector_t<float>;

ostream & 
operator<<(ostream &o, const floatvec &v)
{
    unsigned i;
    for(i=0; i< v.vectorSize(); i++) {
	o << i 
		<< "[" 
		<< v[i] 
		<< "] "; 
    }
    return o;
}

main()
{
    floatvec v(3);

    v[0] = 1.0;
    v[1] = 10.01;
    v[2] = 100.001;

    floatvec w(3);

    w[0] = 2.0;
    w[1] = 20.02;
    w[2] = 200.002;

    floatvec x(v);

    cout << "w = " << w << endl;
    cout << "v = " << v << endl;

    x += w;
    cout << "(x=v) + w = " << x << endl;

    x -= w;
    cout << "(x - w) = " << x << endl;
    x -= v;
    cout << "(x - v) = " << x << endl;

    x = v;
    x -= w;
    cout << "(x=v - w) = " << x << endl;

    cout << "x > w = " << (bool)(x>w) << endl;
    cout << "x < w = " << (bool)(x<w) << endl;
    cout << "x == w = " << (bool)(x==w) << endl;
    cout << "x != w = " << (bool)(x!=w) << endl;
}
