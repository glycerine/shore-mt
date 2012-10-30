
%{
#include <iostream.h> // apparently needed for some assert or other.
#include "metatypes.sdl.h"
#include "sdl_internal.h"
#include "sdl_ext.h"

extern int yylex();
extern "C" void yyerror();
%}
%start specification
/* %union { struct node * nodept; REF(Declaration) *declpt; REF(Type) *typept; REF(ExprNode) *exprpt;short code; } */
	
%union { 
	struct node * nodept; 
	UnRef<sdlDeclaration> declpt; 
	UnRef<sdlType> typept; 
	UnRef<sdlExprNode> exprpt; 
	enum Mode mode;
	Zone zone;
	TypeTag ttag;
	short code; 
}
%token INTERFACE
%token ID
%token DCOLON
%token CONST
%token RSHIFT
%token LSHIFT
%token tTRUE
%token tFALSE
%token TYPEDEF
%token FLOAT
%token DOUBLE
%token LONG
%token SHORT
%token INT /* perhaps dubious sdl addtion */
%token UNSIGNED
%token CHAR
%token BOOLEAN
%token OCTET
%token ANY
%token STRUCT
%token UNION
%token SWITCH
%token CASE
%token DEFAULT
%token ENUM
%token SEQUENCE
%token tSTRING
%token TEXT
%token ATTRIBUTE READONLY EXCEPTION VOID tOUT tIN tINOUT RAISES CONTEXT ONEWAY
%token MODULE
/* ODL addtions */
%token DIRECT RELATIONSHIP UNIQUE ORDERED
%token BI_ARROW /* <-> token */
/* SDL additions */
%token REL_ID  /* Ref, LRef, Set, Bag, List, MultiList */
%token tREF
%token tLREF
%token tSET
%token tBAG
%token LIST
%token MULTILIST
%token EXPORT
%token IMPORT
%token USE
%token ALL
%token AS
%token tPRIVATE
%token PROTECTED
%token PUBLIC
%token STATIC
%token OVERRIDE
%token EXTERNAL
%token CLASS
%token POOL
%token INVERSE
%token ORDERED_BY


/* the following are never actually used as tokens, they correspond
   to nonterminals and are used as type tags for internal data 
   structures
*/
%token SCOPED_NAME
/*
%token FORWARD
%token INTERFACE_HEADER
%token CONST_ID
%token TYPE_DCL
%token MEMBER
%token ARRAY_DCL
%token OPTIONS
%token PARM_LIST
%token OP_DCL
%token OP_NAME
%token  NODE_LIST
%token  CONS
%token  SDL_MODULE
%token  STRUCT_MODULE
%token  UNION_MODULE
%token  INTERFACE_MODULE
*/
/* still more type code tokens */
/*
%token TYPE_ID
%token ARRAY_TYPE
%token UNDEF_NAME
%token TYPEDEF_NAME
%token STRUCT_NAME
%token UNION_NAME
%token ENUM_NAME
%token CONST_NAME
%token EXCEPTION_NAME
%token INTERFACE_NAME
%token ATTRIBUTE_NAME
%token RELATIONSHIP_NAME
%token STRUCT_MEMBER_NAME
%token UNION_MEMBER_NAME
%token MODULE_NAME
%token OP_DCL_NAME
%token OVERRIDE_NAME
*/
%token INDEXABLE
%token tINDEX
%token LAST_TOKEN

/* addtional tokens that badly look like rules */
%token integer_literal string_literal character_literal floating_pt_literal

/* precedence rule to resolve shift-reduce conflict in union decl depending
	on optional tag var id.
*/
%nonassoc UNION
%nonassoc SWITCH

/* type specifications: nonterminals are in general nodept; 
   terminals are code except for identifies and string consts */
%type <declpt> export_list definition_list specification opt_definition_list
%type <declpt> interface interface_dcl forward_interface definition 
%type <declpt> module module_tag
%type <nodept> interface_header
%type <declpt> inheritance_spec interface_body
%type <nodept> scoped_name_list scoped_name 
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
%type <code> opt_readonly
%type <declpt> op_dcl except_dcl param_dcl_list param_dcl rel_dcl override_dcl 
%type <declpt> parameter_dcls 
%type <nodept> opt_raises_expr raises_expr 
%type <code>   opt_context_expr context_expr
%type <mode>   param_attribute
%type <exprpt> string_literal_list 

/*
%type <nodept> opt_rel_pragma rel_pragma_list
%type <nodept> rel_pragma
*/
%type <declpt> mod_export_list mod_import_list mod_export
%type <declpt> export_def mod_import
%type <declpt> export_access_spec export_access_list /* opt_rel_declarator */
%type <nodept> opt_alias
%type <declpt> module_list access_scoped_name access_scoped_name_list
%type <declpt> override_list
%type <ttag> tdecl_kind 
%type <ttag> rel_id
%type <zone>  access_spec
%type <nodept> VOID
%type <nodept> ID ALL USE IMPORT 
%type <code> 	tPRIVATE PROTECTED PUBLIC
%type <exprpt> integer_literal string_literal character_literal module_name
%type <exprpt> floating_pt_literal
%type <exprpt> tTRUE tFALSE  DEFAULT
%type <nodept> FLOAT DOUBLE LONG SHORT INT CHAR BOOLEAN OCTET ANY
%type <nodept> READONLY ONEWAY tIN tOUT tINOUT DIRECT  INDEXABLE
%type <nodept> UNIQUE ORDERED  CONST
%type <nodept> tREF tLREF tSET tBAG LIST MULTILIST POOL
%type <code> UNION STRUCT CLASS ENUM TYPEDEF
%type <code>	'|' '^' '&' RSHIFT LSHIFT '+' '-' '*' '/' '%' '~'
%type <code>   unary_operator

