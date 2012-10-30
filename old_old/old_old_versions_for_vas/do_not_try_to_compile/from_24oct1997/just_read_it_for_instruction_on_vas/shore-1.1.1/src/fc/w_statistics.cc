/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/fc/w_statistics.cc,v 1.18 1997/06/15 02:03:16 solomon Exp $
 */

#ifdef __GNUG__
#pragma implementation
#endif

#include "w_statistics.h"
#include "fc_error.h"
#include <assert.h>
#include <stream.h>
#include <iostream.h>
#include <strstream.h>

W_FASTNEW_STATIC_DECL(w_stat_module_t, 100);

w_stat_t		w_statistics_t::error_stat(-1);
int				w_statistics_t::error_int=-1;
unsigned int	w_statistics_t::error_uint=~0;
float 			w_statistics_t::error_float=-1.0;

bool
operator==(const w_stat_t &one, const w_stat_t &other)
{
	if(&one==&other) return true;

	return (one._u.i == other._u.i);
}
bool
operator!=(const w_stat_t &one, const w_stat_t &other)
{
	return !(one==other);
}

void 
w_statistics_t::append(w_stat_module_t *v)
{
	v->next = 0;
	if(_first==0) {
		_first = v;
	}
	if(_last) {
		_last->next = v;
	}
	_last = v;
}

#ifdef JUNK

w_rc_t		
w_statistics_t::add_module_malloced(
	const char *desc, int base, int count,
	const char **strings, const char *types, w_stat_t *values)
{
	if(empty() || writable()) {
		return _newmodule(desc, true,
			base,count,
			strings,true, 
			types,true,
			0,values,true);
	} else {
		// can't add malloced modules when static ones are already here
		return RC(fcMIXED);
	}
}
#endif /* JUNK */
w_rc_t		
w_statistics_t::add_module_static(
	const char *desc, int base, int count,
	const char **strings, const char *types, const w_stat_t *values)
{
	w_rc_t	e;
	if(empty() || !writable()) {
		e = _newmodule(desc,false,
			base,count,
			strings,false,
			types,false,
			values,0,false);
	} else {
		// cannot add a static entry if dynamic ones are there
		e = RC(fcMIXED);
	}
	return e;
}

w_rc_t		
w_statistics_t::add_module_special(
	const char *desc, 
	bool		d_malloced,
	int base, int count,
	const char **strings, 
	bool		s_malloced,
	const char *types, 
	bool		t_malloced,
	w_stat_t 	*values
)
{
	w_rc_t	e;
	if(empty() || writable()) {
		e = _newmodule(desc,d_malloced,
			base,count,
			strings,s_malloced,
			types,t_malloced,
			0,values,true);
	} else {
		// cannot add a dynamic entry if they're all
		// static, since we're assuming (for special)
		// that values are dynamic
		e = RC(fcMIXED);
	}
	return e;
}

w_rc_t		
w_statistics_t::_newmodule(
	const char *desc, 
	bool d_malloced,

	int base, int count,
	const char **strings, 
	bool s_malloced,

	const char *types, 
	bool t_malloced,

	const w_stat_t *values,
	w_stat_t *w_values,
	bool v_malloced // = false
)
{
	w_stat_module_t *v;

	if(empty()) {
		values_are_dynamic = v_malloced;
	}else {
		w_assert3(values_are_dynamic==v_malloced);
	}
	w_assert3(!(values==0 && w_values==0));


	if( !(v = find(base))) {
		// didn't find it-- that's what we expected
		// if this is the first time << was used,
		// or if we're making a copy.

		v = new w_stat_module_t(
			desc,d_malloced,
			strings,s_malloced,
			types, t_malloced,
			base,count,v_malloced);
		if(!v) {
			return RC(fcOUTOFMEMORY);
		}

		append(v);

		// compute longest string:
		if(v->strings) {
			int j,mx=0,ln=0;
			for(j = 0; j<count; j++) {
				ln = strlen(strings[j]);
				if(mx<ln) mx = ln;
			}
			v->longest=mx;
		}
	} else {

		// Module already there -- this happens when
		// we are using << operator from a class
		// to collect a *new* copy of the stats.
		// (We'd like to allow users to do this twice
		// in a row without having to call free() 
		// or some such thing between uses of <<.

		w_assert3(v->base == base);
		if(v->count != count) {
			cerr <<"Adding module " << desc <<
			 ", which conflicts with module " << v->d
			 <<", which is already registered." << endl;
			 return RC(fcFOUND);
		}
		w_assert3(v->count == count);
		w_assert3(v->strings == strings || strings==0);
		w_assert3(v->types == types || types==0);
		w_assert3(v->d == desc || desc==0);

		// get rid of the old copy if necessary
		if(v->m_values) {
			w_assert3(v->values == v->w_values);
			w_assert3(v->w_values != 0);
			delete[] v->w_values;
			v->w_values=0;
			v->values = 0;
		}
	}

	// fill in or overwrite the values
	if(v_malloced) {
		v->values = v->w_values = w_values;
	} else {
		v->values = values;
	}

	//
	// re-compute signature for whole thing
	//
	hash();

	return RCOK;
}

