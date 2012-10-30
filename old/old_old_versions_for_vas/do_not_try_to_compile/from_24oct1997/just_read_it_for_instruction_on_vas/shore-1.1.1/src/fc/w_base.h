/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_base.h,v 1.32 1997/06/13 22:32:18 solomon Exp $
 */
#ifndef W_BASE_H
#define W_BASE_H

/* get configuration definitions from config/config.h */
#include <config.h>

#ifndef W_OS2
#define W_UNIX
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include <stdlib.h>
#include <iostream.h>

#ifdef W_UNIX
#include <limits.h>
#include <unistd.h>
#endif

#ifndef W_WORKAROUND_H
#include "w_workaround.h"
#endif
#ifndef W_BOOLEAN_H
#include "w_boolean.h"
#endif

#ifdef DEBUG
#define W_DEBUG
#endif

#define NORET		/**/
#define CAST(t,o)	((t)(o))
#define SIZEOF(t)	(sizeof(t))

#ifdef W_DEBUG
#define W_IFDEBUG(x)	x
#define W_IFNDEBUG(x)	/**/
#else
#define W_IFDEBUG(x)	/**/
#define W_IFNDEBUG(x)	x
#endif

#if defined(W_DEBUG_SPACE)
void 	w_space(int line, const char *file);
#define W_SPACE w_space(__LINE__,__FILE__);

#else

#define W_SPACE

#endif


#define w_assert1(x)    {						\
	if (!(x)) w_base_t::assert_failed(__FILE__, __LINE__);		\
	W_SPACE;\
}

#ifdef W_DEBUG
#define w_assert3(x)	w_assert1(x)
#else
#define w_assert3(x)	/**/
#endif

class w_rc_t;

/*--------------------------------------------------------------*
 *  w_base_t							*
 *--------------------------------------------------------------*/
class w_base_t {
public:
    /*
     *  shorthands
     */
    typedef unsigned char	u_char;
    typedef unsigned short	u_short;
    typedef unsigned long	u_long;
    // typedef w_rc_t		rc_t;

    /*
     *  basic types
     */
    typedef char		int1_t;
    typedef u_char		uint1_t;
    typedef short		int2_t;
    typedef u_short		uint2_t;
    typedef long		int4_t;
    typedef u_long		uint4_t;

    typedef float		f4_t;
    typedef double		f8_t;

    static const int1_t		int1_max, int1_min;
    static const int2_t		int2_max, int2_min;
    static const int4_t		int4_max, int4_min;

    static const uint1_t	uint1_max, uint1_min;
    static const uint2_t	uint2_max, uint2_min;
    static const uint4_t	uint4_max, uint4_min;

    /*
     *  miscellaneous
     */
    enum { align_on = 0x8, align_mod = align_on - 1 };

	/*
	// NEH: turned into a macro for the purpose of folding
    // static uint4_t		align(uint4_t sz);
	*/
#ifndef align
#define alignonarg(a) (((uint4_t)a)-1)
#define alignon(p,a) (((uint4_t)((char *)p + alignonarg(a))) & ~alignonarg(a))

#define ALIGNON 0x8
#define ALIGNON1 (ALIGNON-1)
#define align(sz) ((uint4_t)((sz + ALIGNON1) & ~ALIGNON1))
#endif /* align */

    static bool		is_aligned(uint4_t sz);
    static bool		is_aligned(const void* s);

    static bool		is_big_endian();
    static bool		is_little_endian();

    /*
     *  standard streams
     */
    friend ostream&		operator<<(
	ostream&		    o,
	const w_base_t&		    obj);

    static void			assert_failed(
	const char*		    file,
	uint4_t 		    line);

};

/*--------------------------------------------------------------*
 *  w_base_t::align()						*
 *--------------------------------------------------------------*/
/*
 * NEH: turned into a macro
inline w_base_t::uint4_t
w_base_t::align(uint4_t sz)
{
    return (sz + ((sz & align_mod) ? 
		  (align_on - (sz & align_mod)) : 0));
}
*/

/*--------------------------------------------------------------*
 *  w_base_t::is_aligned()					*
 *--------------------------------------------------------------*/
inline bool
w_base_t::is_aligned(uint4_t sz)
{
    return (align(sz) == sz);
}

inline bool
w_base_t::is_aligned(const void* s)
{
    return is_aligned(CAST(uint4_t, s));
}

/*--------------------------------------------------------------*
 *  w_base_t::is_big_endian()					*
 *--------------------------------------------------------------*/
inline bool
w_base_t::is_big_endian()
{
    int i = 1;
    return ((char*)&i)[3] == i;
}

/*--------------------------------------------------------------*
 *  w_base_t::is_little_endian()				*
 *--------------------------------------------------------------*/
inline bool
w_base_t::is_little_endian()
{
    return ! is_big_endian();
}

/*--------------------------------------------------------------*
 *  w_vbase_t							*
 *--------------------------------------------------------------*/
class w_vbase_t : public w_base_t {
public:
    NORET			w_vbase_t()	{};
    virtual NORET		~w_vbase_t()	{};
};

class w_msec_t {
public:
    enum special_t { 
	t_infinity = -1,
	t_forever = t_infinity,
	t_immediate = 0
    };

    NORET			w_msec_t();
    NORET			w_msec_t(w_base_t::uint4_t ms);
    NORET			w_msec_t(special_t s);
    
    bool			is_forever();
    w_base_t::uint4_t		value();
private:
    w_base_t::int4_t		ms;
};

inline w_base_t::uint4_t
w_msec_t::value()
{
    return CAST(w_base_t::uint4_t, ms);
}

inline bool
w_msec_t::is_forever()
{
    return ms == t_forever;
}

#ifndef W_AUTODEL_H
#include <w_autodel.h>
#endif
#ifndef W_FASTNEW_H
#include <w_fastnew.h>
#endif
#ifndef W_ERROR_H
#include <w_error.h>
#endif
#ifndef W_RC_H
#include <w_rc.h>
#endif

#endif /*W_BASE_H*/
