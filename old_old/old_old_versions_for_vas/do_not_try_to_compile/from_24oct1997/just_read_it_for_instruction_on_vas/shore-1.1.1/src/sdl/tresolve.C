/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "sdl_internal.h"
#include "type_globals.h"
#include <string.h>
extern FILE *bf; // output for language binding.
// type name binding section: initially, type names are passed through
// as literals; the role of this section is to resolve type names to
// the actual type declarations they refer to.

// The only 2 scopes that may define type names are the Module and the
// Interface; since interfaces and modules don't nest, there are thus
// at most 2 scopes within which a name may be resolved.  The Module
// class and the InterfaceType class each has a routine to try to 
// get a real type from a typeName
int sdl_errors = 0;
int sdl_linking = 0;
extern char *DeclKind_objtype[];

Set< Ref<sdlDeclaration> > *depends_tmp =0;
extern Set<Ref<sdlType> > TemplatesUsed;

extern char * metatype_version;
char *src_version_string  = 0;


// context maintaining class
class sdl_depends * cur_depend;
class sdl_depends {
	Ref<sdlDeclaration> current; // declaration to add dependency to.
	sdl_depends * old_depend;
public:
	sdl_depends(Ref<sdlDeclaration> new_d)
	{
		old_depend = cur_depend;
		current = new_d;
		cur_depend = this;
	}
	~sdl_depends() { cur_depend = old_depend; }
	add_depend(Ref<sdlDeclaration> ddecl)
	{
		if (current != 0)
			current.update()->dependsOn.add(ddecl);
	}
};

void
add_dependency(Ref<sdlDeclaration> decl)
{
	if (cur_depend)
		cur_depend->add_depend(decl);
}

Ref<Type> 
InterfaceType::resolve_typename(CString typeName) const
{
	// first, check in direct declarations
	Ref<Declaration> dpt;
	for (dpt = Decls; dpt != 0; dpt = dpt->next)
	{
		if (dpt->kind == TypeName && !strcmp(typeName,(char *)dpt->name)) // names match
		{
			// add_dependency(dpt); // add dependency to whoever is looking it up.
			// oops, we don't do dependency analysis within interfaces yet??
			return dpt->type;
		}
		// note: this leaves out InterfaceName, which is a forward decl;
		// that seems to be ok, since we want this resolved to a real
		// type.  Another problem is that typedefs may not be resolved
		// this way.
	}

	// at this point, we couldn't resolve locally, so try the base classes.
	// this of course assumes the base names are problerly bound.
	for (dpt = Bases; dpt != 0; dpt = dpt->next)
	{
		Ref<Type>  rpt = 0;
		if (dpt->type->tag==Sdl_interface)
			rpt = ((Ref<InterfaceType> &)(dpt->type))->resolve_typename(typeName);
		if (rpt!= 0) return rpt;
	}

	// couldn't resolve, return null.

	return 0;

}

// Generalization of resolve_typename: resolve_name returns a declaration
// of a particular kind matching an input string, if it exists.
Ref<Declaration> 
InterfaceType::resolve_name(CString typeName , DeclKind kind) const
{
	// first, check in direct declarations
	Ref<Declaration> dpt;
	for (dpt = Decls; dpt != 0; dpt = dpt->next)
	{
		if (dpt->kind == kind && !strcmp(typeName,(char *)dpt->name)) // names match
			return dpt;
		// note: this leaves out InterfaceName, which is a forward decl;
		// that seems to be ok, since we want this resolved to a real
		// type.  Another problem is that typedefs may not be resolved
		// this way.
	}

	// at this point, we couldn't resolve locally, so try the base classes.
	// this of course assumes the base names are problerly bound.
	for (dpt = Bases; dpt != 0; dpt = dpt->next)
	{
		Ref<Declaration>  rpt = 0;
		Ref<InterfaceType> bpt;
		bpt = (Ref<InterfaceType> &)(dpt->type);
		if (bpt.type_ok()) // it may not be an intet
			rpt = bpt->resolve_name(typeName,kind);
		if (rpt != 0) return rpt;
	}

	// couldn't resolve, return null.

	return 0;

}


Ref<Declaration> 
InterfaceType::lookup_name(CString typeName , DeclKind kind) const
{
	return resolve_name(typeName,kind);
}

Ref<Declaration> 
sdlStructType::lookup_name(CString s , DeclKind kind) const
{
	Ref<Declaration> rpt;
	rpt = findMember(s);
	if (rpt!= 0 && rpt->kind==kind)
		return rpt;
	return 0;
}


Ref<Declaration>
sdlUnionType::lookup_name(CString s , DeclKind kind) const
{
	
	// first, check in direct declarations
	Ref<Declaration> dpt;
	for (dpt = (Ref<sdlDeclaration> &)ArmList; dpt != 0; dpt = dpt->next)
	{
		if (dpt->kind == kind && !strcmp(s,(char *)dpt->name)) // names match
			return dpt;
	}
	if (TagDecl->kind==kind && !TagDecl->name.strcmp(s))
		return TagDecl;
	return 0;
}

// the default type lookup routine returns 0.
Ref<sdlDeclaration>
sdlType::lookup_name(CString s , DeclKind kind) const
{
	return 0;
}

// the following decl. should not be returned from resolve_decl, below.
static Ref<Declaration> ignore_decl =  0;
// another lookup variant: get any name matching the string in this scope.
// of a particular kind matching an input string, if it exists.
Ref<Declaration> 
InterfaceType::resolve_decl(CString typeName) const
{
	// first, check in direct declarations
	Ref<Declaration> dpt;
	for (dpt = Decls; dpt != 0; dpt = dpt->next)
	{
		if (!dpt->name.strcmp(typeName) && dpt != ignore_decl) // names match
			return dpt;
		// note: this leaves out InterfaceName, which is a forward decl;
		// that seems to be ok, since we want this resolved to a real
		// type.  Another problem is that typedefs may not be resolved
		// this way.
	}

	// at this point, we couldn't resolve locally, so try the base classes.
	// this of course assumes the base names are properly bound.
	for (dpt = Bases; dpt != 0; dpt = dpt->next)
	{
		if (dpt->type->tag==Sdl_interface)
		// the base may not yet be an interface due to linking.
		{
			Ref<Declaration>  rpt;
			rpt = ((Ref<InterfaceType> &)(dpt->type))->resolve_decl(typeName);
			if (rpt != 0) return rpt;
		}
	}

	// couldn't resolve, return null.

	return 0;

}

Ref<Type> 
Module::resolve_typename(CString typeName) const
{
	// first, try to match the name within the direct declarations.
	Ref<Declaration> dpt;
	for (dpt = decl_list; dpt != 0; dpt = dpt->next)
	{
		if (dpt->kind == TypeName && !strcmp(typeName,(char *)dpt->name)) // names match
		{
			add_dependency(dpt);
			return dpt->type;
		}
		
	}

	// next, try to resolve among imported types.
	// this is currently not implemented.
	Ref<sdlModDecl> mpt;
	for (mpt= (Ref<sdlModDecl>&)(import_list); mpt != 0; 
		mpt= (Ref<sdlModDecl>&)(mpt->next))
	// go through the export list of each mod.
	{
		if (mpt->dmodule==0) continue;
		for (dpt = mpt->dmodule->export_list; dpt != 0; dpt = dpt->next)
		{
			Ref<sdlType> rpt = 0;
			if (dpt->kind==ExportAll)
			{
				rpt = mpt->dmodule->resolve_typename(typeName);
			}
			else if (dpt->kind==ExportName && !dpt->name.strcmp(typeName))
				rpt = dpt->type;
			if (rpt != 0)
				return rpt;


		}
	}
	return 0;
}

Ref<Declaration>
Module::resolve_name(CString typeName,DeclKind kind) const
{
	// first, try to match the name within the direct declarations.
	Ref<Declaration> dpt;
	for (dpt = decl_list; dpt != 0; dpt = dpt->next)
	{
		if (dpt->kind == kind && !strcmp(typeName,(char *)dpt->name)) // names match
			return dpt;
	}

	// next, try to resolve among imported types.
	// this is currently not implemented.
	// next, try to resolve among imported types.
	// this is currently not implemented.
	Ref<sdlModDecl> mpt;
	for (mpt= (Ref<sdlModDecl>&)(import_list); mpt != 0; 
		mpt= (Ref<sdlModDecl>&)(mpt->next))
	// go through the export list of each mod.
	{
		if (mpt->dmodule==0) continue;
		for (dpt = mpt->dmodule->export_list; dpt != 0; dpt = dpt->next)
		{
			Ref<sdlDeclaration> rpt = 0;
			if (dpt->kind==ExportAll)
			{
				rpt = mpt->dmodule->resolve_name(typeName,kind);
			}
			else if (dpt->kind==ExportName && !dpt->name.strcmp(typeName))
			{
				rpt = mpt->dmodule->resolve_name(typeName,kind);
			}
				
			if (rpt != 0)
				return rpt;


		}
	}

	return 0;
}