void
w_statistics_t::hash()
{
	w_statistics_iw 		iter(*this);
	w_stat_module_t 		*m;
	int 					i;

	_signature = 0;
	for (i=0, m = iter.curr(); m != 0; m = iter.next(),i++) {
		_signature ^= (m->base | m->count);
	}
}

w_statistics_t::w_statistics_t():
	_signature(0),
	_first(0), _last(0),
	values_are_dynamic(false),
	_print_flags(print_nonzero_only)
{ }

w_statistics_t::~w_statistics_t()
{
	make_empty();
}

void
w_statistics_t::make_empty()
{
	w_stat_module_t *save;
	if(_first) {
		for (w_stat_module_t *v = _first; v != 0; ) {
			save = v->next;
			delete v;
			v = save;
		}
		_first = 0;
		_last = 0;
	}
}


void	
w_statistics_t::printf() const
{
	cout << *this;
}


#ifdef __GNUC__
extern "C" int strncmp(const char*, const char *, int);
#endif

char *
w_stat_module_t::strcpy(const char *s) const
{
	int i = strlen(s);
	char *z = (char *)malloc(i+1);
	if(z) {
		memcpy(z,s,i);
		z[i]='\0';
	}
	return z;
}

const char **
w_stat_module_t::getstrings(bool copyall) const
{
	char ** _s = 0;
	if(copyall) {
		_s = (char **)malloc(count * sizeof(char *));
		if(_s) { 
			// copy every stinking string
			int 	j;
			for(int i=0; i<count; i++) {
				j = strlen(strings[i]);
				_s[i] =  strcpy(strings[i]);
				if(!_s[i]) {
					goto bad;
				}
			}
		}
		return (const char **)_s;
	} else {
		return strings;
	}
bad:
	for(int i=0; i<count; i++) {
		if(_s[i]) {
			free(_s[i]);
		} else {
			break;
		}
	}
	free(_s);
	return 0;
}

w_rc_t
w_stat_module_t::copy_descriptors_from(
	const w_stat_module_t &from
)
{
	char			*_d=0;
	char			*_t=0;
	const char		**_s = 0;

	_d = strcpy(from.d);
	if(!_d) {
		goto bad;
	}
	_t = strcpy(from.types);
	if(!_t) {
		goto bad;
	}
	_s = from.getstrings(true);
	if(!_s) {
		goto bad;
	}

	make_empty(false);
	d = _d;
	m_d = 1;
	types = _t;
	m_types = 1;
	strings = _s;
	m_strings = 1;

	return RCOK;


bad:
	if(_s)  {
		for(int i=0; i<count; i++) {
			if(_s[i]) {
				free((char *)_s[i]);
			} else {
				break;
			}
		}
		free(_s);
	}
	if(_d) free(_d);
	if(_t) free(_t);
	return RC(fcOUTOFMEMORY);
}

w_stat_module_t	*
w_statistics_t::find(int base) const	
{
	w_stat_module_t	*v=0;

	for(v = _first; v!=0 ;v=v->next){
		if(v->base == base) {
			return v;
		}
	}
	assert(v==0);
	return v;
}

