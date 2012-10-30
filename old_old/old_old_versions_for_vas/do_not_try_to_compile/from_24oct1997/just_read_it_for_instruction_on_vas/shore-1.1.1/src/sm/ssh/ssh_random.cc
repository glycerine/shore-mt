/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ssh_random.cc,v 1.2 1997/06/15 10:30:31 solomon Exp $
 *
 * Ssh random-number generation functions, consolidated.
 */


#ifdef __GNUG__
#pragma implementation "ssh_random.h"
#endif 
#include "ssh_random.h"
#include <debug.h>

random_generator generator; // in ssh_random.c

bool random_generator::_constructed=false;
unsigned short random_generator::_original_seed[3];

extern "C" {
/* from the man page:
     drand48() returns non-negative double-precision
     floating-point values uniformly distributed over the inter-
     val [0.0, 1.0).

     lrand48() returns non-negative long integers
     uniformly distributed over the interval (0, ~2**31).

     mrand48() returns signed long integers uni-
     formly distributed over the interval [-2**31 ~2**31).
*/
    double drand48();
    long lrand48(); // non-negative
    long mrand48(); // may be negative

    void srand48(long ); // compatibility with ::srand(int)
			// to change the seed


    unsigned short *seed48( unsigned short arg[3] );

}

unsigned short *
random_generator::get_seed() const
{
    unsigned short dummy[3] = {0x0,0x0,0x0};
    unsigned short *result = seed48(dummy);
    (void) seed48(result);
    return result;
}

ostream& 
operator<<(ostream& o, const random_generator&)
{
    /*
     * expect "........,........,........"
     * no spaces
     */

    unsigned short dummy[3] = {0x0,0x0,0x0};
    unsigned short *result = seed48(dummy);

    o << 
	result[0] << "," << 
	result[1] << "," << 
	result[2] << endl;
    (void) seed48(result);
    return o;
}

istream&
operator>>(istream& i, random_generator&)
{
    /*
     * print "0x........,0x........,0x........"
     */
    unsigned short dummy[3];
    char	comma = ',';
    unsigned 	j=0;

    DBG(<<"rdstate: " << i.rdstate()
	<<" bad_bit: " << i.bad()
	<<" fail_bit: " << i.fail()
    );
    while( (comma == ',') && 
	(j < sizeof(dummy)/sizeof(unsigned short)) &&
	(i >>  dummy[j])
	) {
	DBG(
	    <<"rdstate: " << i.rdstate()
	    <<" bad_bit: " << i.bad()
	    <<" fail_bit: " << i.fail()
	);
	if(i.peek() == ',') i >> comma;
	DBG(
	    <<"rdstate: " << i.rdstate()
	    <<" bad_bit: " << i.bad()
	    <<" fail_bit: " << i.fail()
	);
	j++;
    }
    if(j < sizeof(dummy)/sizeof(unsigned short) ) {
	// This actually sets the badbit:
	i.clear(ios::badbit|i.rdstate());
    }
    (void) seed48(dummy);
    return i;
}

void
random_generator::srand(int seed)
{
    if(seed==0) {
	// reset  it to the state at the
	// time the constructor was called
	(void) seed48(_original_seed);
    }
    srand48((long) seed);
}

double
random_generator::drand() const
{
    return drand48();
}

int 
random_generator::mrand() const
{
    return mrand48();
}

unsigned int 
random_generator::lrand() const
{
    return lrand48();
}

int 
random_generator::rand() const
{
    return mrand();
}


#include <fstream.h>

void
random_generator::read(const char *fname)
{
    ifstream f(fname);
    if (!f) {
      cerr << "Cannot read random seed file "
	    << fname
	    << endl;
    }
    f >> *this;
    f.close();
}

void
random_generator::write(const char *fname) const
{
    ofstream f(fname, ios::out);
    if (!f) {
      cerr << "Cannot write to file "
	    << fname
	    << endl;
    }
    f << *this;
    f.close();
}
