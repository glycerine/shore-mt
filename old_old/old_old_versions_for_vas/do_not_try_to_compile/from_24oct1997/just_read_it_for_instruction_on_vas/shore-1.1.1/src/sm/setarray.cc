/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header
 */
#define SETARRAY_C

#ifdef __GNUG__
#   pragma implementation
#endif

#include <ctype.h>
#include <stdlib.h>
#include <sm_int_0.h>
#include <umemcmp.h>
#include "e_error.h"
#include "basics.h"
#include "vec_t.h"
#include "setarray.h"

#define uMAX(a, b, sz) ((umemcmp(a, b, sz) > 0) ? a : b)
#define uMIN(a, b, sz) ((umemcmp(a, b, sz) < 0) ? a : b)

////////////////////////////////////////////////////////////////////////////
// rdtree.c                                                               //
//                                                                        //
//    For general comments, see setarray.h.                               //
////////////////////////////////////////////////////////////////////////////

///////////////////
// ARRAY METHODS //
///////////////////
array_t::array_t()
{
    sz = -1;
    nelems = 0;
    data = NULL;
}

array_t::array_t(int2 size, int2 elemcount, char *idata)
{
    w_assert3(size >= 0);
    w_assert3(elemcount >= 0);
    sz = size;
    nelems = elemcount;
    data = idata;
}

array_t::array_t(const array_t &a1)
{
    sz = a1.elemsz();
    nelems = a1.numelems();
    if (numelems() > 0)
      data = new char[a1.datalen()];
    else data = NULL;
    memcpy(data, a1.data, a1.datalen());
}

// for conversion from ascii for tcl
// for now, we only accept integers that fit in int's, separated by commas,
// e.g. {126, 43, 222,33}
array_t::array_t(const char *s)
{
    char *ptr = (char *)s;
    register int i = 0;
    int errval;
    int tmpi;

    sz = sizeof(int);
    nelems = 0;
    // number of elements is 1 + number of commas
    for (ptr = (char *)s; *ptr != '\0'; ptr++) {
	if (numelems() == 0 && isdigit(*ptr)) nelems++;
	else if (*ptr == ',') nelems++;
    }
    if (numelems() > 0)
      data = (char *)new int[numelems()];
    else data = NULL;

    ptr = (char *)s;
    while (isspace(*ptr)) ptr++;
    w_assert3(*ptr == '{');
    ptr++;

    for (i = 0; i < numelems() ; i++) {
	errval = sscanf(ptr, "%i", &(tmpi));
	w_assert3(errval == 1);
	memcpy((*this)[i], &tmpi, sizeof(tmpi));
	while (isdigit(*ptr)) ptr++;
	while (isspace(*ptr)) ptr++;
	w_assert3(*ptr == ',' || (i == numelems() - 1 && *ptr == '}'));
	ptr++;
	while (isspace(*ptr)) ptr++;
    } 
}
    

array_t& 
array_t::operator=(const array_t &orig)
{
    // don't try to overwrite this with itself
    if (this == &orig) return *this;

    if (data != NULL) delete [] data;
    sz = orig.elemsz();
    nelems = orig.numelems();
    if (numelems() > 0)
      data = new char[orig.datalen()];
    else data = NULL;
    memcpy(data, orig.data, orig.datalen());

    return *this;
}

//
// for tcl testing only!  This routine assumes we're dealing with 
// arrays of int4's, and is not to be used in general.
//
array_t::operator char*()
{
    register int i;
    int int4digits = 10; // max digits in a decimal representation of int4
    char *s, *ptr;

    ptr = s = new char[(int4digits + 1)*(numelems() + 2) + 2];
    sprintf(ptr, "%d.%d{", elemsz(), numelems());
    // BSD stupidity: sprintf doesn't return number of chars
    ptr += strlen(ptr);
    for (i = 0; i < numelems(); i++) {
	sprintf(ptr, "%d", *((int *)((*this)[i])));
	ptr += strlen(ptr);
	if (i < numelems() - 1) {
	    sprintf(ptr, ",");
	    ptr += strlen(ptr);
	}
    }
    sprintf(ptr, "}");
    
    return s;
}