// the following are a set of methods used to replace any unbound (used
// but not defined) type names to the real type objects within declarations
// and types.  These routines may be called during either linking or
// initial compilation, and may be called repeatedly; they replace
// the former bind_namelist and CHeckTypeName.
void
sdlDeclaration::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{

	if (type==0 && (char *)name && mscope != 0) // for exportName ( maybe others)...
	{
		update()->type = mscope->resolve_typename(name);
	}
	else
	if (type != 0) // InterfaceName decl may
	// not have type; are there others?
	{
		Ref<sdlType> newt = type->resolve_names(mscope,iscope);
		if (newt != 0)
		// replace the type field
			update()->type = newt;
	}
	if (type != 0 && type->is_objtype())
	// many things cannot be an object type.
	{
		switch (kind) {
		case InterfaceName: 
		case TypeName:
		case Alias: // what is alias?
		case BaseType:
		case ExportName:
		case SuppressedBase:
			break;
		default:
			print_error_msg("interface type may not be used in this context");
		}
	}
}

void
sdlTypeDecl::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
// TypeDecl is specificly used for tag names; if the type
// node is specified, always follow it to resolve the attribute's
// types.
{
	if (type != 0)
	{
		sdl_depends temp(get_ref()); // set dependency context.
		type->resolve_fields(mscope,iscope);
		Ref<sdlType> newt = type->resolve_names(mscope,iscope);
		if (newt != 0)
		// replace the type field
			update()->type = newt;
	}
}

void
sdlArmDecl::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// do the case expr
	if (CaseList !=0)
	{
		CaseList->resolve_names(mscope,iscope);
		sdlDeclaration::resolve_names(mscope,iscope);
	}
}
void
sdlOpDecl::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	if (kind==Op) // original op declaration
	{
		sdlDeclaration::resolve_names(mscope,iscope);
		// now check parameters.
		Ref<sdlDeclaration> ppt;
		ppt.assign(parameters);
		for (parameters; ppt != 0; ppt = ppt->next)
			ppt->resolve_names(mscope,iscope);
	}
	else if (kind==OpOverride) // match this declaration with 
	// a base class decl.
	{
		Ref<OpDecl> bfct;
		bfct.assign(iscope->resolve_name((char *)(name), Op));
		if (bfct != 0) // set the types.
		{
		// new improved rehack
			update()->type = bfct->type;
			update()->isConst = bfct->isConst;
			update()->parameters = bfct->parameters;
		}
		else
		{
			sdl_errors++;
			print_error_msg("couldn't find corresponding method in base class to override");
		}
	}
}

void
sdlRelDecl::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// resolve type first.
	sdlDeclaration::resolve_names(mscope,iscope);
	if (type != 0 && inverseDecl != 0 &&
		inverseDecl->type==0)
	// there is an inverse; it is not yet "bound"; check
	// if we can bind it.
	{
		// we should make a type->is_ref or something.
		//switch ( bpt->type->tag)
		TypeTag tptag = type->tag;
		switch(tptag) {
		case Sdl_ref: case Sdl_set: case Sdl_bag:
		{
			Ref<InterfaceType> ipt;
			Ref<Declaration> tmp;

			// ipt is the type of the other end
			// of the relationship
			Ref<RelDecl> inv_decl;
			// look up the invers field in the interface at the other
			// end of the relation.

			Ref<sdlRefType> inv_ref;
			inv_ref.assign(this->type);
			if (tptag != Sdl_ref && inv_ref->elementType->tag==Sdl_ref) // then it is Sdl_set or Sdl_bag; go another
			// level.
				inv_ref.assign(inv_ref->elementType);
			//if (((Ref<RefType> &)type)->elementType->tag == Sdl_interface)
			if (inv_ref->elementType->tag == Sdl_interface)
				ipt.assign(inv_ref->elementType);
			else
				return; // ref type is not yet bound properly.
				// but should give some linkage error msg???

			inv_decl.assign(ipt->resolve_name(inverseDecl->name,Relationship));
			if (inv_decl != 0)
			// check if everything is consistent
			{
				if (inv_decl->inverseDecl != 0) // ok, 
				{
					DeclKind ikind =inv_decl->inverseDecl->kind;
					switch(ikind){
					case Relationship: // already bound
						if (inv_decl->inverseDecl != this) // not symmetric
						{
							print_error_msg("inverse declaration doesn't match");
						}
						else
							update()->inverseDecl = (Ref<Declaration> &)inv_decl; // complete the ling
					break;
					case UnboundRelationship: // other side not yet processed.
							update()->inverseDecl = (Ref<Declaration> &)inv_decl;
					}
				}
			}
			else
			{
				print_error_msg("couldn't find inverse attribute");
			}
		}			
		break;
		default:
			print_error_msg("unexpected relationship type");
		}
	  }
}

Ref<sdlType>
sdlType::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// this should ?? be a primitive type with no names to bind.
	return 0;
}


Ref<sdlType>
sdlNamedType::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	Ref<sdlType> rtp;
	rtp = 0;
	if (scope != 0)
	// this name had a scope qualifier.  Look up the scope
	// name within the module first.
	{
		if (sdl_linking)
			mscope->resolve_scope(scope->name,mscope,iscope);
		else // nothing to do here; resolve on later pass
		// hmm, perhaps should do local scoping if we can.
			return 0;
	}
	if (iscope != 0)
	{
		rtp = iscope->resolve_typename(name);
	}
	if (mscope != 0 && rtp == 0) // !rtp imples either no iscope or wasn't resolved ther.
		rtp = mscope->resolve_typename(name);

	if (rtp != 0)
	{
		update()->real_type = rtp;
		return rtp;
		// should delete the original type, perhaps.
	}
	else
	// we couldn't resolve this; when we implement binding modules together,
	// we should build an external undefined list or something, but for
	// now just print an error message.
	{
		if (!sdl_linking)
		{
			// keep track of unresolved names in the current module
			if (mscope != 0)
				rtp = mscope->add_unresolved(this);
			return rtp;
		}
		char buff[300];
		char *m1 = "couldn't find definition ";
		if (iscope!=0)
			sprintf(buff,"%swithin interface %s or module %s\n",
				m1,(char *)iscope->name, (char *)mscope->name);
		else if (mscope != 0)
			sprintf(buff,"%swithin module %s\n",m1, (char *)mscope->name);
		else 
			sprintf(buff,"%swithin unresolved scope\n",m1);
		print_error_msg(buff);
	}
	return 0;
}

void
sdlType::resolve_fields (Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
// nothing to do by default.
}

void
sdlStructType::resolve_fields(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// bind the list of attribute names.
	Ref<sdlDeclaration> dpt;
	for (dpt = members; dpt!=0; dpt=dpt->next)
	{
		if (dpt->scope==0)
			dpt.update()->scope=this;
		dpt->resolve_names(mscope,iscope);
	}
}

void
sdlUnionType::resolve_fields(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// bind the list of arm decl names.
	TagDecl->resolve_names(mscope,iscope);
	sdl_depends temp(0); // set dependency context -don't consider the
	// don't consider union arm types in dependencies.
	Ref<sdlDeclaration> dpt;
	for (dpt = (Ref<sdlDeclaration> &)ArmList; dpt!=0; dpt=dpt->next)
	{
		if (dpt->scope==0)
			dpt.update()->scope=this;
		dpt->resolve_names(mscope,iscope);
	}
	// should check for dupl. names...
}

void
sdlInterfaceType::resolve_fields(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// bind the list of attribute names.
	Ref<sdlDeclaration> dpt;
	// first, set the module backptr if not already set.
	if (myMod == 0)
		update()->myMod = mscope;
	for (dpt = Bases; dpt!=0; dpt=dpt->next)
	//  don't pass through iscope; interface base must be resolved
	// at module level. 
	{
		if (dpt->scope==0)
			dpt.update()->scope = this;
		dpt->resolve_names(mscope,0);
	}
	for (dpt = Decls; dpt!=0; dpt=dpt->next)
	// resolve names of decls in scope of this interface.
	{
		ignore_decl = dpt;
		if (dpt->scope==0) 
			dpt.update()->scope = this;
		dpt->resolve_names(mscope,this);
		// check for multiple declarations
		Ref<Declaration> dpt2 = resolve_decl(dpt->name);
		if (dpt2 != 0 && dpt2 != dpt) // decl should always resolve to itself
		// but because of ignore_decl, it shouldn't.
		{
			if ((dpt2->kind ==Op|| dpt2->kind ==OpOverride) 
				&& dpt->kind == OpOverride)
			// OpOverride should have same name as some Op.
				continue;
			if ((dpt2->kind == Op) &&  (dpt->kind==Op))
			{
				// overloading allowed with warning.
				fprintf(stderr,"warning: overloading operator %s in interface %s\n",
				(char *)dpt->name, (char *)name);
				fprintf(stderr,"operation overloading may not be supported in future releases\n");
			}
			else if (dpt->kind == Attribute && dpt2->kind == Attribute)
			{
				print_error_msg("multiple declarations for attribute in interface");
				dpt2->print_error_msg("was previous declaration");
			}
		}
	}
	ignore_decl = 0;

}

