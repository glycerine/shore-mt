/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// partimpl.C
//

#include <stdio.h>
#include <string.h>
#include "part.h"

void Part::init(long id)
{
    char s[20];

    partid = id;
    sprintf(s, "Part%d", id);
    name.set(s);

    for(int i=0; i < junksize; i++) {
	junk[i] = junksize-i;
    }
    checksum = '\0';

    checksum = check_sum();

}
char Part::check_sum() const
{
    // compute checksum
    char sum = '\0';
    sum = csum(sum, (char *)&partid, sizeof(partid));
    sum = csum(sum, (char *)name, name.strlen());
    sum = csum(sum, (char *)junk, sizeof(junk));
    return sum;
}
void Part::check() const
{
    char c = check_sum();
    if(c != checksum) {
    	cerr << "checksum error" << endl;
    }
    for(int i=0; i < junksize; i++) {
	if(junk[i] != junksize-i) {
	    cerr << "value error" << endl;
	}
    }
}

char Part::csum(char seed, char *buf, long bufsize) const
{
    char *c = buf;
    for(int i=0; i< bufsize; i++, c++) {
	seed += *c;
	seed %= 255;
    }
    return seed;
}

long Part::get_partid() const
{
    return partid;
}

long Part::get_name(char *buf, long bufsize) const
{
    long len;

    // note: this form of sdl_string::get does not null-terminate
    len = name.strlen();
    name.get(buf, 0, bufsize);
    buf[len] = '\0';
    return len;
}
