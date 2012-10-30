%{
/**** YACC FILE ****/
#include <iostream.h>

#ifndef STAND_ALONE
#include <Aglob_vars.h>
#endif  STAND_ALONE

#include <metatypes.sdl.h>
#include "sdl_ext.h"
#include <pFlex.h>
#include <tree.h>
#include <defns.h>
#include <m_list.h>
#include <metatypes.sdl.h>
#include <Bparsestate.h>
#include <parse_support.h>
#include <base_types.h>

const int OQL = 1;

int yyerror(char* s);
extern "C" int yylex();
%}

%union
{
   // Declaration*  decl_ptr;
   // m_list_t<Declaration>* decl_list;

   // Sdl_Type* type_ptr;

   // ExprNode*     expr_ptr;
   // m_list_t<ExprNode>* expr_list;
	struct node * nodept; 
	UnRef<sdlDeclaration> declpt; 
	UnRef<sdlType> typept; 
	UnRef<sdlExprNode> exprpt; 
	enum Mode mode;
	Zone zone;
	TypeTag ttag;
	short code; 

   char*         string;
   Ql_tree_node* value;
   int 	 int_val;
}

%pure_parser              
   /** I want a re-entrant parser **/
%start axiom 

/* the two first ones to solve the (cast vs. (id=query) conflict. cast wins */
%nonassoc LASTPRED
%nonassoc ')'
%left ASSIGN
%left SELECT 
%left FROM
%left WHERE
%left FIELDPREC
%left QUERYPREC
%left ','
%left DOTS ':'
%left OR
%left AND
%left '=' DIFF LIKE
%left '<' '>' INFEQUAL SUPEQUAL
%left '+' '-' UNION EXCEPT
%left '*' '/' '%' INTERSECT 
%left tIN
%left NOT 
%left DOT ARROW '(' '[' 

   /** OQL tokens, that dont double as ODL tokens...  **/
%token <value> DEFINE
%token <value> USET 
%token <value> OQL_ABS 
%token <value> COUNT
%token <value> SUM
%token <value> OQL_MIN
%token <value> OQL_MAX
%token <value> AVG
%token <value> ELT 
%token <value> FIRST
%token <value> LAST 
%token <value> DISTINCT 
%token <value> UNIQUE 
%token <value> LISTOSET
%token <value> FLATTEN 
%token <value> TUPLE
%token <value> EXISTS 
%token <value> FORALL
%token <value> FOR
%token <value> SORT
%token <value> GROUP
%token <value> BY
%token <value> WITH
%token <value> CREATE
%token <value> DROP
%token <value> OPEN
%token <value> CLOSE
%token <value> DB
%token <value> tINDEX
%token <value> CLUSTERED
%token <value> OQL_ON

%token <value> ONLY
%token <value> USING
%token <value> UPDATE
%token <value> INSERT
%token <value> DELETE
%token <value> DESTROY
%token <value> INTO
%token <value> TO
%token <value> APPLY

%token <value> PLUS_EQUAL
%token <value> MINUS_EQUAL

   /** ODL tokens...	      **/
   /** Top level tokens	      **/
%token <value>	INTERFACE
%token <value>	MODULE
%token <value>	TYPEDEF
%token <value>	STRUCT
%token <value>	UNION
%token <value>	ENUM
%token <value>	CONST
%token <value>	EXCEPT


   /** Module related stuff   **/
%token <value>	IMPORT
%token <value>	EXPORT
%token <value>	USE
%token <value>	AS
%token <value>	ALL

   /** Interface related stuff**/
%token <value>	PERSISTENT
%token <value>	TRANSIENT
%token <value>	PUBLIC
%token <value>	tPRIVATE
%token <value>	PROTECTED
%token <value>	KEY
%token <value>	EXTENT
%token <value>	ATTRIBUTE
%token <nodept>	READONLY
%token <value>	RELATIONSHIP
%token <value>	INVERSE
%token <value>	ORDER_BY
   /** Method overrides	      **/
%token <value>	OVERRIDE
%token EXTERNAL
%token CLASS
%token POOL
%token INVERSE
%token ORDERED_BY



   /** Parameter passing modes**/
%token <value>	tIN
%token <value>	tOUT
%token <value>	tINOUT
   /** Havent the faintest idea of what these do   **/
%token <value>	RAISES
%token <value>	DIRECT
%token <value>	ONEWAY
%token <value>	CONTEXT

   /** Union related stuff **/
%token <value>	CASE
%token <value>	DEFAULT
%token <value>	SWITCH

   /** Template types...   **/
%token <value>	ARRAY
%token <value>	SEQUENCE
%token <value>	tREF
%token <value>	tLREF
%token <value>	tBAG
%token <value>	tSET
%token <value>	LIST
%token <value>	MULTILIST

   /** Punctuations...	**/
%token <value>	';'
%token <value>	':'
%token <value>	DCOLON
%token <value>	','
%token <value>	'('
%token <value>	')'
%token <value>	'{'
%token <value>	'}'
%token <value>	'['
%token <value>	']'
%token <value>	'<'
%token <value>	'>'

   /** Some of the operators...	 **/
%token <value>	'^'
%token <value>	'*'
%token <value>	'/'
%token <value>	'='
%token <value>	'|'
%token <value>	'&'
%token <value>	RSHIFT
%token <value>	LSHIFT
%token <value>	'+'
%token <value>	'-'
%token <value>	'%'
%token <value>	'~'

   /** Basic types... INT is a dubious addition. ODL doesnt support it. **/
%token <value>	ANY
%token <value>	OCTET
%token <value>	LONG
%token <value>	SHORT
%token <value>	UNSIGNED
%token <value>	DOUBLE
%token <value>	BOOLEAN
%token <value>	VOID
%token <value>  tSTRING
%token <value>  CHAR
%token <value>  FLOAT
%token <value>  INT
%token <value>  TEXT
%token <value>  INDEXABLE

      /** Constants and values...	 **/
%token <value>	LEX_STRING_LIT
%token <value>	LEX_INT_LIT
%token <value>	LEX_CHAR_LIT
%token <value>	LEX_FLOAT_LIT
%token <value>	LEX_TRUE
%token <value>	LEX_FALSE
%token <value>  LEX_OQL_NIL
%token <value>  LEX_ID
   
      /** Der ID  **/
%token <value>  EOI
%token <value>  ILLEGAL_INPUT
      /** End of input...  **/

%type <value> ref_lit

/** Non-terminals that look like terminals **/
/** Just a kludge to get around LEX        **/
%type <value> ID
%type <value> OQL_NIL
%type <value> FALSE
%type <value> TRUE
%type <value> INT_LIT
%type <value> CHAR_LIT
%type <value> STRING_LIT
%type <value> FLOAT_LIT

      /** OQL non-terminals   **/
%type <value> axiom 
%type <value> utility

%type <value> updates
%type <value> update_statements
%type <value> update_expr_list
%type <value> update_expr
%type <value> assign_statement
%type <value> insert_statements
%type <value> create_statements
%type <value> delete_statements
%type <value> insert_values_statement
%type <value> insert_query_statement