Ref<sdlType>
sdlArrayType::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	class sdl_depends * sav_depend = cur_depend;
	// if (tag==Sdl_sequence)
	// don't consider sequence subtype as a dependency ???
	//	cur_depend = 0;
	// we may want to reinstate this in the future, but for
	// now, keep the dependency.

	Ref<sdlType> new_elt = elementType->resolve_names(mscope,iscope);
	if (new_elt!=0)
	{
		// this only works if no size on sequence; otherwise, we'll
		// have to do better-- this will loose the size...
		if (tag==Sdl_sequence)
			return new_elt->get_dtype(tag);
		// should garbage collect here...
		// eg. thisref().destroy()
		else // for arrays
			update()->elementType = new_elt;
	}
	if (dim_expr != 0)
	{
		dim_expr->resolve_names(mscope,iscope);
		if (!dim)
		{
			long newdim = dim_expr->fold();
			if (newdim)
				update()->dim = newdim;
		}
	}
	// cur_depend = sav_depend;
	return 0;
}
	
Ref<sdlType>
sdlCollectionType::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// we should check that the name is really an interface??
	// also, we may want to store a cannonical ref/set/etc. in
	// the interface and get rid of extra type nodes.
	// this should ?? be a primitive type with no names to bind.
	Ref<sdlType> new_elt = elementType->resolve_names(mscope,iscope);
	if (new_elt != 0)
		return new_elt->get_dtype(tag);
		// update()->elementType = new_elt;
	return 0;
}

Ref<sdlType>
sdlRefType::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// we should check that the name is really an interface??
	// also, we may want to store a cannonical ref/set/etc. in
	// the interface and get rid of extra type nodes.
	// this should ?? be a primitive type with no names to bind.

	sdl_depends temp(0); // set dependency context -don't consider the
	//ref element type a dependency.
	Ref<sdlType> new_elt = elementType->resolve_names(mscope,iscope);
	if (new_elt!= 0)
		return new_elt->get_dtype(tag);
	else
		return 0;
}


Ref<sdlType>
sdlIndexType::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// we should check that the name is really an interface??
	// also, we may want to store a cannonical ref/set/etc. in
	// the interface and get rid of extra type nodes.
	// this should ?? be a primitive type with no names to bind.

	Ref<sdlType> new_key = keyType->resolve_names(mscope,iscope);
	if (new_key != 0 )
	{
		if (new_key->is_objtype()) // convert to ref??
			new_key = new_key->get_dtype(Sdl_ref);
		update()->keyType = new_key;
	}

	Ref<sdlType> new_value = elementType->resolve_names(mscope,iscope);
	if (new_value!= 0)
	{
		if (new_value->is_objtype()) // convert to ref??
			new_value = new_value->get_dtype(Sdl_ref);
		update()->elementType = new_value;
	}
	return 0;
}

// a bunch of functions used in computing consts.

// reduce possible const types to double, long, unsigned, error.
TypeTag
reduce_type1(TypeTag val)
{
	switch (val) {
	// float types.
	case Sdl_double:
	case Sdl_float:
		return Sdl_double;
	// signed integer types
	case Sdl_char:
	case Sdl_short:
	case Sdl_long:
		return Sdl_long;
	// unsigned types
	case Sdl_boolean:
	case Sdl_octet:
	case Sdl_unsigned_short:
	case Sdl_unsigned_long:
		return Sdl_unsigned_long;
	default:
		return NO_Type;
	}
}

TypeTag
reduce_type2(TypeTag val1,TypeTag val2)
{
	TypeTag r1 = reduce_type1(val1);
	TypeTag r2 = reduce_type1(val2);
	if (r1==NO_Type||r1==NO_Type)
		return NO_Type;
	else if (r1==Sdl_double||r2==Sdl_double)
		return Sdl_double;
	else if (r1==Sdl_unsigned_long||r2==Sdl_unsigned_long)
		return Sdl_unsigned_long;
	// by construction of reduce_type1, only Sdl_long can be left.
	return Sdl_long;
}

// convert whatever the type of the value is to double.
// note that get_unsigned and get_long below are exactly the
// same except for return values. 
double
get_double_val(Ref<sdlExprNode> vu)
{
	if (vu==0) return 0;
	switch(reduce_type1(vu->tvalue.get_tagval())) {
		case Sdl_long:
			return vu->tvalue.get_long_val();
		case Sdl_double:
			return vu->tvalue.get_double_val();
		case Sdl_unsigned_long:
			return vu->tvalue.get_ulong_val();
		default: // actually NO_Type
			return 0;
	}
}
			
long
get_long_val(Ref<sdlExprNode> vu)
{
	if (vu==0) return 0;
	switch(reduce_type1(vu->tvalue.get_tagval())) {
		case Sdl_long:
			return vu->tvalue.get_long_val();
		case Sdl_double:
			return (long)vu->tvalue.get_double_val();
		case Sdl_unsigned_long:
			return vu->tvalue.get_ulong_val();
		default: // actually NO_Type
			return 0;
	}
}
			
unsigned long
get_unsigned_long_val(Ref<sdlExprNode>  vu)
{
	if (vu==0) return 0;
	switch(reduce_type1(vu->tvalue.get_tagval())) {
		case Sdl_long:
			return vu->tvalue.get_long_val();
		case Sdl_double:
			return (unsigned long)(vu->tvalue.get_double_val());
		case Sdl_unsigned_long:
			return vu->tvalue.get_ulong_val();
		default: // actually NO_Type by construction.
			return 0;
	}
}



long
compute_long(sdlArithOp *exp)
{

	long v1 = get_long_val(exp->e1);
	long v2 = get_long_val(exp->e2);
	long result;
	switch (exp->etag) {
	case  Plus:
		result = v1 +v2;
	break;
	case Minus:
		result = v1 - v2;
	break;
	case Mult:
		result = v1 * v2;
	break;
	case Div:
		if (v2==0)
		{
			exp->print_error_msg("divide by zero");
			exp->type =0;
			return 0;
		}
		result = v1 / v2;
	break;
	case ModA:
		result = v1 % v2;
	break;
	// we do the following bitwize, should be type based for boolean.
	case Or:
		result = v1 | v2;
	break;
	case And:
		result = v1 & v2;
	break;
	case Xor:
		result = v1 ^ v2;
	break;
	case Complement:
		result = ~v2;
	break;
	case LShift:
		result = v1 << v2;
	break;
	case RShift:
		result = v1 >> v2;
	break;
	}
	exp->type = LongIntegerTypeRef;
	exp->tvalue.set_tagval(Sdl_long);
	exp->tvalue.set_long_val() = result;
	return result;
}

unsigned long
compute_unsigned_long(sdlArithOp *exp)
{

	unsigned long v1 = get_unsigned_long_val(exp->e1);
	unsigned long v2 = get_unsigned_long_val(exp->e2);
	unsigned long result;
	switch (exp->etag) {
	case  Plus:
		result = v1 +v2;
	break;
	case Minus:
		result = v1 - v2;
	break;
	case Mult:
		result = v1 * v2;
	break;
	case Div:
		if (v2==0)
		{
			exp->print_error_msg("divide by zero");
			exp->type =0;
			return 0;
		}
		result = v1 / v2;
	break;
	case ModA:
		result = v1 % v2;
	break;
	// we do the following bitwize, should be type based for boolean.
	case Or:
		result = v1 | v2;
	break;
	case And:
		result = v1 & v2;
	break;
	case Xor:
		result = v1 ^ v2;
	break;
	case Complement:
		result = ~v2;
	break;
	case LShift:
		result = v1 << v2;
	break;
	case RShift:
		result = v1 >> v2;
	break;
	}
	exp->type = UnsignedLongIntegerTypeRef;
	exp->tvalue.set_tagval(Sdl_unsigned_long);
	exp->tvalue.set_ulong_val() = result;
	return result;
}

double
compute_double(sdlArithOp *exp)
{

	double v1 = get_double_val(exp->e1);
	double v2 = get_double_val(exp->e2);
	double result;
	switch (exp->etag) {
	case  Plus:
		result = v1 +v2;
	break;
	case Minus:
		result = v1 - v2;
	break;
	case Mult:
		result = v1 * v2;
	break;
	case Div:
		if (v2==0.0)
		{
			exp->print_error_msg("divide by zero");
			exp->type =0;
			return 0;
		}
		result = v1 / v2;
	break;
	//case ModA:
	//	result = v1 % v2;
	break;
	// mod of floats not allowed by C++ ...
	default:
		result = 0;
	break;
		// exp->print_error_msg("invalid floating point op");

	}
	exp->type = DoublePrecisionTypeRef;
	exp->tvalue.set_tagval(Sdl_double);
	exp->tvalue.set_double_val() = result;
	return result;
}

			
void
compute_value( sdlArithOp *exp)
{
	if (exp->type != 0) return; // if type already set, computation
	// need not be done.
	TypeTag rtag;
	if (exp->e1==0)
	{
		if (exp->e2->type!= 0)
			rtag = reduce_type1(exp->e2->type->tag);
		else
			return; // can't continue.
	}
	else // assumes both e1 & e2 set.
	{
		if ((exp->e1->type != 0 ) && (exp->e2->type!= 0))
			rtag = reduce_type2(exp->e1->type->tag,exp->e2->type->tag);
		else
			return;
	}
	switch(rtag) {
		case Sdl_double:
			compute_double(exp);
			break;
		case Sdl_long:
			compute_long(exp);
			break;
		case Sdl_unsigned_long:
			compute_unsigned_long(exp);
			break;
		default:
			exp->print_error_msg("invalid operation");
			exp->type = 0;
			;
	}
}	

