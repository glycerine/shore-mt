/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <ostream.h>
#include "metatypes.sdl.h"
#include <types.h>
#include <typedb.h>

void GenericDB::add(Type *t)
{
   if (t) add(t->name(), t);
}

void GenericDB::add(const char *name, Type *t)
{
   if (name && name[0])
      overwrite(name, (char *)t);	/* DBs have unique entries */
}

int  GenericDB::remove(const char* name)
{
   return (SymbolTable::remove(name) == -1)? 0: 1;
}

Type *GenericDB::lookup(const char *name)
{
   Type *t = (Type *)0;
   Symbol *s;
   s = SymbolTable::lookup(name);
   if (s)
      t = (Type *)(s->data());
   return t;
}

Type *GenericDB::force_lookup(const char *name)
{
   Type *t = lookup(name);
   if (!t) {
      cerr << form("XXX need type '%s', not found\n", name);
      t = TypeError;
   }
   return t;
}

/* 
   commit new types to the store.
   We happen to know all types are uncommited
   at this point in time, so ... 
   */
void	TypeDB::commit()
{
	SymbolTable_i it(*this);
   Symbol	*s;
   Type	*t;

	for (; s = *it; ++it) {
      t = (Type *) (s->data());
      t->commit();
#if 0
      cout << *t << " -> " << t->size() << ", " << t->inline_size() << endl;
#endif
   }
}


#include <sdl_lookup.h>


Type *TypeDB::lookup(const char *name)
{
   Type *t = (Type *)0;
   Symbol *s;
   s = SymbolTable::lookup(name);
   if (s)
      t = (Type *)(s->data());
   return t;
}

#ifdef oldcode
TypeType sdl_to_odl(TypeTag st)
{
	switch(st) {
	case NO_Type :
	case Sdl_any : return ODL_Any;
	case Sdl_char : return ODL_Primitive;
	case Sdl_short : return ODL_Primitive;
	case Sdl_long : return ODL_Primitive;
	case Sdl_float : return ODL_Primitive;
	case Sdl_double : return ODL_Primitive;
	case Sdl_boolean : return ODL_Primitive;
	case Sdl_octet : return ODL_Primitive;
	case Sdl_unsigned_short : return ODL_Primitive;
	case Sdl_unsigned_long : return ODL_Primitive;
	case Sdl_void :	return ODL_Void;
	case Sdl_pool :		return ODL_Primitive;
	case Sdl_enum :	return ODL_Primitive;
	case Sdl_struct :	return ODL_Struct;
	case Sdl_union :	return ODL_Struct; // no expicit union??
	case Sdl_interface :	return ODL_Interface;
	case Sdl_array :	return ODL_Array;
	case Sdl_string :	return ODL_Primitive;
	case Sdl_sequence :	return ODL_Array;
	case Sdl_text :	return ODL_Primitive;
	case Sdl_ref :	return ODL_Ref;
	case Sdl_lref :	return ODL_Ref;
	case Sdl_set :	return ODL_Set;
	case Sdl_bag :	return ODL_Bag;
	case Sdl_list :	return ODL_List;
	case Sdl_multilist :	return ODL_List;
	case Sdl_NamedType :	return ODL_Typedef;
	case Sdl_ExternType :	return ODL_Typedef;
	case Sdl_Index :	return ODL_List;
	case Sdl_Class :	return ODL_Typedef;
	case Sdl_CUnion: 	return ODL_Typedef;
	}
}



Type *
create_Type(Ref<sdlType> stype)
// create appropro tsi type for given sdl type.
{
	TypeType new_tag = sdl_to_odl(stype->tag);
	Type *tpt;
	Type *btype;
	switch(new_tag)
	{
	// used: ODL_Any; ODL_Primitive; ODL_Void; ODL_Struct; ODL_Union;
	// ODL_Interface; ODL_Array;  ODL_Ref; ODL_Set;
	// ODL_Bag; ODL_List; ODL_Typedef; ODL_Container;
		case ODL_Type: 
		// the following are dubious, misc. named types that need
		// to sit somewhere
		case ODL_Typedef: // typedef needs some work; I'm not
		// sure where it sits.
			tpt = new Type(stype->name);
		break;
		case ODL_Any:
		case ODL_Primitive: 
		case ODL_Void:
			tpt = new PrimitiveType(stype->name,stype->tag, stype->size);
		break;
		case ODL_Struct:
			tpt = new StructType(stype->name);
		break;
		case ODL_Interface:
			tpt = new InterfaceType(stype->name);
		break;
		case ODL_Array:
		// we need to get the base type???
			btype = create_Type(((Ref<sdlEType>&)stype)->elementType);
			tpt = new ArrayType(btype); //
		break;
		case ODL_Ref:
			btype = create_Type(((Ref<sdlEType>&)stype)->elementType);
			tpt = new RefType(btype); //
		break;
		case ODL_Set:
			btype = create_Type(((Ref<sdlEType>&)stype)->elementType);
			tpt = new SetType(btype);
		break;
		case ODL_Bag:
			btype = create_Type(((Ref<sdlEType>&)stype)->elementType);
			tpt = new BagType(btype);
		break;
		case ODL_List:
			btype = create_Type(((Ref<sdlEType>&)stype)->elementType);
			tpt = new ListType(btype);
		break;
		case ODL_Container:
		break;
		default:
			tpt = TypeError;
			
		}
		return tpt;
}

sdl_scope sdl_type_db;
#include <oql_context.h>
#endif
Type *TypeDB::force_lookup(const char *name)
{
	Type *t = lookup(name);
#ifdef oldcode
	if (!t) {
		Ref<sdlType> sdlt;
		sdlt = sdl_type_db.lookup(name);
		if (sdlt != 0) {
		// load it as appropo.
			t  = create_Type(sdlt);
			add(t);
		}
		else {
			t = oql_context()->db.types.lookup(name);
		}
	}
#endif
	if (t==0) {
		cerr << form("XXX need type '%s', not found\n", name);
		t = TypeError;
	}
	return t;
}