//
// for tcl test only: representation for an array is like "2.4{1, 2, 3, 4}"
// 	(elemsz = 2, numelems = 4, rest is data)
//
void
array_t::put(const char* s)
{
    int n;
    register int i;
    char *ptr = (char *)s;

    n = sscanf((char* /*keep g++ happy*/)s, "%hi.%hi{", &sz, &nelems);
        w_assert1(n==2 && elemsz() == sizeof(int));
    ptr = strchr(s, '{') + 1;
    for (i = 0; i < numelems(); i++) {
	w_assert1((sscanf(ptr, "%d", (int*)((*this)[i]))) == 1);
	if (i < numelems() - 1) ptr++;
    }
}

// bytes2array -- transform stored key to array.
void
array_t::bytes2array(const char* key, int klen)
{
    array_t tmp;
    tmp.sz = *((int2 *)key);
    tmp.nelems = (klen - hdrlen())/tmp.elemsz();
    tmp.data = (char *)(key + hdrlen());
    operator=(tmp);
    tmp.data = NULL; // make sure that destructor doesn't free stuff in the key
}

// used by tcl test to print out arrays at specified indentation levels
void
array_t::print(int level) const
{
    register int j;
    char *buf;
    array_t tmp(*this);

    for (j = 0; j < 5 - level; j++) cout << "\t";
    cout << "---------- :\n";
    for (j = 0; j < 5 - level; j++) cout << "\t";
    buf = (char *)tmp;
    cout << buf << endl;
    delete [] buf;
}

/////////////////
// SET METHODS //
/////////////////

set_t::set_t(int2 size, int2 elemcount, char *idata)
      : array_t(size, elemcount, idata) // first set up as a array
{
    // in addition to normal array initialization, also sort and remove dups.
    sort();
    remove_dups();
}

void
set_t::remove_dups()
{
    register int i;
#ifdef UNDEF
    char tmpdata[numelems() * elemsz()];
#endif
    char* tmpdata = new char[numelems() * elemsz()];
    w_assert1(tmpdata);
    int tmpcnt = 0;

    for (i = 0; i < numelems(); i++) 
      if (i == 0 || 
	  memcmp((tmpdata + (tmpcnt - 1)*elemsz()), 
		 (*this)[i], elemsz())) {
	  memcpy((tmpdata + tmpcnt*elemsz()), (*this)[i], elemsz());
	  tmpcnt++;
      }
    
    /* if there were dups, tmpdata is too big.  copy it */
    if (tmpcnt < numelems())
     {
	 delete [] data;
	 data = new char[tmpcnt*elemsz()];
	 memcpy(data, tmpdata, tmpcnt*elemsz());
	 nelems = tmpcnt;
     }

     delete[] tmpdata;
}
	  

// swap two elements in a set
void 
set_t::swap(int i, int j)
{
    char *tmp = new char[elemsz()];
    memcpy(tmp, (*this)[i], elemsz());
    memcpy((*this)[i], (*this)[j], elemsz());
    memcpy((*this)[j], tmp, elemsz());
    delete [] tmp;
}

// set_t::sort -- quicksort
void 
set_t::sort(int low, int high)
{
    if (low >= high) return;
    int lo = low;
    int hi = high + 1;
    void* elem = (*this)[low];

    for (;;) {
	while (++lo < numelems() && umemcmp((*this)[lo], elem, elemsz()) < 0) ;
	while (umemcmp((*this)[--hi], elem, elemsz()) > 0);

	if (lo < hi)
	  swap(lo,hi);
	else break;
    }

    swap(low, hi);
    sort(low, hi-1);
    sort(hi+1, high);
}

set_t& 
set_t::operator=(const set_t &orig)
{
    // don't try to overwrite this with itself
    if (this == &orig) return *this;

    if (data != NULL) delete [] data;
    sz = orig.elemsz();
    nelems = orig.numelems();
    if (numelems() > 0)
      data = new char[numelems()*elemsz()];
    else data = NULL;
    memcpy(data, orig.data, elemsz()*numelems());

    return *this;
}