void
sdlConstDecl::resolve_names(Ref<sdlModule> mscope, 
							Ref<sdlInterfaceType> iscope) const
{
	// do the case expr
	sdl_depends temp(get_ref()); // set dependency context.
	expr->resolve_names(mscope,iscope);
	sdlDeclaration::resolve_names(mscope,iscope);
	// now compare the type of the expression with the declaration.
	// this is a quick hack; a more comprehensive type compatibility
	// test is needed.
	if (type == 0 || type->size== 0 || expr->type==0 || expr->type->size==0)
	// some other error previously detected.
		return;
	TypeTag t1tag = reduce_type1(type->tag);
	TypeTag t2tag = reduce_type1(expr->type->tag);
	if (t1tag==NO_Type) // only non-numerical const type is string
	// but equal tags should be ok.
	{
		if (type->tag== expr->type->tag)
			return;
		print_error_msg("invalid type for constant");
	}
	else if (t2tag==Sdl_double && t1tag !=Sdl_double)
	{
		print_error_msg("floating point value assigned to integral constant");
	}
	// otherwise , the type of the expression is integral, and the declared
	// type is either float or integegral, so things are ~ok; should check
	// for size limits on char, short though...
}



void
ExprNode::resolve_names(Ref<Module> mscope, Ref<InterfaceType> iscope) const
// mainly, resolve any named constants to the corresponding constant
// decl.
{
#ifdef orig
	if (!this) return;
	switch (etag) {
	case EError: return ;
	case  CName:
	// need to look up the name to get its value...
	{
		if (dpt== 0)
		{
			Ref<sdlConstDecl> new_dpt = 0;
			if (e1 != 0) // was a scoped name...
			{
				if (sdl_linking)
					mscope->resolve_scope(e1->imm_value,mscope,iscope);
				else // nothing to do here; resolve on later pass
					return;
			}
			if (iscope!= 0)
				new_dpt.assign(  iscope->resolve_name(imm_value,Constant));
			if (new_dpt== 0 && mscope != 0)
				new_dpt.assign(  mscope->resolve_name(imm_value,Constant));
			if (new_dpt != 0)
			{
				add_dependency(new_dpt);
				update()->dpt = new_dpt;
				update()->type = new_dpt->type;
				update()->tvalue = new_dpt->expr->tvalue;
			}
		}
	}
	break;

	case Literal:
	{
		// set the value union appropriately.
		if (type!=0 && tvalue.get_tagval() == NO_Type)
		{	
			sdlExprNode * upt = update(); // get writeable pointer.
			TypeTag ttag = type->tag;
			upt->tvalue.set_tagval( ttag);

			switch(ttag)
			{
				case Sdl_char:
					upt->tvalue.set_long_val() = imm_value.string()[0];
					break;
				case Sdl_short: case Sdl_long:
					upt->tvalue.set_long_val() = atol(imm_value.string());
					break;
				case Sdl_boolean:
					if (imm_value.string()[0]=='T'
						|| imm_value.string()[0]=='t')
						upt->tvalue.set_ulong_val() = 1;
					else
						upt->tvalue.set_ulong_val() = 0;
					break;
				case Sdl_float: case Sdl_double:
					upt->tvalue.set_double_val() = atof(imm_value.string());
					break;
				case Sdl_string:
					// upt->tvalue.set_str_val() = imm_value; 
					// o
					// duplicate information, but leave it here for
					// consistency.
					// oops, imm_value has quotes still...
					upt->tvalue.set_str_val()->set(imm_value.string()+1,imm_value.strlen()-2);
					break;
			}
		}
	}
	break;
	case CDefault:
		return; // nothing to do
	case  Plus:
	case Minus:
	case Mult:
	case Div:
	case ModA:
	case Or:
	case And:
	case Xor:
	case Complement:
	case LShift:
	case RShift:
	{
		if (type==0 ) // untyped up to here-> compute value.
		{
			if (e1!= 0) { e1->resolve_names(mscope,iscope); }
			if (e2!= 0) { e2->resolve_names(mscope,iscope); }
			compute_value(update());
		}
	}
	}
	return ;
#endif
}
	
void
sdlArithOp::resolve_names(Ref<Module> mscope, Ref<InterfaceType> iscope) const
// mainly, resolve any named constants to the corresponding constant
// decl.
{
	if (!this) return;
	if (type==0 ) // untyped up to here-> compute value.
	{
		if (e1!= 0) { e1->resolve_names(mscope,iscope); }
		if (e2!= 0) { e2->resolve_names(mscope,iscope); }
		compute_value(update());
	}
	return ;
}
	
void
sdlSelectExpr::resolve_names(Ref<Module> mscope, Ref<InterfaceType> iscope) const
// mainly, resolve any named constants to the corresponding constant
// decl.
{
	if (!this) return;
	if (type==0 ) // untyped up to here-> compute value.
	{
		if (RangeList!= 0) { RangeList->resolve_names(mscope,iscope); }
		{
			Ref<sdlDeclaration> dl = ProjList;
			for (; dl !=0; dl= dl->next)
				dl->resolve_names(mscope,iscope);
		}
		if (ProjList != 0) { 
			Ref<sdlDeclaration> dl = ProjList;
			for (; dl !=0; dl= dl->next)
				dl->resolve_names(mscope,iscope);
		}
		if (Predicate!= 0) { Predicate->resolve_names(mscope,iscope); }
	}
	return ;
}
	
void
sdlLitConst::resolve_names(Ref<Module> mscope, Ref<InterfaceType> iscope) const
// mainly, resolve any named constants to the corresponding constant
// decl.
{
	if (!this) return;
	// set the value union appropriately.
	if (type!=0 && tvalue.get_tagval() == NO_Type)
	{	
		sdlExprNode * upt = update(); // get writeable pointer.
		TypeTag ttag = type->tag;
		upt->tvalue.set_tagval( ttag);

		switch(ttag)
		{
			case Sdl_char:
				upt->tvalue.set_long_val() = imm_value.string()[0];
				break;
			case Sdl_short: case Sdl_long:
				upt->tvalue.set_long_val() = atol(imm_value.string());
				break;
			case Sdl_boolean:
				if (imm_value.string()[0]=='T'
					|| imm_value.string()[0]=='t')
					upt->tvalue.set_ulong_val() = 1;
				else
					upt->tvalue.set_ulong_val() = 0;
				break;
			case Sdl_float: case Sdl_double:
				upt->tvalue.set_double_val() = atof(imm_value.string());
				break;
			case Sdl_string:
				upt->tvalue.set_str_val().set(imm_value.string()+1,
					0,
					imm_value.strlen()-2);
				// upt->tvalue.set_str_val() = imm_value; 
				// duplicate information, but leave it here for
				// consistency.
				break;
		}
	}
}
	
void
sdlConstName::resolve_names(Ref<Module> mscope, Ref<InterfaceType> iscope) const
// mainly, resolve any named constants to the corresponding constant
// decl.
{
	if (!this) return;
	// need to look up the name to get its value...
	if (dpt== 0)
	{
		Ref<sdlConstDecl> new_dpt = 0;
		if (scope != 0) // was a scoped name...
		{
			// I'm unclear about this...
			if (sdl_linking)
				mscope->resolve_scope(scope->name,mscope,iscope);
			else // nothing to do here; resolve on later pass
				return;
		}
		if (iscope!= 0)
			new_dpt.assign(  iscope->resolve_name(name,Constant));
		if (new_dpt== 0 && mscope != 0)
			new_dpt.assign(  mscope->resolve_name(name,Constant));
		if (new_dpt != 0)
		{
			add_dependency(new_dpt);
			update()->dpt = new_dpt;
			update()->type = new_dpt->type;
			update()->tvalue = new_dpt->expr->tvalue;
		}
	}
}
	
void
sdlFctExpr::resolve_names(Ref<Module> mscope, Ref<InterfaceType> iscope) const
// just do the body...
{
	body->resolve_names(mscope,iscope);
}

