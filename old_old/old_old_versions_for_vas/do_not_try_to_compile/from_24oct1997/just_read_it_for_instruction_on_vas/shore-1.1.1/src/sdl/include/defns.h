/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef	DEFNS_H
#define	DEFNS_H
#include <iostream.h>


enum SdlTypes {Sdl_NoType_t, Sdl_any_t, Sdl_char_t, 
	       Sdl_float_t, Sdl_double_t,
	       Sdl_long_t, Sdl_short_t, Sdl_ulong_t, Sdl_ushort_t,
	       Sdl_string_t, Sdl_void_t, Sdl_bool_t, Sdl_octet_t,
	       Sdl_EnumType_t, Sdl_StructType_t, Sdl_UnionType_t, 
	       Sdl_ObjectType_t, Sdl_InterfaceType_t, Sdl_MethodType_t, 
	       Sdl_ModuleType_t, Sdl_ArrayType_t, Sdl_SequenceType_t, 
	       Sdl_StringType_t, Sdl_RefType_t, Sdl_LRefType_t,
	       Sdl_BagType_t, Sdl_SetType_t, Sdl_ListType_t, 
	       Sdl_MultilistType_t,
	       Sdl_NamedType_t};

enum	SdlOpAttr {Sdl_oneway, Sdl_not_oneway};
enum	SdlAttrPragma {Sdl_direct};

class Declaration;
class Sdl_Type;
class ExprNode;
class Type;
class Scope;
class Ql_tree_node;

typedef  Ql_tree_node ScopedName;

#define DO(x) if (!(x)) return 0

#include <iostream.h>
#include <assert.h>
#define para_check assert
ostream& errstream();

#define paraNOERROR           0
#define paraQ_ERROR          -1
#define paraQ_PARSE_ERROR    -2
#define paraQ_TYPECHK_ERROR  -3


#endif /** DEFNS_H **/

