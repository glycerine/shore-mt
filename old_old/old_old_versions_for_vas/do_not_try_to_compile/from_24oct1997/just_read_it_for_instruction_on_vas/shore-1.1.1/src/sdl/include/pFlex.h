/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef PFLEX_H
#define PFLEX_H

#include <iostream.h>
#include <FlexLexer.h>

class PFlexLexer: public yyFlexLexer
{
 public:
   PFlexLexer(istream* is = 0, ostream* os = 0)
      : yyFlexLexer(is, os) {}

   void yyrestart(istream* is = 0) {
      yyFlexLexer::yyrestart(is? is: yyin);
   }
};

#endif /* PFLEX_H */
