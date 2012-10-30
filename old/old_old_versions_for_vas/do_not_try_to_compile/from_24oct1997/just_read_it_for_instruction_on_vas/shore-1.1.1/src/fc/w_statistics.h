/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/fc/w_statistics.h,v 1.15 1997/05/19 19:39:28 nhall Exp $
 */


#ifndef W_STATISTICS_H
#define W_STATISTICS_H
#ifdef __GNUG__
#pragma interface
#endif

#ifndef W_BASE_H
#include <w_base.h>
#endif
#ifndef W_RC_H
#include <w_rc.h>
#endif
#ifndef W_FASTNEW_H
#include <w_fastnew.h>
#endif
#include <iostream.h>
#include <stdio.h>

/*
// This is a generic class for collecting and
// printing simple statistics.
//
// Each class C that collects w_statistics_t 
// will provide a
// 	friend w_statistics_t & operator<<(w_statistics_t &, const C &),
// which will add its stats to this class (see below).
// 
// Stats can have these types : int, u_int, long, u_long, or float 
//
// The (ultimate) user program creates an instance of 
// class w_statistics_t, calls Shore::gather_stats()
// to fill it in; Shore::gather_stats() calls OC to 
// add OC stats; OC calls vas to add vas stats; vas calls
// rusage to add unix stats, etc etc.
// When control returns to the user,
// the w_statistics_t contains a list of modules.
// Each module has some descriptive information about
// the module: a module name, a count of the # statistics
// that the module keeps, a string that indicates the type
// of each statistic (i, f, u, etc), a list of strings
// that describe the statistics (one string per stat), and,
// finally, a list of values.   Each value is treated like
// a w_stat_t (a union of : int, float, long, etc), although
// what the module *really* stores is a pointer to the begining
// of a set of contiguous data (int, float, etc).
// 
// Once the user has a w_statistics_t with some modules in it,
// the user program simply uses the w_statistics_t's
// operator<< for ostreams to pretty-print the list of stats.
//
// The w_statistics_t class is rather too complicated; the
// reason is this:
// Statistics are gathered at all layers, both in the server
// and in the client.  We don't want application code to 
// have to have compiled-in knowledge of each layer's statistics
// structures in order to gather and print statistics.
// Therefore, any layer, any process can add modules to a
// w_statistics_t, and the stats are stored in a sort of 
// generic form.  
// This generic structure is conveyed across an RPC, so
// it takes 2 forms: "static" and "malloced".
//
// When a process collects its local statistics, it gets
// a w_statistics_t that contains only "static" data.
//
// When a process collects remote statistics, or copies
// a w_statistics_t instance, it gets a w_statistics_t
// that contains "malloced" data.
// 
// The ramifications are this:
// In the local/static case, the user program "gathers"
// the stats into a w_statistics_t once, and prints it
// any number of times; each time it's printed, the
// current data are displayed because the data are gathered
// right out of the various layers' private structures.
// (There's a small lie here-- see below.)
// A local/static w_statistics_t is not an l-value (for
// the purpose of arithmetic; read on.)
//
// In the remote/malloced cases, the user program "gathers"
// the stats into a w_statistics_t, and a COPY of the values
// is made.  These values aren't updated by the various
// layers.   Arithmetic can be performed on pairs of
// w_statistics_t; as long both w_statistics_t's contain
// the same modules, the arithmetic is performed on 
// pairs of corresponding statistics.
// Only "malloced" forms can be l-values:
// 
//	w_statistics_t m1, m2;  // will be malloced forms
//  rc_t e1 = Shore::gather_stats(m1, true); // true-> remote
//  ...
//  rc_t e2 = Shore::gather_stats(m2, true); // 2nd copy of remote stats
//  ...some computation...
//	m2 -= m1;
//  cout << "Computation cost: " << m2 << endl;
// 
// You cannot do things quite the same way with local stats, because
// you cannot do 
//  m2 -= m1; where m2 is "static"
// (This is unfortunate in a way, but if you want to do that sort 
// of thing, you can always make a copy and do arithmetic on the
// copy). 
//
// Now, for the small lie:  some statistics are computed,
// based on other stats.  Some layers compute these stats
// when the layer's module is stuffed into the w_statistics_t.
// Such computed statistics do not get updated automatically
// when the w_statistics_t is output to a stream.
// This matters only for local stats, and it's a design
// limitation / BUG. 
*/

