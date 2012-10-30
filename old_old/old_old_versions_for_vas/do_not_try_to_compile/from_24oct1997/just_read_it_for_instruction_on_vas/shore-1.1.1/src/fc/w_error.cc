/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_error.cc,v 1.42 1997/09/06 22:34:44 solomon Exp $
 */
#ifdef __GNUC__
#pragma implementation "w_error.h"
#endif

#include <unix_error.h>

#include <string.h>

#define W_SOURCE
#include <w_base.h>

W_FASTNEW_STATIC_DECL(w_error_t, 32);

const
#include <fc_einfo.i>

//
// Static equivalent of insert(..., error_info, ...)
//
const 
w_error_t::info_t*	w_error_t::_range_start[w_error_t::max_range] = {
    w_error_t::error_info, 0 
};
w_base_t::uint4_t	w_error_t::_range_cnt[w_error_t::max_range] = {
	fcERRMAX - fcERRMIN + 1, 0
};

const char *w_error_t::_range_name[w_error_t::max_range]= { 
	"Foundation Classes",
	0
};
w_base_t::uint4_t	w_error_t::_nreg = 1;

static char		no_space[sizeof(w_error_t)] = {0};
w_error_t*		w_error_t::no_error = (w_error_t*) no_space;
const w_base_t::uint4_t	w_error_t::max_sys_err = sys_nerr;

/*** not used ***
static void w_error_t_error_not_checked()
{
}
*/

static void w_error_t_no_error_code()
{
}

#ifdef W_DEBUG
#define CHECKIT \
    w_error_t*	my = _next; \
    w_error_t*	p = my; \
    while(p) { \
	w_assert3(p!= p->_next); w_assert3(my != p->_next);\
	p = p->_next; \
    }
#else
#define CHECKIT
#endif

void 
w_error_t::_dangle()
{
    w_assert1(this != no_error);
    register w_error_t* pp = 0;
    for (register w_error_t* p = _next; p; p = pp)  {
	pp = p->_next;
	w_assert3(pp != p);
	delete p;
    }

    delete this;
}

w_error_t&
w_error_t::add_trace_info(
    const char* const	filename,
    uint4_t		line_num)
{
    if (_trace_cnt < max_trace)  {
	_trace_file[_trace_cnt] = filename;
	_trace_line[_trace_cnt] = line_num;
	++_trace_cnt;
    }

    return *this;
}

/* automagically generate a sys_err_num from an errcode */
inline w_base_t::uint4_t w_error_t::classify(int er)
{
	uint4_t	sys = 0;
	switch (er) {
	case fcOS:
		sys = ::errno;
		break;
	}
	return sys;
}

inline NORET
w_error_t::w_error_t(
    const char* const	fi,
    uint4_t		li,
    uint4_t		er,
    w_error_t*		list)
    : err_num(er),
      file(fi), line(li), 
#ifdef NO_ERROR_SYSTEMS
      sys_err_num(::errno),
#else
      sys_err_num(classify(er)),
#endif
      _ref_count(0),
      _trace_cnt(0),
      _next(list)
{
    CHECKIT
}

w_error_t*
w_error_t::make(
    const char* const	filename,
    uint4_t		line_num,
    uint4_t		err_num,
    w_error_t*		list)
{
    return new w_error_t(filename, line_num, err_num, list);
}

inline NORET
w_error_t::w_error_t(
    const char* const	fi,
    uint4_t		li,
    uint4_t		er,
    uint4_t		sys_er,
    w_error_t*		list)
    : err_num(er),
      file(fi), line(li), 
      sys_err_num(sys_er),
      _ref_count(0),
      _trace_cnt(0),
      _next(list)
{
    CHECKIT
}

w_error_t*
w_error_t::make(
    const char* const	filename,
    uint4_t		line_num,
    uint4_t		err_num,
    uint4_t		sys_err,
    w_error_t*		list)
{
    return new w_error_t(filename, line_num, err_num, sys_err, list);
}

