/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_vector.h,v 1.3 1997/08/01 18:18:56 solomon Exp $
 */

#ifndef W_VECTOR_H
#define W_VECTOR_H

#ifndef W_BASE_H
#include "w_base.h"
#endif

template <class T>
class w_vector_t {

  public :
  w_vector_t(const unsigned setsize)
    {
      w_assert3(setsize > 0);
      size = setsize;
      data = new T[size];
    }
  w_vector_t(const w_vector_t & other)
    {
      size = other.size;
      data = new T[size];
      *this = other;
    }
  ~w_vector_t()
    {
      delete []data;
    }

  const unsigned vectorSize() const
    {
      return size;
    }

  const w_vector_t & operator = (const w_vector_t & other);
  const w_vector_t & operator = (const T & el);

  bool operator == (const T & el) const;
  bool operator > (const T & el) const;
  bool operator >= (const T & el) const;
  bool operator < (const T & el) const;
  bool operator <= (const T & el) const;

  bool operator == (const w_vector_t & other) const;
  bool operator != (const w_vector_t & other) const;
  bool operator <= (const w_vector_t & other) const;
  bool operator >= (const w_vector_t & other) const;
  bool operator < (const w_vector_t & other) const;
  bool operator > (const w_vector_t & other) const;

  const w_vector_t & operator += (const w_vector_t & other);
  const w_vector_t & operator -= (const w_vector_t & other);

  T & operator [] (const unsigned i) const
    {
      return data[i];
    }

  const T sum() const;

  const w_vector_t & max(const w_vector_t & other);
  const w_vector_t & min(const w_vector_t & other);

  private :
  unsigned size;
  T * data;
};


#ifdef __GNUC__
#if defined(IMPLEMENTATION_W_LIST_H) || !defined(EXTERNAL_TEMPLATES)
#include <w_vector.cc>
#endif
#endif

#endif