void sdlModule::resolve_types()
// look up all type names used in the definition of a module, and bind
// them to their real type.  This is less general that it could be;
// we cheat
{
	// bind import list
	Ref<sdlModDecl> mpt;
	if (sdl_linking) // don't try to resolve modules unless we are linking.
	{
		for (mpt= (Ref<sdlModDecl>&)(import_list); mpt != 0; 
			mpt= (Ref<sdlModDecl>&)(mpt->next))
		{
			if (mpt->scope==0) 
				mpt.update()->scope = this;
			if (mpt->dmodule == 0)
			// try to look up a matching module
				mpt.update()->dmodule = lookup_module(mpt->name);
			  
			else if (mpt->dmodule->decl_list == 0)	
			// an alias was supplied; look it up under
			// the original name.
			{
				Ref<sdlModule> nm;
				nm = lookup_module(mpt->dmodule->name);
				if (nm != 0)
					mpt.update()->dmodule = nm;
			}
		}
	}
		
	//bind_namelist(export_list,this,0);
	//bind_namelist(decl_list,this,0);
	int old_sdl_link = sdl_linking;
	if (import_list == 0) // set sdl_linking whether or not this is
	// a linking path.
	{
		sdl_linking = 1;
	}

	Ref<sdlDeclaration> bpt;
	// bind 
	for (bpt = export_list; bpt != 0; bpt = bpt->next)
	{
		if (bpt->scope==0) 
			bpt.update()->scope = this;
		bpt->resolve_names(this,0);
	}
	for (bpt = decl_list; bpt != 0; bpt = bpt->next)
	{
		if (bpt->scope==0) 
			bpt.update()->scope = this;
		bpt->resolve_names(this,0);
	}

	for (bpt = decl_list; bpt != 0; bpt = bpt->next)
	{
		if (bpt->scope==0)
			bpt.update()->scope = this;
		if (bpt->kind==ExternType) continue;
		if (bpt->type != 0 && bpt->type->size == 0)
		{
			if (bpt->type->tag==Sdl_struct&& 
				((Ref<sdlStructType> &)(bpt->type))->members==0)
				continue;
			bpt->type.update()->compute_size();
			if (bpt->type->size==0 && sdl_linking)
			{
				bpt->print_error_msg("unresolved reference in type; size not known");
			}
		}
	}
	sdl_linking = old_sdl_link;
	compute_dependencies();
}

int long_oids  =  0; // special case flag, set in main.C
void 
Type::compute_size()
{
	// default size computation, for primitive/fixed size types.
	// note: the handling of size & particularly alignment 
	// is really quite incomplete;  needs to be
	// redone using perhaps cfront/g++ as a model.
	if (size) // computation already done
		return;
	switch(tag)
	{
		// the first set is really fixed.
		case Sdl_char:
			alignment = size = sizeof(char);
		break;
		case Sdl_short:
			alignment = size = sizeof(short);
		break;
		case Sdl_long:
			alignment = size = sizeof(long);
		break;
		case Sdl_float:
			alignment = size = sizeof(float);
		break;
		case Sdl_double:
			alignment = size = sizeof(double);
		break;
		case Sdl_boolean:
			alignment = size = sizeof(boolean);
		break;
		case Sdl_octet:
			alignment = size = sizeof(char);
		break;
		case Sdl_unsigned_short:
			alignment = size = sizeof(short);
		break;
		case Sdl_unsigned_long:
			alignment = size = sizeof(long);
		break;
		case Sdl_enum:
		// this one is dubious:
			alignment = size = sizeof(long);
		break;
		case Sdl_string:
			size = sizeof(sdl_string);
			alignment = sizeof(long);
		break;
		case Sdl_text:
			size = sizeof(sdl_text);
			alignment = sizeof(long);
		break;
		case Sdl_ref:
			if (long_oids) // special "cross complilation" flag for
			// janet's code to do  8 byte ref attributes.
				size = 8;
			else // normal case
				size = sizeof(OCRef);
			alignment = sizeof(long);
		break;
		case Sdl_lref:
			alignment = size = sizeof(void *);
		break;
		// make all thes the same temporarily.
		case Sdl_list:
		case Sdl_multilist: 
		// not clear what these mean; we translate the tag to
		// sequence for the moment.
		// this may hose us due to the sep. sequence type though...
			tag = Sdl_sequence;
		case Sdl_set:
		case Sdl_bag: 
		case Sdl_sequence:
			size = sizeof(sdl_set);
			alignment = sizeof(long);
		break;
		case Sdl_Index:
			size = sizeof(sdl_index_base);
			alignment = sizeof(long);
		break;
		case Sdl_NamedType:
			if (!sdl_linking) // set size to 0, all the way up.
			{
				size = 0;
				alignment = 1; // duh;
			}
			break;
		case Sdl_ExternType: // set size to 1, must check elsewhere
		case Sdl_pool:
		// for use in attr decls.
			size = 1;
			alignment = 1;
			break;

		default:
			print_error_msg("type size not known");
	}
}

#ifndef roundup
// roundup may or may not have been defined, according to include dependencies.
#define         roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

int
inc_offset(int & current_size, int & current_align,int size, int align)
// increment a cumulative offset according to some size and alignment
// input
{
		
	// figure out an offset for the attribute in the structure.
	// we need to get the size of the type, and the alignment;
	// set the offset to the appropriate alinged offset
	// and bump up the current size of the container.
	int new_off = current_size;
	// round up to next alignment boundary
	if ((align != 0) && (new_off % align))
		new_off +=  align - (new_off % align);
	// e.g. new_off % align should now == 0.
	current_size = new_off + size;
	if (align>current_align)
		current_align = align;
	return new_off;
}

void
compute_offset(int & current_size, int & current_align, Ref<Declaration> attr)
{
		
	// figure out an offset for the attribute in the structure.
	// we need to get the size of the type, and the alignment;
	// set the offset to the appropriate alinged offset
	// and bump up the current size of the container.
	if (attr->type->size==0)
		attr->type.update()->compute_size();
	// if we can't compute the size now, give up and force though
	// a zero.
	if (attr->type->size==0)
	{
		current_size = 0;
		return;
	}
		

	int new_off = 
	inc_offset(current_size, 
		current_align, 
		attr->type->size, 
		attr->type->alignment);
	attr.update()->offset = new_off;
}

void
sdlStructType::compute_size()
{
	if (size != 0) return;
	int current_size = 0;
	int current_align = 0;
	Ref<Declaration> mempt;
	if (members==0) // forward decl...
	{
		size = 1;
		return;
	}
	for (mempt = members; mempt  != 0; mempt = mempt->next)
	{
		compute_offset(current_size,current_align,mempt);
		if (current_size==0) // size computation failed, so leave size 0.
		{
			size = 0;
			return;
		}
	}
	// round up current size
	if (current_size>0)
		size = roundup(current_size,current_align);
	alignment = current_align;
}

void 
sdlClassType::compute_size()
{
	size = 1; // forward dcl hack.
	alignment = 1;
}

Ref<sdlInterfaceType>
find_firstbase_usage(Ref<sdlInterfaceType> sub, Ref<sdlInterfaceType> super)
// find the first use of super as a superclass of sub, and return
// the class which directly declare a  sub as class
// recursively check set of base classes; used to check for duplicates.
// return ref to duplicate class.  Don't try to detect more that one
// for now.
// in c++ terminoly, superclass = base class, subclass = derived class.
// this may only be called after all bases are linked correctly.
{
	Ref<Declaration> mempt;
	for (mempt= sub->Bases; mempt!= 0; mempt = mempt->next)
	{
		Ref<sdlInterfaceType> test_base;  // base clas
		Ref<sdlInterfaceType> bref;
		test_base.assign(mempt->type);
		if (test_base==0 || !test_base.type_ok()) return 0;
		if (test_base==super) // found it
			return sub;
		else if ((bref=find_firstbase_usage(test_base,super))!= 0)
			return bref;
	}
	return 0; // but this shouldn't happen.
}

	
template class Bag<Ref<sdlInterfaceType> >;
void
check_bases(Ref<sdlInterfaceType> oref,
		Ref<sdlInterfaceType> iref,
		Bag<Ref<sdlInterfaceType> > & bbag, 
		Set<Ref<sdlInterfaceType> > & dupset // return set of duplicates.
		) 
{
	if (iref==0)
		iref = oref;
	Ref<Declaration> mempt;
	for (mempt= iref->Bases; mempt!= 0; mempt = mempt->next)
	{
		Ref<sdlInterfaceType> bref;
		bref.assign(mempt->type);
		if (bbag.member(bref)) // duplicate detected.
		{

			// first, find where the duplicate came from 
			Ref<sdlInterfaceType> dup_in = find_firstbase_usage(oref,bref);
			// dassert(dup_in != 0);
			if (dup_in != 0 && dup_in == oref) // try to patch it up. 
			// the duplicate base class was declared directly
			// as a base class of the class we are analyzing,
			// so we can suppress its use (modulo scoping
			// considerations.
			{
				if (oref==iref) // we can just flag mempt
					mempt.update()->kind = SuppressedBase;
				else // go back through oref's base list
				{
					Ref<Declaration> mem2;
					for (mem2= oref->Bases; mem2!= 0; mem2 = mem2->next)
						if (mem2->type==bref && mem2->kind==BaseType)
						{
							mem2.update()->kind = SuppressedBase;
							break;
						}
				}
				fprintf(stderr,"duplicate base class %s in %s suppressed\n",
					bref->name.string(),oref->name.string());
				// continue outer for look, don't mare this an error.
				continue; // 
			}
			else
			// flag duplication of bases as an error.
			{
						
				char err_msg[500];
				char * fmt_str = "%s %s\n\tinherited through %s";
				if (oref == iref)
					// suppress inherited through
				{
					fmt_str = "%s %s";
				}
				sprintf(err_msg,fmt_str,"inheritance: duplicate base class ",
					bref->name.string(),  iref->name.string());
				oref->print_error_msg(err_msg);
				if (dup_in!= 0)
					fprintf(stderr,"\t%s also a base class of %s\n",
						bref->name.string(),dup_in->name.string());
				dupset.add(bref);
			}
		}		
		else // continue analysis, depth-first resursively.
		{
			bbag.add(bref);
			check_bases(oref,bref,bbag,dupset);
		}
	}
}
				