%type <value> query_closure
/**
%type <value> update_query_closure
**/
%type <value> query 
%type <value> querieslist 
%type <value> onevar 
%type <value> varslist 
%type <value> onefield 
%type <value> fieldslist
%type <value> attname
%type <value> keyword
/* to solve the [select distinct ...]/[select q ... with q = distinct...] 
   conflict */
%type <value> distinctq

/* new odl nonterm types. */
%type <declpt> export_list definition_list specification opt_definition_list
%type <declpt> interface interface_dcl forward_interface definition 
%type <declpt> module module_tag
%type <nodept> interface_header
%type <declpt> inheritance_spec interface_body
%type <nodept>	inverse_clause ordered_clause
%type <declpt> const_dcl export
%type <exprpt> const_exp or_expr
%type <exprpt> xor_expr and_expr shift_expr
%type <exprpt> add_expr mult_expr unary_expr
%type <exprpt> primary_expr literal
%type <exprpt> boolean_literal positive_int_const 
%type <declpt> type_dcl type_declarator 
%type <typept> const_type type_spec simple_type_spec
%type <typept> base_type_spec template_type_spec /* collection_type_spec */ constr_type_spec
%type <declpt> declarators declarator simple_declarator
%type <declpt> complex_declarator 
%type <typept> floating_pt_type integer_type
%type <typept> signed_int signed_long_int signed_short_int
%type <typept> unsigned_int unsigned_long_int unsigned_short_int
%type <typept> any_type char_type boolean_type octet_type pool_type
%type <typept> struct_type union_type enum_type
%type <declpt> member member_list case case_list  switch_body
%type <exprpt> case_label_list
%type <exprpt> case_label
%type <declpt> enumerator_list enumerator
%type <declpt> array_declarator element_spec
%type <typept> type_name sequence_type string_type
/* %type <typept> simple_or_collection */
%type <typept> op_type_spec switch_type_spec relation_type
%type <exprpt> fixed_array_size_list 
%type <exprpt> fixed_array_size 
%type <declpt> attr_dcl ex_member_list
%type <int_val> opt_readonly
%type <declpt> op_dcl param_dcl_list param_dcl rel_dcl override_dcl 
%type <declpt> parameter_dcls 
%type <int_val> opt_context_expr context_expr 
%type <mode>   param_attribute
%type <nodept> raises_expr opt_raises_expr
%type <exprpt> string_literal_list 
/*
%type <nodept> opt_rel_pragma rel_pragma_list
%type <nodept> rel_pragma
*/
%type <declpt> mod_export_list mod_import_list mod_export
%type <declpt> export_def mod_import
%type <declpt> export_access_list export_access_spec
%type <zone> access_spec 
%type <ttag> rel_id
%type <nodept> opt_alias
%type <declpt> module_list access_scoped_name access_scoped_name_list
%type <declpt> override_list
%type <ttag> tdecl_kind
%type <exprpt> integer_literal string_literal character_literal module_name
%type <exprpt> floating_pt_literal
%type <value>		literal_list /* ick? */
%type <nodept> SID


      /** ODL non-terminals...	 **/

/* 
%type <decl_list>    	specification definition definition_list
%type <decl_ptr>     	module_dcl interface_dcl const_dcl
%type <decl_ptr>     	forward_interface regular_interface
%type <decl_list>    	type_dcl except_dcl
%type <decl_list>    	mod_export_list mod_use_list mod_import_list
%type <decl_ptr>     	mod_export mod_import mod_use
%type <type_ptr>     	interface_hdr 
%type <int_val>	     	persistence_dcl
%type <decl_list>    	inheritance_spec access_speced_typename_list
%type <decl_ptr>     	access_speced_typename
%type <int_val>	     	inheritance_access_spec access_spec
%type <string>          extent_spec
%type <value>           key_spec key_list
%type <value>  	        one_key single_attribute_key multi_attribute_key

%type <decl_list>    	interface_body export_list export_returns_list
%type <decl_ptr>     	export relnship_dcl access_spec_dcl op_dcl
%type <decl_list>    	attr_dcl override_dcl 
%type <int_val>	     	opt_readonly
%type <int_val>	     	opt_op_attribute
%type <type_ptr>     	op_type_spec
%type <decl_list>    	parameter_dcls param_dcl_list
%type <decl_ptr>     	param_dcl
%type <int_val>	     	param_attribute
%type <value>           opt_raises_expr raises_expr
%type <value>  	        opt_context_expr context_expr

%type <value>  	        string_literal_list

%type <int_val>	     	rel_id

%type <value>    	inverse_clause
%type <value>           ordered_clause

%type <decl_list>	override_list
%type <decl_ptr>	override

%type <decl_list>	declarators
%type <decl_ptr>	declarator simple_declarator complex_declarator
%type <decl_ptr> 	array_declarator

%type <expr_list>	array_size_list
%type <expr_ptr>	fixed_array_size

%type <decl_list>	type_declarator
%type <type_ptr>	type_spec simple_type_spec constr_type_spec 
%type <type_ptr>	base_type_spec template_type_spec type_name
%type <type_ptr> 	floating_pt_type integer_type char_type
%type <type_ptr>	boolean_type octet_type any_type
%type <type_ptr>	string_type relation_type
%type <type_ptr>	struct_type union_type enum_type
%type <type_ptr>	unsigned_int signed_int
%type <type_ptr>	unsigned_short_int unsigned_long_int
%type <type_ptr>	signed_short_int signed_long_int
%type <type_ptr>	void_type

%type <decl_list>	member_list opt_member_list member

%type <type_ptr> 	switch_type_spec
%type <decl_list>	case_list switch_body
%type <decl_ptr>	case
%type <expr_ptr>	case_label
%type <expr_list>	case_label_list
%type <decl_ptr>	element_spec

%type <decl_list>	enumerator_list
%type <decl_ptr>	enumerator

%type <int_val>		array_spec
%type <type_ptr>	array_type

%type <type_ptr>	const_type

%type <expr_ptr>     	const_exp or_expr xor_expr and_expr shift_expr
%type <expr_ptr>     	add_expr mult_expr unary_expr primary_expr 
%type <expr_ptr>     	positive_int_const

%type <value>	     	literal
%type <value>	     	boolean_literal
*/
   /** The following 3 are specific to paradise 
%type <value>		point_const
%type <value>		circle_const
%type <value>		polygon_const
%type <value>		literal_list
%type <value>		point_const_list


**/

   /** This is to represent sets/bags/... homogenous collections...  **/
%type <value>           homogenous_collection_const
   /** This is to represent heterogenous collections like structures...	**/
%type <value>		heterogenous_collection_const	     
%type <int_val>	     	unary_operator

%type <nodept>  	        scoped_name
%type <nodept>           scoped_name_list

   /** End of ODL non-terminals...  **/

   /** Rules begin here..     	    **/
%%	

 /* RETURN THE TREE */
