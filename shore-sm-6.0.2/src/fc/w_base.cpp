/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/

/*<std-header orig-src='shore'>

 $Id: w_base.cpp,v 1.58 2012/01/02 17:02:13 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma implementation "w_base.h"
#endif

#include <w_base.h>
#include <sstream>

/*--------------------------------------------------------------*
 *  constants for w_base_t                                      *
 *--------------------------------------------------------------*/
const w_base_t::int1_t    w_base_t::int1_max  = 0x7f;
const w_base_t::int1_t    w_base_t::int1_min  = (w_base_t::int1_t) 0x80u;
const w_base_t::uint1_t   w_base_t::uint1_max = 0xff;
const w_base_t::uint1_t   w_base_t::uint1_min = 0x0;
const w_base_t::int2_t    w_base_t::int2_max  = 0x7fff;
const w_base_t::int2_t    w_base_t::int2_min  = (w_base_t::int2_t) 0x8000u;
const w_base_t::uint2_t   w_base_t::uint2_max = 0xffff;
const w_base_t::uint2_t   w_base_t::uint2_min = 0x0;
const w_base_t::int4_t    w_base_t::int4_max  = 0x7fffffff;
const w_base_t::int4_t    w_base_t::int4_min  = 0x80000000;
const w_base_t::uint4_t   w_base_t::uint4_max = 0xffffffff;
const w_base_t::uint4_t   w_base_t::uint4_min = 0x0;

#    define LONGLONGCONSTANT(i) i##LL
#    define ULONGLONGCONSTANT(i) i##ULL

#ifdef ARCH_LP64

const w_base_t::uint8_t   w_base_t::uint8_max =
                ULONGLONGCONSTANT(0xffffffffffffffff);
const w_base_t::uint8_t   w_base_t::uint8_min =
                ULONGLONGCONSTANT(0x0);
const w_base_t::int8_t   w_base_t::int8_max =
                LONGLONGCONSTANT(0x7fffffffffffffff);
const w_base_t::int8_t   w_base_t::int8_min =
                LONGLONGCONSTANT(0x8000000000000000);
#else

const w_base_t::uint8_t   w_base_t::uint8_max =
                ULONGLONGCONSTANT(0xffffffff);
const w_base_t::uint8_t   w_base_t::uint8_min =
                ULONGLONGCONSTANT(0x0);
const w_base_t::int8_t   w_base_t::int8_max =
                LONGLONGCONSTANT(0x7fffffff);
const w_base_t::int8_t   w_base_t::int8_min =
                LONGLONGCONSTANT(0x80000000);
#endif

ostream&
operator<<(ostream& o, const w_base_t&)
{
    w_base_t::assert_failed("w_base::operator<<() called", __FILE__, __LINE__);
    return o;
}

void
w_base_t::assert_failed(
    const char*        desc,
    const char*        file,
    uint4_t        line)
{
    stringstream os;
    /* make the error look something like an RC in the meantime. */
    os << "assertion failure: " << desc << endl
        << "1. error in "
        << file << ':' << line
        << " Assertion failed" << endl
        << "\tcalled from:" << endl
        << "\t0) " << file << ':' << line
        << endl << ends;
    fprintf(stderr, "%s", os.str().c_str());
    abort();
}


typedef ios::fmtflags  fmtflags;

#include <w_strstream.h>

/* name is local to this file */
static w_base_t::uint8_t    
__strtou8(        
    const char *str,
    char **endptr,
    int     base,
    bool  is_signed
)
{
#if defined(ARCH_LP64)
    return is_signed? strtol(str, endptr, base): strtoul(str, endptr, base);
#else
    return is_signed? strtoll(str, endptr, base): strtoull(str, endptr, base);
#endif
}

w_base_t::int8_t    
w_base_t::strtoi8(
    const char *str,
    char **endptr,
    int     base
)
{
    w_base_t::int8_t    i8;
    w_base_t::int8_t    u8 =
        __strtou8(str, endptr, base, true);
    i8 = w_base_t::int8_t(u8);
    return i8;
}

w_base_t::uint8_t    
w_base_t::strtou8(
    const char *str,
    char **endptr,
    int     base
)
{
    return __strtou8(str, endptr, base, false);
}

#if defined(SOLARIS2)
#include <ieeefp.h>
#else
#include <cmath>
#endif

