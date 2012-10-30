/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_base.cc,v 1.15 1997/06/15 02:03:10 solomon Exp $
 */
#define W_SOURCE

#ifdef __GNUG__
#pragma implementation "w_base.h"
#endif

#include <w_base.h>

/*--------------------------------------------------------------*
 *  constants for w_base_t                                      *
 *--------------------------------------------------------------*/
const w_base_t::int1_t    w_base_t::int1_max  = 0x7f;
const w_base_t::int1_t    w_base_t::int1_min  = 0x80;
const w_base_t::uint1_t   w_base_t::uint1_max = 0xff;
const w_base_t::uint1_t   w_base_t::uint1_min = 0x0;
const w_base_t::int2_t    w_base_t::int2_max  = 0x7fff;
const w_base_t::int2_t    w_base_t::int2_min  = 0x8000;
const w_base_t::uint2_t   w_base_t::uint2_max = 0xffff;
const w_base_t::uint2_t   w_base_t::uint2_min = 0x0;
const w_base_t::int4_t    w_base_t::int4_max  = 0x7fffffff;
const w_base_t::int4_t    w_base_t::int4_min  = 0x80000000;
const w_base_t::uint4_t   w_base_t::uint4_max = 0xffffffff;
const w_base_t::uint4_t   w_base_t::uint4_min = 0x0;

ostream&
operator<<(ostream& o, const w_base_t&)
{
    cerr << "w (fatal): w_base::operator<<() called ..."
         << endl << flush;
    abort();
    return o;
}

void
w_base_t::assert_failed(
    const char*		file,
    uint4_t		line)
{
    cerr << "w (fatal): assertion failed in \"" 
	 << file << ':' << line << "\"" << endl;
    abort();
}

	
#ifdef W_DEBUG

#ifdef Ultrix42
/* XXX caddr_t is a unixism from <sys/types.h>.  Why doesn't this
   code include <sys/types.h> ?? */
typedef	char	*caddr_t;
#endif

#if defined(SUNOS41) || defined(Ultrix42)
extern "C" caddr_t sbrk(int);
#endif

#if defined(W_DEBUG_SPACE)

///////////////////////////////////////
// for now turn off the space checking,
// even if debugging is compiled in
// We do this here, rather than in the .h
// file so that we can turn it on without
// recompiling the world
///////////////////////////////////////

void 	
w_space(int line, const char *file) { }

#else

void 	
w_space(int line, const char *file) {
    static const char  *lastfile="start";
    static int			lastline=0;
    static int			count   =0;
    static int			cnt_since_print   =0;
    static caddr_t  	last=0;
    caddr_t         	curr = (caddr_t)sbrk(0);
	const 				threshold = 1000;
	int					difference = (int)(curr-last);

	count++;
	cnt_since_print++;
	if((cnt_since_print > 100 || difference > threshold) 
		&&
		difference != 0) {
		cnt_since_print = 0;
		cerr
			<< "count " << count
			<< " at line " << line
			<< ", file " << file
			<< " added: "
			<< difference
			<< "since line " << lastline
			<< ", file " << lastfile
		<< endl;
	}
    last = curr;
	lastline = line;
	lastfile = file;
}
#endif /* NOTDEF */

#endif
