/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "ShoreApp.h"
#include "sdl_internal.h"

REF(Type)
sdlEType::getElementType(void) const
{
	return elementType;
}

void
sdlEType::SetElementType(Ref<sdlType> et)
{
	elementType = et;
}

long
ArrayType::getLength(void) const
{
	if (dim)
		return dim;
	else if (dim_expr != 0)
		return dim_expr->fold();
	else
		return 0;
}

MemberID
AttrDecl::getMid(void)
{
	return myMid;
}

Zone
AttrDecl::getZone(void)
{
	return zone;
}

boolean
AttrDecl::isIndexable(void)
{
	return Indexable;
}

boolean
AttrDecl::isReadOnly(void)
{
	return readOnly;
}



Zone
ConstDecl::getZone(void)
{
	return zone;
}

CString
Declaration::getName(void)
{
	return name;
}

REF(Type)
Declaration::getTypeRef(void)
{	
	return type;
}

DeclKind
Declaration::getkind(void)
{
	return kind;
}

REF(Declaration)
EnumType::enumIterator(void)
{
	return consts;
}

REF(ParamDecl)
OpDecl::parametersIterator(void)
{
	return parameters;
}

Mode
ParamDecl::getMode(void)
{
	return mode;
}

void
ParamDecl::setMode(Mode mval)
{
	mode = mval;
}

	

REF(Declaration)
StructType::findMember(char * memname) const
{
	// not implemented but should be able to crib code.
	REF(Declaration) dpt;
	for (dpt=members; dpt!=0; dpt=dpt->next)
	{
		if (!strcmp(memname,(char *)dpt->name))
			return dpt;
	}
	return 0;
}

REF(Declaration)
StructType::memberIterator(void)
{
	return members;
}

// Type::print_obj(void) const
void
sdlAliasDecl::resolve_names(Ref<sdlModule>, Ref<sdlInterfaceType>) const
{
}
