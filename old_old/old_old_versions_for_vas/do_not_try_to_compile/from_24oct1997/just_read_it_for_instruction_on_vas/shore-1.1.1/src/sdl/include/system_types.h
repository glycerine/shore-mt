#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "metatypes.sdl.h"
class SystemType
{
 private:
   // unsigned int _realType;
   Ref<sdlType> _realType;
 public:
   Ref<sdlType> realType() {return _realType;}
   void setRealType(Ref<sdlType> r) {_realType = r;}

//   SystemType() : _realType(0) {}
#ifdef USE_ALLOCATORS
   void* operator new(long sz);
   void  operator delete(void *) {}
#endif USE_ALLOCATORS
};
#endif SYSTEM_TYPES_H