void 
InterfaceType::compute_size()
{
	if (size != 0) return;
	int current_size = 0;
	int current_align = 0;
	long indexCount = 0;
	Ref<Declaration> mempt;
	// first, do the base classes.
	if (Bases != 0) {
		for (mempt= Bases; mempt!= 0; mempt = mempt->next)
		{
			compute_offset(current_size,current_align,mempt);
			if (current_size==0) // size computation failed, so leave size 0.
			{
				size = 0;
				return; // force a relink.
			}
			// also, count any index attributes.dd
			indexCount += mempt->type->count_fields(Sdl_Index);
		}
	} else { // if no bases, add space for virtual ptr at beginning.
		inc_offset(current_size,current_align,sizeof(void*),sizeof(void *));
	}
	// next, do the attribute decls.
	for (mempt= Decls; mempt!= 0; mempt = mempt->next)
		if (mempt->kind==Attribute || mempt->kind==Relationship)
		{
			compute_offset(current_size,current_align,mempt);
			if (current_size==0) // size computation failed, so leave size 0.
			{
				size = 0;
				return; // force a relink.
			}
			// also count indexes
			indexCount += mempt->type->count_fields(Sdl_Index);
		}
			
	// finally, if this "interface" has no base classes, add in
	// something for the C++ vptr.
	//if (Bases == 0 )
	//	inc_offset(current_size,current_align,sizeof(void*),sizeof(void *));
	// now at beginning of class.
		
	size = roundup(current_size,current_align);
	alignment = current_align;
	numIndexes = indexCount;
	// now, check for duplicate base classes, recursively.
	Bag<Ref<sdlInterfaceType> > BaseBag;
	Set<Ref<sdlInterfaceType> > DupSet;
	check_bases(this,0,BaseBag,DupSet);
	// we could do something with DupSet, but collect_bases will
	// have printed msgs already.

}

void
sdlArrayType::compute_size()
{
	// assumption: size accounts for allignment. hmm.
	if (tag==Sdl_string||tag==Sdl_text) // a special case; we should really fix this.
	// string shares a type with Array???
	{
		size = sizeof(sdl_string);
		alignment = sizeof(long);
		return;
	}
	if (tag==Sdl_sequence) // an unfortunate overloading we should get rid of..
	{
		size = sizeof(sdl_set);
		alignment = sizeof(long);
		return; // for now, don't worry about size of element; this
		// allows recursive definition (which won't work?? )
	}
	if (elementType->size==0)
		elementType.update()->compute_size();
	size = dim * elementType->size;
	alignment = elementType->alignment;
}

void 
UnionType::compute_size()
{
	if (TagDecl==0) // forward decl
	{
		size = 1; // bad hack.
		return;
	}
	int current_size = sizeof(sdl_heap_base);
	int current_align = sizeof(long); // FIX.
	compute_offset(current_size,current_align,TagDecl);
	if (current_size == 0)
	{
		size = 0;
		return;
	}

	int union_size = 0;
	int max_size =  0;
	int tag_size = current_size;
	Ref<sdlDeclaration> armpt;
	// with heap-based unions, the computed size doesn't include the
	// union arms, since that space is heap allocated.
	for (armpt = (Ref<sdlDeclaration> &)ArmList; armpt!=0; armpt = armpt->next)
	{
		if (armpt->type->size==0)
			armpt->type.update()->compute_size();
		// if we can't compute the size now, give up and force though
		// a zero.
		if (armpt->type->size==0)
		{
			size = 0; // flag that type spec. is incomplete.
			return;
		}
		// with heap  based implemetation, the attr. doesn't have
		// a real offset; the sdl_union_base class contains a pointer
		// to space for the arm.
#ifdef  max_size_comp
		// compute_offset(current_size,current_align,armpt);
		if (current_size==0) // size computation failed, so leave size 0.
		{
			size = 0;
			return;
		}
		if (current_size>union_size)
			max_size_size = current_size;
#endif
	}
	//size = union_size;
	size = tag_size; // size of union base + tag decl.
}

long 
InterfaceType::count_fields(TypeTag tt) const
// count the number of attributes of given type in the object.
{
	long count = 0;
	Ref<Declaration> mempt;
	// first, do the base classes.
	for (mempt= Bases; mempt!= 0; mempt = mempt->next)
	{
		count += 
			((Ref<InterfaceType> &)(mempt->type))->count_fields(tt);
	}
	// next, do the attribute decls.
	for (mempt= Decls; mempt!= 0; mempt = mempt->next)
		if (mempt->kind==Attribute || mempt->kind==Relationship)
		{
			count += mempt->type->count_fields(tt);
		}
			
	return count;
}

long
ArrayType::count_fields(TypeTag tt) const
{
	// assumption: size accounts for allignment. hmm.
	if (elementType->size==0)
		elementType.update()->compute_size();
	return dim * elementType->count_fields(tt);
}

long 
UnionType::count_fields(TypeTag tt) const
{
	// not implemented.
	return 0;
}

long
sdlStructType::count_fields(TypeTag tt) const
{
	long count = 0;
	Ref<Declaration> mempt;
	for (mempt = members; mempt  != 0; mempt = mempt->next)
		count += mempt->type->count_fields(tt);
	return count;
}

long 
Type::count_fields(TypeTag tt) const
// default count_fields: return 1 if tag matches, else 0.
{
	return (tag==tt);
}


static
char  *mdir_list[20];
static int mdir_count;

void add_dir(char *p)
{
	mdir_list[mdir_count] = strdup(p);
	++mdir_count;
}

Ref<sdlModule>
lookup_module(const char * mname)
// look for a module with the given name.  For now, we just look
// in the current directory for an appropriately named registerd
// object; later, we will implement a directory path search for 
// this.
{

	Ref<sdlModule> rval;
	shrc rc;
	rval = 0;
	if (mname[0]=='\"') 
	// quoted string: delete the quotes and try again
	{
		char buf[500];
		strcpy(buf,mname);
		buf[strlen(mname)-1] = 0;
		return lookup_module(buf+1);
	}

	rc = Ref<sdlModule>::lookup(mname, rval);
	if(rc  && rc.err_num() != SH_NotFound){
	    rc.fatal();
	}
	if (rval!=0)
		return rval;
	for (int i = 0; i<mdir_count; ++i)
	{
		char buff[200];
		sprintf(buff,"%s/%s",mdir_list[i],mname);
		rc = Ref<sdlModule>::lookup(mname, rval);
		if (!rc )
			return rval;
		else if (rc.err_num() != SH_NotFound){
			rc.fatal();
		}
	}
	return rval;
}

void
sdlModule::resolve_scope(CString sname, Ref<sdlModule> & rmod, 
	Ref<sdlInterfaceType> &rif) const
// sname is a scope qualifier for some type name in "this" module; try
// to resolve it appropriately with the import list and in-scope interfaces.
// replace rmod and rif if an appropriately named scope is found; null
// nem otherwise
{
	Ref<sdlModDecl> dpt;
	if (name.strcmp(sname) != 0)
		rmod = 0;
	// don't leave exiting mod in place???
	for (dpt= (Ref<sdlModDecl>&) import_list; 
		dpt!= 0; 
		dpt= (Ref<sdlModDecl>&)dpt->next)
	{
		if ( dpt->name.strcmp(sname) == 0) // names match
		{
			rmod = dpt->dmodule;
			break;
		}
	}
	// next, try to bind  an interface from the current (this)
	// module;
	rif = 0;
	Ref<sdlDeclaration> idecl;
	idecl = this->resolve_name(sname,InterfaceName);
	if (idecl!= 0)
		rif = (Ref<sdlInterfaceType>&) idecl->type;
}


Ref<sdlType>
sdlModule::add_unresolved(Ref<sdlType> urname) const
// add this name to the set of unresolved type names for this
// module.  If a type of the same name already exists, return the
// type node for that name so we don't duplicate entries with the
// same name. 
// Note: this function isn't really const, but often it will not
// update the module so we declare it as const.
{
	int i;
	for (i=0; i< Unresolved.get_size(); i++)
	{
		Ref<sdlType> uelt = Unresolved.get_elt(i);
		if (!uelt->name.strcmp(urname->name))
		{
			// delete urname
			return uelt;
		}
	}
	// name wasn't found, so add urname to the set.
	update()->Unresolved.add(urname);
	return 0;
}