axiom: query ';'
      {Bparser_state()->setQuery(new Ql_tree_node(query_n, 1, $1)); return 1;}
   | ';'
      {Bparser_state()->setQuery(0); return 1;}
   | utility ';'
      {
       Bparser_state()->setQuery(new Ql_tree_node(utility_n, 1, $1)); 
       return 1;
      }
   | updates ';'
      {
       Bparser_state()->setQuery(new Ql_tree_node(update_n, 1, $1));
       return 1;
      }
   | DEFINE ID AS query ';'
      {
       Bparser_state()->setQuery(new Ql_tree_node(define_n, 2, $2, $4));
       return 1;
      }
   | specification
      {Bparser_state()->setDecls($1); return 1;}
   | EOI  /** An end-of-input occurred   **/
      {Bparser_state()->setEof(); return 1;}
   ;

utility: CREATE DB ID
      {$$ = new Ql_tree_node(create_db_n, 1, $3);}
   | DROP DB ID
      {$$ = new Ql_tree_node(drop_db_n, 1, $3);}
   | OPEN DB ID
      {$$ = new Ql_tree_node(open_db_n, 1, $3);}
   | OPEN DB STRING_LIT
      {$$ = new Ql_tree_node(open_db_n, 1, $3);}
   | CLOSE DB
      {$$ = new Ql_tree_node(close_db_n, 0);}
   | DROP tINDEX ID
      {$$ = new Ql_tree_node(drop_index_n, 1, $3);}
   | DROP EXTENT ID
      {$$ = new Ql_tree_node(drop_extent_n, 1, $3);}
   ;
/** Fill in the rest of this stuff later
   | CREATE tINDEX ID 
**/


/** Updates begin here...  **/
updates: update_statements
   | delete_statements
   | create_statements
   ;

create_statements: CREATE query
      {$$ = new Ql_tree_node(cfw_n, 1, $2);}
   | CREATE query FROM varslist
      {$$ = new Ql_tree_node(cfw_n, 2, $2, $4);}
   | CREATE query FROM varslist WHERE query
      {$$ = new Ql_tree_node(cfw_n, 3, $2, $4, $6);}
   ;

insert_statements: insert_values_statement
   | insert_query_statement
   ;
insert_values_statement: INSERT INTO ID heterogenous_collection_const
      {$$ = new Ql_tree_node(iiv_n, 2, $3, $4);}
   ;
insert_query_statement: INSERT query INTO query
      {$$ = new Ql_tree_node(iiw_n, 2, $2, $4);}
   ;
/**
   TO DO. Allow this stuff to be named, so I can say
      DELETE b from b in Boats where
   or
      delete b in Boats where
**/
delete_statements: DESTROY FROM query_closure
      {$$ = new Ql_tree_node(df_n, 1, $3);}
   | DESTROY ID tIN query_closure
      {$$ = new Ql_tree_node(df_n, 2, $4, $2);}
   | DESTROY FROM query_closure WHERE query
      {$$ = new Ql_tree_node(dfw_n, 2, $3, $5);}
   | DESTROY ID tIN query_closure WHERE query
      {$$ = new Ql_tree_node(dfw_n, 3, $4, $2, $6);}
   ;
update_statements: UPDATE ID tIN query_closure tSET update_expr_list
      {$$ = new Ql_tree_node(uf_n, 3, $4, $6, $2);}
   | UPDATE query_closure tSET update_expr_list
      {$$ = new Ql_tree_node(uf_n, 2, $2, $4);}
   | UPDATE query_closure tSET update_expr_list WHERE query
      {$$ = new Ql_tree_node(ufw_n, 3, $2, $4, $6);}
   | UPDATE ID tIN query_closure tSET update_expr_list WHERE query
      {$$ = new Ql_tree_node(ufw_n, 4, $4, $6, $2, $8);}
   ;
update_expr_list: update_expr
      {$$ = new Ql_tree_node(update_expr_n, 1, $1);}
   | update_expr_list ',' update_expr
      {$$ = $1->add($3);}
   ;
/** Murali.modify 3/23 **/
/** Dont allow nested INSERT, ... statements anymore. **/
update_expr: assign_statement
   | query PLUS_EQUAL query
      {$$ = new Ql_tree_node(plus_equal_n, 2, $1, $3);}
   | query '-' query
      {$$ = new Ql_tree_node(minus_equal_n, 2, $1, $3);}
   ;
/** Murali.comment out 3/23
   | '(' updates ')'
      {$$ = $2;}
   ;
**/
assign_statement: query '=' query
      {$$ = new Ql_tree_node(assign_n, 2, $1, $3);}
   ;


/** 
   Murali.add, I want the reduce to happen only if no more shifts
**/

query_closure: ONLY query %prec LASTPRED
      {$$ = new Ql_tree_node(only_n, 1, $2);}
   | ALL query %prec LASTPRED
      {$$ = new Ql_tree_node(closure_n, 1, $2);}
   | query %prec LASTPRED
      {$$ = new Ql_tree_node(closure_n, 1, $1);}
   ;   

 /* operations of arity 0 */
 query 	: OQL_NIL                 
  {$$ = $1; 
   }	
 | TRUE
  {$$ = $1;
   }
 | FALSE
  {$$ = $1;
   }
 | ID %prec LASTPRED
  {$$ = $1;
   }
 | CHAR_LIT
  {$$ = $1;
   }
 | STRING_LIT
  {$$ = $1;
   }
 | FLOAT_LIT
  {$$ = $1;
   }
 | INT_LIT
  {$$ = $1;
   }
 | ref_lit
   {$$ = $1;}
 | '(' query ')'
  {$$ = $2;
   }
 | ARRAY '(' ')'
  {$$ = new Ql_tree_node(empty_array_n, "");
   }
 | tSET '(' ')'
  {if (OQL)
       $$ = new Ql_tree_node(empty_set_n, "");
   else
       $$ = new Ql_tree_node(empty_bag_n, "");
   }
 | tBAG '(' ')'
  {$$ = new Ql_tree_node(empty_bag_n, "");
   }
 | USET '(' ')'
   {$$ = new Ql_tree_node(empty_set_n, "");
   }
 | LIST '(' ')'
   {$$ = new Ql_tree_node(empty_list_n, "");
   }
 | ID '(' ')'
