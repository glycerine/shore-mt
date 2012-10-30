/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stdlib.h>
#include <iostream.h>

#include <auto_string.h>
char* auto_string::EmptyString = "";

ostream &operator<<(ostream &out, auto_string &as)
{
	return out << as.str();
}

// <str> is non-NULL and has at least 1 character
char* mystrdup(const char* str)
{
#ifdef USE_ALLOCATORS
   char* tmp = DefaultAllocator()->alloc(strlen(str) + 1);
   strcpy(tmp, str);
   return tmp;
#else  USE_ALLOCATORS
   return strdup(str);
#endif USE_ALLOCATORS
}

#ifdef USE_ALLOCATORS
auto_string::auto_string(Allocator* ator, char* str)
{
   if (!str || !str[0])
   {
      _string = EmptyString;
      return;
   }
   _string = ator ? ator->alloc(strlen(str) + 1) : new char[strlen(str) + 1];
   strcpy(_string, str);
   return;
}
#endif USE_ALLOCATORS