// set equality: compare sorted sets by merging
bool 
set_t::operator==(const set_t& other) const
{
    register int i;

    w_assert3(elemsz() == other.elemsz());

    if (numelems() != other.numelems()) return false;

    for (i=0; i<numelems(); i++) {
	if (memcmp((*this)[i], other[i], elemsz()) != 0)
	    return false;
    }
    return true;
}

// set intersect: works sort of like merge-join.  Walk down both sets
// (which are stored in sorted order) and output common elements.
set_t 
set_t::operator^(const set_t& other) const
{
    w_assert3(elemsz() == other.elemsz());

    int minelems = MIN(numelems(), other.numelems());
    register int acount = 0, bcount = 0, tmpcount = 0;
    char *tmpdata = new char[minelems*elemsz()];
    char *outdata;
    
    // form the intersection by merging the sorted arrays
    while(acount < numelems() && bcount < other.numelems())
     {
	 int diff = umemcmp((*this)[acount], other[bcount], elemsz());
	 if (diff < 0)
	   acount++;

	 else if (diff > 0)
	   bcount++;

	 else // found an element that's in both sets
	  {
	      memcpy(tmpdata + tmpcount*elemsz(), (*this)[acount++], elemsz());
	      tmpcount++;
	      bcount++;
	  }
     }

    // tmpdata is probably too big
    if (tmpcount < minelems)
     {
	 outdata = new char[tmpcount*elemsz()];
	 memcpy(outdata, tmpdata, tmpcount*elemsz());
	 delete [] tmpdata;
     }
    else
      outdata = tmpdata;

    return(set_t(elemsz(), tmpcount, outdata));
}
    
// set union: works sort of like merge-join.
// Walk down both sets, and output all distinct elements as you see them.
set_t 
set_t::operator+(const set_t& other) const 
{
    w_assert3(elemsz() == other.elemsz());

    int2 maxelems = numelems() + other.numelems();
    register int acount = 0, bcount = 0, tmpcount = 0;
    char *tmpdata = new char[maxelems*elemsz()];
    char *outdata;
    int diff;

    /* form the union by merging the sorted arrays */
    while(acount < numelems() && bcount < other.numelems())
     {
	 if ((diff = umemcmp((*this)[acount],other[bcount], elemsz())) <= 0)
	  {
	      memcpy(tmpdata + tmpcount*elemsz(), (*this)[acount++], elemsz());
	      if (diff == 0) 
		// they're equal, and we only output one,
		// so advance bcount as well.
		bcount++;
	  }
	 else 
	   memcpy(tmpdata + tmpcount*elemsz(), other[bcount++], elemsz());
	 tmpcount++;
     }

    // pick up the stragglers from this
    memcpy(tmpdata + tmpcount*elemsz(), (*this)[acount],
	   elemsz() * (numelems() - acount));
    tmpcount += numelems() - acount;

    // and pick up stragglers from other
    memcpy(tmpdata + tmpcount*elemsz(), other[bcount],
	   elemsz() * (other.numelems() - bcount));
    tmpcount += numelems() - bcount;
    
    // temp may be too big, if there was overlap.  copy into outdata
    if (tmpcount < maxelems)
     {
	 outdata = new char[tmpcount*elemsz()];
	 memcpy(outdata, tmpdata, tmpcount*elemsz());
	 delete [] tmpdata;
     }
    else
      outdata = tmpdata;

    return(set_t(elemsz(), tmpcount, outdata));
}

// increase this set by unioning in other
set_t& 
set_t::operator+=(const set_t& other) 
{
    set_t tmp;

    w_assert3(elemsz() == other.elemsz());

    tmp = *this + (other);
    if (tmp.numelems() > numelems()) 
      operator=(tmp);
    return(*this);
}

// is this set contained in other set?
bool 
set_t::operator/(const set_t& other) const
{
    w_assert3(elemsz() == other.elemsz());

    set_t tmp(operator^(other));
    if (tmp == other)
      return(true);
    else return(false);
}

// set overlap (i.e. non-empty intersection)
bool 
set_t::operator||(const set_t& other) const
{
    w_assert3(elemsz() == other.elemsz());

    set_t tmp(operator^(other));

    if (tmp.numelems() > 0)
      return(true);
    else return(false);
}