boolean 
sdlType::is_objtype() const
{
	return false;
}

boolean 
sdlInterfaceType::is_objtype() const
{
	return true;
}

		

Ref<sdlType>
sdlType::get_dtype(TypeTag kind) const
// kind is some kind of reftype, presumably.
{
	WRef<sdlEType>  rtp=0;
	if (this->size==0)
		update()->compute_size();
	switch(kind) {
	case Sdl_ref: // this isn't really allowed for non objects...
	case Sdl_lref: 
	case Sdl_list: case Sdl_multilist:
		rtp = NEW_T RefType;
		rtp->elementType = this;
		rtp->tag = kind;
		rtp->compute_size();
		return rtp;
		break;
	break;
	case Sdl_set: 
		if (setOf == 0)
		{
			rtp = NEW_T RefType;
			rtp->elementType = this;
			rtp->tag = kind;
			rtp->compute_size();
			update()->setOf = rtp;
		}
		return setOf;
	break;
	case Sdl_bag:
		if (bagOf == 0)
		{
			rtp = NEW_T RefType;
			rtp->elementType = this;
			rtp->tag = kind;
			rtp->compute_size();
			update()->bagOf = rtp;
		}
		return bagOf;
	break;
	case Sdl_sequence:
		if (sequenceOf == 0)
		{
			rtp = NEW_T sdlSequenceType;
			rtp->elementType = this;
			rtp->tag = kind;
			rtp->compute_size();
			update()->sequenceOf = rtp;
		}
		return sequenceOf;
	}
	return 0;
}

Ref<sdlType>
sdlInterfaceType::get_dtype(TypeTag kind) const
// this is some kind of reftype, presumably.
// this differs from the generic get_dtype in that set, bag, sequence
// types are always by ref.
{
	WRef<sdlEType> rtp;
	switch(kind) {
	case Sdl_ref: 
		if (refTo == 0)
			update()->refTo = sdlType::get_dtype(kind);
		return refTo;
	break;
	case Sdl_set: 
		if (setOf == 0)
		{
			rtp = NEW_T RefType;
			rtp->elementType = get_dtype(Sdl_ref);
			rtp->tag = kind;
			rtp->compute_size();
			update()->setOf = rtp;
		}
		return setOf;
	break;
	case Sdl_bag:
		if (bagOf == 0)
		{
			rtp = NEW_T RefType;
			rtp->elementType = get_dtype(Sdl_ref);
			rtp->tag = kind;
			rtp->compute_size();
			update()->bagOf = rtp;
		}
		return bagOf;
	break;
	case Sdl_sequence:
		if (sequenceOf == 0)
		{
			rtp = NEW_T sdlSequenceType;
			rtp->elementType = get_dtype(Sdl_ref);
			rtp->tag = kind;
			update()->sequenceOf = rtp;
		}
		return sequenceOf;
	break;
	}
	return sdlType::get_dtype(kind);
}


void 
sdlModule::compute_dependencies()
// compute an ordered set of all declarations in the modules, such that
// dependencies are resolved properly.
// print all the declarations, such that any dependent things are printed
// first.
{
	Bag<Ref<sdlDeclaration> > 
		TestDecls,  // pointer to list of decls we are testing.
		NewTestDecls; // pointer to set of decls tnat need to be retested.

	// algorithm: walk through the set of declarations to be printed( PrintDecls);
	// if the decl has no dependencies or all of its dependencies
	// have been printed, print it and put the decl into the set
	// of things that have been printed (Printed); else put it in 
	// the set of things to retry (newPrintDecls)
	Set<Ref<sdlModule> >   mods;

	// first, collect all the declarations
	int i,j;
	//thisSet.add(mpt);
	if (Resolved.get_size()==0 && Unresolved_decls.get_size()==0)
	// we've not yet tried to resolve things.
	// so initialize the Unresolved set.
	{
		mods.add(this);
		Ref<sdlModDecl> mpt;
		for (mpt= (Ref<sdlModDecl>&)(import_list); mpt != 0;
				mpt= (Ref<sdlModDecl>&)(mpt->next))
		{
			if (mpt->dmodule==0) // ???
				continue;
			mods.add(mpt->dmodule);
		}
		// new 
		for (i=0; i<mods.get_size(); i++)
		{
			Ref<Declaration> dpt;
			for (dpt = mods.get_elt(i)->decl_list; dpt!= 0; dpt = dpt->next)
			{
				// maybe there should be some filtering here?
				TestDecls.add(dpt);
			}
		}

	}
	else
		TestDecls = Unresolved_decls;
	// now, everything is initializes, so go to it.
	while (TestDecls.get_size() > 0)
	{

		for ( i=0; i< TestDecls.get_size(); i++)
		{
			Ref<sdlDeclaration> TestDecl = TestDecls.get_elt(i);
			Bag <Ref<sdlDeclaration> > new_resolved;
			bool depends = true;
			for (j = 0; j < TestDecl->dependsOn.get_size(); j++)
			{
				if ( Resolved.member(TestDecl->dependsOn.get_elt(j)))
					continue;
				else // cant print now.
				{
					depends = false;
					break; // the for j loop.
				}
			}
			if (depends) // all dependencies are resolved.
			{
				Resolved.add(TestDecl);
			}
			else // retry on next pass.
				NewTestDecls.add(TestDecl);
		}
		if (TestDecls.get_size() == NewTestDecls.get_size())
			break;
		
		// we didn't resolve anything, so quit.
		// now, delete newly resolved names from the Unresolved list.
		// we've printed everything we can, so move newPrintDecls
		// to PrintDecls and clear PrintDecls.
		TestDecls = NewTestDecls;
		NewTestDecls.set_size(0); // clear the set.
	}
	Unresolved_decls = TestDecls;
}

void
sdlModule::add_dmodules(Set<Ref<sdlModule> > & dset) const
// add this module and any imported modules into the set dset
// if this module is not already present.
{
	Ref<sdlModDecl> mpt;
	if (dset.member(this))
		return;
	dset.add(this);
	for (mpt= (Ref<sdlModDecl>&)(import_list); mpt != 0;
			mpt= (Ref<sdlModDecl>&)(mpt->next))
	{
		if (mpt->dmodule==0) // ???
		{
			mpt->print_error_msg(
			"module not linked; couldn't find corresponding module definition");
			return;
		}
		dset.add(mpt->dmodule);
	}
}

void 
compute_dependencies(
	Set<Ref<sdlModule> >   imods, 
	Set<Ref<sdlModule> >   &mods, 
	Set<Ref<sdlDeclaration> > & Resolved,
	Set<Ref<sdlDeclaration> > & Unresolved_decls
)
// compute an ordered set of all declarations in the modules, such that
// dependencies are resolved properly.
// print all the declarations, such that any dependent things are printed
// first.
{
	Bag<Ref<sdlDeclaration> > 
		TestDecls,  // pointer to list of decls we are testing.
		NewTestDecls; // pointer to set of decls tnat need to be retested.

	// algorithm: walk through the set of declarations to be printed( PrintDecls);
	// if the decl has no dependencies or all of its dependencies
	// have been printed, print it and put the decl into the set
	// of things that have been printed (Printed); else put it in 
	// the set of things to retry (newPrintDecls)

	// first, collect all the declarations
	int i,j;
	//thisSet.add(mpt);
	for (i=0; i<imods.get_size(); i++)
	// we've not yet tried to resolve things.
	// so initialize the Unresolved set.
	{
		
		Ref<sdlModule> mref = imods.get_elt(i);
		mref->add_dmodules(mods);
	}
	// next, collect all the decls.
	for (i=0; i<mods.get_size(); i++)
	{
		Ref<sdlModule> mref = mods.get_elt(i);
		for (j=0; j<mref->Unresolved_decls.get_size(); j++)
			TestDecls.add( mref->Unresolved_decls.get_elt(j));
		for (j=0; j<mref->Resolved.get_size(); j++)
			Resolved.add( mref->Resolved.get_elt(j));
	}
	// now, everything is initializes, so go to it.
	while (TestDecls.get_size() > 0)
	{

		for ( i=0; i< TestDecls.get_size(); i++)
		{
			Ref<sdlDeclaration> TestDecl = TestDecls.get_elt(i);
			Bag <Ref<sdlDeclaration> > new_resolved;
			bool depends = true;
			for (j = 0; j < TestDecl->dependsOn.get_size(); j++)
			{
				if ( Resolved.member(TestDecl->dependsOn.get_elt(j)))
					continue;
				else // cant print now.
				{
					depends = false;
					break; // the for j loop.
				}
			}
			if (depends) // all dependencies are resolved.
			{
				Resolved.add(TestDecl);
			}
			else // retry on next pass.
				NewTestDecls.add(TestDecl);
		}
		if (TestDecls.get_size() == NewTestDecls.get_size())
			break;
		
		// we didn't resolve anything, so quit.
		// now, delete newly resolved names from the Unresolved list.
		// we've printed everything we can, so move newPrintDecls
		// to PrintDecls and clear PrintDecls.
		TestDecls = NewTestDecls;
		NewTestDecls.set_size(0); // clear the set.
	}
	Unresolved_decls = TestDecls;
}

