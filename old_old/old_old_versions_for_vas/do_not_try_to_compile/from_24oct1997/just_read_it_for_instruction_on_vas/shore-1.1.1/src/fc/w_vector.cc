/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_vector.cc,v 1.2 1997/06/15 02:03:19 solomon Exp $
 */

#define W_SOURCE

#ifndef __GNUC__
#include "w_vector.h"
#endif

template <class T> 
const w_vector_t<T> & w_vector_t<T>::operator = (const w_vector_t<T> & other)
{
  w_assert3(size == other.size);
  memcpy(data, other.data, sizeof(T) * size);
  return *this;
}

template<class T> 
const w_vector_t<T> & w_vector_t<T>::operator = (const T & el)
{
  for (unsigned int i = 0; i < size; i++)
    data[i] = el;
  return *this;
}

template<class T> 
bool w_vector_t<T>::operator == (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] != other.data[i])
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator < (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] >= other.data[i])
      return false;

  return true;
}
  
template<class T> 
bool w_vector_t<T>::operator > (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] <= other.data[i])
      return false;

  return true;
}
  
template<class T> 
bool w_vector_t<T>::operator != (const w_vector_t<T> & other) const
{
  return !(*this == other);
}

template<class T> 
bool w_vector_t<T>::operator <= (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] > other.data[i])
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator >= (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] < other.data[i])
      return false;

  return true;
}

template<class T> 
const w_vector_t<T> & w_vector_t<T>::operator += (const w_vector_t<T> & other)
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    data[i] += other.data[i];

  return *this;
}
  
template<class T> 
const w_vector_t<T> & w_vector_t<T>::operator -= (const w_vector_t<T> & other)
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    data[i] -= other.data[i];

  return *this;
}

template<class T> 
const T w_vector_t<T>::sum() const
{
  T acc = data[0];

  for (unsigned int i = 1; i < size; i++)
    acc += data[i];

  return acc;
}
  
template<class T> 
const w_vector_t<T> & w_vector_t<T>::max(const w_vector_t<T> & other)
{
  for (unsigned int i = 0; i < size; i++)
    if (other.data[i] > data[i])
      data[i] = other.data[i];

  return *this;
}

template<class T> 
const w_vector_t<T> & w_vector_t<T>::min(const w_vector_t<T> & other)
{
  for (unsigned int i = 0; i < size; i++)
    if (other.data[i] < data[i])
      data[i] = other.data[i];

  return *this;
}

template<class T> 
bool w_vector_t<T>::operator == (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] != el)
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator > (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] <= el)
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator < (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] >= el)
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator >= (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] < el)
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator <= (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] > el)
      return false;

  return true;
}
