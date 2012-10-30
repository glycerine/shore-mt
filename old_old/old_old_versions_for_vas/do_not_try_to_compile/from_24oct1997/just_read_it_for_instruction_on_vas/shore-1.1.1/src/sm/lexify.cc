/* --------------------------------------------------------------- */
/* -- Copyright (c) 1997 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
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
#ifndef __GNUC__
#define __attribute__(x)
#endif
static char *rcsid  __attribute__((unused)) =
"$Header: /p/shore/shore_cvs/src/sm/lexify.cc,v 1.10 1997/08/01 18:20:54 solomon Exp $";

/* Compile-time options:
 *  BIGDOUBLE declares that doubles are stored "big-endian" -- that is,
 *    byte 0 contains the sign and high-order part of exponent, etc., and
 *    the the mantissa is stored with decreasingly significant parts
 *    at increasing addresses.
 *  BIGLONG declares that longs are big-endian -- that is,
 *    most-significant-byte first.
 * These options need only be specified to get more efficient code tailored
 * to a particular platform.  If they are specified in error, the code will
 * not work.  If they are not specified, the code should work fine on all
 * platforms, albeit sub-optimally.
 *
 * So far as I know, every BIGLONG platform is also BIGDOUBLE and vice versa,
 * but they are logically independent properties.  For example, little-endian
 * platforms (longs stored least-significan-byte first) use a variety of
 * byte-orderings for doubles.  Thus it would perhaps be worthwhile to
 * have a few alternatives to BIGDOUBLE.
 *
 * Most of this code should work for any floating-point representation I know
 * of, although it has only been tested for IEEE-488.  The assumptions on which
 * it depends are
 *    size = 8 bytes
 *    sign/magnitude representation of negative value, with the sign bit
 *        (0=pos, 1=neg) as the high bit.
 *    positive doubles compare lexicographically (bitwise).
 * The magic constant in byteorder, however, is specific to the IEEE format.
 *
 * In production use, these options should be replaced by tests on
 * pre-processor symbols such as __sparc__, etc.
 */

#define SM_SOURCE
#define BTREE_C

#ifdef __GNUG__
#   pragma implementation "lexify.h"
#endif

#include "sm_int_0.h"
#include "lexify.h"

sortorder SortOrder;

NORET
sortorder::sortorder()
{
    Ibyteorder(I4perm);
    // Create I2perm:
    if(I4perm[0] == 0) {
	I2perm[0] = I4perm[0];
	I2perm[1] = I4perm[1];
    } else if(I4perm[0] == 3) {
	I2perm[0] = I4perm[2];
	I2perm[1] = I4perm[3];
    } else {
	// this byte order isn't implemented!
	W_FATAL(eINTERNAL);
    }
    // Create I1perm:
    I1perm[0] = 0;

    Fbyteorder(Fperm);
    Dbyteorder(Dperm);
}

NORET
sortorder::~sortorder()
{
}

/* Discover the byte order of the current machine and store in permutation
 * (which should be an array of length 8) a permutation for converting to/from
 * "standard" (big-endian) order.
 */

void 
sortorder::Ibyteorder(int permutation[4]) 
{
    /* The following magic constant has the representation
     * 0x3f404142 on a BIGLONG machine.
     */
    int magic = 1061175618;
    u_char *p = (u_char *)&magic;
    int i;
    for (i=0;i<4;i++)
	    permutation[i] = p[i] - 63;
#ifdef BIGLONG
    /* verify that the BIGLONG assertion is correct */
    for (i=0;i<4;i++) w_assert1(permutation[i] == i);

   w_assert3(w_base_t::is_big_endian()); 
#else
#ifdef DEBUG
    // Make sure lexify agrees with w_base_t
    if(permutation[1] == 1) {
       w_assert3(w_base_t::is_big_endian()); 
    } else {
       w_assert3(w_base_t::is_little_endian()); 
    }
#endif /* DEBUG */
#endif
}

void 
sortorder::Fbyteorder(int permutation[4]) 
{
    /* The following magic constant has the representation
     * 0x3f404142 on a BIGFLOAT machine.
     */
    f4_t f = 0.7509957552;
    u_char *p = (u_char *)&f;
    int i;
    for (i=0;i<4;i++)
	    permutation[i] = p[i] - 63;
#ifdef BIGFLOAT
    /* verify that the BIGFLOAT assertion is correct */
    for (i=0;i<4;i++)
	    w_assert1(permutation[i] == i);
#endif
}