/** Murali.change 4/4. Was **/
/**  {$$ = new Ql_tree_node(function_call_n, 1, $1);
   }
**/
   {$$ = new Ql_tree_node(function_or_create_n, 1, $1);}

 /* operations  that will be of arity 1 */
 | '(' ID ')' query %prec NOT
   {$$ = new Ql_tree_node(cast_n, 2, $2, $4);
   }
 | '*' query 
   {$$ = new Ql_tree_node(dereference_n, 1, $2);
    }
 | COUNT '(' query ')'
   {$$ = new Ql_tree_node(count_n, 1, $3);
    }
 | SUM '(' query ')'
   {$$ = new Ql_tree_node(sum_n, 1, $3);
    }
 | OQL_MIN '(' query ')'
   {$$ = new Ql_tree_node(min_n, 1, $3);
    }
 | OQL_MAX '(' query ')'
   {$$ = new Ql_tree_node(max_n, 1, $3);
    }
 | AVG '(' query ')'
   {$$ = new Ql_tree_node(avg_n, 1, $3);
    }
 | ELT '(' query ')'
   {$$ = new Ql_tree_node(element_n, 1, $3);
    }
 | FIRST '(' query ')'
   {$$ = new Ql_tree_node(first_n, 1, $3);
    }
 | LAST '(' query ')'
   {$$ = new Ql_tree_node(last_n, 1, $3);
    }
 | DISTINCT '(' distinctq ')' 
   {$$ = new Ql_tree_node(distinct_n, 1, $3);
    }
 | LISTOSET '(' query ')'
   {$$ = new Ql_tree_node(listoset_n, 1, $3);
    }
 | FLATTEN '(' query ')'
   {$$ = new Ql_tree_node(flatten_n, 1, $3);
    }
 | '-' query %prec NOT
   {$$ = new Ql_tree_node(unary_minus_n, 1, $2);
    }
 | OQL_ABS '(' query ')' 
   {$$ = new Ql_tree_node(abs_n, 1, $3);
    }
 | NOT '('  query ')'
   {$$ = new Ql_tree_node(not_n, 1, $3);
    }

 /* operations  of arity 2 */
 | query '+' query
   {$$ = new Ql_tree_node(plus_n, 2, $1, $3);
    }
 | query UNION query
   {$$ = new Ql_tree_node(union_n, 2, $1, $3);
    }
 | query '-' query
   {$$ = new Ql_tree_node(minus_n, 2, $1, $3);
    }
 | query EXCEPT query
   {$$ = new Ql_tree_node(except_n, 2, $1, $3);
    }
 | query '*' query
   {$$ = new Ql_tree_node(mult_n, 2, $1, $3);
    }
 | query INTERSECT query
   {$$ = new Ql_tree_node(intersect_n, 2, $1, $3);
    }
 | query '/' query
   {$$ = new Ql_tree_node(div_n, 2, $1, $3);
    }
 | query '%' query
   {$$ = new Ql_tree_node(mod_n, 2, $1, $3);
    }
 | query '=' query
   {$$ = new Ql_tree_node(equal_n, 2, $1, $3);
    }
 | query LIKE query 
   {$$ = new Ql_tree_node(like_n, 2, $1, $3);
    }
 | query DIFF query
   {$$ = new Ql_tree_node(diff_n, 2, $1, $3);
    }
 | query '<' query
   {$$ = new Ql_tree_node(inf_n, 2, $1, $3);
    }
 | query INFEQUAL query
   {$$ = new Ql_tree_node(inf_equal_n, 2, $1, $3);
    }
 | query '>' query
   {$$ = new Ql_tree_node(inf_n, 2, $3, $1);
    }
 | query SUPEQUAL query
   {$$ = new Ql_tree_node(inf_equal_n, 2, $3, $1);
    }
 | query tIN query
   {$$ = new Ql_tree_node(in_n, 2, $1, $3);
    }
 | query '[' query ']'	
   {$$ = new Ql_tree_node(ith_n, 2, $1, $3);
    }
 | query '[' ':' query ']'
   {$$ = new Ql_tree_node(head_n, 2, $1, $4);
    }
 | query '[' query ':' ']'	
   {$$ = new Ql_tree_node(tail_n, 2, $1, $3);
    }	
 | LIST '(' query DOTS query ')' 
   {$$ = new Ql_tree_node(range_n, 2, $3, $5);
    }




 /* operations  of arity n (or that will be represented as such) */
 | query '[' query ':' query ']'
   {$$ = (new Ql_tree_node(sublist_n, 1, $1))->add($3)->add($5);}
 | ARRAY '(' querieslist ')'
   {$$ = new Ql_tree_node(array_n, 1, $3);
    }
 | USET '(' querieslist ')'
   {$$ = new Ql_tree_node(set_n, 1, $3);
    }
 | tBAG '(' querieslist ')'
   {$$ = new Ql_tree_node(bag_n, 1, $3);
    }
 | LIST '(' querieslist ')'
   {$$ = new Ql_tree_node(list_n, 1, $3);
    }
 | tSET '(' querieslist ')'
   {if (OQL)
	$$ = new Ql_tree_node(set_n, 1, $3);
    else
        $$ = new Ql_tree_node(bag_n, 1, $3);
    }
 | TUPLE '(' fieldslist ')'
   {$$ = new Ql_tree_node(tuple_n, 1, $3);
    }
 | STRUCT '(' fieldslist ')'
      /** Murali. Bolo used to return TUPLE even if the token was
      	  "struct". I've changed that becoz I need STRUCT for other 
          purposes.. **/
   {$$ = new Ql_tree_node(tuple_n, 1, $3);
    }
 | query AND query
   {$$ = new Ql_tree_node(and_n, 2, $1, $3);
    }
 | query OR query
   {$$ = new Ql_tree_node(or_n, 2, $1, $3);
    }
 | query ARROW attname
   {$$ = new Ql_tree_node(extract_attribute_n, 2, $1, $3);
    }
 | query DOT attname
   {$$ = new Ql_tree_node(extract_attribute_n, 2, $1, $3);
    }
 | query ARROW ID '(' querieslist ')'
   {$$ = new Ql_tree_node(method_call_n, 3, $1, $3, $5);
    }
/** Murali.add 4/4. How do I support methods with no args ? **/
 | query ARROW ID '(' ')'
   {$$ = new Ql_tree_node(named_method_call_n, 2, $1, $3);}
/** End Murali.add **/
 | query ARROW ID '(' fieldslist ')'
   {$$ = new Ql_tree_node(named_method_call_n, 3, $1, $3, $5);
    }
/** Heck, I dont want this -- Murali 4/4 **/
/**
 | query ARROW ID '(' ')'
   {$$ = new Ql_tree_node(extract_attribute_n, 2, $1, $3);
    }
**/
/** End don't want **/
 | ID '(' querieslist ')'
   {$$ = new Ql_tree_node(function_or_create_n, 2, $1, $3);
    }
 | ID '(' fieldslist ')'
   {
 /* dts note: it seems that this should be a object_constructor_n,
	not a tuple_obj_n, by my reading of the grammar. */
		/*$$ = new Ql_tree_node(tuple_obj_n, 2, $1, $3); */
		$$ = new Ql_tree_node(object_constructor_n, 2, $1,$3); 
		//		new Ql_tree_node(tuple_n, 1, $3));
		// ok, treat fieldlist like struct/tuple, then turn
		// the tuple into the obj. costructor args.  This
		// seems bogus...
    }


/* quantifier */
 | EXISTS ID tIN query ':' query %prec AND
   {$$ = new Ql_tree_node(exists_n, 3, $2, $4, $6);
    }
 | FORALL ID tIN query ':' query %prec AND
   {$$ = new Ql_tree_node(forall_n, 3, $2, $4, $6);
    }
 | FOR ALL ID tIN query ':' query %prec AND
   {$$ = new Ql_tree_node(forall_n, 3, $3, $5, $7);
    }