// set right -- is this set's biggest element bigger than other's biggest?
// We must break ties right to make < and > a matching total order!
bool 
set_t::operator>(const set_t& other) const
{
    register int i;
    int diff;

    w_assert3(elemsz() == other.elemsz());

    if (data == NULL) return false;
    else if (other.data == NULL) return true; // data can't be NULL at this pt.
    for (i = 0; i < numelems(); i++) {
	if (i == other.numelems())
	  return true;  // longer wins
	if ((diff = umemcmp((*this)[i], other[i], elemsz())) > 0)
	  return(true);
	if (diff < 0) return(false);
    }
    return false; // longer wins
}
      
// set left 
bool 
set_t::operator<(const set_t& other) const
{
    w_assert3(elemsz() == other.elemsz());

    register int i;
    int diff;

    w_assert3(elemsz() == other.elemsz());

    if (other.data == NULL) return false;
    else if (data == NULL) return true; // other.data can't be NULL at this pt.
    for (i = 0; i < other.numelems(); i++) {
	if (i == numelems())
	  return true;  // other is longer, so this is less
	if ((diff = umemcmp((*this)[i], other[i], elemsz())) < 0)
	  return(true);
	if (diff > 0) return(false);
    }
    return false; // longer wins
}

//////////////////////
// RANGESET METHODS //
//////////////////////

rangeset_t::rangeset_t()
{
    sz = -1;
    nranges = 0;
    span = 0;
    data = NULL;
}

rangeset_t::rangeset_t(int2 size, int2 rangecount, int2 itemcount, char *idata)
{
    w_assert3(size >= 0);
    w_assert3(rangecount >= 0);
    w_assert3(itemcount >= 0);
    sz = size;
    nranges = rangecount;
    span = itemcount;
    data = idata;
}

rangeset_t::rangeset_t(const rangeset_t &a1)
{
    // w_assert3(a1.isok());
    sz = a1.elemsz();
    nranges = a1.numranges();
    span = a1.area();
    if (numranges() > 0)
      data = new char[a1.datalen()];
    else data = NULL;
    memcpy(data, a1.data, a1.datalen());
}

// Convert a set to a rangeset of k keys.
// You can use fewer than k keys if you keep the error introduced less than
// tolerance.
rangeset_t::rangeset_t(const set_t &s1, int2 k, float tolerance)
{
    register int i;
    rangeset_t tmprs(s1.elemsz(), s1.numelems(), s1.numelems(), NULL);
    char *tmpdata = new char[s1.elemsz()*s1.numelems()*2];

    sz = s1.elemsz();
    data = NULL;
    tmprs.data = tmpdata;
    // default for k is s1.numelems(), i.e. no lossiness
    if (k == 0 && s1.numelems() != 0)
      k = s1.numelems();

    // create a trivial rangeset of s1.numelems() singleton ranges
    for (i = 0; i < s1.numelems(); i++) {
	memcpy(tmprs[i], s1[i], sz);
	memcpy(tmprs.upper(i), s1[i], sz);
    }

    if (tmprs.numranges() > k || tolerance > 0) 
      tmprs.compress(k, tolerance);

    (*this) = tmprs;   
}

