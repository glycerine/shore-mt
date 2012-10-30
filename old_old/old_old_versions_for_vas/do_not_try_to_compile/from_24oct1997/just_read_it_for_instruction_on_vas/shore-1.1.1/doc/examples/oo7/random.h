#ifndef _RANDOM_H_
#define _RANDOM_H_

/*$Header: /p/shore/shore_cvs/src/oo7/random.h,v 1.7 1994/10/17 00:00:34 mcauliff Exp $*/

//
// random.h
//

#if defined(_HPUX_SOURCE) || defined(I860)
#include <stdlib.h>
#endif

#ifdef _HPUX_SOURCE

#include <stdlib.h>

inline long random(void)
{
    return (long)(lrand48());
}


inline void srandom(int seed)
{
    srand48((long int)seed);
}

#endif


#if defined(I860) || defined(sparc) || defined(Mips)
extern "C" long random(void);
extern "C" int srandom(unsigned );
#endif

#endif /* _RANDOM_H_ */
