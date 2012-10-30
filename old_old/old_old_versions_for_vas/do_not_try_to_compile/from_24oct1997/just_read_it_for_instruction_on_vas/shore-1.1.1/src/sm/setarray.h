/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: setarray.h,v 1.14 1995/07/14 22:07:18 nhall Exp $
 */

#ifndef SETARRAY_H
#define SETARRAY_H

///////////////////////////////////////////////////////////////////////////////
// setarray.[ch]                                                             //
//       These files implement storage manager arrays, sets, and "rangesets" //
// of k-byte strings.  Within a given array, set, or rangeset, all strings   //
// are of equal length.  There is currently no support for variable-length   //
// strings.                                                                  //
///////////////////////////////////////////////////////////////////////////////

#include "nbox.h"

#define RS_NUMRANGES 20 // default number of ranges in a rangeset
#define RS_TOLERANCE 0  // default error tolerance we allow in a rangeset

#ifdef __GNUG__
#pragma interface
#endif

#define NORET

// Array: a Variable length array of k-byte strings
class array_t {
protected:
    int2 sz;                    // # of bytes in an element
    int2 nelems;	        // number of elements in array
    char *data;	                // storage for actual data
public:
    NORET array_t();
    NORET array_t(int2 size, int2 elemcount, char *idata);
    NORET array_t(const array_t&);
    NORET array_t(const char* key, int klen) {// for conversion from tuple key 
    	// initialize to avoid Purify errors
    	nelems = 0;
    	data = NULL;
    	bytes2array(key, klen);
    }
    NORET array_t(const char* s);		// for conversion from ascii for tcl
    NORET ~array_t() { if (data != NULL) delete [] data; };
    
    numelems() const { return (int)nelems; };
    elemsz() const { return (int)sz; };
    array_t& operator=(const array_t &orig);
    void *operator[] (int ix) const
     { w_assert3(ix>=0);  assert(ix<=nelems); 
       return ((void *)(data + sz*ix)); };
    const void* dataaddr() const { return (void *)data; }

    void bytes2array(const char *key, int klen);
    const void* kval() const { return (void *) this; }
    int   klen() const { return 2*sizeof(int2) + sz*nelems; }
    static int hdrlen() { return 2*sizeof(int2); }
    int datalen() const { return sz*nelems; }
    bool isempty() const { return (numelems() == 0); }

    //
    // for tcl use only
    //
    operator char*();
    void put(const char*);  // conversion from ascii for tcl

    void print(int level) const;
};

// set_t: a set, stored as a sorted array with no duplicates
class set_t : public array_t {
    void swap(int, int);
    void sort(int, int);
    void sort(){ sort(0, nelems - 1); };
    void remove_dups();
public:
    NORET set_t() : array_t() {} ;
    NORET set_t(int2 size, int2 elemcount, char *idata);
    NORET set_t(const char* s, int len): array_t(s, len) {};
    // for conversion from ascii for tcl
    NORET set_t(const char* s): array_t(s) {sort(); remove_dups();};
    NORET set_t(const set_t &s): array_t(s) {};

    //
    // some binary operations:
    //	^: intersection  ->  set
    //	+: union  ->  set 
    //	+=: enlarge by unioning in the new set
    //	==: exact match  ->  boolean
    //	/: containment   ->  boolean
    //	||: overlap	 ->  boolean (i.e. non-empty intersection)
    //	>: bigger (compare high values) -> boolean
    //	<: smaller (compare low values) -> boolean
    //	*: square of distance between centers of two boxes ??
    //
    set_t operator^(const set_t& other) const;
    set_t operator+(const set_t& other) const;
    
    set_t& operator+=(const set_t& other);
    set_t& operator=(const set_t& other);
    bool operator==(const set_t& other) const;
    bool operator!=(const set_t& other) const
     { return !(operator==(other)); }
    bool operator/(const set_t& other) const;
    bool operator||(const set_t& other) const;
    bool operator>(const set_t& other) const;
    bool operator<(const set_t& other) const;
    double operator*(const set_t& other) const;
};

// rangeset: a set of ranges, e.g. {1-7, 11-14, 16-16}, which represents 
// the set {1, 2, 3, 4, 5, 6, 7, 11, 12, 13, 14, 16}.
class rangeset_t {
    int2 sz;			// # of bytes per data item
    int2 nranges;		// number of ranges in set
    int2 span; 			// sum of widths of ranges in set
    char *data;			// storage for actual data
  public:
    NORET rangeset_t();
    NORET rangeset_t(int2 size, int2 rangecount, int2 itemcount, char *idata);
    NORET rangeset_t(const rangeset_t&);
    NORET rangeset_t(const char *key, int klen) {// conversion from tuple key
	// initialize to avoid Purify errors
	nranges = 0;
	span = 0;
	data = NULL;
	bytes2rangeset(key, klen);
    }
    NORET rangeset_t(const set_t&, int2 numranges = RS_NUMRANGES, 
	       float tolerance = RS_TOLERANCE);
    NORET ~rangeset_t() { if (data != NULL) delete [] data; }
    
    void bytes2rangeset(const char *key, int klen);
    rc_t compress(int2 k = RS_NUMRANGES, float tolerance = RS_TOLERANCE);

    numranges() const {return (int)nranges; }
    area() const {return (int)span; }
    elemsz() const { return (int)sz; }
    rangeset_t& operator=(const rangeset_t &orig);

    // operator[i] gives the lower bound of the i'th range
    void *operator[] (int ix) const
     { w_assert3(ix>=0);  assert(ix<=nranges); 
       return ((void *)(data + 2*sz*ix)); };
    void *lower(int ix) const { return (*this)[ix];}
    void *upper(int ix) const { return (void*) (((char*)(*this)[ix]) + sz);}

    const void* dataaddr() const {return (void *)data; }
    const void* kval() const { return (void *)this; }
    int klen() const { return 3*sizeof(int2) + 2*sz*nranges; }
    static int hdrlen() { return 3*sizeof(int2); }
    int datalen() const { return 2*sz*nranges; }
    bool isempty() const { return (numranges() == 0); }
    bool isok() const;

    //
    // for tcl use only
    //
    operator char*() const;
    void put(const char*);  // conversion from ascii for tcl
    void print(int level) const;

    //
    // some binary operations:
    //	^: intersection  ->  set
    //	+: union  ->  set 
    //	+=: enlarge by unioning in the new set
    //	==: exact match  ->  boolean
    //	/: containment   ->  boolean
    //	||: overlap	 ->  boolean (i.e. non-empty intersection)
    //	>: bigger (compare high values) -> boolean
    //	<: smaller (compare low values) -> boolean
    //	*: square of distance between centers of two boxes ??
    //
    rangeset_t operator^(const rangeset_t& other) const;
    rangeset_t operator+(const rangeset_t& other) const;
    
    rangeset_t& operator+=(const rangeset_t& other);
    bool operator==(const rangeset_t& other) const;
    bool operator!=(const rangeset_t& other) const
     { return !(operator==(other)); }
    bool operator/(const rangeset_t& other) const;
    bool operator||(const rangeset_t& other) const;
    bool operator>(const rangeset_t& other) const;
    bool operator<(const rangeset_t& other) const;
    double operator*(const rangeset_t& other) const;
};

// used in compressing rangesets.  A gap is the size of the space between 
// range ix and range ix + 1.
struct gap {
    int ix;
    int gapsz;
};

int gapcmp(const void *i, const void *j);
#endif				// SETARRAY_H
