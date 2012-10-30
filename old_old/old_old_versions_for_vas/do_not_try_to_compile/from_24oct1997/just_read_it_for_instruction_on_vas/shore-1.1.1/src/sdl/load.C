/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "metatypes.sdl.h"
// ditch paradise catalogs, load from sdl.
#ifndef STAND_ALONE
#include <catalog.h>

#include <types.h>
#include <assert.h>
#include <oql_context.h>
#include <string.h>

typedef unsigned long uint4;

extern Ref<sdlModule> lookup_module(const char * mname);

extern Type * create_Type(Ref<sdlType> stype);
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



Type * oqlDatabase::AddShoreType(Ref<sdlType> stype)
// create appropro tsi type for given sdl type.
{
	TypeType new_tag = sdl_to_odl(stype->tag);
	Type *tpt;
	Type *btype;
	int add_to_db = 0;
	new_tag = sdl_to_odl(stype->tag);
	switch(new_tag)
	{
	// check for named types which may be in db already.
		case ODL_Type: 
		case ODL_Typedef: // typedef needs some work; I'm not
		case ODL_Primitive: 
		case ODL_Void:
		case ODL_Struct:
		case ODL_Interface:
		case ODL_Container:
		if (stype->name.strlen()>0) // it has a name; try to look it up.
		{
			tpt = types.lookup(stype->name);
			if (tpt)	return tpt;
		}
		else
			add_to_db = 1;
	}
	new_tag = sdl_to_odl(stype->tag);

	// now, add iin the type.
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
		{

			StructType *new_s = new StructType(stype->name);
			tpt = new_s;
			// add to db right away to supress possible recursion.
			types.add(stype->name,new_s);
			add_to_db = 0;
			if (stype->tag==Sdl_struct)
			{
				Ref<sdlStructType> spt;
				spt.assign(stype);
				Ref<sdlDeclaration> mpt;
				for (mpt = spt->members; mpt !=0; mpt=mpt->next)
					new_s->add(mpt->name,AddShoreType(mpt->type));
			}
			else if (stype->tag==Sdl_union)
			{
				Ref<sdlUnionType> upt;
				upt.assign(stype);
				Ref<sdlDeclaration> apt;
				for ( apt = upt->ArmList; apt != NULL; apt =  apt->next)
					new_s->add(apt->name,AddShoreType(apt->type));
			}
					
		}

		break;
		case ODL_Interface:
		{
			Ref<sdlInterfaceType> apt;
			InterfaceType *ipt = new InterfaceType(stype->name);
			tpt = ipt;
			// add to db right away to supress possible recursion.
			types.add(stype->name,ipt);
			add_to_db = 0;
			apt.assign(stype);
			Ref<sdlDeclaration> dpt;
			for (dpt = apt->Bases; dpt != 0; dpt= dpt->next) {
				Type *baset = AddShoreType(dpt->type);
				// error check needed...
				if (baset->isObject())
					ipt->AddBaseClass(baset->isObject());
			}
			for (dpt = apt->Decls; dpt != 0; dpt= dpt->next) {
			// also need to do methods & whatever...
				if (dpt->kind==Attribute)
					ipt->add(dpt->name,AddShoreType(dpt->type));
			}
		}
		break;
		case ODL_Array:
		// we need to get the base type???
			btype = AddShoreType(((Ref<sdlEType>&)stype)->elementType);
			tpt = new ArrayType(btype); //
		break;
		case ODL_Ref:
			btype = AddShoreType(((Ref<sdlEType>&)stype)->elementType);
			tpt = new RefType(btype); //
		break;
		case ODL_Set:
			btype = AddShoreType(((Ref<sdlEType>&)stype)->elementType);
			tpt = new SetType(btype);
		break;
		case ODL_Bag:
			btype = AddShoreType(((Ref<sdlEType>&)stype)->elementType);
			tpt = new BagType(btype);
		break;
		case ODL_List:
			btype = AddShoreType(((Ref<sdlEType>&)stype)->elementType);
			tpt = new ListType(btype);
		break;
		case ODL_Container:
		break;
		default:
			tpt = new ErrorType;
			
	}
	if (add_to_db && tpt && tpt->me() != ODL_Error)
		types.add(stype->name,tpt);

	tpt->setRealType( stype);
	return tpt;
}


oql_rc_t oqlDatabase::LoadOtherTypes()
{
	if (_name[0]=='\"') // a path name; do a lookup & check if
	// it is a module, a different registered object, or a
	// directory.
	{
		shrc rc;
		// strip quotes
		_name++;
		_name[strlen(_name)-1]=0;
		Ref<any> nref;
		rc = Ref<any>::lookup(_name,nref);
		if(rc ) //  && rc.err_num() != SH_NotFound)
			rc.fatal();
		rType *my_type;
		rc = nref.get_type(my_type);
		if (rc)
			rc.fatal();
		// we could load types explicitly; let's see if
		// we can do it fault-based...
		if (my_type==TYPE_PT(sdlModule))
			return LoadSdlModule();
		
		Type * oql_type;
		oql_type = types.lookup(my_type->name);
		if (oql_type==0) // try to fault it in.
		{
			Ref<sdlType> tref;
			tref.assign(my_type->loid);
			oql_type = AddShoreType(tref);
		}

		if (oql_type) // insert the name in the extent database...
		{
			// note : we should extend this so that either
			// the full path or the oid is stored for the
			// object, for things that are not modules.
			oql_type = new RefType(oql_type);
			char * npt = strrchr(_name,'/');
			if (npt)
				extents.add(npt+1,oql_type);
			else
				extents.add(_name,oql_type);
		}
		return OQL_OK;

	}
	else // if an id, just add it,
		return LoadSdlModule();
}
		
// path in module specific load
oql_rc_t oqlDatabase::LoadSdlModule()
{
	// for shore, scan a module
#ifndef NO_SDL
	Ref<sdlModule> m;
	// the name can be 1: an sdl module
	// 2: a registered, sdl object
	// 3: a directory.
	// get a ref.
	m = lookup_module(_name);
	if (m==0)
	{
		errstream() << "couldn't find module" << _name;
	   return OQL_OK;
	}
	Ref<sdlDeclaration> bpt;
	for (bpt = m->decl_list; bpt != 0; bpt = bpt->next)
	// insert types in db.
	{
		Type *nt;
		if(bpt->kind==TypeName)
		{
			nt = AddShoreType(bpt->type);
			if (nt!=0)
			{
				if (nt->isObject()) {
					// Add to the list of extents
					extents.add(bpt->name, new SetType(nt));  
				}
			}
			else
				errstream()<< "create_Type faied for " 
					<< bpt->name.string() << "in module " << _name;
		}
	}

#endif
   // Assume that the _cat is open. Assume that the 
   // mutex has been taken
#ifdef NO_SDL
   AutoMutex myMutex(*(_cat->mutex));
   W_COERCE(myMutex.acquire());

   uint4 numExtents = _cat->dbCache.extentCnt;
   uint4 i;
   
   if (numExtents) assert(t_array = new Type*[numExtents]);

   // Now go thru each extent...
   for (i = 0; i < numExtents; i++)
      Do(_cat->extentCache[i], t_array[i]);

   // That's it...
   if (t_array) delete [] t_array;
#endif

   return OQL_OK;
}

#endif