bool
w_base_t::is_finite(const f8_t x)
{
    bool value = false;
    value = finite(x);
    return value;
}

bool
w_base_t::is_infinite(const f8_t x)
{
    bool value = false;
#if defined(SOLARIS2)
    value = !finite(x) && !isnand(x);
#elif defined(MacOSX) && W_GCC_THIS_VER >= W_GCC_VER(3,0)
    value = !finite(x) && !__isnand(x);
#else
    value = !finite(x) && !isnan(x);
#endif
    return value;
}

bool
w_base_t::is_nan(const f8_t x)
{
    bool value = false;
#if defined(SOLARIS2)
    value = isnand(x);
#elif defined(MacOSX) && W_GCC_THIS_VER >= W_GCC_VER(3,0)
    value = __isnand(x);
#else
    value = isnan(x);
#endif
    return value;
}

bool
w_base_t::is_infinite_or_nan(const f8_t x)
{
    bool value = false;
    value = !finite(x);
    return value;
}


void    w_base_t::abort()
{
    cout.flush();
    cerr.flush();
    ::abort();
}


/* Provide our own pure_virtual() so we can generate abort's if
   a virtual function is abused.  Name choice is somewhat
   hacky, since it is system and compiler dependent. */

#ifdef __GNUG__
#define    PURE_VIRTUAL    extern "C" void __pure_virtual()
#else
#define    PURE_VIRTUAL    void pure_virtual()
#endif

PURE_VIRTUAL
{
    /* Just in case the iostreams generate another error ... */
    static    bool    called = false;
    if (!called)
        cerr << "** Pure virtual function called" << endl;
    called = true;

    w_base_t::abort();
    /*NOTREACHED*/
}


#include <netinet/in.h>
w_base_t::uint2_t w_base_t::w_ntohs(w_base_t::uint2_t net)
{
    return ntohs(net);
}

w_base_t::uint2_t w_base_t::w_htons(w_base_t::uint2_t host)
{
    return htons(host);
}

w_base_t::uint4_t w_base_t::w_ntohl(w_base_t::uint4_t net)
{
    return ntohl(net);
}

w_base_t::uint4_t w_base_t::w_htonl(w_base_t::uint4_t host)
{
    return htonl(host);
}

#ifdef __GNUC__
#if defined(__x86_64) || defined(i386) || defined(__i386__)
#define NEED_POPC64 0
#elif defined(__sparcv9)
#define NEED_POPC64 0
#else
#define NEED_POPC64 1
#endif
#else // !defined(__GNUC__)
#define NEED_POPC64 1
#endif

#if NEED_POPC64==1
// adapted from http://chessprogramming.wikispaces.com/Population+Count#SWAR-Popcount
typedef unsigned long long u64;
static inline
long popc64(u64 x) {
    u64 k1 = 0x5555555555555555ull;
    u64 k2 = 0x3333333333333333ull;
    u64 k4 = 0x0f0f0f0f0f0f0f0full;
    u64 kf = 0x0101010101010101ull;
    x =  x       - ((x >> 1)  & k1); //put count of each 2 bits into those 2 bits
    x = (x & k2) + ((x >> 2)  & k2); //put count of each 4 bits into those 4 bits
    x = (x       +  (x >> 4)) & k4 ; //put count of each 8 bits into those 8 bits
    x = (x * kf) >> 56; //returns 8 most significant bits of x + (x<<8) + (x<<16) + (x<<24) + ...
    return x;
}
#endif

size_t w_base_t::pop_count(w_base_t::uint8_t bm)
{
#if NEED_POPC64==1
#warning "using home-brew popc routine"
    return popc64(bm);
#elif defined(__x86_64) || defined(i386) || defined(__i386__)
// #warning "Using __builtin_popcountll"
    return __builtin_popcountll(bm);
#elif defined(__sparcv9)
#warning "Using gcc inline asm to access sparcv9 'popc' instruction"
    long rval;
    __asm__("popc    %[in], %[out]" : [out] "=r"(rval) : [in] "r"(x));
    return rval;
#else
	// Brian Kernighan's population count
	// from http://chessprogramming.wikispaces.com/
	// Population Count
	// This is not the fastest for densely-populated maps though.
	int count=0;
	while(bm) {
		count++;
		bm &= bm-1; // reset LS1B
	}
	return count;
#endif 
}
