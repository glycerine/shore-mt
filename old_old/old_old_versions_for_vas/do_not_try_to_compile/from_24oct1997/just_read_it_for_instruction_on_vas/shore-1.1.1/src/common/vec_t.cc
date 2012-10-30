/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/vec_t.cc,v 1.55 1997/09/19 11:50:06 solomon Exp $
 */

#ifdef __GNUC__
#pragma implementation "vec_t.h"
#endif

#define VEC_T_C
#include <stdlib.h>
#include <memory.h>
#include <iostream.h>
#include "w_base.h"
#include "w_minmax.h"
#include "basics.h"
#include "dual_assert.h"
#include "vec_t.h"
#include "umemcmp.h"


#ifdef __GNUC__
// these templates are not used by vec_t, but are so common that
// we instantiate them here
template class w_auto_delete_array_t<cvec_t>;
template class w_auto_delete_array_t<vec_t>;
#endif


cvec_t		cvec_t::pos_inf;
cvec_t		cvec_t::neg_inf;
void		*cvec_t::zero_location=(void *)&(cvec_t::neg_inf);
vec_t&		vec_t::pos_inf = *(vec_t*) &cvec_t::pos_inf;
vec_t&		vec_t::neg_inf = *(vec_t*) &cvec_t::neg_inf;

cvec_t::cvec_t(const cvec_t& /*v*/)
{
    cerr << "cvec_t: disabled member called" << endl;
    cerr << "failed at \"" << __FILE__ << ":" << __LINE__ 
	 << "\"" << endl;
    abort();
}

cvec_t::~cvec_t()
{
    if (_is_large()) {
	free(_base);
    }
}

void cvec_t::split(size_t l1, cvec_t& v1, cvec_t& v2) const
{
    size_t min = 0;
    int i;
    for (i = 0; i < _cnt; i++)  {
	if (l1 > 0)  {
	    min = (_base[i].len > l1) ? l1 : _base[i].len;
	    v1.put(_base[i].ptr, min);
	    l1 -= min; 
	}
	if (l1 <= 0) break;
    }
    
    for ( ; i < _cnt; i++)  {
	v2.put(_base[i].ptr + min, _base[i].len - min);
	min = 0;
    }
}

cvec_t& cvec_t::put(const cvec_t& v, size_t start, size_t num_bytes)
{
    int i;
    size_t start_byte, start_elem, total;

    if (v.size() < start+num_bytes) {
	dual_assert3(v.size() >= start+num_bytes );
    }

    // find start in v
    for (i = 0, total = 0; i < v._cnt && total <= start; i++) {
	total += v._base[i].len;
    }
   
    // check for possible overflow
    if (_cnt+v._cnt > max_small) {
	_grow(_cnt+v._cnt);
    }

    start_elem = i-1;
    // first byte in the starting array element
    start_byte = start - (total - v._base[start_elem].len);

    // fill in the new vector
    _base[_cnt].ptr = v._base[start_elem].ptr+start_byte;
    _base[_cnt].len = v._base[start_elem].len-start_byte;
    _cnt++;
    w_assert3(_cnt <= _max_cnt()); 
    for (i = 1, total = _base[_cnt-1].len; total < num_bytes; i++) {
	_base[_cnt].ptr = v._base[start_elem+i].ptr;
	_base[_cnt].len = v._base[start_elem+i].len;
	total += _base[_cnt++].len;
	w_assert3(_cnt <= _max_cnt()); 
    }
    _base[_cnt - 1].len -= total-num_bytes;

    _size += num_bytes;
    dual_assert3(check_size());
    return *this;
}

cvec_t& cvec_t::put(const void* p, size_t l)  
{
    if (_cnt+1 > max_small) {
	_grow(_cnt+1);
    }

    // to make zvecs work:
    _base[_cnt].ptr = (unsigned char*)p; 
    _base[_cnt].len = l;
    if(l>0) {
	_cnt++;
   	w_assert3(_cnt <= _max_cnt()); 
	_size += l;
    }
    return *this;
}

bool cvec_t::check_size() const
{
    dual_assert1(_size == recalc_size());
    return true;
}

size_t cvec_t::recalc_size() const 
{
    // dual_assert1(! is_pos_inf() && ! is_neg_inf() );
    size_t l;
    int i;
    for (i = 0, l = 0; i < _cnt; l += _base[i++].len);
    return l;
}

