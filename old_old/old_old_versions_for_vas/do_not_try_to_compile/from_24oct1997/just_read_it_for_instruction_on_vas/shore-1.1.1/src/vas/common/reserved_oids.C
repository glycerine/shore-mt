/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/reserved_oids.C,v 1.17 1995/08/15 22:26:11 schuh Exp $
 */
#include <copyright.h>
#include <assert.h>
#include <stream.h>
#include <basics.h>
#include <serial_t.h>
#include <lid_t.h>
#include <cat.h>
#include <reserved_oids.h>

#define RESERVE(name)\
_cat(_cat(const ReservedOid ReservedOid::,name),(__LINE__<<3));\
_cat(_cat(const ReservedSerial ReservedSerial::,name),(__LINE__<<3));

RESERVE(_RootDir)

#ifdef notdef
// TODO REMOVE FROM .h FILE TOO:
RESERVE(_Meta)
RESERVE(_Module)
RESERVE(_char)
RESERVE(_u_char)
RESERVE(_short)
RESERVE(_u_short)
RESERVE(_int)
RESERVE(_u_int)
RESERVE(_Interface)
// ... etc
#endif

RESERVE(_nil)
RESERVE(_Directory)
RESERVE(_Xref)
RESERVE(_Symlink)
RESERVE(_Pool)
RESERVE(_UnixFile)
RESERVE(MaxReservedFS)

RESERVE(_UserDefined)

// sdl type system metatype objects
RESERVE(_sdlExprNode);
RESERVE(_sdlDeclaration);
RESERVE(_sdlTypeDecl);
RESERVE(_sdlConstDecl);
RESERVE(_sdlArmDecl);
RESERVE(_sdlAttrDecl);
RESERVE(_sdlRelDecl);
RESERVE(_sdlParamDecl);
RESERVE(_sdlModDecl);
RESERVE(_sdlType);
RESERVE(_sdlNamedType);
RESERVE(_sdlStructType);
RESERVE(_sdlEnumType);
RESERVE(_sdlArrayType);
RESERVE(_sdlSequenceType);
RESERVE(_sdlCollectionType);
RESERVE(_sdlRefType);
RESERVE(_sdlIndexType);
RESERVE(_sdlOpDecl);
RESERVE(_sdlInterfaceType);
RESERVE(_sdlModule);
RESERVE(_sdlUnionType);
RESERVE(_sdlClassType);
RESERVE(_sdlExtTypeDecl);
RESERVE(_sdlEType);
RESERVE(_sdlAliasDecl);
RESERVE(_sdlNameScope);
RESERVE(MaxReservedSDL)

RESERVE(MaxReserved)