/* BOLO enhancement: assignment */
/** Murali.change 2/25/95. I don't want this any more.
   I've moved this out to update_expr
 | query ASSIGN query
   {$$ = new Ql_tree_node(assign_n, 2, $1, $3);
    }
**/

/* algebraic iterator */
 | SELECT query FROM varslist WHERE query
   {$$ = new Ql_tree_node(sfw_n, 3, $2, $4, $6);
    }
 | SELECT query FROM varslist
   {$$ = new Ql_tree_node(sf_n, 2, $2, $4);
    }
 | SELECT UNIQUE query FROM varslist WHERE query
   {$$ = new Ql_tree_node(usfw_n, 3, $3, $5, $7);
    }
 | SELECT UNIQUE query FROM varslist
   {$$ = new Ql_tree_node(usf_n, 2, $3, $5);
    }
 | SELECT DISTINCT query FROM varslist WHERE query
   {$$ = new Ql_tree_node(usfw_n, 3, $3, $5, $7);
    }
 | SELECT DISTINCT query FROM varslist
   {$$ = new Ql_tree_node(usf_n, 2, $3, $5);
    }


/* sort and group */
  | SORT ID tIN query BY querieslist
   {$$ = new Ql_tree_node(sort_n, 3, $2, $4, $6);
    }
 | GROUP ID tIN query BY '(' fieldslist ')'
   {$$ = new Ql_tree_node(group_n, 3, $2, $4, $7);
    }
 | GROUP ID tIN query BY '(' fieldslist ')' WITH '(' fieldslist ')' 
   {$$ = new Ql_tree_node(group_with_n, 4, $2, $4, $7, $11);
    }
 ;


/* auxilaries */
querieslist : query %prec QUERYPREC
	   {$$ = new Ql_tree_node(actual_parameters_n, 1, $1);}
	 | query ',' querieslist
	   {$$ = $3->add($1);}
	 ;
onefield : attname ':' query
	   {$$ = new Ql_tree_node(one_field_n, 2, $1, $3);
	    }
	 ;
fieldslist : onefield %prec FIELDPREC
	   {$$ = new Ql_tree_node(fields_n, 1, $1);}
	 |  onefield ',' fieldslist
	   {$$ = $3->add($1);}
	 ;
onevar	: ID tIN query_closure
	   {$$ = new Ql_tree_node(one_scan_var_n, 2, $3, $1);}
	 ;
varslist : onevar %prec WHERE
      {$$ = new Ql_tree_node(from_n, 1, $1);}
   | onevar ',' varslist
      {$$ = $3->add($1);}
   | query_closure %prec WHERE
      {$$ = new Ql_tree_node(from_n, 1, 
                             new Ql_tree_node(one_scan_var_n, 1, $1));}
   | query_closure ',' varslist
      {$$ = $3->add(new Ql_tree_node(one_scan_var_n, 1, $1));}
   ;

distinctq : query %prec LASTPRED
	   {$$ = $1;}
	;

attname	: ID
	{$$ = $1;}
	| keyword
	{$$ = $1;}
	;

/* keywords */
 keyword : DEFINE
	   {$$ = new Ql_tree_node(id_n, "define");}
	 | AS
           {$$ = new Ql_tree_node(id_n, "as");}
	 | OQL_NIL
	   {$$ = new Ql_tree_node(id_n, "nil");}
	 | TRUE
	   {$$ = new Ql_tree_node(id_n, "true");}
	 | FALSE
	   {$$ = new Ql_tree_node(id_n, "false");}
	 | tSET
	   {$$ = new Ql_tree_node(id_n, "set");}
	 | LIST
	   {$$ = new Ql_tree_node(id_n, "list");}
	 | tBAG
	   {$$ = new Ql_tree_node(id_n, "bag");}
	 | ELT
	   {$$ = new Ql_tree_node(id_n, "element");}
	 | FLATTEN
	   {$$ = new Ql_tree_node(id_n, "flatten");}
	 | COUNT
	   {$$ = new Ql_tree_node(id_n, "count");}
	 | SUM
	   {$$ = new Ql_tree_node(id_n, "sum");}
	 | OQL_MIN
	   {$$ = new Ql_tree_node(id_n, "min");}
	 | OQL_MAX
	   {$$ = new Ql_tree_node(id_n, "max");}
	 | AVG
	   {$$ = new Ql_tree_node(id_n, "avg");}
	 | DISTINCT
	   {$$ = new Ql_tree_node(id_n, "distinct");}
	 | UNIQUE
	   {$$ = new Ql_tree_node(id_n, "unique");}
         | LISTOSET
	   {$$ = new Ql_tree_node(id_n, "listoset");}
	 | FIRST
	   {$$ = new Ql_tree_node(id_n, "first");}
	 | LAST
	   {$$ = new Ql_tree_node(id_n, "last");}
	 | OQL_ABS
	   {$$ = new Ql_tree_node(id_n, "abs");}
	 | '%'
	   {$$ = new Ql_tree_node(id_n, "mod");}
	 | NOT
	   {$$ = new Ql_tree_node(id_n, "not");}
	 | tIN
	   {$$ = new Ql_tree_node(id_n, "in");}
	 | LIKE
	   {$$ = new Ql_tree_node(id_n, "like");}
	 | AND
	   {$$ = new Ql_tree_node(id_n, "and");}
	 | OR
	   {$$ = new Ql_tree_node(id_n, "or");}
	 | TUPLE
	   {$$ = new Ql_tree_node(id_n, "tuple");}
      	 | STRUCT
      	    /** See previous comment about structs... **/
      	   {$$ = new Ql_tree_node(id_n, "tuple");}
	 | EXISTS
	   {$$ = new Ql_tree_node(id_n, "exists");}
	 | FORALL
	   {$$ = new Ql_tree_node(id_n, "forall");}
	 | SELECT
	   {$$ = new Ql_tree_node(id_n, "select");}
	 | FROM
	   {$$ = new Ql_tree_node(id_n, "from");}
	 | WHERE
	   {$$ = new Ql_tree_node(id_n, "where");}
	 | GROUP
	   {$$ = new Ql_tree_node(id_n, "group");}
	 | SORT
	   {$$ = new Ql_tree_node(id_n, "sort");}
	 | BY
	   {$$ = new Ql_tree_node(id_n, "by");}
	 | WITH
	   {$$ = new Ql_tree_node(id_n, "with");}
	 ;

ref_lit: tREF '(' STRING_LIT ')'
      {$$ = new Ql_tree_node(refconst_n, $3->id());}

/* this is the sdl grammar... */
/* we need to fix this to handle non-module decls... */
		
/* specification:		module_list */ 
specification:		definition_list 
	{	set_module_list($1); 	}
	;					
module_list:	module	
	{	$$ .set( decl_list_append(0,$1));  }
	|	module_list module
	{	$$ .set( decl_list_append($1,$2)); }
	|	module_list ';' /* allow semi separator/terminator. */
	;

module:	module_tag '{' mod_export_list mod_import_list  opt_definition_list '}'
	{
		 $$ .set( sdl_module($1,$3,$4,$5));
	}
module_tag:	MODULE SID
	{
		$$ .set( module_tag($2));
	}
	;