// Scatter:
// write data from void *p into the area described by this vector.
// Copy no more than "limit" bytes. 
// Does NOT update the vector so caller must keep track
// of the limit to know what is good and what isn't.
// It is a const function because it does not update *this
// even though it does update that which *this describes.
const vec_t& vec_t::copy_from(
    const void* p,
    size_t limit,
    size_t offset)		 const// offset in the vector
{
    dual_assert1(! is_pos_inf() && ! is_neg_inf() );
    dual_assert1( _base[0].ptr != zero_location );

    char* s = (char*) p;
    for (int i = 0; (i < _cnt) && (limit>0); i++) {
	if ( offset < _base[i].len ) {
	    size_t elemlen = ((_base[i].len - offset > limit)?
				 limit : (_base[i].len - offset)) ;
	    memcpy((char*)_base[i].ptr + offset, s, elemlen); 
	    w_assert3(limit >= elemlen);
	    limit -= elemlen;
	    s += elemlen;
	} 
	if (_base[i].len > offset) {
	    offset = 0;
	} else {
	    offset -= _base[i].len;
	}
    }
    return *this;
}

// copies from vec to p;  returns # bytes copied
size_t cvec_t::copy_to(void* p, size_t limit) const 
{
    dual_assert1(! is_pos_inf() && ! is_neg_inf() );
    char* s = (char*) p;
    for (int i = 0; i < _cnt && limit > 0; i++) {
	size_t n = limit > _base[i].len ? _base[i].len : limit;
	if(_base[i].ptr == zero_location) {
	    memset(s, '\0', n);
	} else {
	    memcpy(s, _base[i].ptr, n);
	}
	w_assert3(limit >= n);
	limit -= n;
	s += n;
    }
    return s - (char*)p;
}

// copies from s to this
vec_t& vec_t::copy_from(const cvec_t& s) 
{
    bool	zeroing=false;
    int 	j = 0;
    char* 	p = (char*) _base[j].ptr;
    size_t 	l = _base[j].len;

    dual_assert1(size() >= s.size());
    dual_assert1(_base[0].ptr != zero_location);
    
    for (int i = 0; i < s._cnt; i++)  {
	const unsigned char* pp = s._base[i].ptr;
	if(pp == zero_location) zeroing = true;
	size_t left = s._base[i].len;
	while (l <= left && j < _cnt)  {
	    if( zeroing) {
		memset(p, '\0', l);
	    } else {
		memcpy(p, pp, l);
	    }
	    pp += l;
	    left -= l;
	    j++;
	    if (j >= _cnt) break;  // out of while loop
	    l = _base[j].len;
	    p = (char*) _base[j].ptr;
	}
	if (left)  {
	    if( zeroing) {
		memset(p, '\0', left);
	    } else {
		memcpy(p, pp, left);
	    }
	    pp += left;
	    w_assert3(l >= left);
	    l -= left;
	}
    }
    return *this;
}

vec_t& vec_t::copy_from(const cvec_t& ss, size_t offset, size_t limit, size_t myoffset)
{
    bool	zeroing=false;
    vec_t s;
    s.put(ss, offset, limit);

    dual_assert1(size() >= s.size());
    dual_assert1(_base[0].ptr != zero_location);

    size_t ssz = s.size(), dsz = size();
    if (offset > ssz) 		offset = ssz;
    if (limit > ssz - offset)   limit = ssz - offset;
    if (myoffset > dsz)		offset = dsz;

    int j;
    for (j = 0; j < _cnt; j++)  {
	if (myoffset > _base[j].len)
	    myoffset -= _base[j].len;
	else  
	    break;
    }
    char* p = ((char*)_base[j].ptr) + myoffset;
    size_t l = _base[j].len - myoffset;

    dual_assert1(dsz <= limit);

    size_t done;
    int i;
    for (i = 0, done = 0; i < s._cnt && done < limit; i++)  {
	const unsigned char* pp = s._base[i].ptr;
	if(pp == zero_location) zeroing = true;
	size_t left = s._base[i].len;
	if (limit - done < left)  left = limit - done;
	while (l < left)  {
	    if(zeroing) {
		memset(p, '\0', l);
	    } else {
		memcpy(p, pp, l);
	    }
	    done += l, pp += l;
	    left -= l;
	    j++;
	    if (j >= _cnt) break;  // out of while loop
	    l = _base[j].len;
	    p = (char*) _base[j].ptr;
	}
	if (left)  {
	    if(zeroing) {
		memset(p, '\0', left);
	    } else {
		memcpy(p, pp, left);
	    }
	    pp += left;
	    l -= left;
	    done += left;
	}
    }
    return *this;
}

#ifdef UNDEF
void cvec_t::common_prefix(
    const cvec_t& v1,
    const cvec_t& v2,
    cvec_t& ret)
{
    size_t l1 = v1.size();
    size_t l2 = v2.size();
    int i1 = 0;
    int i2 = 0;
    int j1 = 0;
    int j2 = 0;

    while (l1 && l2) {
	if (v1._ptr[i1][j1] - v2._ptr[i2][j2])  break;
	if (++j1 >= v1._len[i1])	{ j1 = 0; ++i1; }
	if (++j2 >= v2._len[i2])	{ j2 = 0; ++i2; }
	--l1, --l2;
    }

    if (l1 < v1.size())  {
	ret.put(v1, 0, v1.size() - l1);
    }
}
#endif /*UNDEF*/

