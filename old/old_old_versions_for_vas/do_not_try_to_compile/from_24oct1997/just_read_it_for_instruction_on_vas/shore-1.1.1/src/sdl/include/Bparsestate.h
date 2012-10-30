/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef BPARSE_STATE_H
#define BPARSE_STATE_H

#ifndef STAND_ALONE
#include <Aglob_vars.h>
// #include <Berrstream.h>
// #include <Bparsetree.h>
#endif STAND_ALONE
// #include <Berrstream.h>
// #include <Bparsetree.h>

class Ql_tree_node;             // An OQL query tree...
#ifndef STAND_ALONE
class Aquery_t;                 // The old old PQL tree...
class Bastnode_t;               // The old, old old, ...
#else
#define A_MAXNAMELEN 256
#endif STAND_ALONE

#include <pFlex.h>              // The C++ scanner...
#include <iostream.h>
#include <strstream.h>

#include <m_list.h>
class Declaration;
#include <metatypes.sdl.h>

class Bparser_state_t {
private:
#ifndef STAND_ALONE
  // Alist_t<Bastnode_t> attrs;	// list of all attrs in the query.
#endif STAND_ALONE
public:
#ifndef STAND_ALONE
  // Berrstream_t errstream;
  ostream errstream;
  int typechecking_select_clause;
				// see comment in Bquery_stmt_op_t::typechk
  int target_list_has_aggregates;

  int Bline;			// line number
  // Bastnode_t query;		// the query
  // Bastnode_t *update_list;	// this points to the update list in an
				// update statement. some dirty coding here.
				// Aptree_t::generate_name uses this 
  // Bastnode_t *rel_array;	// pointer to the array of relations referred
				// to by this query/update/delete stmt.
				// this is used during typechecking to see
				// if a relation in a where clause is actually
				// present in the from clause or update clause
				// or delete clause (as the case may be).
				// obviously, in case of delete/update stmts,
				// the array will contain just one relation.
#endif STAND_ALONE
  int num_rels;			// number of elements in the rel_array.
  char *input_string;		// where the query is read from
  // char result_filename[A_MAXNAMELEN];
				// name of the file in which the
				// result of the query is stored.

  // Murali.additions....
  int _line;                    // Current line number...
  int _currentPos;              // What's the parser looking at NOW ?
  int _eof;                     // Has the end of the input been reached?
  int _len;                     // Length of the current input string

  Ql_tree_node* _query;         // An OQL query...
  // m_list_t<Declaration>* _decls;// A list of type declarations
   Ref<sdlDeclaration> _decls;// A list of type declarations
                                // BIG kludge...

// Murali.additions 3/31 
// I'm trying to make the parser re-entrant. Unfortunately, bison
// and flex are not very easily convertible to a re-entrant format.
// So I need some global storage, to store the semantic values of 
// lex tokens, and also the current flex scanner
  PFlexLexer* _scanner;
  istream* _in;
public:
  PFlexLexer*   scanner() {return _scanner;}
  int           line()    {return _line;}
  int           eof()     {return _eof;}
  Ql_tree_node* Query()   {return _query;}
  // m_list_t<Declaration>* Decls() {return _decls;}
  Ref<sdlDeclaration> Decls() {return _decls;}
  // Berrstream_t& errStrm() {return errstream;}
  ostream& errStrm() {return cerr;}
  void          setupInput() {_query = 0; _decls = 0;}
  // char*         resultFilename() {return result_filename;}

  Bparser_state_t* setQuery(Ql_tree_node* qry) {
     _query = qry; 
     return this;
  }
  // Bparser_state_t* setDecls(m_list_t<Declaration>* dcls) {
  Bparser_state_t* setDecls(Ref<sdlDeclaration> dcls) {
     _decls = dcls;
     return this;
  }
  // Bparser_state_t* setDecls(Ref<sdlDeclaration> dcls);
  Bparser_state_t* setLine(int line) {_line = line; return this;}
  Bparser_state_t* incLine()         {++_line; return this;}
  Bparser_state_t* setEof()          {_eof = 1; return this;}
public:
  void Bparser_state (void);
  void reset (void);		// reset the state at the beginning of a query.
#ifndef STAND_ALONE
  // int error (void) const {return errstream.error ();}
  // const char *error_str (void) const {return errstream.error_str ();}
  void add_uniq_attr (Bastnode_t *attr);
  // Alist_t<Bastnode_t> attr_list (void) const {return attrs;}
#endif
  // Murali.additions
  Bparser_state_t();
  ~Bparser_state_t();

  int Over() {return (_currentPos == _len && _eof);}
  void Init(char* inp = 0, char* res_file = 0);
  void Init(istream *);
  void Reset(); // A bit different from the previous reset()
};

#ifdef STAND_ALONE
ostream& errstream();
extern Bparser_state_t gBparser_state;
#else  STAND_ALONE
// Berrstream_t& errstream();
ostream& errstream();
#endif STAND_ALONE

inline Bparser_state_t *Bparser_state(void) 
{
#ifndef STAND_ALONE
  return Aglob_vars ()->parser_state;
#else
  return &gBparser_state;
#endif
}

/***************************************************************************

  inline functions

***************************************************************************/

inline void Bparser_state_t::reset (void)
{
#ifndef STAND_ALONE
  Bline = 1; // errstream.reset (); // attrs.DeleteList ();
  typechecking_select_clause = target_list_has_aggregates = 0;
  // result_filename[0] = '\0';
#endif
}

inline void
Bparser_state_t::Reset()
{
   reset(); 
   _line = 0; 
   input_string = 0; 
   _eof = 1;
   _currentPos = 0; 
   _len = 0; 
   _decls = 0; // (m_list_t<Declaration> *)0;
   _query = (Ql_tree_node *)0;
}

inline void
Bparser_state_t::Init(char* inp_str, char* res_file)
{
   Reset(); 
   input_string = inp_str;
   // if (res_file) strcpy(result_filename, res_file);
   _len = inp_str? strlen(inp_str): 0;
   _eof = _len? 0: 1;
   _line = 1;

   // Take care of mem_allocation here...
   if (_in)
   {
      delete _in; _in = NULL ;
   }

   if ( inp_str ) _in = new istrstream(inp_str) ;  

   _scanner->yyrestart(_in);
}

inline Bparser_state_t::Bparser_state_t()
{
   _scanner = new PFlexLexer();
   _in = (istream *)0;
   Init();
}

inline Bparser_state_t::~Bparser_state_t()
{
   // Go around deleting lots o'stuff...
   // Delete the lexical analyzer...
   if (_scanner)
      delete _scanner;
   // Delete the input stream...
   if (_in)
      delete _in;

   // The query/decl. DO NOT have to be deleted. They will be taken
   // care of...

   // Sic transit gloria mundi...
   return;
}

inline void Bparser_state_t::Bparser_state (void) 
{
   reset ();
}

#endif /* BPARSE_STATE_H */
