/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef W_RC_H
#define W_RC_H

/* w_sptr_t is visible from w_rc for historical reasons.  Users of
   w_sptr_t should include it themselves */
#ifndef W_SPTR_H
#include "w_sptr.h"
#endif

#if defined(CHEAP_RC)
#include "w_cheaprc.h"
#else

#ifdef __GNUG__
#pragma interface
#endif


/*********************************************************************
 *
 *  class w_rc_t
 *
 *  Return code.
 *
 *********************************************************************/
class w_rc_i; // forward
class w_rc_t : private w_sptr_t<w_error_t> {
    friend class w_rc_i;
public:
    NORET			w_rc_t();
    NORET			w_rc_t(w_error_t* e);
    NORET			w_rc_t(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num,
	w_base_t::int4_t	    err_num);
    NORET			w_rc_t(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num,
	w_base_t::int4_t	    err_num,
	w_base_t::int4_t	    sys_err);
    NORET			w_rc_t(const w_rc_t&);
    w_rc_t&			operator=(const w_rc_t&);
    NORET			~w_rc_t();

    w_error_t&			operator*() const;
    w_error_t*			operator->() const;
    NORET			operator const void*() const;
    bool			is_error() const;
    w_base_t::int4_t		err_num() const;
    w_base_t::int4_t		sys_err_num() const;
    w_rc_t&			reset();

    w_rc_t&			add_trace_info(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num);

    w_rc_t&			push(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num,
	w_base_t::int4_t	    err_num);

    void			verify();
    void			error_not_checked();

    w_error_t*			delegate();

    void			fatal();

    /*
     *  streams
     */
    friend ostream&             operator<<(
        ostream&                    o,
        const w_rc_t&	            obj);

    static void			return_check(bool on_off);
private:
    static bool		do_check;
};

/*********************************************************************
 *
 *  class w_rc_i
 *
 *  Iterator for w_rc_t -- allows you to iterate
 *  over the w_error_t structures hanging off a
 *  w_rc_t
 *
 *********************************************************************/
class w_rc_i {
	w_rc_t 		&_rc;
	w_error_t  	*_next;
public:
	w_rc_i(w_rc_t &x) : _rc(x), _next(x.ptr()) {};
	w_base_t::int4_t	next_errnum() {
	    w_base_t::int4_t temp = 0;
	    if(_next) {
		temp = _next->err_num;
		_next = _next->next();
	    }
	    return temp;
	}
	w_error_t 	*next() {
	    w_error_t *temp = _next;
	    if(_next) {
		_next = _next->next();
	    }
	    return temp;
	}
private:
	// disabled
	w_rc_i(const w_rc_i &x);
//	: _rc( w_rc_t(w_error_t::no_error)),
//		_next(w_error_t::no_error) {};
};



/*********************************************************************
 *
 *  w_rc_t::w_rc_t()
 *
 *  Create an rc with no error. Mark as checked.
 *
 *********************************************************************/
inline NORET
w_rc_t::w_rc_t()
    : w_sptr_t<w_error_t>(w_error_t::no_error)
{
    set_flag();
}


/*********************************************************************
 *
 *  w_rc_t::w_rc_t(e)
 *
 *  Create an rc for error e. Rc is not checked.
 *
 *********************************************************************/
inline NORET
w_rc_t::w_rc_t(w_error_t* e)
    : w_sptr_t<w_error_t>(e)
{
}


/*********************************************************************
 *
 *  w_rc_t::reset()
 *
 *  Mark rc as not checked.
 *
 *********************************************************************/
inline w_rc_t&
w_rc_t::reset()
{
    clr_flag();
    return *this;
}


/*********************************************************************
 *
 *  w_rc_t::w_rc_t(rc)
 *
 *  Create an rc from another rc. Mark other rc as checked. Self
 *  is not checked.
 *
 *********************************************************************/
inline NORET
w_rc_t::w_rc_t(
    const w_rc_t& rc)
    : w_sptr_t<w_error_t>(rc)
{
    ((w_rc_t&)rc).set_flag();
    ptr()->_incr_ref();
}


/*********************************************************************
 *
 *  w_rc_t::verify()
 *
 *  Verify that rc has been checked. If not, call error_not_checked().
 *
 *********************************************************************/
inline void
w_rc_t::verify()
{
    W_IFDEBUG( if (do_check && !is_flag() )  error_not_checked(); );
}


/*********************************************************************
 *
 *  w_rc_t::operator=(rc)
 *
 *  Copy rc to self. First verify that self is checked. Set rc 
 *  as checked, set self as unchecked.
 *
 *********************************************************************/
inline w_rc_t&
w_rc_t::operator=(
    const w_rc_t& rc)
{
    w_assert3(&rc != this);
    verify();

    ptr()->_decr_ref();
    ((w_rc_t&)rc).set_flag();
    set_val(rc);
    ptr()->_incr_ref();
    return *this;
}