cvec_t& cvec_t::put(const cvec_t& v)
{
    dual_assert1(! is_pos_inf() && ! is_neg_inf());
    if (_cnt+v._cnt > max_small) {
	_grow(_cnt+v._cnt);
    }
    for (int i = 0; i < v._cnt; i++)  {
	_base[_cnt + i].ptr = v._base[i].ptr;
	_base[_cnt + i].len = v._base[i].len;
    }
    _cnt += v._cnt;
    w_assert3(_cnt <= _max_cnt()); 
    _size += v._size;

    dual_assert3(check_size());
    return *this;
}

int cvec_t::cmp(const void* s, size_t l) const
{
    if (is_pos_inf()) return 1;
    if (is_neg_inf()) return -1;

    size_t acc = 0;
    for (int i = 0; i < _cnt && acc < l; i++)  {
	int d = umemcmp(_base[i].ptr, ((char*)s) + acc,
		       _base[i].len < l - acc ? _base[i].len : l - acc);
	if (d) return d;
	acc += _base[i].len;
    }
    return acc - l;	// longer wins
}

int cvec_t::cmp(const cvec_t& v, size_t* common_size) const
{
    // dual_assert1(! (is_pos_inf() && v.is_pos_inf()));
    // dual_assert1(! (is_neg_inf() && v.is_neg_inf()));

    // it is ok to compare +infinity with itself or -infinity with itself
    if (&v == this) {	// same address
	if (common_size) *common_size = v.size();
	return 0; 
    }

    // Common size is not set when one of operands is infinity 
    if (is_neg_inf() || v.is_pos_inf())  return -1;
    if (is_pos_inf() || v.is_neg_inf())  return 1;

	
    // Return value : 0 if equal; common_size not set
    //              : <0 if this < v
    //                 or v is longer than this, but equal in
    //                 length of this
    //              : >0 if this > v
    //		       or this is longer than v, but equal in 
    //                 length of v

    int result = 0;

    // l1, i1, j1 refer to this
    // l2, i2, j2 refer to v

    size_t l1 = size();
    size_t l2 = v.size();
    int i1 = 0;
    int i2 = 0;
    size_t j1 = 0, j2 = 0;
    
    while (l1 && l2)  {
	dual_assert3(i1 < _cnt);
	dual_assert3(i2 < v._cnt);
	size_t min = _base[i1].len - j1;
	if (v._base[i2].len - j2 < min)  min = v._base[i2].len - j2;

	dual_assert3(min > 0);
	result = umemcmp(&_base[i1].ptr[j1], &v._base[i2].ptr[j2], min);
	if (result) break;
	
	if ((j1 += min) >= _base[i1].len)	{ j1 = 0; ++i1; }
	if ((j2 += min) >= v._base[i2].len)	{ j2 = 0; ++i2; }

	l1 -= min, l2 -= min;
    }

    if (result)  {
	if (common_size)  {
	    while (_base[i1].ptr[j1] == v._base[i2].ptr[j2])  {
		++j1, ++j2;
		--l1;
	    }
	    *common_size = (l1 < size() ? size() - l1 : 0);
	}
    } else {
	result = l1 - l2; // longer wins
	if (result && common_size)  {
	    // common_size is min(size(), v.size());
	    if (l1 == 0) {
		*common_size = size();
	    } else {
		w_assert3(l2 == 0);
		*common_size = v.size();
	    }
	    w_assert3(*common_size == MIN(size(), v.size()));
	}
    }
    return result;
}

int cvec_t::checksum() const 
{
    int sum = 0;
    dual_assert1(! is_pos_inf() && ! is_neg_inf() );
    for (int i = 0; i < _cnt; i++) {
	for(size_t j=0; j<_base[i].len; j++) sum += ((char*)_base[i].ptr)[j];
    }
    return sum;
}

void cvec_t::calc_kvl(uint4& rh) const
{
    if (size() <= sizeof(uint4))  {
	rh = 0;
	copy_to(&rh, size());
    } else {
	register h, g;
	h = g = 0;
	for (int i = 0; i < _cnt; i++)  {
	    register const unsigned char* s = _base[i].ptr;
	    for (size_t j = 0; j < _base[i].len; j++) {
		h = (h << 4) + *s++;
		if ((g = h & 0xf000000)) h ^= g >> 24;
		h &= ~g;
	    }
	}
	rh = h;
    }
}