// Compress a rangeset down to k ranges.
// We can compress it yet further, as long as we keep the error introduced
// lower than tolerance.  This is not yet implemented -- for now, we
// just compress to k ranges.
rc_t rangeset_t::compress(int2 k, float tolerance) 
{
    register int i;
    gap* gaps;
    rangeset_t tmprs((*this));
    int2 newranges;

    if (k >= numranges() && tolerance == 0)
      return RCOK;

//    cout << "Compress: " << (char *)(*this) << "\n\t ~= ";

    // We use a greedy algorithm to choose which ranges to smush
    // together (e.g. {1-7, 11-12} -> {1-12}).  The idea is to repeatedly
    // find the pair of consecutive ranges with the least space between them,
    // and smush those two, until we're down to k ranges.  To do that, we 
    // form the "gaps" array. The gaps array represents the spaces
    // between consecutive elements of s1. We sort the gaps array
    // by increasing gap size, and smush things in the order given by
    // the sorted gaps array.
    gaps = new gap[numranges() - 1];
    for (i = 0; i < numranges() - 1; i++) {
	gaps[i].ix = i;
	gaps[i].gapsz = (*(int4 *)lower(i+1)) - (*(int4 *)upper(i));
    }
    qsort((void *)gaps, numranges() - 1, sizeof(gap), gapcmp);
    
    // now walk down the gaps array and smush the first 
    // numelems() - k ranges
    for (i = 0; i < numranges() - k; i++)
      memcpy(tmprs.upper(gaps[i].ix), tmprs.upper(gaps[i].ix + 1), sz);
    
    // and now pick up the k distinct ranges from tmprs and put them in this
    for (i = 0, newranges = 0; i < numranges(); i++)
     {
	 if (i == 0 || (umemcmp(tmprs[i], tmprs.upper(i - 1), sz)) > 0) {
	     memcpy((*this)[newranges], tmprs[i], 2*sz);
	     newranges++;
	 }
	 else if (i > 0 && (umemcmp(tmprs.upper(i), tmprs.upper(i-1), sz)) > 0)
	   memcpy(upper(newranges - 1), tmprs.upper(i), sz);
     }
	
    nranges = newranges;
    
    // count up span
    for (i = 0, span = 0; i < numranges(); i++)
      span += ((*(int4 *)upper(i)) - (*(int4*)lower(i)) + 1);

    // data is probably too long.  make a new copy of the correct size
//    newdata = new char[datalen()];
//    memcpy(newdata, dataaddr(), datalen());
//    delete [] data;
//    data = newdata;
    delete [] gaps;
    
//    cout << (char *)(*this) << endl;
    // w_assert3(isok());
    return RCOK;
}

// gap comparison routine for qsort
int gapcmp(const void *i, const void *j)
{
    return((*((gap *)i)).gapsz - (*((gap *)j)).gapsz);
}

rangeset_t& 
rangeset_t::operator=(const rangeset_t &orig)
{
    // w_assert3(orig.isok());
    // don't try to overwrite this with itself
    if (this == &orig) return *this;

    if (data != NULL) delete [] data;
    sz = orig.elemsz();
    nranges = orig.numranges();
    span = orig.area();
    if (numranges() > 0)
      data = new char[orig.datalen()];
    else data = NULL;
    memcpy(data, orig.data, orig.datalen());

    return *this;
}

//
// for tcl test only! -- This assumes that we have sets of int4's, and it
// will not work in general.
//
rangeset_t::operator char*() const
{
    register int i;
    int int4digits = 10; // max digits in a decimal representation of int4
    char *s, *ptr;

    ptr = s = new char[(int4digits + 1)*(numranges() + 2) + 2];
    sprintf(ptr, "%d.%d.%d{", elemsz(), numranges(), area());
    // BSD stupidity: sprintf doesn't return number of chars
    ptr += strlen(ptr);
    for (i = 0; i < numranges(); i++) {
	if (umemcmp(lower(i), upper(i), elemsz()) == 0)
	  sprintf(ptr, "%d", *((int *)((*this)[i])));
	else
	  sprintf(ptr, "%d-%d", *((int *)((*this)[i])), 
		  *((int *)(upper(i))));
	ptr += strlen(ptr);
	if (i < numranges() - 1) {
	    sprintf(ptr, ", ");
	    ptr += strlen(ptr);
	}
    }
    sprintf(ptr, "}");
    
    return s;
}

//
// for tcl test only
//
void
rangeset_t::put(const char* s)
{
    set_t tmpset;
    tmpset.put(s);

    rangeset_t dummy(tmpset);
}

// used by tcl test to print out rangesets at specified indentation levels
void
rangeset_t::print(int level) const
{
    register int j;
    char *buf;
    rangeset_t tmp(*this);

    for (j = 0; j < 5 - level; j++) cout << "\t";
    cout << "---------- :\n";
    for (j = 0; j < 5 - level; j++) cout << "\t";
    buf = (char *)tmp;
    cout << buf << endl;
    delete [] buf;
}

