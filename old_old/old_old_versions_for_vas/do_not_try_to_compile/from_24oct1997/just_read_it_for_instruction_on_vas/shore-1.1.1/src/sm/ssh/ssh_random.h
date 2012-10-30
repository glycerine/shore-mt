/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ssh_random.h,v 1.1 1997/03/14 18:42:05 nhall Exp $
 *
 * Ssh random-number generation functions, consolidated.
 */

#ifdef __GNUG__
#pragma interface 
#endif 

#include <w.h>
#include <stream.h>

class random_generator {

private:
    unsigned short *get_seed() const;
    static unsigned short _original_seed[3];
    static bool _constructed;

public:
    double drand() const;
    unsigned int lrand() const;
    int mrand() const;

    /* some compatibility methods */
    void srand(int seed);
    int rand() const; // may be negative

    friend ostream& operator<<(ostream&, const random_generator&);
    friend istream& operator>>(istream&, random_generator&);

public:
    NORET
    random_generator()  
    { 
	// Disallow a 2nd random-number generator
	if(!_constructed) {
	    *_original_seed = *get_seed(); 
	    _constructed = true;
	}
    }
    void random_generator::read(const char *fname);
    void random_generator::write(const char *fname) const;
};

extern random_generator generator; // in ssh_random.c