/* this seems debatable; why not a list after export instead of
   export before each element? */
mod_export_list:				/* empty */
	{	$$ .set( 0); }
	|	mod_export_list mod_export
	{	$$ .set( decl_list_append($1,$2)); }
	;
mod_export:	EXPORT export_def ';'
	{	$$ = $2; }
	;
export_def:	SID
	{	$$ .set( export_declarator($1)); }
	|	ALL
	{	$$ .set( export_declarator(0)); }
	;
mod_import_list: 			/* empty */
	{	$$ .set( 0); }
	|	mod_import_list mod_import
	{
		$$ .set( decl_list_append($1,$2));
	}
	;
mod_import:	USE module_name opt_alias ';'
	{	$$ .set( mod_import(USE,$2,$3));	}
	/* sdlq: is there a distinction between use and import ? */
	/* if not get rid of $1 */
	|	IMPORT	module_name ';'
	{	$$ .set( mod_import(IMPORT,$2,0));	}
	;
module_name: 	string_literal
	|	SID 
	{	$$ .set( const_name($1));		}
	;
/* used to include string literal here as possible module name; why?
   now deleted to keep expr/id as separate types.
module_name: string_literal
	|	SID
	;
*/

	/* why do we want this? */
opt_alias:	/* empty */
	{	$$ = 0; }
	|	AS SID
	{	$$ = $2; }
	;
/* allow empty definition list. */
opt_definition_list :	definition_list
	|	{	$$ .set( 0); }
	;
definition_list :	definition
	{
		$$ .set( decl_list_append(0,$1));
	}
	|	definition_list definition
	{
		$$ .set( decl_list_append($1,$2));
	}
	;
definition:	type_dcl ';'
	|	const_dcl ';'
	|	interface ';'
	/* no nested modules? */ 
	|	module  ';'
	|	';' 	{ ; /* ignore redundant semi */ }
	|	error ';'	{ $$ .set( 0); }/* attempt at error recovery */
	;
override_dcl : OVERRIDE override_list
	{
		/* $$  = override_dcl($2);*/
		$$ = $2;
	}
	;
override_list:	SID {
		$$  .set( override_dcl($1));
		/* $$ = list_append(0,$1); */
	}
	|	override_list ',' SID
	{
		$$ .set( decl_list_append($1,override_dcl($3)));
		/* $$ = list_append($1,$3); */
	}
	;
interface :	interface_dcl
	| forward_interface
	;
interface_dcl:	interface_header '{' interface_body '}'
	{
		$$ .set( interface_dcl($1,$3));
	}
	;
forward_interface:	INTERFACE SID
	{
		$$ .set( forward_dcl($2));
		/* this should be changed to be consistent
			with struct/union/enum handling */
	}
	;	
interface_header:	INTERFACE SID 
	{
		$$ = interface_header($2,0);
	}
	|	INTERFACE SID inheritance_spec
	{
		$$ = interface_header($2,$3);
	}
	;
/* in order to accomodate public, private specifiers, interface body
   is now a list of lists. */
interface_body:	export_access_list
	;
export_access_list:	/* empty */
	{	$$ .set( 0); }
	|
	export_access_list export_access_spec
	{
		$$ .set( decl_list_append($1,$2));
	}
	;
export_access_spec : access_spec ':' export_list
	{
		$$ .set( export_access_spec($1,$3));
	}
	;
export_list:	/* empty */
	{	$$ .set( 0);}
	|	export_list export
	{
		$$ .set( decl_list_append($1,$2));
	}
	;
export:	type_dcl ';'
	|	const_dcl ';'
	|	attr_dcl ';'
	|	op_dcl ';'
	|	rel_dcl	';' /* ODL addition */
	|	override_dcl ';'
	|	error ';'	
	{ $$ .set( 0); }/* attempt at error recovery */
	;
inheritance_spec:	':' access_scoped_name_list
	{	$$ = $2; }
	;
access_scoped_name_list:	access_scoped_name
	{
		$$ .set( decl_list_append(0,$1));
	}
	|	access_scoped_name_list ','  access_scoped_name
	{
		$$ .set( decl_list_append($1,$3));
	}
	;
access_scoped_name: access_spec scoped_name
	{	$$ .set( access_scoped_name($1,type_name($2)));
	}
	;
access_spec:	tPRIVATE	{ $$ = Private; }
	|	PROTECTED			{ $$ = Protected; }
	|	PUBLIC				{ $$ = Public;	}
	| /* empty */			{ $$ = Public;  }
	;
scoped_name_list:	type_name /* was scoped_name */
	{
		$$ = type_list_append(0,$1);
	}
	|	scoped_name_list ',' type_name /* was scoped_name */
	{
		$$  = type_list_append($1,$3);
	}
	;
type_name:	scoped_name	
	/* we replace all instances of scoped_name that are used
		as type names with the production type_name; this
		allows us to flag the ast node appropriately{.
	*/
	{ 	$$ .set( type_name($1));	}
	;
scoped_name:	SID %prec UNION
	|	DCOLON SID
	{	$$ = scoped_name(0,$2); 	}
	|	scoped_name DCOLON SID
	{
		$$ = scoped_name($1,$3);
	}
	;
const_dcl:	CONST const_type SID '=' const_exp
	{
		$$ .set( const_dcl($2,$3,$5));
	}
	;
const_type:	integer_type
	|	boolean_type
	|	floating_pt_type
	|	string_type
	|	type_name /* was scoped_name */
	;
const_exp:	or_expr
	;
or_expr:	xor_expr
	|	or_expr '|' xor_expr
	{	$$ .set( expr($1,'|',$3)); 	}
	;
xor_expr:	and_expr
	|	xor_expr '^' and_expr
	{	$$ .set( expr($1,'^',$3)); 	}
	;
and_expr:	shift_expr
	|	and_expr '&' shift_expr
	{	$$ .set( expr($1,'&',$3)); 	}
	;
shift_expr:	add_expr
	|	shift_expr RSHIFT add_expr
	{	$$ .set( expr($1,RSHIFT,$3)); 	}
	|	shift_expr LSHIFT add_expr
	{	$$ .set( expr($1,LSHIFT,$3)); 	}
	;
add_expr:	mult_expr
	|	add_expr '+' mult_expr
	{	$$ .set( expr($1,'+',$3)); 	}
	|	add_expr '-' mult_expr
	{	$$ .set( expr($1,'-',$3)); 	}
	;
mult_expr:	unary_expr
	|	mult_expr  '*' unary_expr
	{	$$ .set( expr($1,'*',$3)); 	}
	|	mult_expr  '/' unary_expr
	{	$$ .set( expr($1,'/',$3)); 	}
	|	mult_expr  '%' unary_expr
	{	$$ .set( expr($1,'%',$3)); 	}
	;
unary_expr:	unary_operator primary_expr
	{	$$ .set( expr(0,$1,$2)); 	}
	|	primary_expr
	;
unary_operator:	'-'	{ $$ = '-'; }
	|	'+'	{ $$ = '+'; }
	|	'~'	{ $$ = '~'; }
	;