void cvec_t::_grow(int total_cnt)
{
    w_assert3(total_cnt > max_small);
    w_assert3(_cnt <= _max_cnt()); 

    int prev_max = _max_cnt();
  
    if (total_cnt > prev_max) {
	// overflow will occur

	int grow_to = MAX(prev_max*2, total_cnt);
	vec_pair_t* tmp = 0;

	if (_is_large()) {
	    tmp = (vec_pair_t*) realloc((char*)_base, grow_to * sizeof(*_base));
	    if (!tmp) W_FATAL(fcOUTOFMEMORY)
	} else {
	    tmp = (vec_pair_t*) malloc(grow_to * sizeof(*_base));
	    if (!tmp) W_FATAL(fcOUTOFMEMORY)
	    for (int i = 0; i < prev_max; i++) {
		tmp[i] = _base[i];
	    }
	}
	_pair[0].len = grow_to;
	_base = tmp;
    }
}

#include <ctype.h>

ostream& operator<<(ostream& o, const cvec_t& v)
{
    char	*p;
    u_char 	c;
    int		nparts = v.count();
    int		i = 0;
    size_t	j = 0;
    size_t	l = 0;

    o << "{ ";
    for(i=0; i< nparts; i++) {
	// l = len(i);
	l = (i < v._cnt) ? v._base[i].len : 0;

	// p = (char *)v.ptr(i);
	p = (i < v._cnt) ? (char *)v._base[i].ptr : 0; 

	o << "{" << l << " " << "\"" ;

	for(j=0; j<l; j++,p++) {
	    c = *p;
	    if( c == '"' ) {
		o << "\\\"" ;
	    } else if( isprint(c) ) {
		if( isascii(c) ) {
		    o << c ;
		} else {
		    // high bit set: print its octal value
		    o << "\\0" << oct << c << dec ;
		}
	    } else if(c=='\0') {
		o << "\\0" ;
	    } else {
		o << "\\0" << oct << (unsigned int)c << dec ;
	    }
	}
	o <<"\" }";
	w_assert3(j==l);
	w_assert3(j==v._base[i].len);
    }
    o <<"}";
    return o;
}

istream& operator>>(istream& is, cvec_t& v)
{
    char 	c=' ';
    size_t	len=0;
    int		err = 0;
    char	*temp = 0;

    is.clear();
    v.reset();

    enum	{ 
	starting=0,
	getting_nparts, got_nparts,
	getting_pair, got_len,got_string,
	done
    } state;

    state = starting;
    while(state != done) {
	is >> ws; // swallow whitespace
	c = is.peek();
	/*
	cerr << __LINE__ << ":" 
	    << "state=" << state
	    << " peek=" << (char)is.peek() 
	    << " pos=" << is.tellg()
	    << " len=" << len
	    << " err=" << err
	    <<endl;
	*/

	if(is.eof()) {
	    err ++;
	} else {
	    switch(state) {
	    case done:
		break;

	    case starting:
		if(c=='{') { /*}*/
		    is >> c;
		    state = getting_nparts;
		} else err ++;
		break;

	    case getting_nparts:
		is >> ws; // swallow whitespace
		if(is.bad()) { err ++; }
		else state = got_nparts;
		break;

	    case got_nparts:
		is >> ws; // swallow whitespace
		if(is.peek() == '{') {
		    state = getting_pair;
		} else {
		    err ++;
		}
		break;

	    case getting_pair:
		is >> ws; 
		is >> c;
		if( c == '{') {
		    is >> ws; // swallow whitespace
		    is >> len; // extract a len
		    if(is.bad()) { 
			err ++;
		    } else state = got_len;
		} else if( c== '}' ) {
		    state = done;
		} else {
		    err ++;
		} 
		break;

	    case got_len:
		if(c == '"') {
		    (void) is.get();

		    char *t;
		    temp = new char[len];
		    size_t j = 0;
		    for(t=temp; j<len; j++, t++) {
			is >> *t;
		    }
		    state = got_string;
		    c = is.peek();
		}
		if(c != '"') {
		    err++;
		} else {
		    (void) is.get();
		}
		break;

	    case got_string:
		is >> c; // swallow 1 char
		if(c != '}') {
		    err ++;
		} else {
		    v.put(temp, len);
		    temp = 0;
		    len = 0;
		    state = getting_pair;
		}
		break;

	    }
	    /*
	    cerr << __LINE__ << ":" 
		<< "state=" << state
		<< " peek=" << (char)is.peek() 
		<< " pos=" << is.tellg()
		<< " len=" << len
		<< " err=" << err
		<<endl;
	    */
	}
	if(err >0) {
	    is.set(ios::badbit);
	    state = done;
	    err = is.tellg();
	    //cerr << "error at byte #" << err <<endl;
	}
    }
    return is;
}

