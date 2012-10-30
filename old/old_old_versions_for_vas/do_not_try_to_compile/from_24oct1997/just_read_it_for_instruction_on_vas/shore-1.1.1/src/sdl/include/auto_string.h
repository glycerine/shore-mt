/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef _AUTO_STRING_H_
#define _AUTO_STRING_H_

#include <iostream.h>
#include <lalloc.h>

extern "C" {
   extern       char    *strdup(const char *);
   extern       void    free(void *);
}

char* mystrdup(const char* str);

/*
   A simple string class which always owns the string it contains.
   The owned string is deleted when the string object is  destroyed.
   */  

class auto_string {
   static char* EmptyString;
   friend ostream &operator<<(ostream &, auto_string &);
   const char *_string;
 public:
   auto_string(const char *string = 0) {
      _string = (string && string[0]) ? mystrdup(string) 
                                      : EmptyString;
   }
   auto_string(const char *string, bool given/* = false*/) {
   // note that opt 2nd parm makes this ambiguous
      if (given)
         _string = string;
      else
	 _string = (string && string[0]) ? mystrdup(string) 
                                         : EmptyString;
   }

#ifdef USE_ALLOCATORS
   auto_string(Allocator* ator, char* string = 0);
#endif USE_ALLOCATORS

   ~auto_string() {
#ifndef USE_ALLOCATORS
      if (_string != EmptyString)
         free((void *)_string);
#endif USE_ALLOCATORS
      _string = 0;
   }
   /*protected: */
   /* give ownership of the char * to the auto_string */
   void give(char *newer) {
      char *older = (char *)_string;
      _string = newer;
#ifndef USE_ALLOCATORS
      if (older != EmptyString)
         free(older);
#endif USE_ALLOCATORS
   }

   const char *str() { return _string; }
   void set(const char *string) {
      char *newer= (string && string[0]) ? mystrdup(string)
                                         : EmptyString;
      give(newer);
   }

   operator const char *() { return _string; }
   const char operator[](int n) { return _string[n]; }

   operator =(const char *astring) { set(astring); }
};

#endif /* _AUTO_STRING_H_ */ 