w_stat_module_t	*
w_statistics_iw::find(int base) const	
{
	w_stat_module_t	*v=0;

	for(v = _su.s->first_w(); v!=0 ;v=v->next){
		if(v->base == base) {
			return v;
		}
	}
	assert(v==0);
	return v;
}


int	
w_statistics_t::int_val(int base, int i)
{
	const w_stat_module_t *m = find(base);
	if(m)  {
		const w_stat_t &v = find_val(m,i);
		if(v != error_stat) 
			return int(v);
	}
	return error_int;
}

unsigned int
w_statistics_t::uint_val(int base, int i)
{
	const w_stat_module_t *m = find(base);
	if(m)  {
		const w_stat_t &v = find_val(m,i);
		if(v != error_stat) 
			return (unsigned int)v;
	}
	return error_uint;
}

float
w_statistics_t::float_val(int base, int i)
{
	const w_stat_module_t *m = find(base);
	if(m)  {
		const w_stat_t &v = find_val(m,i);
		if(v != error_stat) return (float)v;
	}
	return error_float;
}


char	
w_statistics_t::typechar(int base, int i)
{
	w_stat_module_t *v = find(base);
	if(v) {
		return v->types[i];
	}
	return '\0';
}
const	char	*
w_statistics_t::typestring(int base)
{
	w_stat_module_t *v = find(base);
	if(v) return v->types;
	return "";
}
const	char	*
w_statistics_t::module(int base)
{
	w_stat_module_t *v = find(base);
	if(v) return v->d;
	return "unknown";
}
const	char	*
w_statistics_t::module(int base, int /*i_not_used*/)
{
	return module(base);
}

const	char	*
w_statistics_t::string(int base, int i)
{
	w_stat_module_t *v = find(base);
	if(v) return find_str(v, i);
	return "unknown";
}

const w_stat_t	&
w_statistics_t::find_val(const w_stat_module_t *m, int i) const
{
	if(i<0 || i >= m->count) {
		return error_stat;
	}
	return m->values[i];
}

const char		*
w_statistics_t::find_str(const w_stat_module_t *m, int i) const	
{
	if(i<0 || i >= m->count) {
		return "bad index";
	}
	return m->strings[i];
}

ostream	&
operator<<(ostream &out, const w_statistics_t &s) 
{
	w_statistics_i 		iter(s);
	const w_stat_module_t 	*m;
	int					i, field, longest;

	// compute longest strings in all modules
	for (longest=0,m = iter.curr(); m != 0; m = iter.next()) {
		longest = m->longest>longest ? m->longest : longest;
	}

	field = longest+2;
	if(field < 10) field=10;

	iter.reset();

	for (m = iter.curr(); m != 0; m = iter.next()) {
		if(m->d) {
			out << endl << m->d << ": " << endl;
		} else {
			out << "unnamed module: ";
		}
		for(i=0; i< m->count; i++) {
			switch(m->types[i]) {
			case 'v':
				if((s._print_flags & w_statistics_t::print_nonzero_only)==0 || 
					(m->values[i]._u.v != 0)) {
					W_FORM(out)("%*.*s", field, field, m->strings[i]) << ": ";
					W_FORM(out)("%10d",m->values[i]._u.v);
					out << endl;
				}
				break;
			case 'l':
				if((s._print_flags & w_statistics_t::print_nonzero_only)==0 || 
					(m->values[i]._u.l != 0)) {
					W_FORM(out)("%*.*s", field, field, m->strings[i]) << ": ";
					W_FORM(out)("%10d",m->values[i]._u.l);
					out << endl;
				}
				break;
			case 'i':
				if((s._print_flags & w_statistics_t::print_nonzero_only)==0 || 
					(m->values[i]._u.i != 0)) {
					W_FORM(out)("%*.*s", field, field, m->strings[i]) << ": ";
					W_FORM(out)("%10d",m->values[i]._u.i);
					out << endl;
				}
				break;
			case 'u':
				if((s._print_flags & w_statistics_t::print_nonzero_only)==0 || 
					(m->values[i]._u.u != 0)) {
					W_FORM(out)("%*.*s", field, field, m->strings[i]) << ": ";
					W_FORM(out)("%10u",m->values[i]._u.u);
					out << endl;
				}
				break;
			case 'f':
				if((s._print_flags & w_statistics_t::print_nonzero_only)==0 || 
					(m->values[i]._u.f != 0.0)) {
					W_FORM(out)("%*.*s", field, field, m->strings[i]) << ": ";
					W_FORM(out)("%#10.6g",m->values[i]._u.f);
					out << endl;
				}
				break;
			default:
				out << "Fatal error at file:" 
				<< __FILE__ << " line: " << __LINE__ << endl;
				assert(0);
			}
		}
	}
	out << flush;

	return out;
}