primary_expr:	scoped_name
	{	$$ .set( const_name($1)); }
	|	literal
	|	'(' const_exp ')'
	{
		$$ = $2;
	}
	;
literal:	integer_literal
	|	string_literal
	|	character_literal
	|	floating_pt_literal
	|	boolean_literal
	;
boolean_literal:
		TRUE	{ $$ .set( v_to_expr($1)); }
	|	FALSE	{ $$ .set( v_to_expr($1)); }
	;
   /** I cant really do much checking here... **/
literal_list: literal
      {$$ = new Ql_tree_node(literalList_n); $$->add(expr_to_v($1));}
   | literal_list ',' literal
      {$$ = $1->add(expr_to_v($3));}
   ;
homogenous_collection_const: '[' literal_list ']'
      {$$ = $2; $$->setType(homogenousCollectionConst_n);}
   ;
heterogenous_collection_const: '{' literal_list '}'
      {$$ = $2; $$->setType(heterogenousCollectionConst_n);}
   ;
positive_int_const:	const_exp
	;
type_dcl:	TYPEDEF type_declarator
	{	$$ .set(typedef_dcl($2)); }
	|	struct_type
	{	$$ .set( get_dcl_for_type($1)); 	}
	|	union_type
	{	$$ .set( get_dcl_for_type($1)); 	}
	|	enum_type
	{	$$ .set( get_dcl_for_type($1)); 	}
	|	EXTERNAL tdecl_kind SID
	{	$$ .set( get_extern_type($2,$3)); } 
	;

tdecl_kind: 
	CLASS 
	{	$$ = Sdl_Class; }
	|  TYPEDEF 
	{	$$ = Sdl_ExternType; }
	|	ENUM
	{	$$ = Sdl_enum; }
	|	STRUCT
	{	$$ = Sdl_struct; }
	|	UNION
	{	$$ = Sdl_union; }
	;

type_declarator:	type_spec declarators
	{
		$$ .set( type_declarator($1,$2));
	}
	
	;
type_spec:	simple_type_spec
	|	constr_type_spec
/*	|	collection_type_spec  .. ODL addition */
	;
simple_type_spec:	base_type_spec
	|	template_type_spec
	|	type_name /* was scoped_name */
	;
base_type_spec:	floating_pt_type
	|	integer_type
	|	char_type
	|	boolean_type
	|	octet_type
	|	any_type
	|	pool_type
	;
template_type_spec:	sequence_type
	|	string_type
	|	relation_type
	;
constr_type_spec:	struct_type
	|	union_type
	|	enum_type
	;
declarators:	declarator
	{
		$$ .set( decl_list_append(0,$1));
	}
	|	declarators ',' declarator
	{
		$$ .set( decl_list_append($1,$3));
	}
declarator:	simple_declarator
	|	complex_declarator
	;
simple_declarator:	SID
	{
		$$ .set( id_declarator($1));
	}
	;
complex_declarator: array_declarator
	;
floating_pt_type:	FLOAT
	{
		$$ .set( primitive_type(Sdl_float));
	}
	|	DOUBLE
	{
		$$ .set( primitive_type(Sdl_double));
	}
	;
integer_type:	signed_int
	|	unsigned_int
	;
signed_int:	signed_long_int
	|	signed_short_int
	;
signed_long_int:	LONG
	{
		$$ .set( primitive_type(Sdl_long));
	}
	;
signed_short_int:	SHORT
	{
		$$ .set( primitive_type(Sdl_short));
	}
	;
unsigned_int:	unsigned_long_int
	|	unsigned_short_int
	;
unsigned_long_int: UNSIGNED LONG
	{
		$$ .set( primitive_type(Sdl_unsigned_long));
	}
	;
unsigned_short_int: UNSIGNED SHORT
	{
		$$ .set( primitive_type(Sdl_unsigned_short));
	}
	;
char_type:	CHAR
	{
		$$ .set( primitive_type(Sdl_char));
	}
	;
boolean_type:	BOOLEAN
	{
		$$ .set( primitive_type(Sdl_boolean));
	}
	;
octet_type:	OCTET
	{
		$$ .set( primitive_type(Sdl_octet));
	}
	;
any_type:	ANY
	{
		$$ .set( primitive_type(Sdl_any));
	}
pool_type:	POOL
	{
		$$ .set( primitive_type(Sdl_pool));
	}
	;
struct_type:	STRUCT SID '{' member_list '}'
	{
		$$ .set( struct_type($2,$4));
	}
	|	STRUCT SID /* forward dcl */
	{
		$$ .set( struct_type($2,0));
	}
	;
member_list:	member
	{
		$$ .set( decl_list_append(0,$1));
	}
	|	member_list member
	{
		$$ .set( decl_list_append($1,$2));
	}
	;
member:		type_spec declarators ';'
	{
		$$ .set( member($1,$2));
	}
	;
/* used to just have a type for the tag; now add in an id (opt) */
union_type:	UNION SID SWITCH '(' switch_type_spec SID')' '{' switch_body '}' %prec UNION
	{
		$$ .set( union_type($2,$5,$6,$9));
	}
	| UNION SID SWITCH '(' switch_type_spec ')' '{' switch_body '}'
	{
		$$ .set( union_type($2,$5,0,$8));
	}
	|	UNION SID /* forward dlc */
	{
		$$ .set( union_type($2,0,0,0));
	}
	;

switch_type_spec:	integer_type
	|	char_type
	|	boolean_type
	|	enum_type 
	|	type_name /* was scoped_name */ 
	;
switch_body:	case_list
	;
case_list:	case
	{
		$$ .set( decl_list_append(0,$1));
	}
	|	case_list case
	{
		$$ .set( decl_list_append($1,$2));
	}
	;
case:		case_label_list element_spec ';'
	{
		$$ .set( case_elt($1,$2));
	}
	;
case_label_list:	case_label
	{
		$$ .set( expr_list_append(0,$1));
	}
	|	case_label_list case_label
	{
		$$ .set( expr_list_append($1,$2));
	}

	;
case_label:	CASE const_exp ':'
	{
		$$ = $2;
	}
	|	DEFAULT ':'
	{
		/* $$ = $1; */
		set_yylval("default",Sdl_void);
		// this won't work.
		$$  = yylval.exprpt;
	}
	;
element_spec:	type_spec declarator
	{
		$$ .set( element_spec($1,$2));
	}
	;
enum_type:	ENUM SID '{' enumerator_list '}'
	{
		$$ .set( enum_type($2,$4));
	}
	;
enumerator_list:	enumerator
	{
		$$ .set( decl_list_append(0,$1));
	}
	|	enumerator_list ',' enumerator
	{
		$$ .set( decl_list_append($1,$3));
	}
	;
enumerator:	SID
	{
		/* $$ = enumerator($1); */
		$$ .set( enum_declarator($1));
	}
	;
sequence_type:	SEQUENCE '<' simple_type_spec ',' positive_int_const '>'
	{
		$$ .set( sequence_type($3,$5));
	}
	|	SEQUENCE '<' simple_type_spec '>'
	{
		$$ .set( sequence_type($3,0));
	}
	;
