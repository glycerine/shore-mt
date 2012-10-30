/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "metatypes.sdl.h"
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC extern
#endif
typedef struct node *  node_pt;
typedef struct Ql_tree_node * anode_pt;
class sdlDeclaration;
class sdlExprNode;
class sdlType;
typedef REF(sdlDeclaration)  decl_pt;
typedef struct REF(sdlType)  type_pt;
typedef struct REF(sdlExprNode)  expr_pt;
template <class t> class UnRef
{
public:
	OTEntry * ote;
	void set(const Ref<t> & r ) { ote = r.u.otentry; };
	operator Ref<t> () { Ref<t> rval; rval.u.otentry = ote; return rval;}
};
extern int yylex();
EXTERNC 
node_pt 
get_string_node (char *string);
void
insert_rwords();
void
dump_str_tab();
EXTERNC
node_pt 
list_append(node_pt list, node_pt lastinfo);
EXTERNC
node_pt  
module(node_pt id, node_pt def_list);
EXTERNC
decl_pt 
interface_dcl(node_pt  header, decl_pt body);
EXTERNC
decl_pt 
forward_dcl(node_pt id);
EXTERNC
node_pt 
interface_header(node_pt id,decl_pt inherit);
EXTERNC
node_pt 
scoped_name(node_pt  left, node_pt  right);
EXTERNC
decl_pt 
const_dcl(type_pt type,node_pt id,expr_pt expr);
EXTERNC
expr_pt 
expr(expr_pt left,short op, expr_pt right);
EXTERNC
decl_pt 
type_declarator(type_pt spec,decl_pt decls);
EXTERNC
decl_pt 
id_declarator(node_pt  p);
EXTERNC
decl_pt 
export_declarator(node_pt  p);
EXTERNC
decl_pt 
enum_declarator(node_pt  p);
EXTERNC
type_pt 
primitive_type(TypeTag p);
EXTERNC
type_pt
struct_type(node_pt id, decl_pt members);
EXTERNC
type_pt
class_type(node_pt id);
EXTERNC
decl_pt 
member(type_pt spec,decl_pt decls);
EXTERNC
type_pt
union_type(node_pt id, type_pt desc, node_pt tagid, decl_pt  body);
EXTERNC
decl_pt 
case_elt(expr_pt clist,decl_pt elem);
EXTERNC
decl_pt 
element_spec(type_pt spec, decl_pt decl);
EXTERNC
type_pt
enum_type(node_pt tag, decl_pt  elist);
EXTERNC
type_pt 
sequence_type(type_pt tparm, expr_pt bound);
EXTERNC
type_pt 
string_type( expr_pt bound);
EXTERNC
type_pt 
text_type( expr_pt bound);
EXTERNC
decl_pt
array_declarator(node_pt id, expr_pt sizelist);
EXTERNC
decl_pt 
attr_dcl(short readonly,type_pt spec,decl_pt decls);
EXTERNC
decl_pt 
op_dcl(type_pt type,node_pt id,decl_pt parms,node_pt exception,short context);
EXTERNC
decl_pt 
parm_dcl(Mode  attr, type_pt spec, decl_pt dcl);
EXTERNC
node_pt 
context_expr(node_pt  slist);
EXTERNC
node_pt 
raises_expr(node_pt  slist);
EXTERNC
decl_pt 
rel_dcl(type_pt type,decl_pt  decls,node_pt  rel_pragma, decl_pt rel_declarator);
EXTERNC
decl_pt 
relationship_dcl(type_pt type,decl_pt  decl,node_pt  inv, node_pt ord);
EXTERNC
decl_pt 
except_dcl(node_pt type,decl_pt  decls);
EXTERNC
type_pt 
unsigned_type(short type);
EXTERNC
decl_pt 
sdl_module(decl_pt,decl_pt,decl_pt,decl_pt);
EXTERNC
decl_pt 
module_tag(node_pt);
EXTERNC
decl_pt
mod_import(short,expr_pt,node_pt);
EXTERNC
decl_pt
typedef_dcl(decl_pt);
EXTERNC
node_pt
forward_interface(node_pt);
EXTERNC
decl_pt
access_scoped_name(Zone ,type_pt);
EXTERNC
decl_pt
export_access_spec(Zone,decl_pt);
EXTERNC
type_pt
relation_type(TypeTag,type_pt);
EXTERNC
type_pt
index_type(type_pt,type_pt);
EXTERNC
type_pt
type_name(node_pt);

EXTERNC
expr_pt
const_name(node_pt);

EXTERNC
void
init_sdl_db();	/* initially, this reads from standard input and builds the db */
EXTERNC
type_pt
collection_type_spec(type_pt );
EXTERNC
decl_pt
override_dcl(node_pt );
EXTERNC
type_pt
constr_type_spec(type_pt);
EXTERNC
decl_pt
decl_list_append(decl_pt,decl_pt);
EXTERNC
node_pt
type_list_append(node_pt,type_pt);
EXTERNC
expr_pt
expr_list_append(expr_pt,expr_pt);
EXTERNC
decl_pt
get_extern_type(TypeTag,node_pt);
EXTERNC
type_pt
get_type_from_dcl(decl_pt);
EXTERNC
void
set_module_list(decl_pt);
EXTERNC
expr_pt
case_label(expr_pt);
EXTERNC
decl_pt
get_dcl_for_type(type_pt tpt);
EXTERNC
void
check_module_name(node_pt);
EXTERNC
short check_id_token(char *);
// aqua to sdl node translation ???
node_pt
v_to_node(anode_pt);
expr_pt
v_to_expr(anode_pt);
anode_pt
expr_to_v(expr_pt);
extern "C"
void
set_yylval(char *str,TypeTag code);