class w_statistics_t;

class w_stat_t 
{
	friend class w_statistics_t;
	friend class w_stat_module_t;
	/* grot */
	friend ostream	& operator<<(ostream &out, const w_statistics_t &s);

protected:
	union {
		unsigned long	v; // 'v'
		long			l; // 'l'
		int				i; // 'i'
		unsigned int 	u; // 'u'
		float 			f; // 'f'
	}_u;

public:
	inline w_stat_t() { _u.i=0; }

	inline w_stat_t(int i) { _u.i=i; }
	inline operator int() const { return _u.i; }

	inline w_stat_t(unsigned int i) { _u.u=i; }
	inline operator unsigned int() const { return _u.u; }

	inline w_stat_t(float i) { _u.f=i; }
	inline operator float() const { return _u.f; }

	inline w_stat_t(long l) { _u.l=l; }
	inline operator long() const { return _u.l; }

	inline w_stat_t(unsigned long v) { _u.v=v; }
	inline operator unsigned long() const { return _u.v; }


	friend bool	operator==(const w_stat_t &, const w_stat_t &);
	friend bool	operator!=(const w_stat_t &, const w_stat_t &);

};

class	w_stat_module_t 
{
	friend class w_statistics_t;
	friend class w_statistics_i;
	friend class w_statistics_iw;

	enum  operators { add, subtract, assign, zero };
	w_rc_t  op(enum operators which, const w_stat_module_t *r=0) const;

public: // sorry-- must be public for RPC reasons
	const char		*d;		/* module description */
	const char		**strings;		/* strings[] */
	const char 		*types;	/* types[] */
	const int		base;	/* identifier of first */ 
	const int		count;	/* # values in this module */
	const w_stat_t 	*values;	/* values[] */
	w_stat_t 		*w_values;	/* writable values[] */

	int				longest;	/* longest string */

// for purify reasons, we've gotta make this a union.
// (initialize the whole mess o' bit fields 
// w/o getting UMC reports).

	typedef unsigned int bit;
	union {
		bit				    initializer_only;
		struct {
			// bit fields indicating which are malloced:
			bit				_m_d:1;
			bit				_m_types:1;
			bit				_m_strings:1;
			bit				_m_values:1;
		}_st;
	}_un;
#define m_d       _un._st._m_d
#define m_types   _un._st._m_types
#define m_strings _un._st._m_strings
#define m_values  _un._st._m_values

	char			*strcpy(const char *s) const;
	const char		**getstrings(bool copyall) const; 
						// either copy or return ptr

	w_stat_module_t *copy_all()const;

private:
	void make_empty(bool valuesalso) {
		if(m_d && d) {
			free((char *)d);
		}
		if(m_strings && strings) {
			for(int i = 0; i< count; i++) {
				free((char *)strings[i]);
			}
			free((char *)strings);
		}
		if(m_types && types) 	{
			free((char *)types);
		}
		if(valuesalso) {
			if(m_values && values) {
				w_assert3(w_values == values);
				delete[] w_values;
			}
		}
	}
protected:
	w_stat_module_t	*next;

	w_stat_module_t(const char *_d,
		bool d_malloced,
		const char **_strings,
		bool strings_malloced,
		const char *_types,
		bool types_malloced,
		const int	_base,
		const int 	_count,
		bool values_will_be_dynamic
		) : 
			d(_d), 
			strings(_strings), 
			types(_types), 
			base(_base), count(_count), 
			values(0),w_values(0),
			longest(0),
			// bit fields
			next(0)
	{ 
			// DO NOT CHANGE the way this is initialized!
			// It's the only way I could get purify to work.
			_un.initializer_only = 0;
			m_d = d_malloced?1:0;
			m_types = types_malloced?1:0;
			m_strings = strings_malloced?1:0;
			m_values = values_will_be_dynamic?1:0;
	}
	~w_stat_module_t()  { make_empty(true); }
	w_rc_t copy_descriptors_from( const w_stat_module_t &from); 