// bytes2rangeset -- transform stored key to rangeset
void
rangeset_t::bytes2rangeset(const char* key, int )
{
    rangeset_t tmp;
    tmp.sz = *((int2 *)key);
    tmp.nranges = *((int2 *) (key + sizeof(int2)));
    tmp.span = *((int2 *) (key + 2*sizeof(int2)));
    tmp.data = (char *)(key + hdrlen());
    operator=(tmp);
    tmp.data = NULL; // make sure that destructor doesn't free stuff in the key
}

// rangeset equality: two rangesets are the same if they're the same size
// as their intersection (sneaky, huh?)
bool 
rangeset_t::operator==(const rangeset_t& other) const
{
    w_assert3(elemsz() == other.elemsz());

    if (area() != other.area()) return false;

    rangeset_t tmp(operator^(other));

    if (area() != tmp.area()) return false;
    
    return true;
}

// rangeset intersect -- works like merge-join.  Walk down the two 
// arrays, and when you find a range in this that overlaps with a range in
// other, output the overlapping range.
rangeset_t 
rangeset_t::operator^(const rangeset_t& other) const
{
    rangeset_t tmp(elemsz(), numranges() + other.numranges(), 0, NULL);
    register int i, thisix, otherix, newranges = 0;
    int diff;
    char *tmpdata = new char[2*elemsz()*(numranges() + other.numranges())];

    w_assert3(elemsz() == other.elemsz());
    tmp.data = tmpdata;
    for (thisix = 0, otherix = 0;  thisix < numranges(); /* nothing */) {
	// Skip all ranges in other that fall entirely to the left
	// of the current range in this.  
	for (; 
	     otherix < other.numranges() &&
	     umemcmp((*this)[thisix], other.upper(otherix), elemsz()) > 0;
	     otherix++)
	  /* no body in for loop */;

	// If all ranges in other are to the left of current range in this,
	// we're done.
	if (otherix == other.numranges())
	  break;
	else {
	    // If we reached a range in other
	    // that's entirely to the right of the current range in this,
	    // advance thisix.  Otherwise, we have an overlap.
	    if (umemcmp(other[otherix], upper(thisix), elemsz())
		> 0) {
		thisix++;
		continue; 
	    }
	    // form range from the greatest lower bound to the least upper bount
	    memcpy(tmp[newranges], uMAX(other[otherix], (*this)[thisix], 
					elemsz()), elemsz());
	    memcpy(tmp.upper(newranges),
		   uMIN(upper(thisix), other.upper(otherix), elemsz()),
		   elemsz());

	    // now, advance whichever of the two has the 
	    // least upper bound.  If equal, advance both.
	    if ((diff = 
		 umemcmp(upper(thisix), other.upper(otherix), elemsz())) < 0)
	      thisix++;
	    else if (diff > 0)
	      otherix++;
	    else {
		thisix++;
		otherix++;
	    }
	    newranges++;
	}
    }
    tmp.nranges = newranges;

    // count up span
    for (i = 0, tmp.span = 0; i < tmp.numranges(); i++)
      tmp.span += ((*(int4 *)tmp.upper(i)) - (*(int4*)tmp[i]) + 1);

    W_COERCE(tmp.compress());
    return(tmp);
}

    
// rangeset union.  This is pretty tricky, since ranges can overlap in many
// different ways, and we don't want to introduce any duplicates.  We do it
// in one pass, like a merge join, but handle the ways that ranges can overlap
// on a case-by-case basis.
rangeset_t 
rangeset_t::operator+(const rangeset_t& other) const 
{
    rangeset_t tmp(elemsz(), numranges() + other.numranges(), 0, NULL);
    char *tmpdata = new char[2*elemsz()*(numranges() + other.numranges())];
    register int thisix, otherix, newranges, i;
    int diff;

    w_assert3(elemsz() == other.elemsz());
    tmp.data = tmpdata;
    for (thisix = 0, otherix = 0, newranges = 0; 
	 thisix < numranges(); newranges++) {
	if (otherix >= other.numranges()) {
	    // We've used up other.  Add in the rest of this and quit
	    if ((diff = numranges() - thisix) > 0) {
		// First remaining range of this may overlap last range
		// we put into tmp.  Here we take this into account.
		if (newranges > 0 && 
		    umemcmp((*this)[thisix], tmp.upper(newranges - 1),
			    tmp.elemsz()) <= 0) {
		    memcpy(tmp.upper(newranges - 1), upper(thisix), 
			   tmp.elemsz());
		    thisix++;
		    diff--;
		}
		memcpy(tmp[newranges], (*this)[thisix], 2 * elemsz() * diff);
		newranges += diff;
	    }
	    break;
	}

	// difference between lower bound of other and lower bnd of this
	int lowlowdiff = umemcmp(other[otherix], (*this)[thisix], elemsz());
	// difference between upper bound of other and upper bnd of this
	int upupdiff = umemcmp(other.upper(otherix), upper(thisix), elemsz());

	if (lowlowdiff <= 0) {
	    // difference between upper bound of other and lower bnd of this
	    int uplowdiff = umemcmp(other.upper(otherix), (*this)[thisix], 
				   elemsz());
	    if (uplowdiff < 0) {
		// current range of other is entirely left of currange of this
		memcpy(tmp[newranges], other[otherix], elemsz());
		memcpy(tmp.upper(newranges), other.upper(otherix), elemsz());
		otherix++;
	    }
	    else if (upupdiff <= 0) {
		// should prepend currange of other to currange of this
		memcpy(tmp[newranges], other[otherix], elemsz());
		memcpy(tmp.upper(newranges), upper(thisix), elemsz());
		otherix++;
		// skip all ranges of other that are subsumed 
		while (otherix < other.numranges()
		       && umemcmp(other.upper(otherix), upper(thisix), elemsz())
		          <= 0)
		  otherix++;
		thisix++;
	    }
	    else if (upupdiff > 0) {
		// currange of other subsumes currange of this
		memcpy(tmp[newranges], other[otherix], elemsz());
		memcpy(tmp.upper(newranges), other.upper(otherix), elemsz());
		thisix++;
		// skip all ranges of this that are also subsumed
		while (thisix < numranges()
		       && umemcmp(other.upper(otherix), upper(thisix), elemsz())
		          >= 0)
		  thisix++;
		otherix++;
	    }
	    
	}
	else { // lowlowdiff > 0
	// difference between lower bound of other and upper bnd of this
	int lowupdiff = umemcmp(other[otherix], upper(thisix), elemsz());

	    if (lowupdiff > 0) {
		// current range of this is entirely left of currange of other
		memcpy(tmp[newranges], (*this)[thisix], elemsz());
		memcpy(tmp.upper(newranges), upper(thisix), elemsz());
		thisix++;
	    }
	    else if (upupdiff >= 0) {
		// should prepend currange of this to currange of other
		memcpy(tmp[newranges], (*this)[thisix], elemsz());
		memcpy(tmp.upper(newranges), other.upper(otherix), elemsz());
		thisix++;
		// skip all ranges of this that are subsumed 
		while (thisix < numranges()
		       && umemcmp(upper(thisix), other.upper(otherix), elemsz())
		          <= 0)
		  thisix++;
		otherix++;
	    }
	    else if (upupdiff < 0) {
		// currange of this subsumes currange of other
		memcpy(tmp[newranges], (*this)[thisix], elemsz());
		memcpy(tmp.upper(newranges), upper(thisix), elemsz());
		otherix++;
		// skip all ranges of other that are also subsumed
		while (otherix < other.numranges() 
		       && umemcmp(upper(thisix), other.upper(otherix), elemsz())
		          >= 0)
		  otherix++;
		thisix++;
	    }
	} // lowlowdiff > 0
	// The new range we added may overlap the preceding range.  If this
	// is the case, just set the upper bound of the preceding range
	// to be the upper bound of this range, and decrement newranges.
	if (newranges > 0 && 
	    umemcmp(tmp.lower(newranges), tmp.upper(newranges - 1),
		    tmp.elemsz()) <= 0) {
	    memcpy(tmp.upper(newranges - 1), tmp.upper(newranges), 
		   tmp.elemsz());
	    newranges--;
	}
    } // for loop
    
    // add in the rest of other
    if ((diff = other.numranges() - otherix) > 0) {
	// First remaining range of other may overlap last range
	// we put into tmp.  Here we take this into account.
	if (newranges > 0 && 
	    umemcmp(tmp.lower(newranges - 1), other[otherix],
		    tmp.elemsz()) <= 0) {
	    memcpy(tmp.upper(newranges - 1), other.upper(otherix),
		   tmp.elemsz());
	    otherix++;
	    diff--;
	}
	memcpy(tmp[newranges], other[otherix], 2 * elemsz() * diff);
	newranges += diff;
    }

    // count up span
    for (i = 0, tmp.span = 0; i < newranges; i++)
      tmp.span += ((*(int4 *)tmp.upper(i)) - (*(int4*)tmp[i]) + 1);
    tmp.nranges = newranges;

//    cout << "Union: " << (char *)(*this)
//	 << "\n\t + " <<  (char *)other
//	 << "\n\t = " << (char *)tmp
//	 << "\n";

    W_COERCE(tmp.compress());

    // w_assert3(tmp.isok());
    // w_assert3(tmp / (*this)  && tmp / other);
    return(tmp);
}