/*********************************************************************
 *
 *  w_rc_t::delegate()
 *
 *  Give up my error code. Set self as checked.
 *
 *********************************************************************/
inline w_error_t*
w_rc_t::delegate()
{
    w_error_t* t = ptr();
    set_val(w_error_t::no_error);
    set_flag();
    return t;
}


/*********************************************************************
 *
 *  w_rc_t::~w_rc_t()
 *
 *  Destructor. Verify status.
 *
 *********************************************************************/
inline NORET
w_rc_t::~w_rc_t()
{
    verify();
    ptr()->_decr_ref();
}


/*********************************************************************
 *
 *  w_rc_t::operator->()
 *  w_rc_t::operator*()
 *
 *  Return pointer (reference) to content. Set self as checked.
 *
 *********************************************************************/
inline w_error_t*
w_rc_t::operator->() const
{
    ((w_rc_t*)this)->set_flag();
    return ptr();
}
    

inline w_error_t&
w_rc_t::operator*() const
{
    return *(operator->());
}


/*********************************************************************
 *
 *  w_rc_t::is_error()
 *
 *  Return true if pointing to an error. Set self as checked.
 *
 *********************************************************************/
inline bool
w_rc_t::is_error() const
{
    ((w_rc_t*)this)->set_flag();
    return ptr()->err_num != 0;
}


/*********************************************************************
 *
 *  w_rc_t::err_num()
 *
 *  Return the error code in rc.
 *
 *********************************************************************/
inline w_base_t::int4_t
w_rc_t::err_num() const
{
    // consider this to constitite a check.
    ((w_rc_t*)this)->set_flag();
    return ptr()->err_num;
}


/*********************************************************************
 *
 *  w_rc_t::sys_err_num()
 *
 *  Return the system error code in rc.
 *
 *********************************************************************/
inline w_base_t::int4_t
w_rc_t::sys_err_num() const
{
    return ptr()->sys_err_num;
}


/*********************************************************************
 *
 *  w_rc_t::operator const void*()
 *
 *  Return non-zero if rc contains an error.
 *
 *********************************************************************/
inline NORET
w_rc_t::operator const void*() const
{
    return (void*) is_error();
}



/*********************************************************************
 *
 *  Basic macros for using rc.
 *
 *  RC(x)   : create an rc for error code x.
 *  RCOK    : create an rc for no error.
 *  MAKERC(bool, x):	create an rc if true, else RCOK
 *
 *  e.g.  if (eof) 
 *            return RC(eENDOFFILE);
 *        else
 *            return RCOK;
 *  With MAKERC, this can be converted to
 *       return MAKERC(eof, eENDOFFILE);
 *
 *********************************************************************/
#define RC(x)		w_rc_t(__FILE__, __LINE__, x)
#define	RC2(x,y)	w_rc_t(__FILE__, __LINE__, x, y)
#define RCOK		w_rc_t(w_error_t::no_error)
#define	MAKERC(condition,err)	((condition) ? RC(err) : RCOK)



/********************************************************************
 *
 *  More Macros for using rc.
 *
 *  RC_AUGMENT(rc)   : add file and line number to the rc
 *  RC_PUSH(rc, e)   : add a new error code to rc
 *
 *  e.g. 
 *	w_rc_t rc = create_file(f);
 *      if (rc)  return RC_AUGMENT(rc);
 *	rc = close_file(f);
 *	if (rc)  return RC_PUSH(rc, eCANNOTCLOSE)
 *
 *********************************************************************/

#define RC_AUGMENT(rc)					\
    rc.add_trace_info(__FILE__, __LINE__)

#define RC_PUSH(rc, e)					\
    rc.push(__FILE__, __LINE__, e)



#define W_DO(x)  					\
{							\
    w_rc_t __e = (x);					\
    if (__e) return RC_AUGMENT(__e);			\
}

// W_DO_GOTO must use an w_error_t* parameter (err) since
// HP_CC does not support labels in blocks where an object
// has a destructor.  Since w_rc_t has a destructor this
// is a serious limitation.
#define W_DO_GOTO(err/*w_error_t**/, x)  		\
{							\
    (err) = (x).delegate();				\
    if (err != w_error_t::no_error) goto failure;	\
}

#define W_DO_PUSH(x, e)					\
{							\
    w_rc_t __e = (x);					\
    if (__e)  { return RC_PUSH(__e, e); }		\
}

#define W_COERCE(x)  					\
{							\
    w_rc_t __e = (x);					\
    if (__e)  {						\
	RC_AUGMENT(__e);				\
	__e.fatal();					\
    }							\
}

#define W_FATAL(e)					\
    W_COERCE(RC(e));

#define W_IGNORE(x)	((void) x.is_error());

#endif /*CHEAP_RC*/

#endif /*W_RC_H*/
