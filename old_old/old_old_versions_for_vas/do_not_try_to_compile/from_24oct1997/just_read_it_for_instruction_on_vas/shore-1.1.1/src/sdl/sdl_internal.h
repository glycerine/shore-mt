/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

// sdl_internal.h: this file is included by all source implementing
// the sdl metatype system runtime.
#include "metatypes.sdl.h"
// compatibility hack:  define in sdl prefix for all sdl types.
#define ExprNode sdlExprNode
#define Declaration sdlDeclaration
#define ConstDecl sdlConstDecl
#define ArmDecl sdlArmDecl
#define AttrDecl sdlAttrDecl
#define RelDecl sdlRelDecl
#define ParamDecl sdlParamDecl
#define ModDecl sdlModDecl
#define Type sdlType
#define StructType sdlStructType
#define EnumType sdlEnumType
#define ArrayType sdlArrayType
#define CollectionType sdlCollectionType
#define RefType sdlRefType
#define IndexType sdlIndexType
#define OpDecl sdlOpDecl
#define InterfaceType sdlInterfaceType
#define Module sdlModule
#define UnionType sdlUnionType
