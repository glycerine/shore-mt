/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef W_WORKAROUND_H
#define W_WORKAROUND_H
/*
 *  $Id: w_workaround.h,v 1.19 1996/06/28 18:57:11 schuh Exp $
 */

/* see below for more info on GNUG_BUG_12 */
#define GNUG_BUG_12(arg) arg

#if 	defined(__GNUC__) && (__GNUC__ < 2)
#error	This software requires gcc 2.5.x or a later release.
#error  Gcc 2.6.0 is preferred.
#endif

#ifdef __GNUC__

#   if (__GNUC_MINOR__ < 6)
    /*
     * G++ has a bug where static members are not properly
     * handled.  Any code ifdef'd with GNUG_BUG_1 is meant
     * to workaround this bug
     */
#   define GNUG_BUG_1 1

    /*
     * G++ also has a bug in calling the destructor of a template
     */
#   define GNUG_BUG_2 1

    /*
     * G++ seems to have a problem calling ::operator delete 
     */
#   define GNUG_BUG_3 1

    /*
     * G++ cannot handle inheritence of typedef in class
     */
#   define GNUG_BUG_4 1

    /*
     * G++ -O cannot handle certain uses of endl
     */
#   define GNUG_BUG_5 1

    /*
     * g++ generates global data symbols for 
     * constants defined in common .h files.
     */
#   define GNUG_BUG_6 1

    /*
     * G++ version 2.4.5 has problems with templates that don't have
     * destructors explicitly defined.  It also seems to have problems
     * with classes used to instantiate templates if those classes
     * do not have destructors.
     */
#   define GNUG_BUG_7 1

    /* bug #8:
     * gcc include files don't signal() as in ANSI C.
     * we need to get around that
	 */
#   define GNUG_BUG_8 1

    /*
	 * #9:
     * g++ (2.5.8 but not 2.4.5) cannot handle this use of typedef
	class remote_m {
	public:
	    typedef int (remote_m::*msg_func_ptr_t)();
	    static msg_func_ptr_t _msg_func[];
	    int foo();
	};
	remote_m::msg_func_ptr_t remote_m::_msg_func[] = {
	    foo,
	};
     *
     */
#   define GNUG_BUG_9 1

     /*
	  * #10:
      *  gnu tries to use the copy constructor when constructing an
      *  array
      */
#   define GNUG_BUG_10 1
	/* 
	 * 2.5.8 cannot handle anonymous constructed types like this:
	 * funccall( a(), false, true);
	 * where a is a constructor of a type
	 */

	/* 11 removed */
#   define GNUG_BUG_11 1

#endif /* __GNUC_MINOR__ < 6 */

	/*
	// #12
    // This is a bug in parsing specific to gcc 2.6.0.
    // The compiler misinterprets:
    //	int(j)
    // to be a declaration of j as an int rather than the correct
    // interpretation as a cast of j to an int.  This shows up in
    // statements like:
    // 	istrstream(c) >> i;
	*/
#   if __GNUC_MINOR__ > 5
#	undef GNUG_BUG_12	
#   	define GNUG_BUG_12(arg) (arg)
#   endif

#   if __GNUC_MINOR__ > 5
/*
 * 	GNU 2.6.0  : template functions that are 
 *  not member functions don't get exported from the
 *  implementation file.
 */
#define 	GNUG_BUG_13 1

/*
 * Cannot explicitly instantiate function templates.
 */
#define 	GNUG_BUG_14 1

#endif

#endif /* __GNUC__ */

/*
 * HP CC bugs
 */
#if defined(Snake) && !defined(__GNUC__)

    /*
     * HP CC does not like some enums with the high order bit set.
     */
#   define HP_CC_BUG_1 1

    /*
     * HP CC does not implement labels within blocks with destructors
     */
#   define HP_CC_BUG_2 1

    /*
     * HP CC does not support having a nested class as as a
     * parameter type to a template class
     */
#   define HP_CC_BUG_3 1

#endif /* defined(Snake) && !defined(__GNUC__) */

/* Note if the compiler does not support the built-in type bool */
#if defined(__GNUC__) && __GNUC_MINOR__ > 5
#   undef NO_BOOL
#else
#   define NO_BOOL
#endif /* gcc > 2.5 */
/* also note that gcc 2.7+ seems to have sizeof(bool) == sizeof(int) */
#if defined(__GNUC__) && ( __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7))
#	define LARGE_BOOL_SIZE sizeof(bool)
/* there may be other cases which need LARGE_BOOL_SIZE defined... */
#else
#	undef LARGE_BOOL_SIZE
#endif


/* This is really a library problem; stream.form() and stream.scan()
   aren't standard, but GNUisms.  On the other hand, they should
   be in the standard, because they save us from static form() buffers.
   Using the W_FORM() and W_FORM2() macros instead of
   stream.form() or stream << form() encapsulates this use, so the
   optimal solution can be used on each platform.
   If a portable scan() equivalent is written, a similar set
   of W_SCAN macros could encapuslate input scanning too.
 */  
#ifdef __xlC__
#define	W_FORM(stream)		stream << form	
#else
#define	W_FORM(stream)		stream . form
#endif
#define	W_FORM2(stream,args)	W_FORM(stream) args

#endif /* W_WORKAROUND_H */
