/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: nbox.h,v 1.3 1997/06/13 22:32:17 solomon Exp $
 */
#ifndef NBOX_H
#define NBOX_H

#include <stdio.h>
#include <iostream.h>

//??  /* get configuration definitions from config/config.h */
//??  #include <config.h>

#include <w_workaround.h>
#include <w.h>


//
// spatial object class: n-dimensional box 
//	represented as: xlow, ylow, zlow, xhigh, yhigh, zhigh
// 

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif  // MIN

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif  // MAX

#ifndef ABS
#define ABS(x) ((x) >= 0 ? (x) : -(x))
#endif  // ABS



#ifdef __GNUG__
#pragma interface
#endif

class nbox_t {
	friend ostream& operator<<(ostream& os, const nbox_t& box);

public:
	const w_base_t::int4_t	max_int4 = 0x7fffffff;	/*  (1 << 31) - 1;  */
	const w_base_t::int4_t	min_int4 = 0x80000000; 	/* -(1 << 31);	*/
	const int max_dimension = 4;

private:
	struct fill4 {
	    w_base_t::int4_t u4;
	    fill4() : u4(0) {}
	};

public:
	enum sob_cmp_t { t_exact = 1, t_overlap, t_cover, t_inside, t_bad };

protected:
	int  array[2*max_dimension];	// boundary points
	int  dim;  			// dimension
private:
	fill4	filler; 		// 8 byte alignment

	int* box() { return array; }	// reveal internal storage

public:
	nbox_t();
	nbox_t(int dimension);
	nbox_t(int dimension, int box[]);
	nbox_t(const nbox_t& nbox);
	nbox_t(const char* s, int len);	// for conversion from tuple key 
	nbox_t(const char* s);		// for conversion from ascii for tcl

	virtual ~nbox_t() {}

	int dimension() const	 { return dim; }
	int bound(int n) const	 { return array[n]; }
	int side(int n) const 	 { return array[n+dim]-array[n]; }
	int center(int n) const { return (array[n+dim]-array[n])/2+array[n]; }

	bool  empty() const;	// test if box is empty
	void    squared();	// make the box squared
	void    nullify();	// make the box empty

	int hvalue(const nbox_t& universe, int level=0) const; // hilbert value
	int hcmp(const nbox_t& other, const nbox_t& universe, 
			int level=0) const; // hilbert value comparison

	void print(int level) const;
	void draw(int level, FILE* DrawFile, const nbox_t& CoverAll) const;

	//
	// area of a box :
	//	>0 : valid box
	//	=0 : a point
	//	<0 : null box 
	//
	double area() const;

	//
	// margine of a Rectangle
	//
	int margin();

	//
	// some binary operations:
	//	^: intersection  ->  box
	//	+: bounding box  ->  box (result of addition)
	//	+=: enlarge by adding the new box  
	//	==: exact match  ->  boolean
	//	/: containment   ->  boolean
	//	||: overlap	 ->  boolean
	//	>: bigger (comapre low values) -> boolean
	//	<: smaller (comapre low values) -> boolean
	//	*: square of distance between centers of two boxes 
	//
	nbox_t operator^(const nbox_t& other) const;
	nbox_t operator+(const nbox_t& other) const;

	nbox_t& operator+=(const nbox_t& other);
	nbox_t& operator=(const nbox_t& other);
	bool operator==(const nbox_t& other) const;
	bool operator/(const nbox_t& other) const;
	bool operator||(const nbox_t& other) const;
	bool operator>(const nbox_t& other) const;
	bool operator<(const nbox_t& other) const;
	double operator*(const nbox_t& other) const;

	//
	// for tcl use only
	//
	operator char*();
	void put(const char*);  // conversion from ascii for tcl

	//
	// conversion between key and box
	//
	void  bytes2box(const char* key, int klen);
	const void* kval() const { return (void *) array; }
	int   klen() const { return 2*sizeof(int)*dim; }

};

inline nbox_t& nbox_t::operator=(const nbox_t& other)
{
    register i;
    dim = other.dim;
    for (i=0;i<dim*2;i++) array[i] = other.array[i];
    return *this;
}

#endif // NBOX_H
