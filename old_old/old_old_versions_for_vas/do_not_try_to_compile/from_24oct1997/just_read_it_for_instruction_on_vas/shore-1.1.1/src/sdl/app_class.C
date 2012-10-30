/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "metatypes.sdl.h"
#include "app_class.h"

void app_class::action(const sdlExprNode * arg)
{ // base class: nothing to do
	
}

void app_class::action(const sdlLitConst * arg)
{
	action((sdlExprNode *)arg);
}

void app_class::action(const sdlConstName * arg)
{
	action((sdlExprNode *)arg);
}

void app_class::action(const sdlArithOp * arg)
{
	action((sdlExprNode *)arg);
}

void app_class::action(const sdlSelectExpr * arg)
{
	action((sdlExprNode *)arg);
}

void app_class::action(const sdlProjectExpr * arg)
{
	action((sdlExprNode *)arg);
}
void app_class::action(const sdlFctExpr * arg)
{
	action((sdlExprNode *)arg);
}

	// declarations
void app_class::action(const sdlDeclaration * arg)
{ // base class
}

void app_class::action(const sdlAttrDecl * arg)
{
	action((sdlDeclaration *)arg);
}

void app_class::action(const sdlRelDecl * arg)
{
	action((sdlDeclaration *)arg);
}

void app_class::action(const sdlParamDecl * arg)
{
	action((sdlDeclaration *)arg);
}

void app_class::action(const sdlModDecl * arg)
{
	action((sdlDeclaration *)arg);
}

void app_class::action(const sdlOpDecl * arg)
{
	action((sdlDeclaration *)arg);
}

void app_class::action(const sdlConstDecl * arg)
{
	action((sdlDeclaration *)arg);
}
void app_class::action(const sdlAliasDecl * arg)
{
	action((sdlDeclaration *)arg);
}
void app_class::action(const sdlArmDecl * arg)
{
	action((sdlDeclaration *)arg);
}
void app_class::action(const sdlTypeDecl * arg)
{
	action((sdlDeclaration *)arg);
}
void app_class::action(const sdlExtTypeDecl * arg)
{
	action((sdlDeclaration *)arg);
}

// types
void app_class::action(const sdlType * arg)
{ // base class
}

void app_class::action(const sdlNamedType * arg)
{
	action((sdlType *)arg);
}

void app_class::action(const sdlStructType * arg)
{
	action((sdlType *)arg);
}

void app_class::action(const sdlEnumType * arg)
{
	action((sdlType *)arg);
}

void app_class::action(const sdlEType * arg)
{
	action((sdlType *)arg);
}

void app_class::action(const sdlArrayType * arg)
{
}

void app_class::action(const sdlSequenceType * arg)
{
}

void app_class::action(const sdlCollectionType * arg)
{
}

void app_class::action(const sdlRefType * arg)
{
}

void app_class::action(const sdlIndexType * arg)
{
}

void app_class::action(const sdlInterfaceType * arg)
{
	action((sdlType *)arg);
}

void app_class::action(const sdlUnionType * arg)
{
	action((sdlType *)arg);
}

void app_class::action(const sdlClassType * arg)
{
	action((sdlType *)arg);
}


// this class is to be used to traverse all the data elements
// in an (in-memory) data structure.  It knows to iterate
// over attributes in interface objects, members of structs,
// the appropriate union arms, and sequence & array elements.
// The iteration will stop at "primitive types", and will
// not traverse refs.  To use this class, minimally,
// the action function for the data type to which the
// action applies should be overriden, at the lowest level
// needed.  The only b



void traverse_data::action(const sdlStructType * arg)
{
	// traverse struct members
	char *orig_data_pt = data_pt;
	Ref<sdlDeclaration> dpt;
	for (dpt = arg->members; dpt != 0; dpt = dpt->next)
	{
		if(dpt->kind==Member) 
		{
			data_pt = orig_data_pt + dpt->offset;
			dpt->type->Apply(this);
		}
	}
	data_pt = orig_data_pt; // restore old value.
}


void traverse_data::action(const sdlArrayType * arg)
{
	char *orig_data_pt = data_pt;
	// traverse array elts
	caddr_t ar_pt = (caddr_t)data_pt;
	switch (arg->tag) {
	// for dubious reasons we've cheated and left strings as
	// an ArrayType.
	case Sdl_string:
	case Sdl_text:
		arg->elementType->Apply(this);
	break;
	case Sdl_array:
	for (int i=0; i<arg->dim; i++)
	{
		data_pt = ar_pt +(i*arg->elementType->size);
		arg->elementType->Apply(this);
	}
	break;
	}
	data_pt = orig_data_pt; // restore old value.
}

void traverse_data::action(const sdlSequenceType * arg)
{
	// traverse sequence elts
	char *orig_data_pt = data_pt;
	caddr_t ar_pt = (caddr_t)data_pt;
	switch(arg->tag) {
	case Sdl_string:
	case Sdl_text:
		arg->elementType->Apply(this);
	break;
	default:
	sdl_set * hpt = (sdl_set *)data_pt;
	// hpt->Apply(this); // heap management??
	// oops, unresolved at present -FIX
	for (int i=0; i<hpt->num_elements; i++)
	{
		data_pt = hpt->space +(i*arg->elementType->size);
		arg->elementType->Apply(this);
	}
	}
	data_pt = orig_data_pt; // restore old value.
}

void traverse_data::action(const sdlCollectionType * arg)
{
	// traverse collection elts?
}


void traverse_data::action(const sdlIndexType * arg)
{
}

void traverse_data::action(const sdlInterfaceType * arg)
{
	// traverse interface attributes.
}

void traverse_data::action(const sdlUnionType * arg)
{
	// traverse correct union arm?
}

// void traverse_data::action(const sdlClassType * arg) { }




template class Sequence< Ref<sdlConstDecl> >;
