/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/stringhash.C,v 1.4 1995/04/24 19:44:59 zwilling Exp $
 */

#include <copyright.h>
#include <debug.h>
#include <basics.h>
#include <w_base.h>

w_base_t::uint4_t
hash(const char *p) {
	FUNC(hash(char *));
	const char *c=p;

	int len;
	int	i; int	b; int m=1 ; unsigned long sum = 0;

	DBG(
		"Computing hash of " << p
	)

	c++;
	len = strlen(c);

	for(i=0; i<len; i++) {
		// compute push-button signature
		// of a sort (has to include the rest of the
		// ascii characters
		switch(*c) {
#define CASES(a,b,c,d,e,f) case a: case b: case c: case d: case e: case f:
			CASES('a','b','c','A','B','C')
				b = 2; break;
			CASES('d','e','f','D','E','F')
				b = 3; break;
			CASES('g','h','i','G','H','I')
				b = 4; break;
			CASES('j','k','l','J','K','L')
				b = 5; break;
			CASES('m','n','o','M','N','O')
				b = 6; break;
			CASES('p','r','s','P','R','S')
				b = 7; break;
			CASES('t','u','v','T','U','V')
				b = 8; break;
			CASES('w','x','y','W','X','Y')
				b = 9; break;
			default:
				b = 1;
				break;
		}
		sum += (unsigned) (b * m);
		m = m * 10;
	}
	DUMP(end of hash(char *));
	return sum;
}