%%
specification:		module_list
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
module_tag:	MODULE ID
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
export_def:	ID
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
	|	ID 
	{	$$ .set( const_name($1));		}
	;
/* used to include string literal here as possible module name; why?
   now deleted to keep expr/id as separate types.
module_name: string_literal
	|	ID
	;
*/

	/* why do we want this? */
opt_alias:	/* empty */
	{	$$ = 0; }
	|	AS ID
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
	|	except_dcl ';'
	|	interface ';'
	/* no nested modules? */ |	module |';'
		error ';'	{ $$ .set( 0); }/* attempt at error recovery */
	;
override_dcl : OVERRIDE override_list
	{
		/* $$  = override_dcl($2);*/
		$$ = $2;
	}
	;
override_list:	ID {
		$$  .set( override_dcl($1));
		/* $$ = list_append(0,$1); */
	}
	|	override_list ',' ID
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
forward_interface:	INTERFACE ID
	{
		$$ .set( forward_dcl($2));
		/* this should be changed to be consistent
			with struct/union/enum handling */
	}
	;	
interface_header:	INTERFACE ID 
	{
		$$ = interface_header($2,0);
	}
	|	INTERFACE ID inheritance_spec
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
	|	except_dcl ';'
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
scoped_name:	ID %prec UNION
	|	DCOLON ID
	{	$$ = scoped_name(0,$2); 	}
	|	scoped_name DCOLON ID
	{
		$$ = scoped_name($1,$3);
	}
	;
const_dcl:	CONST const_type ID '=' const_exp
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
	{	$$ .set( expr($1,$2,$3)); 	}
	;
xor_expr:	and_expr
	|	xor_expr '^' and_expr
	{	$$ .set( expr($1,$2,$3)); 	}
	;
and_expr:	shift_expr
	|	and_expr '&' shift_expr
	{	$$ .set( expr($1,$2,$3)); 	}
	;
shift_expr:	add_expr
	|	shift_expr RSHIFT add_expr
	{	$$ .set( expr($1,RSHIFT,$3)); 	}
	|	shift_expr LSHIFT add_expr
	{	$$ .set( expr($1,LSHIFT,$3)); 	}
	;
add_expr:	mult_expr
	|	add_expr '+' mult_expr
	{	$$ .set( expr($1,$2,$3)); 	}
	|	add_expr '-' mult_expr
	{	$$ .set( expr($1,$2,$3)); 	}
	;
mult_expr:	unary_expr
	|	mult_expr  '*' unary_expr
	{	$$ .set( expr($1,$2,$3)); 	}
	|	mult_expr  '/' unary_expr
	{	$$ .set( expr($1,$2,$3)); 	}
	|	mult_expr  '%' unary_expr
	{	$$ .set( expr($1,$2,$3)); 	}
	;
unary_expr:	unary_operator primary_expr
	{	$$ .set( expr(0,$1,$2)); 	}
	|	primary_expr
	;
unary_operator:	'-'
	|	'+'
	|	'~'
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
		tTRUE
	|	tFALSE
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
	|	EXTERNAL tdecl_kind ID
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
simple_declarator:	ID
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
struct_type:	STRUCT ID '{' member_list '}'
	{
		$$ .set( struct_type($2,$4));
	}
	|	STRUCT ID /* forward dcl */
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
union_type:	UNION ID SWITCH '(' switch_type_spec ID')' '{' switch_body '}' %prec UNION
	{
		$$ .set( union_type($2,$5,$6,$9));
	}
	| UNION ID SWITCH '(' switch_type_spec ')' '{' switch_body '}'
	{
		$$ .set( union_type($2,$5,0,$8));
	}
	|	UNION ID /* forward dlc */
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
		$$ = $1;
	}
	;
element_spec:	type_spec declarator
	{
		$$ .set( element_spec($1,$2));
	}
	;
enum_type:	ENUM ID '{' enumerator_list '}'
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
enumerator:	ID
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
array_declarator:	ID fixed_array_size_list
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
attr_dcl:	opt_readonly ATTRIBUTE simple_type_spec declarators /* opt_attr_pragma  */
/* opt_attr_pragma is an ODL addition */
	{
		$$ .set( attr_dcl($1,$3,$4));
	}
	;
opt_readonly:	/* empty */
	{ $$  = 0;
	}
	|	READONLY { $$ = READONLY; }
	|	INDEXABLE { $$ = INDEXABLE; }/* a cheap hack for indexable; readonly is not used?? */
	;
except_dcl:	EXCEPTION ID '{' ex_member_list '}'
	{
		$$ .set( except_dcl($2,$4));
	}
	;
ex_member_list:	/* empty */
	{	$$ .set( 0); }
	|	ex_member_list member
	{
		$$ .set( decl_list_append($1,$2));
	}
	;
op_dcl:	op_type_spec ID parameter_dcls opt_raises_expr opt_context_expr
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

/* more odl stuff */
/* attr pragma crap deleted */
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
		
		

%%