bool	
w_stat_module_t::operator==(const w_stat_module_t&r) const
{
	return( 
		this->base == r.base
		&&
		this->count == r.count
		&&
		(strcmp(this->types, r.types)==0)
		&&
		(strcmp(this->d, r.d)==0) );
}

w_statistics_t	&
operator+=(w_statistics_t &l, const w_statistics_t &r)
{
	w_rc_t e;
	if(l.writable()) {
		e = l.add(r);
	} else {
		e = RC(fcREADONLY);
	}
	if(e) {
		// cerr << e ;
		cerr << "Cannot apply += operator to read-only w_statistics_t." << endl;
		// w_assert3(0);
	}
	return l;
}

w_statistics_t	&
operator-=(w_statistics_t &l, const w_statistics_t &r)
{
	w_rc_t e;
	if(l.writable()) {
		e = l.subtract(r);
	} else {
		e = RC(fcREADONLY);
	}
	if(e) {
		cerr << e << ":" << endl;
		cerr << "Cannot apply -= operator to read-only w_statistics_t." << endl;
		// w_assert3(0);
	}
	return l;
}

w_statistics_t	*
w_statistics_t::copy_brief()const 
{
	w_statistics_i 			iter(*this);
	const w_stat_module_t 	*m;
	w_statistics_t 			*a = new w_statistics_t;
	w_rc_t					e;

	if(a) {
		for (m = iter.curr(); m != 0; m = iter.next()) {
			// get space for the values
			// NB: use malloc/free instead of new/delete
			// because these things are passed around over RPC
			w_stat_t *w = // new w_stat_t[m->count];
				(w_stat_t *)malloc(sizeof(w_stat_t) * m->count);

			memcpy(w, m->values, sizeof(w_stat_t)*(m->count));
			e = a->add_module_special(
				m->d, false,
				m->base,m->count,
				m->strings, false,
				m->types, false, w);
			if(e) {
				cerr << e << endl;
				delete a;
				return 0;
			}
		}
		a->values_are_dynamic = true;
	}
	w_assert3(a->writable());
	return a;
}

w_statistics_t	*
w_statistics_t::copy_all()const 
{
	w_statistics_t	*res = copy_brief();
	if(res) {
		w_rc_t e;
		e = res->copy_descriptors_from(*this);
		if(e) {
			cerr << e << endl;
			delete res;
			res = 0;
		}
	}
	w_assert3(res->writable());
	return res;
}

w_rc_t			
w_statistics_t::copy_descriptors_from(const w_statistics_t &from)
{
	w_statistics_iw		l(*this);
	w_statistics_i		r(from);
	const w_stat_module_t 	*rm;
	w_stat_module_t 	*lm;
	w_rc_t				e;

	for (lm = l.curr(), rm=r.curr(); 
		lm != 0 && rm !=0; 
		lm = l.next(), rm = r.next()
	) {
		// is this module the same in both structures?
		// (not only do we expect to *find* the module
		// in the right-hand structure, we expect the
		// two structs to be exactly alike
		if( !(*lm == *rm))  {
			return RC(fcNOTFOUND);
		}
		e = lm->copy_descriptors_from(*rm);
		if(e) break;
	}
	return e;
}