	bool	operator==(const w_stat_module_t&) const;
	W_FASTNEW_CLASS_DECL; 
};


class w_statistics_t 
{
	friend class w_statistics_i;
	friend class w_statistics_iw;

	////////////////////////////////////////////////////
	// data
	////////////////////////////////////////////////////

private:
	unsigned int	_signature; // hash value
	w_stat_module_t *_first;
	w_stat_module_t *_last;

	bool			values_are_dynamic; 
		// Either all modules in a w_statistics_t are 
		// dynamic or they are all  static.

	unsigned int	_print_flags; // available to output operator

protected:
	// for scanning:
	const w_stat_module_t *first() const { return _first; }
		  w_stat_module_t *first_w()       { return _first; }
	const w_stat_module_t *last() const { return _last; }
	w_rc_t copy_descriptors_from( const w_statistics_t &from); 


	////////////////////////////////////////////////////
	// methods for internal use
	////////////////////////////////////////////////////
private:
	void 	append(w_stat_module_t *v);

	void 	hash();
	w_rc_t	_newmodule(
		const char *desc, 
		bool d_malloced,
		int base, int count,
		const char **strings, 
		bool s_malloced,
		const char 	*types, 
		bool t_malloced,
		const w_stat_t 	*values, // const, static
		w_stat_t 	*w_values, // writable, dynamic, to be delete[]-ed
		bool		dynamic = false
		);


public: // these should be protected but there's no way to
	// collect the arbitrary classes that would have to be
	// listed as friends
	//
	unsigned int		  signature() { return _signature; }
	bool				  empty() const { return _first == 0; }
	bool				  writable() const { return empty() || values_are_dynamic; }
	void 			      make_empty(); // clean up

#ifdef JUNK
	w_rc_t		add_module_malloced(
		const char *desc, 	// from malloced space
		int base, int count,
		const char **strings, // from malloced space
		const char *types, // from malloced space
		w_stat_t 	*values // writable, from malloced space
		);
#endif /* JUNK */
	w_rc_t		add_module_static(
		const char *desc, 
		int base, int count,
		const char **strings, 
		const char *types, 
		const w_stat_t *values // const, static
		);

		// each item is marked malloced or static:
	w_rc_t		add_module_special( 
		const char *desc, 
		bool		d_malloced,
		int base, int count,
		const char **strings, 
		bool		s_malloced,
		const char *types, 
		bool		t_malloced,
		w_stat_t 	*values
	);
	const char	**getstrings(int i, bool copyall) const {
								// locate by module base
						const w_stat_module_t *v = find(i);
						const char **c=0;
						if(v) c = v->getstrings(copyall);
						return c;
			}


private:
	// disable copy operator so that we don't confuse users
	// with questions about whether/how to delete an object 
	// and its parts that was created by a copy
	// They'll have to use explicit copy*() instead
	w_statistics_t	&operator=(const w_statistics_t &); // disabled
							// that does *not* make a copy of the
							// stats

public:
	////////////////////////////////////////////////////////////////////// 
	//
	//    PUBLIC INTERFACE:
	//    Interesting methods for w_statistics_t users:
	//
	////////////////////////////////////////////////////////////////////// 
	w_statistics_t();
	~w_statistics_t();

		// copy_brief()
		// Make a copy of the contents for later diffing,
		// Caller must delete the result.
		// Makes duplicate pointers to the descriptors;
		// copies ONLY the values.  User had better
		// NOT delete the item copied and continue to
		// use this one; nor can user gather remote
		// statistics into the item copied, nor can
		// user make_empty the item copied.
		//
	w_statistics_t	*copy_brief()const; 

		// copy_all()
		// Makes copies of *all* descriptive
		// information. Makes a self-contained copy
	w_statistics_t	*copy_all()const; 