string_type:	tSTRING '<' positive_int_const '>'
	{
		$$ .set( string_type($3));
	}
	|	tSTRING
	{
		$$ .set( string_type(0));
	}
	|	TEXT '<' positive_int_const '>'
	{
		$$ .set( text_type($3));
	}
	|	TEXT
	{
		$$ .set( text_type(0));
	}
	;
relation_type:	rel_id '<' simple_type_spec '>'
	{
		$$ .set( relation_type($1,$3));
	}
	|	tINDEX '<' simple_type_spec ',' simple_type_spec '>'
	{
		$$ .set( index_type($3,$5));
	}
	;
rel_id:	
	tREF { $$ = Sdl_ref;}
	|tLREF 
	{	$$ = Sdl_lref; }
	| tSET 
	{	$$ = Sdl_set; }
	| tBAG 
	{	$$ = Sdl_bag; }
	| LIST 
	{	$$ = Sdl_list; }
	| MULTILIST
	{	$$ = Sdl_multilist; }
	;
array_declarator:	SID fixed_array_size_list
	{
		$$ .set( array_declarator($1,$2));
	}
	;
fixed_array_size_list: fixed_array_size
	{
		$$ .set( expr_list_append(0,$1));
	}
	|	fixed_array_size_list fixed_array_size
	{
		$$ .set( expr_list_append($1,$2));
	}
	;
fixed_array_size:	'[' positive_int_const ']'
	{
		$$ = $2;
	}
	;
attr_dcl:	opt_readonly ATTRIBUTE simple_type_spec declarators
/* opt_attr_pragma is an ODL addition */
	{
 		$$ .set( attr_dcl($1,$3,$4));
	}
	;
opt_readonly:	/* empty */
	{ $$  = 0;
	}
	|	READONLY { $$ = READONLY; }
	|	INDEXABLE /* a cheap hack for indexable; readonly is not used?? */ { $$ = INDEXABLE; }
	;
ex_member_list:	/* empty */
 	{	$$ .set( 0); }
	|	ex_member_list member
	{
 		$$ .set( decl_list_append($1,$2));
	}
	;
op_dcl:	op_type_spec SID parameter_dcls opt_raises_expr opt_context_expr
	{
 		$$ .set( op_dcl($1,$2,$3,$4,$5));
	}
	;
op_type_spec:	simple_type_spec
	| VOID
	{
 		$$ .set( primitive_type(Sdl_void));
	}
	;
parameter_dcls:	'(' param_dcl_list ')'
	{	$$ = $2; }
	|	'(' ')'
	{	$$ .set( 0);	}
	;
param_dcl_list:	param_dcl
	{
		$$ .set( decl_list_append(0,$1));
	}
	|	param_dcl_list ',' param_dcl
	{
		$$ .set( decl_list_append($1,$3));
	}
	;
param_dcl:	param_attribute simple_type_spec declarator
	{
 		$$ .set( parm_dcl($1,$2,$3));
	}
	;
param_attribute:	tIN { $$ = In; }
	|	tOUT { $$ = Out; }
	|	tINOUT { $$ = InOut; }
	;
opt_raises_expr:	/* empty */
	{       $$ = 0; }
	|	raises_expr
	;
raises_expr:	RAISES '(' scoped_name_list ')'
	{
		$$ = raises_expr($3);
	}
	;
opt_context_expr:	/* empty */
	{	$$ = 0; }
	|	context_expr
	;
context_expr:	/* CONTEXT '(' string_literal_list ')'
	{
		$$ = $3;
	}
	|	 this was the old context_expr from odl/idl; no known semantics. */ 
	CONST
	{
		$$ = CONST; 
	}
	{
		$$ = CONST; 
	}
	;
/*  temporarly hijack this for const decl. */
/* it would be better to put this in the opt_attribute place, but
   that would be different than c++ usage, unneccesarily. */
string_literal_list:	string_literal
	{
		$$ .set( expr_list_append(0,$1));
	}
	|	string_literal_list ',' string_literal
	{
		$$ .set( expr_list_append($1,$3));
	}
	;

/* solomon reinterpretation of relationship */
/* note: relation_type not quite right here as it allows lref */
rel_dcl:	RELATIONSHIP relation_type simple_declarator inverse_clause ordered_clause
	{
		$$ .set( relationship_dcl($2,$3, $4,$5));
	}
inverse_clause: /* empty */
	{	$$ = 0;		}
	| INVERSE scoped_name
	{	$$ = $2;	}

	;
ordered_clause: /* empty */
	{ 	$$ = 0;		}
	|  ORDERED_BY scoped_name
	{	$$ = $2;	}
		
		
		
/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
/** Token-like non-terminals.
    I didnt have this stuff before. I've moved them out from the LEX
    file to here, becoz the LEX file needed to use global variables, and 
    I didnt want global variables in a re-entrant parser
  -- Murali 3/31
**/
OQL_NIL: LEX_OQL_NIL
      {$$ = new Ql_tree_node(nil_n, "");}
   ;
FALSE: LEX_FALSE
      {$$ = new Ql_tree_node(false_n, "");}
   ;
TRUE: LEX_TRUE
      {$$ = new Ql_tree_node(true_n, "");}
   ;
ID: LEX_ID
      {$$ = new Ql_tree_node(id_n, yylval.string);}
   ;
/* ID used in context of sdl/odl grammar */
SID: LEX_ID
    {
		check_id_token(yylval.string); 
		$$ = get_string_node(yylval.string);
	}

   ;
INT_LIT: LEX_INT_LIT
      {$$ = new Ql_tree_node(int_n, yylval.string);}
   ;
FLOAT_LIT: LEX_FLOAT_LIT
    {$$ = new Ql_tree_node(float_n, yylval.string);}
   ;  
CHAR_LIT: LEX_CHAR_LIT
    {$$ = new Ql_tree_node(char_n, yylval.string);}
   ;
STRING_LIT: LEX_STRING_LIT
    {$$ = new Ql_tree_node(string_n, yylval.string);}
   ;

/* odl backpatch for lits */
integer_literal	:	INT_LIT
	{	$$ .set( v_to_expr($1)); }
	;
string_literal:	STRING_LIT
	{	$$ .set(v_to_expr($1)); }
	;
character_literal:	CHAR_LIT
	{	$$ .set(v_to_expr($1)); }
	;
floating_pt_literal:	FLOAT_LIT
	{	$$ .set( v_to_expr($1)); }
	;

%%


/*************** The end ***********************************************/
/** C code begins now...  **/

extern "C" {int yywrap()  {return 1;}}

int yyerror(char* s)
{
   errstream() << "Parse error in line " << Bparser_state()->line()
               << ". Current token is \"" 
               << Bparser_state()->scanner()->YYText()
               << "\"\n Ignoring rest of this input" << endl;
   Bparser_state()->Reset();
}

int oql_parser()
{
   int	 i;

   Bparser_state()->setupInput();
   i = yyparse();
   if (Bparser_state()->eof())
      Bparser_state()->scanner()->yyrestart();
   return i;
}