// increase rangeset by unioning in other
rangeset_t& 
rangeset_t::operator+=(const rangeset_t& other) 
{
    rangeset_t tmp;

    w_assert3(elemsz() == other.elemsz());

    tmp = *this + (other);
    operator=(tmp);
    return(*this);
}

// is this rangeset contained in other rangeset?
bool 
rangeset_t::operator/(const rangeset_t& other) const
{
    w_assert3(elemsz() == other.elemsz());

    rangeset_t tmp(operator^(other));
    if (tmp == other)
      return(true);
    else return(false);
}

// rangeset overlap (i.e. non-empty intersection)
bool 
rangeset_t::operator||(const rangeset_t& other) const
{
    w_assert3(elemsz() == other.elemsz());

    rangeset_t tmp(operator^(other));

    if (tmp.numranges() > 0)
      return(true);
    else return(false);
}

// rangeset right --
// We must break ties right to make < and > a matching total order!
bool 
rangeset_t::operator>(const rangeset_t& other) const
{
    w_assert3(elemsz() == other.elemsz());
    register int i;
    int diff;

    if (data == NULL) return false;
    else if (other.data == NULL) return true; // data can't be NULL at this pt.
    for (i = 0; i < numranges(); i++) {
	if (i == other.numranges())
	  return true;  // longer wins
	if ((diff = umemcmp((*this)[i], other[i], elemsz())) > 0)
	  return(true);
	if (diff < 0) return(false);
	if ((diff = umemcmp(upper(i), other.upper(i), elemsz())) > 0)
	  return(true);
	if (diff < 0) return(false);
    }
    return false; // longer wins
}
      
