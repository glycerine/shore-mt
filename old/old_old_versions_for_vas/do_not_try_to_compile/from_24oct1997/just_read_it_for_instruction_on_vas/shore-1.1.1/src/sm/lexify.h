/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef LEXIFY_H
#define LEXIFY_H
/* Routines for translating integers and floating point numbers
 * into a form that allows lexicographic
 * comparison in an architecturally-neutral form.
 *
 * Original work for IEEE double-precision values by
 * 	Marvin Solomon (solomon@cs.wisc.edu) Feb, 1997.
 * Extended for integer and IEEE single-precision values by
 * 	Nancy Hall Feb, 1997.
 *
 */

#ifdef __GNUG__
#   pragma interface
#endif

class sortorder : private smlevel_0 {
	typedef w_base_t::f4_t f4_t;
	typedef w_base_t::f8_t f8_t;
	typedef w_base_t::uint1_t uint1_t;
	typedef w_base_t::uint2_t uint2_t;
	typedef w_base_t::uint4_t uint4_t;
	typedef w_base_t::int1_t int1_t;
	typedef w_base_t::int2_t int2_t;
	typedef w_base_t::int4_t int4_t;
public:
	enum keytype {
            kt_nosuch,
	    /* signed, unsigned 1-byte integer values */
	    kt_i1, kt_u1, 
	    /* signed, unsigned 2-byte integer values */
	    kt_i2, kt_u2, 
	    /* signed, unsigned 4-byte integer values */
	    kt_i4, kt_u4, 
	    /* IEEE single, double precision floating point values */
	    kt_f4, 
	    kt_f8, 
	    /* unsigned byte string */
	    kt_b,
	    /* not used here */
	    kt_spatial,
	    /* custom key type; comparison function passed in */
	    kt_custom
	};

	NORET sortorder();
	NORET ~sortorder();
	// returns true if it worked, false otherwise
	bool lexify(const key_type_s *kp, const void *d, void *res); 

	// returns true if it worked, false otherwise
	bool unlexify(const key_type_s *kp, const void *str, void *res) ;

private:
	void int_lexify( const void *d, bool is_signed, int len, 
		void *res, int perm[]);
	void int_unlexify( const void *str, bool is_signed, int len, 
		void *res, int perm[]);

public: // TODO: make private
	void float_lexify(w_base_t::f4_t d, void *res, int perm[]) ;
	void float_unlexify( const void *str, int perm[], w_base_t::f4_t *result); 
	void dbl_lexify(w_base_t::f8_t d, void *res, int perm[]) ;
	void dbl_unlexify( const void *str, int perm[], w_base_t::f8_t *result);

	void Ibyteorder(int permutation[4]) ;
	void Fbyteorder(int permutation[4]) ;
	void Dbyteorder(int permutation[8]) ;

	static keytype convert(const key_type_s *k);

private:
	int I1perm[1];
	int I2perm[2];
	int I4perm[4];
	int Fperm[4];
	int Dperm[8];
};

extern class sortorder SortOrder;
#endif /*LEXIFY_H*/