void 
sortorder::Dbyteorder(int permutation[8]) 
{
	/* The following magic constant has the representation
	 * 0x3f40414243444546 on a BIGDOUBLE machine.
	 */
	f8_t d = 0.00049606070982314491;
	u_char *p = (u_char *)&d;
	int i;
	for (i=0;i<8;i++)
		permutation[i] = p[i] - 63;
#ifdef BIGDOUBLE
	/* verify that the BIGDOUBLE assertion is correct */
	for (i=0;i<8;i++)
		w_assert1(permutation[i] == i);
#endif
}

/* Translate (double, float, int, unsigned int, short, unsigned short,
 *  char, unsigned char)
 * to an (8,4,2,1)-byte string such that
 * lexicographic comparison of the strings will give the same result
 * as numeric comparison of the corresponding numbers.
 * The permutation perm is the result of Dbyteorder() or Ibyteorder() above.
 */

void 
sortorder::int_lexify(
    const void *d, 
    bool is_signed, 
    int len, 
    void *res, 
    int perm[]
) 
{
    int i;

#ifdef BIGLONG
    switch(len) {
    case 4:
	    ((int4_t *)res)[0] = ((int4_t *)d)[0];
	    break;
    case 2:
	    ((int2_t *)res)[0] = ((int2_t *)d)[0];
	    break;
    case 1:
	    ((int1_t *)res)[0] = ((int1_t *)d)[0];
	    break;
    }
#else
    /* reorder bytes to big-endian */
    for (i=0;i<len;i++) {
	((uint1_t *)res)[i] = ((uint1_t *)d)[perm[i]];
    }
#endif
    
    if(is_signed) {
	/* correct the sign */
	uint1_t x = ((uint1_t *)res)[0];
	((uint1_t *)res)[0] = (x ^ 0x80);
    }
}

void 
sortorder::int_unlexify(
    const void *str, 
    bool is_signed, 
    int len, 
    void *res, 
    int perm[]
) 
{

#ifdef BIGLONG
    switch(len) {
    case 4:
	((int4_t *)res)[0] = ((int4_t *)str)[0] ^ 0x80000000;
	break;
    case 2:
	((int2_t *)res)[0] = ((int2_t *)str)[0] ^ 0x8000;
	break;
    case 1:
	((int1_t *)res)[0] = ((int1_t *)str)[0] ^ 0x80;
	break;
    }
#else
    uint1_t cp[len];

    memcpy(cp, str, len);
    uint1_t x = cp[0];
    if(is_signed) {
	/* correct the sign */
	x ^= 0x80;
    }
    cp[0] = x;
    /* reorder bytes to big-endian */
    int i;
    for (i=0;i<len;i++)
	    ((uint1_t *)res)[i] = cp[perm[i]];
#endif
}

void 
sortorder::float_lexify(f4_t d, void *res, int perm[]) 
{
    int i;

#ifdef BIGFLOAT
    ((int4_t *)res)[0] = ((int4_t *)&d)[0];
#else
    /* reorder bytes to big-endian */
    for (i=0;i<4;i++)
	    ((int1_t *)res)[i] = ((int1_t *)&d)[perm[i]];
#endif
    
    /* correct the sign */
    if (*(int1_t *)res & 0x80) {
	/* negative -- flip all bits */
	((uint4_t *)res)[0] ^= 0xffffffff;
    }
    else {
	/* positive -- flip only the sign bit */
	*(int1_t *)res ^= 0x80;
    }
}