void
print_cxx_binding(
	Set<Ref<sdlModule> >   imods 
) 
{
	Set<Ref<sdlModule> >   mods; 
	Set<Ref<sdlDeclaration> >  Resolved;
	Set<Ref<sdlDeclaration> >  Unresolved_decls;
	Ref<sdlModule> mpt;
	Ref<sdlModule> meta_mod;
	Ref<Declaration> dpt;
	int i = 0;
	meta_mod = 0;
	TemplatesUsed.set_size(0);

	compute_dependencies(imods,mods,Resolved,Unresolved_decls);

	bf = stdout;



	if (Unresolved_decls.get_size()>0)
	{
		fprintf(stderr," can't print binding  %d unresolved references\n",
			 Unresolved_decls.get_size());
		Ref<sdlDeclaration> dpt;
		int i;
		for (i= 0; i < Unresolved_decls.get_size(); i++)
		{	
			dpt = Unresolved_decls.get_elt(i);
			dpt->print_error_msg("depends on following unresolved decl(s)\n");
			Ref<sdlDeclaration> ddpt;
			int j;
			for (j=0; j<dpt->dependsOn.get_size(); j++)
			{
				ddpt = dpt->dependsOn.get_elt(i);
				if (Unresolved_decls.member(ddpt))
					ddpt->print_error_msg("");
			}
		}
		return;	
	}
	// first, do initial cpp stuff for the module.
	for ( i = 0; i<mods.get_size(); i++)
	{
		mpt = mods.get_elt(i);
		fprintf(bf,"#ifndef %s_mod\n#define %s_mod 1\n",(char *)mpt->name,(char *)mpt->name);
	}
	// next, global include.
	fprintf(bf,"#include \"ShoreApp.h\"\n");
	// next get loid for the module
	for ( i = 0; i<mods.get_size(); i++)
	{
		mpt = mods.get_elt(i);
		LOID mod_oid;
		VolId Mod_volume;
		W_COERCE(mpt.get_primary_volid(Mod_volume));
		W_COERCE(mpt.get_primary_loid(mod_oid));
		sdl_string name = mpt->name;

		if (!name.strcmp("metatypes"))
			meta_mod = mpt;
		// print forward decls for interfaces
		// first, check for consistent versions by printing an external
		// variable name encodeing the serial # of the module, and making a
		// ref to it.
		fprintf(bf,"class sdlModule_%s 	{ public: virtual int oid_%d(); };\n",
			(char *)name,
			mod_oid.id.serial.guts._low);
		fprintf(bf,"static sdlModule_%s %s_header_version;\n", 
				(char *)name, (char *)name);
	}

	// next print pre-decls for interface decls.
	for (i = 0; i<Resolved.get_size(); i++)
	{
		dpt = Resolved.get_elt(i);
		if (dpt->kind == TypeName 
			&& dpt->type != 0
			&& dpt->type->tag==Sdl_interface 
			// need to check if it's a typedef
			&&  (char *)dpt->type->name 
			&& !strcmp((char *)dpt->type->name,(char *)dpt->name))
		{
			// this used to be INTERFACE_PREDEFS, but forward class
			// decl is all thats left. Now reinstate.
			// fprintf(bf,"INTERFACE_PREDEFS( %s);\n",(char *)dpt->name);
			// oops, to handle derived classes, we need to declare
			// the ref template properly, so check for derived classes.
			Ref<sdlInterfaceType> ipt = (Ref<sdlInterfaceType> &)dpt->type;
			if (ipt->Bases != 0) // there are base classes...
			{
				int bcount = 0;
				Ref<sdlDeclaration> bpt;
				for (bpt = ipt->Bases;  bpt != 0; bpt = bpt->next)
					++bcount;
				fprintf(bf,"INTERFACE_PREDEFS%1d(%s",bcount,(char *)dpt->name);
				for (bpt = ipt->Bases;  bpt != 0; bpt = bpt->next)
					fprintf(bf,",%s",(char *)bpt->name);
				fprintf(bf,");\n");

				// this doesn't work for mulit
			}
			else
				fprintf(bf,"INTERFACE_PREDEFS( %s);\n",(char *)dpt->name);
				
		}
	}
	// next, the class definitions themselves
	// lpt->print_sdl();
	for (i = 0; i<Resolved.get_size(); i++)
	{
		dpt = Resolved.get_elt(i);
		// yuck, check for duplicates...
		bool dup_found = false;
		int j;
		for (j=0; j<i; j++)
		{	Ref<sdlDeclaration> dpt2 = Resolved.get_elt(j);
			if (dpt2->kind==dpt->kind && dpt->name.strcmp(dpt2->name) == 0)
			{
				fprintf(stderr,
				"multiple use of name %s, source lines %d and %d\n",
				(char *)dpt->name, dpt->lineno, dpt2->lineno);
				fprintf(stderr,"2nd declaration is suppressed\n");
				// this is not reall correct; should abort in some cases.
				dup_found = true;
				break;
			}
		}
		if (!dup_found)
			dpt->print_sdl();
		fprintf(bf,";\n");
	}

	// next, post-definition stuff
	// do these by module.
	for (i = 0; i<Resolved.get_size(); i++)
	{
		dpt = Resolved.get_elt(i);
		if (dpt->kind == TypeName && 
			dpt->type->tag==Sdl_interface
			// need to check if it's a typedef
			&&  (char *)dpt->type->name && !strcmp((char *)dpt->type->name,(char *)dpt->name))
		{
			// for want of a better place, print
			// out the "oid" here.:
			// metatype interfaces are registered objects; special
			// kludge for reserved names
			if (  meta_mod != 0 && ((Ref<sdlInterfaceType> &)dpt->type)->myMod == meta_mod)
				fprintf(bf,"#define %s_OID ReservedSerial::_%s.guts._low\n",
					(char*)dpt->name,(char *)dpt->name);
			else
			{
				LOID type_oid;
				W_COERCE(dpt->type.get_loid(type_oid));
				fprintf(bf,"const int %s_OID = %d;\n", (char *)dpt->name,
					type_oid.id.serial.guts._low);
				// dubiously, we just print the serial number...
			}
			// OID is needed for INTERFACE_POSTDEFS
			fprintf(bf,"INTERFACE_POSTDEFS(%s)\n", (char *)dpt->name);
		}
	}

	// finally, the .C portion
	fprintf(bf,"\n#ifdef MODULE_CODE\n");

	for (i= 0; i<mods.get_size(); i++)
	{
		mpt = mods.get_elt(i);
		LOID mod_oid;
		VolId Mod_volume;
		W_COERCE(mpt.get_primary_volid(Mod_volume));
		W_COERCE(mpt.get_primary_loid(mod_oid));
		sdl_string name = mpt->name;
		fprintf(bf,"struct rModule %s(\"%s\",%d,%d,%d );\n",
			(char *)name,(char *)name,
			Mod_volume.high,Mod_volume.low,
			mod_oid.id.serial.guts._low);
		fprintf(bf,"#define CUR_MOD %s\n",(char *)name);
		// above is sleazy hack to get module name into other
		// macros.
		char buf[20];
		if (mpt == meta_mod)
		{
			sscanf(mpt->src_file.string(),"%*s %*s %s",buf);
			for (int j=0; j<sizeof(buf); j++)
				if (buf[j]=='.') buf[j]= '_';
			metatype_version = strdup(buf);
			fprintf(bf,"char * metatype_version_%s = \"%s\";\n",buf,mpt->src_file.string());
			fprintf(bf,"char * metatype_version = \"%s\";\n",metatype_version);
		}
		else
		{
			fprintf(bf,"extern char * metatype_version_%s;\n",metatype_version);
		}
	
		// define the per module version checking class.
		if (metatype_version == 0) // default to something
			metatype_version = "0_00";
		fprintf(bf,"int sdlModule_%s::oid_%d(){ return (int)metatype_version_%s; };\n",
			(char *)name,
			mod_oid.id.serial.guts._low,metatype_version);

		for(dpt= mpt->decl_list; dpt!=0; dpt = dpt->next)
		{
			dpt->print_cxx_support();
		}
		fprintf(bf,"#undef CUR_MOD %s\n",(char *)name);
	}
	// finally finally, explicitly instancitate templates.
	for (i=0; i<TemplatesUsed.get_size(); i++)
	{
		fprintf(bf,"template class ");
			TemplatesUsed.get_elt(i)->print_sdl_var(";\n");
	}
	fprintf(bf,"\n#endif MODULE_CODE\n");
	// finally finally finally, complete initial module ifdefs.
	for ( i = 0; i<mods.get_size(); i++)
	{
		mpt = mods.get_elt(i);
		fprintf(bf,"#endif %s_mod\n",(char *)mpt->name);
	}

}
