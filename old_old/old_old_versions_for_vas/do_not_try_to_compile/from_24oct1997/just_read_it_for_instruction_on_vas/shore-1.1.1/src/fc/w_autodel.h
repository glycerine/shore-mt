/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_autodel.h,v 1.11 1995/04/24 19:31:46 zwilling Exp $
 */
#ifndef W_AUTODEL_H
#define W_AUTODEL_H

/*********************************************************************
 *
 *  class w_auto_delete_t<T>
 *
 *  This class is used to ensure that a "new"ed object of type T 
 *  will be "delete"d when the scope is closed.
 *  During destruction, automatically call "delete" on the pointer
 *  supplied during construction.
 *
 *  eg. f()
 *	{
 *          int* p = new int;
 *	    if (!p)  return OUTOFMEMORY;
 *	    w_auto_delete_t<int> autodel(p);
 *
 *		 ... do work ...
 *
 *	    if (error)  {	// no need to call delete p
 *		return error;
 *	    }
 *
 *	    // no need to call delete p
 *	    return OK;
 *	}
 *
 *  	delete p will be called by the autodel object. Thus user do 
 *	not need to code 'delete p' explicitly, and can be assured
 *	that p will be deleted when the scope in which autodel 
 *      was constructed is closed.
 *
 *********************************************************************/
template <class T>
class w_auto_delete_t {
public:
    NORET			w_auto_delete_t()
	: obj(0)  {};
    NORET			w_auto_delete_t(T* t)
	: obj(t)  {};
    NORET			~w_auto_delete_t()  {
	if (obj) delete obj;
    }
    w_auto_delete_t&		set(T* t)  {
	return obj = t, *this;
    }
private:
    T*				obj;

    // disabled
    NORET			w_auto_delete_t(const w_auto_delete_t&);
    w_auto_delete_t&		operator=(const w_auto_delete_t);
};



/*********************************************************************
 *  
 *  class w_auto_delete_array_t<T>
 *
 *  Same as w_auto_delete_t, except that this class operates on
 *  arrays (i.e. the destructor calls delete[] instead of delete.)
 *
 *  eg. f()
 *	{
 *          int* p = new int[20];
 *	    if (!p)  return OUTOFMEMORY;
 *	    w_auto_delete_array_t<int> autodel(p);
 *
 *		 ... do work ...
 *
 *	    if (error)  {	// no need to call delete[] p
 *		return error;
 *	    }
 *
 *	    // no need to call delete[] p
 *	    return OK;
 *	}
 *
 *********************************************************************/
template <class T>
class w_auto_delete_array_t {
public:
    NORET			w_auto_delete_array_t()
	: obj(0)  {};
    NORET			w_auto_delete_array_t(T* t)
	: obj(t)  {};
    NORET			~w_auto_delete_array_t()  {
	if (obj) delete [] obj;
    }
    w_auto_delete_array_t&	set(T* t)  {
	return obj = t, *this;
    }
private:
    T*				obj;

    // disabled
    NORET			w_auto_delete_array_t(
	const w_auto_delete_array_t&);
    w_auto_delete_array_t&	operator=(const w_auto_delete_array_t);
};

#endif /*W_AUTODEL_H*/