bool
w_error_t::insert(
    const char *        modulename,
    const info_t	info[],
    uint4_t		count)
{
    if (_nreg >= max_range)
	return false;

    uint4_t start = info[0].err_num;

    for (uint4_t i = 0; i < _nreg; i++)  {
	if (start >= _range_start[i]->err_num && start < _range_cnt[i])
	    return false;
	uint4_t end = start + count;
	if (end >= _range_start[i]->err_num && end < _range_cnt[i])
	    return false;
    }
    _range_start[_nreg] = info;
    _range_cnt[_nreg] = count;
    _range_name[_nreg] = modulename;

    ++_nreg;
    return true;
}

const char* const
w_error_t::error_string(uint4_t err_num)
{
    if(err_num ==  w_error_t::no_error->err_num ) {
	return "no error";
    }
    uint4_t i;
    for (i = 0; i < _nreg; i++)  {
	if (err_num >= _range_start[i]->err_num && 
	    err_num <= _range_start[i]->err_num + _range_cnt[i]) {
	    break;
	}
    }
    
    if (i == _nreg)  {
	// try OS
	if( err_num > 0  && err_num < max_sys_err ) {
	    return  sys_errlist[err_num];
	} else {
	    w_error_t_no_error_code();
	    return error_string( fcNOSUCHERROR );
	    // return "unknown error code";
	}
    }

    const j = CAST(int, err_num - _range_start[i]->err_num);
    return _range_start[i][j].errstr;
}

const char* const
w_error_t::module_name(uint4_t err_num)
{
    if(err_num ==  w_error_t::no_error->err_num ) {
	return "all modules";
    }
    uint4_t i;
    for (i = 0; i < _nreg; i++)  {
	if (err_num >= _range_start[i]->err_num && 
	    err_num <= _range_start[i]->err_num + _range_cnt[i]) {
	    break;
	}
    }
    
    if (i == _nreg)  {
	// try OS
	if( err_num > 0  && err_num < max_sys_err ) {
	    return  "Operating system";
	} else {
	    return "unknown module";
	}
    }
    return _range_name[i];
}

extern "C" void w_stop();
void
w_stop()
{
	w_assert1(0);
}

ostream& w_error_t::print_error(ostream &o) const
{
    if (this == w_error_t::no_error) {
	return o << "no error";
    }

    int cnt = 1;
    for (const w_error_t* p = this; p; p = p->_next, ++cnt)  {

	const char* f = strrchr(p->file, '/');
	f ? ++f : f = p->file;
	o << cnt << ". error in " << f << ':' << p->line << " ";
	if(cnt > 1) {
	    if(p == this) {
		w_stop();
	    }
	    if(p->_next == p) {
		w_stop();
	    }
	}
	if(cnt > 20) {
	    w_stop();
	}
	o << p->error_string(p->err_num);

	switch (p->err_num) {
	case fcOS:
	    o << " --- " << ::strerror(CAST(int, p->sys_err_num));
	    break;
	}

	o << endl;

	if (p->_trace_cnt)  {
	    o << "\tcalled from:" << endl;
	    for (unsigned i = 0; i < p->_trace_cnt; i++)  {
		f = strrchr(p->_trace_file[i], '/');
		f ? ++f : f = p->_trace_file[i];
		o << "\t" << i << ") " << f << ':' 
		  << p->_trace_line[i] << endl;
	    }
	}
    }

    return o;
}

ostream &operator<<(ostream &o, const w_error_t &obj)
{
	return obj.print_error(o);
}

ostream &
w_error_t::print(ostream &out)
{
    for (unsigned i = 0; i < _nreg; i++)  {
	unsigned int first	= _range_start[i]->err_num;
	unsigned int last	= first + _range_cnt[i] - 1;

	for (unsigned j = first; j <= last; j++)  {
		const char *c = w_error_t::module_name(j);
		const char *s = w_error_t::error_string(j);

	    out <<  c << ":" << j << ":" << s << endl;
	}
    }
    
    return out;
}
