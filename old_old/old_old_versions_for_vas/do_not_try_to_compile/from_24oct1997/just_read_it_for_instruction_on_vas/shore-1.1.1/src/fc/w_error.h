/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_error.h,v 1.41 1997/05/19 19:39:26 nhall Exp $
 */
#ifndef W_ERROR_H
#define W_ERROR_H

#ifdef __GNUG__
#pragma interface
#endif

struct w_error_info_t {
    unsigned int	err_num;
    const char*		errstr;
};

class w_error_t : public w_base_t {
public:
    typedef w_error_info_t info_t;

#ifndef __fc_error_h__
#include "fc_error.h"
#endif

    // kludge: make err_num come first:
    const uint4_t		err_num;

    const char* const		file;
    const uint4_t		line;
    const int4_t		sys_err_num;

    w_error_t*			next() { return _next; }

    w_error_t&			add_trace_info(
	const char* const 	    filename,
	uint4_t			    line_num);

    ostream			&print_error(ostream &o) const;

    static w_error_t*		make(
	const char* const 	    filename,
	uint4_t			    line_num,
	uint4_t			    err_num,
	w_error_t*		    list = 0);
    static w_error_t*		make(
	const char* const 	    filename,
	uint4_t			    line_num,
	uint4_t			    err_num,
	uint4_t			    sys_err,
	w_error_t*		    list = 0);
    static bool			insert(
	const char		    *modulename,
	const info_t		    info[],
	uint4_t			    count);

    NORET			W_FASTNEW_CLASS_DECL;

    static w_error_t*		no_error;
    static const char* const	error_string(uint4_t err_num);
    static const char* const	module_name(uint4_t err_num);
    // maximun OS error code
    static const uint4_t max_sys_err;

    NORET			~w_error_t();

#ifdef __BORLANDC__
public:
#else
private:
#endif /* __BORLANDC__ */
    enum { max_range = 10, max_trace = 3 };
    

private:
    friend class w_rc_t;
    void			_incr_ref();
    void			_decr_ref();
    void			_dangle();
    uint4_t			_ref_count;
				     
    uint4_t			_trace_cnt;
    w_error_t*			_next;
    const char*			_trace_file[max_trace];
    uint4_t			_trace_line[max_trace];

    NORET			w_error_t(
	const char* const 	    filename,
	uint4_t			    line_num,
	uint4_t			    err_num,
	w_error_t*		    list);
    NORET			w_error_t(
	const char* const 	    filename,
	uint4_t			    line_num,
	uint4_t			    err_num,
	uint4_t			    sys_err,
	w_error_t*		    list);
    NORET			w_error_t(const w_error_t&);
    w_error_t&			operator=(const w_error_t&);

    static const info_t*	_range_start[max_range];
    static uint4_t		_range_cnt[max_range];
    static const char *		_range_name[max_range];
    static uint4_t		_nreg;

    static inline uint4_t	classify(int err_num);
public:
	// make public so it  can be exported to client side
    static const info_t		error_info[];
    static ostream & 		print(ostream &out);
private:
	// disabled
	static void init_errorcodes(); 
};

extern ostream  &operator<<(ostream &o, const w_error_t &obj);

inline void
w_error_t::_incr_ref()
{
    ++_ref_count;
}

inline void
w_error_t::_decr_ref()
{
    if (--_ref_count == 0 && this != no_error)  _dangle();
}

inline NORET
w_error_t::~w_error_t()
{
}

const fcINTERNAL	= w_error_t::fcINTERNAL;
const fcOS		= w_error_t::fcOS;
const fcFULL		= w_error_t::fcFULL;
const fcEMPTY		= w_error_t::fcEMPTY;
const fcOUTOFMEMORY	= w_error_t::fcOUTOFMEMORY;
const fcNOTFOUND	= w_error_t::fcNOTFOUND;
const fcNOTIMPLEMENTED  = w_error_t::fcNOTIMPLEMENTED;
const fcREADONLY	= w_error_t::fcREADONLY;
const fcMIXED	= w_error_t::fcMIXED;
const fcFOUND	= w_error_t::fcFOUND;
const fcNOSUCHERROR	= w_error_t::fcNOSUCHERROR;

#endif /*W_ERROR_H*/