w_rc_t			
w_statistics_t::add(const w_statistics_t &right)
{
	w_statistics_i		l(*this);
	w_statistics_i		r(right);
	const w_stat_module_t 	*lm,*rm;
	w_rc_t				e;

	for (lm = l.curr(), rm=r.curr(); 
		lm != 0 && rm !=0; 
		lm = l.next(), rm = r.next()
	) {
		// is this module the same in both structures?
		// (not only do we expect to *find* the module
		// in the right-hand structure, we expect the
		// two structs to be exactly alike
		if( !(*lm == *rm))  {
			return RC(fcNOTFOUND);
		}
		e = lm->op(w_stat_module_t::add,rm);
		if(e) return e;
	}
	return RCOK;
}


w_rc_t			
w_statistics_t::subtract(const w_statistics_t &right)
{
	w_statistics_i		l(*this);
	w_statistics_i		r(right);
	const w_stat_module_t 	*lm,*rm;
	w_rc_t				e;

	for (lm = l.curr(), rm=r.curr(); 
		lm != 0 && rm !=0; 
		lm = l.next(), rm = r.next()
	) {
		// is this module the same in both structures?
		// (not only do we expect to *find* the module
		// in the right-hand structure, we expect the
		// two structs to be exactly alike
		if( !(*lm == *rm))  {
			return RC(fcNOTFOUND);
		}
		e = lm->op(w_stat_module_t::subtract,rm);
		if(e) return e;
	}
	return RCOK;
}

w_rc_t			
w_statistics_t::zero() const
{
	w_statistics_i		iter(*this);
	const w_stat_module_t 	*m;
	w_rc_t				e;

	for (m = iter.curr(); m != 0; m = iter.next()) {
		e = m->op(w_stat_module_t::zero);
		if(e) return e;
	}
	return RCOK;
}

w_rc_t
w_stat_module_t::op(enum operators which, const w_stat_module_t *r) const
{
	int i;

	if(!m_values) {
		return RC(fcREADONLY);
	} else {
		w_assert3(values==w_values);
	}

	if(which != zero) {
		w_assert3(r!=0);
		w_assert3(count == r->count);
	}

	for(i=0; i< count; i++) {
		if(which != zero) {
			w_assert3(types[i] == r->types[i]);
		}
		switch(types[i]) {
		case 'i':
			if(which==add) {
				w_values[i]._u.i += r->values[i]._u.i;
			} else if (which==subtract) {
				w_values[i]._u.i -= r->values[i]._u.i;
			} else if (which==zero) {
				w_values[i]._u.i = 0;
			} else if (which==assign) {
				w_values[i]._u.i = r->values[i]._u.i;
			} else {
				w_assert3(0);
			}
			break;
		case 'l':
			if(which==add) {
				w_values[i]._u.l += r->values[i]._u.l;
			} else if (which==subtract) {
				w_values[i]._u.l -= r->values[i]._u.l;
			} else if (which==zero) {
				w_values[i]._u.l = 0;
			} else if (which==assign) {
				w_values[i]._u.l = r->values[i]._u.l;
			} else {
				w_assert3(0);
			}
			break;
		case 'v':
			if(which==add) {
				w_values[i]._u.v += r->values[i]._u.v;
			} else if (which==subtract) {
				w_values[i]._u.v -= r->values[i]._u.v;
			} else if (which==zero) {
				w_values[i]._u.v = 0;
			} else if (which==assign) {
				w_values[i]._u.v = r->values[i]._u.v;
			} else {
				w_assert3(0);
			}
			break;
		case 'u':
			if(which==add) {
				w_values[i]._u.u += r->values[i]._u.u;
			} else if (which==subtract) {
				w_values[i]._u.u -= r->values[i]._u.u;
			} else if (which==zero) {
				w_values[i]._u.u = 0;
			} else if (which==assign) {
				w_values[i]._u.u = r->values[i]._u.u;
			} else {
				w_assert3(0);
			}
			break;
		case 'f':
			if(which==add) {
				w_values[i]._u.f += r->values[i]._u.f;
			} else if (which==subtract) {
				w_values[i]._u.f -= r->values[i]._u.f;
			} else if (which==zero) {
				w_values[i]._u.f = 0.0;
			} else if (which==assign) {
				w_values[i]._u.f = r->values[i]._u.f;
			} else {
				w_assert3(0);
			}
			break;
		default:
			w_assert3(0);
		}
	}
	return RCOK;
}