void
sortorder::float_unlexify(
    const void *str, 
    int perm[], 
    f4_t *result
) 
{
    FUNC(sortorder::float_unlexify);
    int i=0;
    f4_t res = *(f4_t *)str;
    DBG(<<"float_unlexify converting " << res);

#ifdef BIGFLOAT
    /* correct the sign */
    uint4_t bits = 0;
    if (*(uint1_t *)str & 0x80) {
	bits = 0x80000000;
    } else {
	bits = 0xffffffff;
    }

    /* fast inline version of memcpy(&res, str, 4) */
    *((int4_t *)&res) = *((int4_t *)str) ^ bits;
#else
    uint1_t cp[sizeof(float)];

    memcpy(cp, str, sizeof(float));

    /* correct the sign -- set the bits for
     * the first byte
     */
    uint1_t bits = 0;
    if (cp[0] & 0x80) {
	bits = 0x80;
    } else {
	bits = 0xff;
    }
    DBG(<<"sortorder::float_unlexify: bits=" << bits);

    cp[0] ^= bits;
    if(bits == 0xff) for (i=1;i<4;i++) { cp[i] ^= bits; }

    /* reorder bytes from big-endian */
    DBG(<<"sortorder::float_unlexify: bits=" << bits);
    for (i=0;i<4;i++) {
	((int1_t *)&res)[i] = cp[perm[i]];
    }
#endif
    DBG(<<"float_unlexify result " << res);
    *result = res;
}

void 
sortorder::dbl_lexify(f8_t d, void *res, int perm[]) 
{
    int i;

#ifdef BIGDOUBLE
    /* fast inline version of memcpy(res, &d, 8) */
    ((int4_t *)res)[0] = ((int4_t *)&d)[0];
    ((int4_t *)res)[1] = ((int4_t *)&d)[1];
#else
    /* reorder bytes to big-endian */
    for (i=0;i<8;i++)
	    ((int1_t *)res)[i] = ((int1_t *)&d)[perm[i]];
#endif
    
    /* correct the sign */
    if (*(int1_t *)res & 0x80) {
	/* negative -- flip all bits */
	((uint4_t *)res)[0] ^= 0xffffffff;
	((uint4_t *)res)[1] ^= 0xffffffff;
    } else {
	/* positive -- flip only the sign bit */
	*(int1_t *)res ^= 0x80;
    }
}

void
sortorder::dbl_unlexify(
    const void *str, 
    int perm[], 
    f8_t *result
) 
{
    FUNC(dbl_unlexify);
    int i;
    f8_t res;

#ifdef BIGDOUBLE
    uint4_t bits = 0;
    if (*(uint1_t *)str & 0x80) {
	bits = 0x80000000;
    } else {
	bits = 0xffffffff;
    }
    /* fast inline version of memcpy(&res, str, 8) */
    ((int4_t *)&res)[0] = ((int4_t *)str)[0] ^ bits;
    if(bits== 0x80000000) bits = 0x0;
    ((int4_t *)&res)[1] = ((int4_t *)str)[1] ^ bits;
#else
    uint1_t cp[sizeof(double)];

    // Can't count on solaris memcpy working here if the address
    // isn't 8-byte aligned.
    // memcpy(cp, str, sizeof(double));
    for(i=0; i<8; i++) ((char *)cp)[i] = ((char *)str)[i];

    /* correct the sign */
    uint1_t bits = 0;
    if (cp[0] & 0x80) {
	bits = 0x80;
    } else {
	bits = 0xff;
    }
    DBG(<<"sortorder::float_unlexify: bits=" << bits);
    cp[0] ^=  bits;
    if(bits == 0xff) for (i=1;i<8;i++) { cp[i] ^= bits; }
    DBG(<<"sortorder::float_unlexify: bits=" << bits);

    /* reorder bytes from big-endian */
    for (i=0;i<8;i++)
	((int1_t *)&res)[i] = cp[perm[i]]; 
#endif
    
    // don't do anything that requires alignment
    // *result = res;
    // memcpy((char *)result, (char *)&res, sizeof(f8_t));
    // brain-damaged memcpy(? )on solaris appears to do
    // double assignment, so we do a byte copy to avoid
    // alignment problems here:
    switch((unsigned)result & 0x00000007) {
	case 0x00:
	    *result = res;
	    break;

	case 0x04:
	    for(i=0; i<2; i++) 
		((unsigned int *)result)[i] = ((unsigned int *)&res)[i];
	    break;

	default:
	    for(i=0; i<8; i++) 
		((char *)result)[i] = ((char *)&res)[i];
	    break;
    }
}


