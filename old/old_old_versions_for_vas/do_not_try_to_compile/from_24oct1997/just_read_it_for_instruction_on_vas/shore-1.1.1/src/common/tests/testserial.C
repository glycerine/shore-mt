/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#undef BITS64 

#define MaxTestInc 5
#define MXDIFF (MaxTestInc+3)

#include <stream.h>
#include <basics.h>
#include <serial_t.h>
#include <assert.h>

#define P(z)  pp(#z,z); 

#define PLN(z)  pp(#z,z); cerr << endl;

void
pp(char *str, const serial_t &z) 
{
	cerr << "["  << str << "=" << z ;
	if(z.is_null()) {
		cerr << ",null" ;
	}
	if(z.is_remote()) {
		cerr << ",remote" ;
	}
	if(z.is_local()) {
		cerr << ",local" ;
	}
	assert(z.is_remote() == !(z.is_local()));
	if(z.is_on_disk()) {
		cerr << ",on-disk" ;
	}
	if(z.is_in_memory()) {
		cerr << ",in_memory" ;
	}
	assert(z.is_in_memory() == !(z.is_on_disk()));
	cerr << "]";
}

#define EQUIV(a,b)\
	if(a.equiv(b)) {\
		pp(#a,a); cerr<<"equiv"; pp(#b,b);cerr<<endl; \
	} else {\
		pp(#a,a); cerr<<" !equiv "; pp(#b,b);cerr<<endl; \
	}

#define CMP(a,op,b)\
	if(a op b) {\
		pp(#a,a); cerr << #op; pp(#b,b); cerr << endl;\
	} else {\
		pp(#a,a); cerr << "!" << #op; pp(#b,b); cerr << endl;\
	}

void
compare(
	const serial_t &a,
	const serial_t &b
)
{
	EQUIV(a,b);
	EQUIV(b,a);
	CMP(a,==,b);
	CMP(a,!=,b);
	if(a.is_remote() ) return;
	if(b.is_remote() ) return;
	CMP(a,<=,b);
	CMP(a,>=,b);
	CMP(a,>,b);
	CMP(a,<,b);
}
void
compare(
	const serial_t_data &a,
	const serial_t &b
)
{
	CMP(a,==,b);
	CMP(a,!=,b);
	if(b.is_remote() ) return;
	CMP(a,<=,b);
	CMP(a,>=,b);
	CMP(a,>,b);
	CMP(a,<,b);
}
void
compare(
	const serial_t &a,
	const serial_t_data &b
)
{
	CMP(a,==,b);
	CMP(a,!=,b);
	if(a.is_remote() ) return;

	CMP(a,<=,b);
	CMP(a,>=,b);
	CMP(a,>,b);
	CMP(a,<,b);
}
#define COMPARE(a,b) {\
	cerr << "BEGIN { comparisons at "<< __LINE__ << " :" << __FILE__ << endl;\
	compare(a,b);\
	cerr << "}" << endl;\
}

main()
{
	int i;

	cerr << "other_bits          " << hex((long)serial_t::other_bits) << endl;
	cerr << "max_any             " << hex((long)serial_t::max_any) << endl;
	cerr << "overflow_shift_bits " << dec((long)serial_t::overflow_shift_bits) << endl;
	cerr << "max_inc             " << hex((long)serial_t::max_inc) << endl;
	cerr << "mask_remote         " << hex((long)serial_t::mask_remote) << endl;
	cerr << "expect overflow after " << dec((long)(serial_t::max_any<<1)+1) << endl;

#ifdef notdef
	serial_t xnull;

	PLN(xnull);

	serial_t xstartwith8(8,false);
	PLN(xstartwith8);
	COMPARE(xnull,xstartwith8);

	serial_t x8remote(8,true);
	PLN(x8remote);
	COMPARE(x8remote,xstartwith8);

	serial_t x8local(8,false);
	PLN(x8local);
	COMPARE(x8remote,x8local);
	COMPARE(x8local,xstartwith8);

	serial_t x8(8,false);
	serial_t x8copy(x8);
	PLN(x8);
	PLN(x8copy);
	COMPARE(x8,x8copy);
	COMPARE(x8local,x8copy);
	COMPARE(x8remote,x8copy);

	serial_t_data	g8  = {  8
#ifdef BITS64
		, 8
#endif
	};
	serial_t xg(g8);
	PLN(xg);
	COMPARE(xg,g8);
	COMPARE(g8,xg);
#endif /* notdef */

	// g-l-over should overflow into the high word but not overflow
	// completely
	static serial_t_data	glover  = { 
#ifdef BITS64
		serial_t::max_any-MXDIFF, // don't overflow the whole thing
		~0 - MXDIFF
#else
		(serial_t::max_any<<1) - MXDIFF
#endif
	};
	serial_t	lowover(glover);
	PLN(lowover);
	for(i=0; i<MaxTestInc; i++) {
		cerr << "inc lowover by " << i << " ";
		if(lowover.increment((uint4)i)) {
			cerr << "OVERFLOWED";
		} else {
			cerr << "did not overflow";
		}
		cerr << "; new value = ";
		PLN(lowover);
	}

	// g-h-over should overflow the high word 

	static serial_t_data	ghover  = {
#ifdef BITS64
		serial_t::max_any, 
		~0 - MXDIFF
#else
		(serial_t::max_any<<1) - MXDIFF
#endif
	};
	serial_t	highover(ghover);
	serial_t	hosave(highover);
	PLN(highover);
	for(i=0; i<MaxTestInc; i++) {
		cerr << "inc highover by " << i << " ";
		if(highover.increment((uint4)i)) {
			cerr << "OVERFLOWED";
		} else {
			cerr << "no overflow";
		}
		cerr << ":";
		PLN(highover);
	}
#ifdef notdef
	COMPARE(xg,glover);
	COMPARE(glover,xg);

	COMPARE(xg,ghover);
	COMPARE(ghover,xg);
#endif

	COMPARE(highover,hosave);
	COMPARE(highover,lowover);


	serial_t hi_local(serial_t::max_local);

	if( ! hi_local.increment((uint4)1)) {
		cerr << "at line " << __LINE__ << " :" << __FILE__ 
		<< " ERROR - hi-local did not overflow" << endl;
	} 

	serial_t hi_remote(serial_t::max_remote);

	if( ! hi_remote.increment((uint4)1)) {
		cerr << "at line " << __LINE__ << " :" << __FILE__ 
		<< " ERROR - hi-remote did not overflow" << endl;
	}

}
