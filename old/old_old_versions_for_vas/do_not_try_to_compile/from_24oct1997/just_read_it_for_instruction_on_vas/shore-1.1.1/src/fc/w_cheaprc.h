/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_cheaprc.h,v 1.7 1996/07/17 17:21:28 bolo Exp $
 */
#ifndef W_CHEAPRC_H
#define W_CHEAPRC_H

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

class w_rc_t {
    friend class w_rc_i;
    int				_err_num;

public:
	
    inline
    NORET			w_rc_t(w_base_t::int4_t e = 0) {
				    _err_num = e;
				}
    NORET			w_rc_t(w_error_t* e);
    NORET			w_rc_t(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num,
	w_base_t::int4_t	    err_num);
    NORET			w_rc_t(const w_rc_t&);
    w_rc_t&			operator=(const w_rc_t&);
    // NORET			~w_rc_t(){}

    w_error_t&			operator*() const;
    // w_error_t*			operator->() const;

    inline
    bool			is_error() const {
				    return _err_num != 0; 
				}
    inline
    bool			operator !=(const w_rc_t &b) const {
				    return _err_num != b._err_num;
				}
    inline
    bool			operator ==(const w_rc_t &b) const {
				    return _err_num == b._err_num;
				}
#if !defined(__xlC__)
    inline
    NORET			operator const void*() const {
				    return (void *)is_error();
				}
#endif
    inline
    NORET			operator bool() const {
				    return is_error();
				}
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

    inline w_rc_t&		identity()  {return *this;};

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
	w_rc_i(w_rc_t &x) : _rc(x) {};
	w_base_t::int4_t	next_errnum() {
	    return 0;
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
 *  w_rc_t::w_rc_t(e)
 *
 *  Create an rc for error e. Rc is not checked.
 *
 *********************************************************************/
inline NORET
w_rc_t::w_rc_t(w_error_t* e)
{
     _err_num = e?e->err_num:0;
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
{
   _err_num = rc._err_num;
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
}


/*********************************************************************
 *
 *  w_rc_t::operator=(rc)
 *********************************************************************/
inline w_rc_t&
w_rc_t::operator=(
    const w_rc_t& rc)
{
    _err_num = rc._err_num;
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
    return (w_error_t *)&_err_num;
}


/*********************************************************************
 *
 *  w_rc_t::operator->()
 *  w_rc_t::operator*()
 *
 *  Return pointer (reference) to content. Set self as checked.
 *
 *********************************************************************/
// inline w_error_t*
// w_rc_t::operator->() const
// {
    // return (w_error_t *)&_err_num;
// }
    

inline w_error_t&
w_rc_t::operator*() const
{
    return *((w_error_t *)&_err_num);
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
    return _err_num;
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
    return _err_num;
}





/*********************************************************************
 *
 *  Basic macros for using rc.
 *
 *  RC(x)   : create an rc for error code x.
 *  RCOK    : create an rc for no error.
 *
 *  e.g.  if (eof) 
 *            return RC(eENDOFFILE);
 *        else
 *            return RCOK;
 *
 *********************************************************************/
#define RC(x)		w_rc_t(x)
#define RCOK		w_rc_t((w_base_t::int4_t)0)



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

#define RC_AUGMENT(rc) rc.identity()

// #define RC_PUSH(rc, e) rc				
// DTS 6-26: we need to do the old RC_PUSH from w_rc.h ; the above will loose e.
#define RC_PUSH(rc, e)					\
    rc.push(__FILE__, __LINE__, e)



#define W_DO(x)  					\
{							\
    w_rc_t __e = (x);					\
    if (__e) return __e;				\
}

// W_DO_GOTO must use an w_error_t* parameter (err) since
// HP_CC does not support labels in blocks where an object
// has a destructor.  Since w_rc_t has a destructor this
// is a serious limitation.
// But in the cheap implementation, this is not so...
#define W_DO_GOTO(err/*w_error_t**/, x)  		\
{							\
    w_rc_t __e = (x);					\
    if (__e) {  					\
	err = w_error_t::make(__FILE__, __LINE__, __e.err_num(), 0);\
	goto failure;					\
    }							\
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
	__e.fatal();					\
    }							\
}

#define W_FATAL(e)					\
    W_COERCE(RC(e));

#define W_IGNORE(x)	((void) x.is_error());

#endif /*W_CHEAPRC_H*/