bool 
sortorder::lexify(
    const key_type_s  *kp,
    const void *d, 
    void *res
) 
{
    FUNC(lexify);
    keytype k = convert(kp);
    DBG(<<" k=" << k);
    switch(k) {
	case kt_nosuch:
	case kt_spatial:
	    return false;

	case kt_i1:
	    int_lexify(d, true, 1, res, I1perm);
	    break;

	case kt_i2:
	    int_lexify(d, true, 2,  res, I2perm);
	    break;

	case kt_i4:
	    int_lexify(d, true, 4, res, I4perm);
	    break;

	case kt_u1:
	    int_lexify(d, false, 1, res, I1perm);
	    break;

	case kt_u2:
	    int_lexify(d, false, 2, res, I2perm);
	    break;

	case kt_u4:
	    int_lexify(d, false, 4, res, I4perm);
	    break;

	case kt_f4:
	    DBG(<<"");
	    float_lexify(*(f4_t *)d, res, Fperm);
	    break;

	case kt_f8:
	    dbl_lexify(*(f8_t *)d, res, Dperm);
	    break;

	case kt_b:
	    if(kp->variable) {
		return false;
	    }
	    memcpy(res, str, kp->length);
	    break;
    }
    return true;
}

bool 
sortorder::unlexify(
    const key_type_s  *kp,
    const void *str, 
    void *res
) 
{
    FUNC(unlexify);
    keytype k = convert(kp);
    DBG(<<" k=" << k);
    switch(k) {
	case kt_nosuch:
	case kt_spatial:
	     return false;
	     break;

	case kt_i1:
	    int_unlexify(str,  true, 1, res, I1perm);
	    break;

	case kt_i2:
	    w_assert3(((unsigned)res & 0x1) == 0x0);
	    int_unlexify(str, true, 2,  res, I2perm);
	    break;

	case kt_i4:
	    w_assert3(((unsigned)res & 0x3) == 0x0);
	    int_unlexify(str, true, 4, res, I4perm);
	    break;

	case kt_u1:
	    int_unlexify(str, false, 1, res, I1perm);
	    break;

	case kt_u2:
	    w_assert3(((unsigned)res & 0x1) == 0x0);
	    int_unlexify(str, false, 2, res, I2perm);
	    break;

	case kt_u4:
	    w_assert3(((unsigned)res & 0x3) == 0x0);
	    int_unlexify(str, false, 4, res, I4perm);
	    break;

	case kt_f4:
	    // should be at least 4-byte aligned
	    w_assert3(((unsigned)res & 0x3) == 0x0);
	    float_unlexify(str, Fperm, (f4_t *)res);
	    break;

	case kt_f8:
	    // should be at least 4-byte aligned
	    // architectures' alignment requirements
	    // for doubles might differ.

	    w_assert3(((unsigned)res & 0x3) == 0x0);
	    dbl_unlexify(str, Dperm, (f8_t *)res);
	    break;

	case kt_b:
	    if(! kp->variable) {
	     	memcpy(res, str, kp->length);
	    } else {
		return false;
	    }
	    break;
    }
    return true;
}

sortorder::keytype
sortorder::convert(const key_type_s *kp) 
{
    DBG(<<"convert type " << kp->type
	<< " length " << kp->length);
    keytype result = kt_nosuch;

    switch (kp->type)  {
    case key_type_s::i:
    case key_type_s::u:
        w_assert3(kp->length == 1 || kp->length == 2 
		|| kp->length == 4 );
    	w_assert3(! kp->variable);
        switch(kp->length) {
	    case 1:
		result = kp->type==key_type_s::i?kt_i1:kt_u1;
		break;
	    case 2:
		result = kp->type==key_type_s::i?kt_i2:kt_u2;
		break;
	    case 4:
		result = kp->type==key_type_s::i?kt_i4:kt_u4;
		break;
	    default:
		break;
	}
        break;

    case key_type_s::f:
        w_assert3(kp->length == 4 || kp->length == 8); 
    	w_assert3(! kp->variable);
        switch(kp->length) {
	    case 4:
		result = kt_f4;
		break;
	    case 8:
		result = kt_f8;
		break;
	    default:
		break;
	}
        break;

    case key_type_s::b: // unsigned byte compare
    	// may be  kp->variable
	result = kt_b;
        break;

    default:
        W_FATAL(eNOTIMPLEMENTED);
    }
    DBG(<<"convert returns " << result);
    return result;
}
