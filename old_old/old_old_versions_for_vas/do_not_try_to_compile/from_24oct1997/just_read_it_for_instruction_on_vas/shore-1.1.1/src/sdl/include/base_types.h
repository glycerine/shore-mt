/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef BASE_TYPES_H
#define BASE_TYPES_H

class Type;
typedef Type* TypePtr;

/** First the ODL types... **/
extern TypePtr TypeLongRef, TypeUlongRef, TypeShortRef, TypeUshortRef;
extern TypePtr TypeDoubleRef, TypeFloatRef, TypeBoolRef, TypeCharRef;
extern TypePtr TypeVoidRef, TypeStringRef, TypeAnyRef, TypeOctetRef;

/** Bolo additions...**/
extern TypePtr TypeNilRef, TypeErrorRef;

/** Paradise ADTs **/
extern TypePtr TypeRealRef, TypeIntRef, TypePointRef, TypePolygonRef;
extern TypePtr TypePolylineRef, TypeRasterRef, TypeVideoRef, TypeCircleRef;

#endif /** BASE_TYPES_H **/