	// for setting characteristics of print
	enum printflags	{
		print_nonzero_only=0x1,
	};
	void			addflag(printflags x) { _print_flags |= (unsigned int)x;}
	void			rmflag(printflags x){_print_flags &= ~((unsigned int)x);}
	void			setflag(printflags x){_print_flags = x; }

protected:
	// these methods are to 
	// support the following:
	/*
	w_statistics_t	&operator+(w_statistics_t &, const w_statistics_t &);
	w_statistics_t	&operator-(w_statistics_t &, const w_statistics_t &);
	*/
	w_rc_t			add(const w_statistics_t &);  
	w_rc_t			subtract(const w_statistics_t &);  

public:
	w_rc_t			zero() const;
private:

	const	w_stat_t	&find_val(const w_stat_module_t *, int i) const;	
	const	char		*find_str(const w_stat_module_t *, int i) const;	
	w_stat_module_t		*find(int i) const;	// locate by module base 

public:

	/* 
	// These values are for returning
	// when one of the following functions
	// runs into an error condition
	*/
	static	w_stat_t		error_stat;
	static	int			error_int;
	static	unsigned int	error_uint;
	static	float 		error_float;

	/*
	// for formatting your own output:
	// When you call these, use the #defined constants, which
	// are actually pairs of ints
	*/
    int								int_val(int base, int i) ; 
	unsigned int					uint_val(int base, int i) ;
	float 							float_val(int base, int i);
	char	 						typechar(int base, int i); 
	const	char	 				*typestring(int base);

	const	char	 				*string(int base, int i); 

	const	char	 				*module(int base, int i); 
	const	char	 				*module(int base); 

	/*
	// the simplest "pretty" output
	*/
	friend ostream	&operator<<(ostream &, const w_statistics_t &);
	void	printf() const;

	/*
	// Arithmetic
	*/
	friend w_statistics_t	&operator+=(w_statistics_t &, 
		const w_statistics_t &);
	friend w_statistics_t	&operator-=(w_statistics_t &, 
		const w_statistics_t &);

};


#ifdef OLD
// iterator for read-only scans
class w_statistics_i 
{
	const w_stat_module_t *_curr;
	const w_statistics_t  &s;
public:
	w_statistics_i (const w_statistics_t &_s): s(_s) { _curr =  s.first(); }
	~w_statistics_i () {}

	void					reset() { _curr = s.first(); }

	const w_stat_module_t	*next();	// returns 0 if none left
	const w_stat_module_t	*curr() const;	// returns 0 if next()-ed past end

	const w_stat_module_t	*find(int i) const;	// locate by module base 
};
#else

// iterator for read_write scans
class w_statistics_iw
{
protected:
	union {
		w_stat_module_t 		*_curr;
		const w_stat_module_t 	*_curr_c;
	}_cu;
	union {
		w_statistics_t  		*s;
		const w_statistics_t  	*s_c;
	}_su;
	// for w_statistics_i only 
	w_statistics_iw () { _su.s = 0; _cu._curr = 0; }
public:
	w_statistics_iw (w_statistics_t &_s) { _su.s = &_s; _cu._curr =  _s.first_w(); }
	~w_statistics_iw () {}

	void					reset() { _cu._curr = _su.s->first_w(); }

	w_stat_module_t	*curr() const { return _cu._curr; }
	w_stat_module_t	*next()  {
		_cu._curr = _cu._curr->next;
		return curr();
	}

	w_stat_module_t	*find(int i) const;	// locate by module base 
};

class w_statistics_i  : private w_statistics_iw
{
public:
	w_statistics_i (const w_statistics_t &_s) : w_statistics_iw() { 
		_su.s_c = &_s;
		_cu._curr_c =  _s.first(); 
	}
	~w_statistics_i () {}

	void					reset() { _cu._curr_c = _su.s_c->first(); }

	const w_stat_module_t	*next() {
		_cu._curr_c = _cu._curr_c->next;
		return curr();
	}
	const w_stat_module_t	*curr() const { return _cu._curr_c; }

	const w_stat_module_t	*find(int i) const {
		return (const w_stat_module_t *) ((w_statistics_iw *)this)->find(i);
	}
};
#endif
#endif /* W_STATISTICS_H */