// rangeset left 
bool 
rangeset_t::operator<(const rangeset_t& other) const
{
    register int i;
    int diff;

    w_assert3(elemsz() == other.elemsz());

    if (other.data == NULL) return false;
    else if (data == NULL) return true; // other.data can't be NULL at this pt.
    for (i = 0; i < other.numranges(); i++) {
	if (i == numranges())
	  return true;  // other is longer, so this is less
	if ((diff = umemcmp((*this)[i], other[i], elemsz())) < 0)
	  return(true);
	if (diff > 0) return(false);
	if ((diff = umemcmp(upper(i), other.upper(i), elemsz())) < 0)
	  return(true);
	if (diff > 0) return(false);
    }
    return false; // longer wins
}

// sanity check routine
bool
rangeset_t::isok() const
{
    register int i;

    if (elemsz() <= 0) return(false);
    if (numranges() < 0) return(false);
    if (numranges() == 0) return (true);

    if (area() <= 0) return(false); // since numranges > 0
    if (area() < numranges()) return (false);

    for (i = 0; i < numranges(); i++) {
	if (i > 0 && (umemcmp(lower(i), upper(i - 1), elemsz()) < 0))
	  return(false);
	if (umemcmp(lower(i), upper(i), elemsz()) > 0)
	  return(false);
    }
    return(true);
}

