/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header:
 */

#include "sdl_internal.h"
#include "metatype_ext.h"
#include "string.h"
#include "app_class.h"
#include "sdl_gen_set.h"
int cxx_mode=1;
FILE *bf; // language binding output file.

// builting types: maybe refs?
// note: it seems there should be size in here someplace...
extern Type ShortIntegerType;
extern Type LongIntegerType;
extern Type CharacterType;
extern Type BooleanType;
extern Type UnsignedLongIntegerType;
extern Type UnsignedShortIntegerType;
extern Type UnsignedCharacterType;
// the swap value is bogus for the following.
extern Type FloatingPointType;
extern Type DoublePrecisionType;
extern Type AnyType;
// RefType anyRefType;
extern Type VoidType;
extern Type StringLiteralType;

Set<Ref<sdlType> > TemplatesUsed;
// when printing language binding, track these so we can explicitly
// instanciate anything used.


char * tag_to_string[] = { // one foe each TypeTage
	"NOTYPE", 	// NO_Type = 0,	// null instance
	"any",		// Sdl_any,
	// primitive types
	"char", 	// Sdl_char
	"short", 	//Sdl_short
	"long", 	// Sdl_long
	"float", 	// Sdl_float
	"double", 	// Sdl_double,
	"boolean", 	//Sdl_boolean
	"octet", 	// Sdl_octet
	"unsigned short", // Sdl_unsigned_short
	"unsigned long", // Sdl_unsigned_long,
	"void ",	// Sdl_void
	"pool ", 	// Sdl_pool
	"enum", 	//Sdl_enum
	"struct", 	// Sdl_struct
	"union", 	// Sdl_union
	"interface", 	// Sdl_interface,
	"array", 	//Sdl_array,
	"string", 	//Sdl_string
	"sequence", 	//, Sdl_sequence,
	"text",		//, Sdl_text
	"ref",		//Sdl_ref
	"lref", 	// Sdl_lref
	"set", 		// Sdl_set
	"bag", 		//Sdl_bag
	"list", 	//Sdl_list
	"multilist", 	//Sdl_multilist,
	"typedef" ,	//Sdl_NamedType // type named; not linked.
	"external", // Sdl_ExternType - defined externally.
	"index",		// Sdl_index
	"class",		// Sdl_Class
	"union"		// Sdl_Class
};

// translate to c++ instead of sdl.
char * tag_to_cxx[] = { // one foe each TypeTage
	"NOTYPE", 	// NO_Type = 0,	// null instance
	"void",		// Sdl_any,
	// primitive types
	"char", 	// Sdl_char
	"short", 	//Sdl_short
	"long", 	// Sdl_long
	"float", 	// Sdl_float
	"double", 	// Sdl_double,
	"boolean", 	//Sdl_boolean
	"unsigned char", 	// Sdl_octet
	"unsigned short", // Sdl_unsigned_short
	"unsigned long", // Sdl_unsigned_long,
	"void ",	// Sdl_void
	"Pool ",	// Sdl_pool
	"enum", 	//Sdl_enum
	"struct", 	// Sdl_struct
	"struct", 	// Sdl_union is actually a struct.
	"class", 	// Sdl_interface,
	"array", 	//Sdl_array,
	"sdl_string", 	//Sdl_string
	"Sequence", 	//, Sdl_sequence,
	"sdl_text",	//, Sdl_text
	"Ref",		//Sdl_ref
	"LREF", 	// Sdl_lref
	"Set", 		// Sdl_set
	"Bag", 		//Sdl_bag
	"Sequence", 	//Sdl_list < maybe should be something else>
	"MULTILIST", 	//Sdl_multilist, <who knows what this shoudl be.
	"typedef", 	//Sdl_NamedType // type named; not linked.
	"EXTERNAL", // Sdl_ExternType - defined externally.
	"INDEX",	// Sdl_index
	"class",	// Sdl_Class
	"union",	// Sdl_CUnion
};

// for builtin types, map from type tag to the corresponding type object.
char * TypeTag_to_tobj[] = { // one foe each TypeTage
	0, 	// NO_Type = 0,	// null instance
	"VoidType",		// Sdl_any,
	// primitive types
	"CharacterType", 	// Sdl_char
	"ShortIntegerType", 	//Sdl_short
	"LongIntegerType", 	// Sdl_long
	"FloatingPointType", 	// Sdl_float
	"DoublePrecisionType", 	// Sdl_double,
	"BooleanType", 	//Sdl_boolean
	"UnsignedCharacterType", 	// Sdl_octet
	"UnsignedShortIntegerType", // Sdl_unsigned_short
	"UnsignedLongIntegerType", // Sdl_unsigned_long,
	"VoidType",	// Sdl_void
	"PoolType",	// Sdl_pool
	0, 	//Sdl_enum
	0, 	// Sdl_struct
	0, 	// Sdl_union
	0, 	// Sdl_interface,
	0, 	//Sdl_array,
	0, 	//Sdl_string
	0, 	//, Sdl_sequence,
	0,	//  Sdl_text
	0,		//Sdl_ref
	0, 	// Sdl_lref
	0, 		// Sdl_set
	0, 		//Sdl_bag
	0, 	//Sdl_list
	0, 	//Sdl_multilist,
	0 	//Sdl_NamedType // type named; not linked.
};

// output array for TypeTag enum.
char * TypeTag_str[] = { // one foe each TypeTage
	"NO_Type",
	"Sdl_any",
	// primitive types
	"Sdl_char",
	"Sdl_short",
	"Sdl_long",
	"Sdl_float",
	"Sdl_double,",
	"Sdl_boolean",
	"Sdl_octet",
	"Sdl_unsigned_short",
	"Sdl_unsigned_long",
	"Sdl_void",
	"Sdl_pool",
	"Sdl_enum",
	"Sdl_struct",
	"Sdl_union",
	"Sdl_interface",
	"Sdl_array",
	"Sdl_string",
	"Sdl_sequence",
	"Sdl_text",
	"Sdl_ref",
	"Sdl_lref",
	"Sdl_set",
	"Sdl_bag",
	"Sdl_list",
	"Sdl_multilist",
	"Sdl_NamedType", // I'm not sure about this.
	"Sdl_ExternType",
	"Sdl_index",
	"Sdl_Class",
	"Sdl_CUnion"
};

// actual object declartion type for each type tag.
char * TypeTag_to_metatobj[] = { // one foe each TypeTage
	"Type",	// NO_Type
	"Type",	// Sdl_any
	"Type",	// Sdl_char
	"Type",	// Sdl_short
	"Type",	// Sdl_long
	"Type",	// Sdl_float
	"Type",	// Sdl_double,
	"Type",	// Sdl_boolean
	"Type",	// Sdl_octet
	"Type",	// Sdl_unsigned_short
	"Type",	// Sdl_unsigned_long
	"Type",	// Sdl_void
	"Type",	// Sdl_pool
	"EnumType",	// Sdl_enum
	"StructType",	// Sdl_struct
	"UnionType",	// Sdl_union
	"InterfaceType",	// Sdl_interface
	"ArrayType",	// Sdl_array
	"SequenceType",	// Sdl_string
	"SequenceType",	// Sdl_sequence
	"SequenceType",	// Sdl_text // but what's going on here?
	"RefType",	// Sdl_ref
	"RefType",	// Sdl_lref
	"RefType",	// Sdl_set
	"RefType",	// Sdl_bag
	"RefType",	// Sdl_list
	"RefType",	// Sdl_multilist
	"Type",	// Sdl_NamedType
	// I'm not sure about this.
	"Type",
	"IndexType",	// Sdl_index
};

char *zone_to_string[] =
{
	"public",
	"private",
	"protected"
};

char * exprop_to_string[] = {
	" error ", 	// EError
	" Cname ",	// CName
	" Literal ",	// Literal
	" default ",	// CDefault
	" + ",		// Plus
	" - ",		// Minus
	" * ",		// Mult
	" / ",		// Div
	" % ",		// ModA
	" | ",		// Or
	" & ",		// And
	" ^ ",		// Xor
	" ~ ",		// Complement
	" , ",		// comma
	" << ",		// LShift
	" >> ", 	// RShift
	" selectexpr ",
	" project ",
	" Assign ",
	" Select ",
	" Range ",
	" RangeVar ",
	" . "
};

// print translations for DeclKind enum
char * DeclKind_str[] = {
	"ERROR",
	"Constant",
	"TypeName",
	"Alias",
	"Member",
	"Arm",
	"Attribute",
	"Op",
	"OpOverride",
	"Param",
	"Mod",
	"BaseType",
	"EnumName",
	"Exception",
	"InterfaceName",
	"Relationship",
	"UnboundRelationship",
	"ImportMod",
	"ExportName",
	"ExportAll",
	"UseMod",
	"ExternType"
};
// error strings for DeclKind enum.
char * DeclKind_desc[] = {
	"ERROR",
	"constant",
	"type name",
	"alias",
	"struct member",
	"union arm",
	"interface attribute",
	"method",
	"method override",
	"parameter",
	"module",
	"interface base class",
	"enum",
	"Exception",
	"interface",
	"relationship",
	"relationship",
	"import module",
	"export",
	"export all",
	"use module",
	"extern type",
	"interface base class"
};
// metatype object types corresponding to various decl kinds.
char * DeclKind_objtype[] = {
	"Declaration",	//ERROR
	"ConstDecl",	//Constant
	"Declaration",	//TypeName
	"Declaration",	//Alias
	"Declaration",	//Member
	"ArmDecl",	//Arm
	"AttrDecl",	//Attribute
	"OpDecl",	//Op
	"OpDecl",	//OpOverride
	"ParamDecl",	//Param
	"ModDecl",	//Mod
	"Declaration",	//BaseType
	"ConstDecl",	//EnumName
	"Declaration",	//Exception
	"Declaration" 	// InterfaceName"
};
// print translations for Zone enum
char *Zone_str[] = {
	"Public",
	"Private",
	"Protected"
};

// print translations for Swap enum
char *Swap_str[] = {
	"None",
	"EveryTwo",
	"EveryFour",
	"Constructed"
};

// print translations for Boolean enum
char *Bool_str[] = {
	"False",
	"True"
};

// print translations for Mode enum
char *Mode_str[] = {
	"In",
	"Out",
	"InOut"
};
Ref<Declaration>
Declaration::ListAppend(Ref<Declaration> apt)
// append the object pointed to by apt to the linked list implemented
// through the "next" pointer.
{
	if (!this) return apt; //
	Ref<Declaration> lpt = this;
	while (lpt->next != 0)
		lpt = lpt->next;
	lpt.update()->next = apt;
	return this;
}

// compute the value & type associated with the expression at 
extern TypeTag reduce_type1(TypeTag);

long
ExprNode::fold() const
// compute the value of some expression, and return it.
// currently we only handle integer values; this should
// be extended to handle float/string.
// plan b: set the val_u field specifying the type of the value.
// expr types ar always derived from the type of the subexpressions.
{
	switch(reduce_type1(tvalue.get_tagval()))
	{
		case Sdl_long:
			return tvalue.get_long_val();
			break;
		case Sdl_unsigned_long:
			return tvalue.get_ulong_val();
			break;
		case Sdl_float: case Sdl_double:
			return (long)tvalue.get_double_val();
			break;
		default:
			return 0;
	}
}

// sdl print section
void
print_sdl_dcl_list(Ref<Declaration> lpt, char * prefix, char *sep, char  *suffix,
int printzone = 0)
// print a list of declarations, prefixed by the prefix string, terminated
// by the suffix string, and separated bye sep.
// printzone is a hack for stuff inside interfaces.
{
	Zone lastzone;
	if ( prefix)
		fprintf(bf,"%s",prefix);
	if (printzone && lpt != 0 ) // print out the public/private/protected scope.
	{
		fprintf(bf,"%s:\n",zone_to_string[lpt->zone]);
		lastzone = lpt->zone;
	}

	for(;lpt != 0 ;lpt = lpt->next)
	{
		if (lpt->kind==SuppressedBase)
			continue;
		if (printzone && (lpt->zone != lastzone || lpt->kind==BaseType))
		{
			fprintf(bf,"%s:\n",zone_to_string[lpt->zone]);
			lastzone = lpt->zone;
		}
		lpt->print_sdl();
		if (lpt->next != 0)
			fprintf(bf,"%s",sep);
	}
	fprintf(bf,"%s",suffix);
}

void
get_scoped_name(char *buf, Ref<sdlDeclaration> decl)
// return in buf a string constructeded from the declaration's name
// and (possibly) the scope in which it was declared.  Normally, we
// don't print module names but interface names may be required.
{
	if (decl==0 || decl->scope==0 || decl->scope->isa(TYPE_PT(sdlModule)))
	// scoped name is just the string from the decl.
		sprintf(buf,(const char *)decl->name);
	else  // deviously, it seems we can print nested scopes recursively.
	{
		get_scoped_name(buf,decl->scope->myDecl);
		strcat(buf,"::");
		strcat(buf,(const char *)decl->name);
	}

}


// sdl printing section
void
ConstDecl::print_sdl() const
{
	bool in_interface = false;
	if (kind==EnumName) // an exception to general rule; should
	// probablly be a differnet metatype for the decl.
	{
		fprintf(bf,"%s ",(char *)name);
		return;
	}
		
	if (scope != 0 && scope->isa(TYPE_PT(sdlInterfaceType)))
		in_interface = true;
	else
		in_interface = false;

	if (in_interface)

		fprintf(bf,"static const ");
	else
		fprintf(bf,"const ");
	if (type != 0)
		type->print_sdl_var(name);
	else
		fprintf(bf," %s ",name.string());
	if (in_interface) return; // value printed elsewhere.
	if (expr!= 0)
	{
		fprintf(bf,"=");
		expr->print_sdl();
	}
}

// sdl printing section
void
ConstDecl::print_cxx_support() const
{
	bool in_interface = false;
	if (kind==EnumName) // an exception to general rule; should
	// probablly be a differnet metatype for the decl.
	{
		fprintf(bf,"%s ",(char *)name);
		return;
	}
		
	if (! scope->isa(TYPE_PT(sdlInterfaceType))) return; 
	// if this is within an interface type, print out the
	// actual constant in code section, since it is static const.
	fprintf(bf,"const ");
	char dbuf[200];
	get_scoped_name(dbuf,this);

	type->print_sdl_var(dbuf);
	if (expr!= 0)
	{
		fprintf(bf,"=");
		expr->print_sdl();
	}
	fprintf(bf,";\n");

	// replace this with expression code
}

void
ModDecl::print_sdl() const
// print the sdl module: exports and imports are now ignored; 
// for now just print the module header and walk down the
// declaration list.
{
	if (cxx_mode)
		fprintf(bf,"// sdl module %s \n",(char *)name);
	else
		fprintf(bf,"module %s {\n",(char *)name);
	for (Ref<Declaration> dpt = dmodule->decl_list; dpt!= 0; dpt=dpt->next)
	{
		dpt->print_sdl();
		fprintf(bf,";\n"); // module decls are ; terminated.
	}
	if (cxx_mode)
		fprintf(bf,"// end sdl module %s \n",(char *)name);
	else
		fprintf(bf,"}\n");
}

void
print_sdl_case_list(Ref<sdlExprNode>  l)
// this is dummied out temporarily; node * is not the correct form...
{
	static int i=0;
	Ref<sdlArithOp> lpt = (Ref<sdlArithOp>&)l;
	// ok, try again: iterate over the node "list", printing
	// out each info field as an expression.
	for (; lpt!= 0; lpt= (Ref<sdlArithOp> &)(lpt->e2))
	{
		Ref<ExprNode>  ept = lpt->e1;
		if (ept->etag != CDefault)
			fprintf(bf,"case ");
		ept->print_sdl();
		fprintf(bf,":\n");
	}
}

void
ArmDecl::print_sdl() const
// Arms are always inside unions; they have a list of constants
// to switch on, and single type declaration.
{
	if (!cxx_mode) // no cases for c++; just print decl
		print_sdl_case_list(CaseList); // not yet well defined.
	type->print_sdl_var(name);
}

void
AttrDecl::print_sdl() const
// attributes always are inside of interfaces; always have keyword
// attribute; have public/private/protected scope.  We leave scope
// printing up to the containing interface though.
{
	//
	char * rstr = readOnly? "readonly ":"";
	char *istr = Indexable?"indexable ":"";
	if (!cxx_mode)
		fprintf(bf,"\t%s%sattribute ", rstr,istr);
	type->print_sdl_var(name);
}

char * cur_interface_name; // very tacky

void
print_rel_type(Ref<sdlDeclaration> d)
// break out the code from RelDecl::print_sdl so the type
{
	Ref<sdlRelDecl> r;
	r.assign(d); // is really a reldecl, or else!
	char * mstr; // type of relation
	Ref<RefType> thisType = (Ref<RefType> &)(r->type);
	TypeTag tptag = r->type->tag;
	switch(tptag)
	{
		case Sdl_ref:
			mstr = "RefInv";
		break;
		case Sdl_set:
			mstr = "SetInv";
			TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_set));
			TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_sequence));
		break;
		case Sdl_bag:
			mstr = "BagInv";
			TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_bag));
			TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_set));
			TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_sequence));
		break;
	}
	fprintf(bf,"%s<%s,%s,%d> ",
		mstr,
		r->inverseDecl->scope->myDecl->name.string(),
		r->scope->myDecl->name.string(),
		r->offset
	);
}
void
RelDecl::print_sdl() const
// relationships always are inside of interfaces; always have keyword
// relationship; have public/private/protected scope.  We leave scope
// printing up to the containing interface though.
// note: this is just a placeholder, needs a bunch(!) of work.
{
	//
	char * rstr = readOnly? "readonly ":"";
	char *istr = Indexable?"indexable ":"";
	if (!cxx_mode)
		fprintf(bf,"\t%s%srelationship ", rstr,istr);
	if (inverseDecl==0)
		type->print_sdl_var(name);
	else // it is a thing with inverse; we need to print
	// a proper macro. do we want to do this in place? no.
	// but then it must be prerinted? what?
	// DECLARE_REL_REF, DECLARE_REL_SET, each take 4 args:
	{
		char * mstr; // type of relation
		Ref<RefType> thisType = (Ref<RefType> &)(type);
		char * inv_tstr= 0;
		if (thisType->elementType->name.string() != NULL)
			inv_tstr = thisType->elementType->name;
		else // presumably, set<ref<t>>
		{
			inv_tstr = ((Ref<RefType> &)(thisType->elementType))->elementType->name;
		}
		// yuk
		// inv_tstr is name of other side of relation

		TypeTag tptag = type->tag;
		switch(tptag)
		{
			case Sdl_ref:
				mstr = "RefInv";
			break;
			case Sdl_set:
				mstr = "SetInv";
				TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_set));
				TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_sequence));
			break;
			case Sdl_bag:
				mstr = "BagInv";
				TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_bag));
				TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_set));
				TemplatesUsed.add(thisType->elementType->get_dtype(Sdl_sequence));
			break;
		}
		if (zone!=Public) // we need to make the inverse decl type a friend.
		{
			fprintf(bf,"friend ");
			print_rel_type(inverseDecl);
			fprintf(bf,";\n");
		}
		fprintf(bf,"%s<%s,%s,%d> %s\n",
			mstr,
			inv_tstr,
			// (char *)inverseDecl->name,
			cur_interface_name,
			offset,
			(char *)name
		);
	}
	// TemplatesUsed.add(Ref<sdlType>(this));
}

void
ParamDecl::print_sdl() const
{
	char *modstr;
	if (cxx_mode) // print out/inout as C++ reference types; we hack
	// that by passing in a modified name string.
	{
		switch (mode)
		{
			case In: // same as before
				type->print_sdl_var(name);
			break;
			case Out:
			case InOut:
			{	char rname[100];
				sprintf(rname,"&%s",(char *)name);
				type->print_sdl_var(rname);
			}
			break;
		}
	}
	else
	{
		switch (mode)
		{
			case In: modstr="in ";break;
			case Out:modstr="out ";break;
			case InOut: modstr="inout ";break;
		}
		fprintf(bf,"%s ", modstr);
		type->print_sdl_var((char *)name);
	}
}

void
OpDecl::print_sdl() const
{
	if (cxx_mode)
		fprintf(bf,"virtual ");
	type->print_sdl_var((char *)name);
	print_sdl_dcl_list((Ref<Declaration> &)parameters, "(", ",", ")" );
	if (isConst)
		fprintf(bf," const ");
};

void
sdlExtTypeDecl::print_sdl() const
// this is an external type name; we print out a c++ stype tag only
// forward decl as appropriate.
// eg class a; union b; enum c; struct d;
// Typedefs unfortunately can't be handled this way.
{
	if (type->tag == Sdl_ExternType) // a typedef name; can't really do anything
		return;
	type->print_sdl_dcl();
}



	
void
Declaration::print_sdl() const
// print a declaration in sdl format
// the subtypes of Declaration that are structurally different have
// explicit dispatch to a class - specific print_sdl here; we don't
// use virtual functions in order to make compiled-in initialization
// easier.
{
	switch(kind) {
	case TypeName:
	// first, check for struct/union/interface dcl
		if ((char *)type->name && !strcmp((char *)name , (char *)type->name))
			type->print_sdl_dcl();
		else
		{
			fprintf(bf,"typedef ");
			type->print_sdl_var((char *)name);
		}
	break;
	case Member:  // structure member
		type->print_sdl_var((char *)name);
	break;
	case BaseType:
		fprintf(bf,"%s %s ",zone_to_string[zone],(char *)name); 
		// really need scoped name
		// always print zone.
	break;
	case EnumName:
		fprintf(bf,"%s ",(char *)name); // really need scoped name
	break;
	case Exception:
		; // ignore temporarily
	break;
	case InterfaceName:
		if (cxx_mode)
			fprintf(bf," class %s ",(char *)name);
		else
			fprintf(bf,"interface %s ",(char *)name);
	break;
	default:
		fprintf(bf,"unknown/unhandled decl kind %d\n",kind);
		
	}
}

void
sdlEnumType::print_sdl_dcl() const
{
	if (consts==0) //forward decl..
	{
		// fprintf(bf,"enum %s;\n",(char *)name);
		// c++ forward decls aren't allowed...
		return;
	}
	fprintf(bf,"enum %s {",(char *)name);
	print_sdl_dcl_list(consts,"\n\t",",\n\t","};\n");
	fprintf(bf,"OVERRIDE_INDVAL(%s)\n",(char *)name);
	fprintf(bf,"NO_APPLY(%s)\n",(char *)name);
}
void
print_apply(CString vname, Ref<Type> tp);

void
sdlStructType::print_sdl_dcl() const
// just run through things in sdl format.
{
	if (members==0) // null member list -> fwd decl only
	{
		fprintf(bf," struct %s ",(char *)name);
		return;
	}
	fprintf(bf,"struct %s {\n",(char *)name);
	print_sdl_dcl_list(members,"",";\n",";\n");
	// now, also print heap apply, hm.
	// fprintf(bf,"APPLY_DEF(%s)\n",(char *)name);
	// oops, APPLY_DEF macro doesn't quite work because qualifier
	// causes inline function to be defined global.  So do it
	// by hand.
	// to suppress unused arg warnings, check if we actually
	// do annything.
	// fprintf(bf,"\tvoid __apply(HeapOps op) {\n");
	bool empty_apply = true;
	// next, need to walk through the attributes printing
	// the calls.  For attributes that are refs, print the
	// apply call  directly; for structs and arrays, call
	// some support code.
	if (has_refs() || has_heapsp())
	{
		fprintf(bf,"\tvoid __apply(HeapOps op) {\n");
		Ref<Declaration> dpt;
		for (dpt = members; dpt != 0; dpt = dpt->next)
		{
			if((dpt->kind==Member) && 
				(dpt->type->has_refs() ||dpt->type->has_heapsp()))
			{
				if (empty_apply)
				{
					empty_apply = false;
				}
				print_apply((char *)dpt->name,dpt->type);
			}
		}
		fprintf(bf,"};\n");
	}
	else // empty apply
		fprintf(bf,"\tvoid __apply(HeapOps) {}\n");
	fprintf(bf,"static void sdl_apply(void *p,HeapOps op);\n};\n");
		

}

void
InterfaceType::print_sdl_dcl() const
// similar to struct, but need inheritance...
{
	char *list_prefix="{\n"; // default prefix before printing dcl list
	char buf[200];
	cur_interface_name = (char *)name;
	if (cxx_mode)
		fprintf(bf,"class %s ",(char *)name);
	else
		fprintf(bf,"interface %s ",(char *)name);
	if (Bases!= 0) // there is a base list to print
		print_sdl_dcl_list(Bases,":",","," ");
	else // make class derive from sdlObj
		fprintf(bf," : public sdlObj ");
	if (cxx_mode)
	{
		sprintf(buf,"{\nCOMMON_FCT_DECLS(%s)\n",(char *)name);;
		list_prefix=buf;
	}
	print_sdl_dcl_list(Decls,list_prefix,";\n",";\n}",1);
}

void
sdlClassType::print_sdl_dcl() const
{
		fprintf(bf,"class %s ",(char *)name);
}

void
sdlUnionType::print_sdl_dcl() const
// union is a little unnatural: format is
// union <name> switch ( <tagtype> ) { <case_list> }
{
	// c++ implementaton: struct containing heap space manager,
	// and  tag; allocate space for variants as they are used.
	if (TagDecl==0) // forward decl/external
	{
		fprintf(bf," union %s ",(char *)name);
		return;
	}

	if (cxx_mode)
	{
		// the following should perhaps be macroized...

		// declare a struct; first field is the tag variable, which is
		// declared const to avoid accidental update.
		char * uname = (char *)name;
		fprintf(bf,"struct %s : sdl_heap_base {\n%s(){}\n",
			uname,uname);
		fprintf(bf,"%s(const %s & arg) { *this = arg;}\nconst %s & operator=(const %s &);\nconst "
			,uname,uname,uname,uname);
		TagDecl->type->print_sdl_var(TagDecl->name);
		fprintf(bf,";\nint _armi( ) const;\n");
		// _armi returns the index of the arm corresponding to the
		// current tgag value, or -1 if invalid.

		// set_function  for tag: clear space if the new tag indicates
		// a different arm, cast away const of the tag field and
		// reset it.
		// const tagtype tag;
		// void set_tag( tagtype tva) { 
		//   int old_armi = _armi();
		//   (tag_type &)tag = tva;
		//	 if (old_armi  != _armi() __free_space();
		// }
		fprintf(bf,"void set_utag(");
		TagDecl->type->print_sdl_var(" _tg_val");
		fprintf(bf, "){int old_arm =_armi();\n(");
		TagDecl->type->print_sdl_var("&");
		fprintf(bf,")%s = _tg_val;\n",(char *)TagDecl->name);
		fprintf(bf,"if (old_arm!= _armi()) __free_space();}\n");

		// print out set, for tag name just for consistentcy
		fprintf(bf,"void set_%s(",(char *)TagDecl->name);
		TagDecl->type->print_sdl_var(" _tg_val");
		fprintf(bf, "){set_utag(_tg_val);}\n");
		// also add in get_utag
		TagDecl->type->print_sdl_var("");
		fprintf(bf,"get_utag() const { return %s;}\n",(char *)TagDecl->name);
		TagDecl->type->print_sdl_var("");
		fprintf(bf,"get_%s() const { return %s;}\n",(char *)TagDecl->name,(char *)TagDecl->name);



		fprintf(bf,"void __apply(HeapOps op);\n");
		int i =  0;
		Ref<ArmDecl> apt;
		for ( apt = ArmList; 
			apt != NULL; 
			apt = (Ref<sdlArmDecl> &) apt->next,++i)
		{
			char buf[200];
			// special case for array fields...
			if (apt->type->tag==Sdl_array)
			{
				Ref<sdlArrayType> art = (Ref<sdlArrayType> &)apt->type;

				// the set function
				sprintf(buf,"(* set_%s())",(char *)apt->name);
				art->elementType->print_sdl_var(buf);
				fprintf(bf,"{ assert(_armi()==%d);\n",i);
				// check for space allocated
				// fprintf(bf,"if (!space) __set_space(sizeof(");
				// apt->type->print_sdl_var(" ");
				// fprintf(bf,"));\nreturn(");
				// to avoid making the declaration dependent on
				// the arm type, don't uses sizeof.
				fprintf(bf,"if (!space) __set_space(%d);\nreturn(",
					art->size);
				art->elementType->print_sdl_var(" (*) ");
				fprintf(bf,")space;}\nconst");

				// the get function
				sprintf(buf,"(* get_%s() const)",(char *)apt->name);
				art->elementType->print_sdl_var(buf);
				fprintf(bf,"{assert(_armi()==%d);\n",i);
				fprintf(bf,"return (",i);
				art->elementType->print_sdl_var("(*)");
				fprintf(bf,")space;}\n");
			}
			else
			{

				apt->type->print_sdl_var(" "); //null print
				// this is somewhat bogus...
				fprintf(bf,"& set_%s() {\n",(char *)apt->name);
				fprintf(bf,"assert(_armi()==%d);\n",i);
				fprintf(bf,"if (!space) __set_space(%d);\n",
					apt->type->size);
				fprintf(bf,"\nreturn *(");
				apt->type->print_sdl_var(" *  ");
				fprintf(bf,")space ;}\n",(char *)name);

				fprintf(bf,"const ");
				apt->type->print_sdl_var(" "); //null print
				// this is somewhat bogus...
				fprintf(bf,"& get_%s() const {\n",(char *)apt->name);
				fprintf(bf,"assert(_armi()==%d);\n",i);
				// fix to make this const..
				fprintf(bf,"return  *(const ");
				apt->type->print_sdl_var(" * ");
				fprintf(bf,") space;}\n");
			}
		}	
	}
	else
	{
		fprintf(bf,"union %s switch (%s) {\n",(char *)name,(char *)TagDecl->type->name);
		print_sdl_dcl_list((Ref<Declaration> &)ArmList,"const ",";\nconst ",";\n");
	}
	fprintf(bf,"};\n");
}

void 
RefType::print_sdl_dcl() const
// this is used for a family of types.
// note: the element type must allways be a named type??
{

	if (cxx_mode)
	// c++ mode still uses macros
	// oops, only use macro for LREF?
	{
		switch(tag) {
		case Sdl_lref:
			fprintf(bf," %s(%",tag_to_cxx[tag]);
			elementType->print_sdl_var("");
			fprintf(bf,") ");
		break;
		case Sdl_bag:
			TemplatesUsed.add(this->elementType->get_dtype(Sdl_bag));
		case Sdl_set:
			TemplatesUsed.add(this->elementType->get_dtype(Sdl_set));
		case Sdl_sequence:
			TemplatesUsed.add(this->elementType->get_dtype(Sdl_sequence));
		default: // always print inner type as a declaration; forget string.
			fprintf(bf," %s<",tag_to_cxx[tag]);
			elementType->print_sdl_var("");
			fprintf(bf,"> ");

		}
	}
	else
	{
		fprintf(bf," %s<",tag_to_string[tag]);
		elementType->print_sdl_dcl();
		fprintf(bf," > ");
	}
}

void
IndexType::print_sdl_dcl() const
// this is used for a family of types.
// note: the element type must allways be a named type??
{ 	// some (hopefully) temporary hacks:
	// special case for string keys and ref values.
	char *Imacro;
	// yuck, what if val is a string??
	// if (keyType->tag==Sdl_struct!!keyType->val
	// unified version.
	// now, just check for object types-> transform to refs if so.
	fprintf(bf,"Index<");
	keyType->print_sdl_var("");
	fprintf(bf,",");
	elementType->print_sdl_var("");
	fprintf(bf," > ");
}

void
sdlArrayType::print_sdl_var(CString vname) const
// this is a bit messy because of multidimensional arrays.
// first, print out the lhs by finding the first non-array
// element type; then, print out the name; then, print out
// the dimension list.
{
	Ref<Type> bpt =  this;
	while (bpt->tag == Sdl_array)
		bpt = ((Ref<ArrayType> &)(bpt))->elementType;
	bpt->print_sdl_var(vname);
	// now, tack on the dimension list
	Ref<Type> apt = this;
	while (apt->tag == Sdl_array)
	{
		Ref<ArrayType> apt2 = (Ref<ArrayType> &)apt;
		fprintf(bf,"[");
#ifdef nofold
		if (apt2->dim_expr != 0)
			apt2->dim_expr->print_sdl();
		else
#endif
			fprintf(bf,"%d",apt2->dim);
		fprintf(bf,"]");
		apt = apt2->elementType;
	}
}
void
sdlType::print_sdl_var(CString vname) const
// print out a declaration for "this" type with the given name.
// this could be pushed down to the subtypes, but it's pretty
// simple to do it here.
{
	switch(tag)
	{
		// first, builtin types
		case Sdl_char:
		case Sdl_short:
		case Sdl_long:
		case Sdl_float:
		case Sdl_double:
		case Sdl_boolean:
		case Sdl_octet:
		case Sdl_unsigned_short:
		case Sdl_unsigned_long:
		case Sdl_void:
		case Sdl_pool:
		case Sdl_any: // a somewhat bogus addition
			if (cxx_mode)
			// name may be different
			{
				fprintf(bf," %s %s ",tag_to_cxx[tag],vname);
				break;
			}
				
		// constructed types always have names..
		case Sdl_enum:
		case Sdl_struct:
		case Sdl_union:
		case Sdl_Class:
		case Sdl_CUnion:
		// these may be forward declared...
		{ 	char buf[200];
			if (myDecl != 0)
			{
				get_scoped_name(buf, myDecl);
				fprintf(bf," %s %s %s ",tag_to_cxx[tag],buf, vname);
			}
			else
				fprintf(bf," %s %s %s ",tag_to_cxx[tag],(char *)name, vname);
		}
		break;
		case Sdl_NamedType:
		{ 	char buf[200];
			get_scoped_name(buf, myDecl);
			fprintf(bf," %s %s ",buf, vname);
		}
			
		case Sdl_interface:
		case Sdl_ExternType:
			fprintf(bf," %s %s ",(char *)name, vname);
		break;
		// the following all have the same form; we
		// temporarily make them all ref types.
		// we can print the declarations for the
		// following types...
		case Sdl_ref:
		case Sdl_lref:
		case Sdl_set:
		case Sdl_bag:
		case Sdl_list:
		case Sdl_multilist:
			((RefType *)this)->print_sdl_dcl();
			fprintf(bf," %s ",vname);
		break;
		case Sdl_Index:
			((IndexType *)this)->print_sdl_dcl();
			fprintf(bf," %s ",vname);
		break;
		case Sdl_array:
			((ArrayType *)this)->print_sdl_var(vname);

		break;
		default:
		{
			fprintf(bf,"tag %s not handled\n",tag_to_string[tag]);
		}
	}
}

void
sdlSequenceType::print_sdl_var(CString vname) const
{
	if (tag==Sdl_string || tag==Sdl_text)
	// special cased.
	{
		if (cxx_mode)
		{
			fprintf(bf," %s %s ", tag_to_cxx[tag],vname);
		}
		else // sdl mode
		{
			if (((sdlSequenceType *)this)->dim_expr != 0)
			{
				fprintf(bf," %s < ",tag_to_string[tag]);
				((sdlSequenceType *)this)->dim_expr->print_sdl();
				fprintf(bf,">");
			}
			else
				// this must be a string
					fprintf(bf," %s ",tag_to_string[tag]);
				
			fprintf(bf,"%s ",vname);
		}
	}
	else
	{
		TypeTag tptag = elementType->tag;
		fprintf(bf,"%s<",tag_to_cxx[tag]);
		elementType->print_sdl_var(" ");
		fprintf(bf,"> %s",vname);
		TemplatesUsed.add(this);
	}
}
	
void
sdlType::print_sdl_dcl() const
// print out the declaration of the type in sdl format.
// again, this should properly be based on virtual function
// dispatch...
{
	switch(tag)
	{
		// first, builtin types
		case Sdl_char:
		case Sdl_short:
		case Sdl_long:
		case Sdl_float:
		case Sdl_double:
		case Sdl_boolean:
		case Sdl_octet:
		case Sdl_unsigned_short:
		case Sdl_unsigned_long:
			if (cxx_mode)
				fprintf(bf,"%s",tag_to_cxx[tag]);
			else
				fprintf(bf,"%s",(char *)name);
		break;
		case Sdl_ExternType:
		// nothing to do
			;
		break;
		default:
		{
			fprintf(bf,"tag %s not handled\n",tag_to_string[tag]);
		}
	}
}

void
ExprNode::print_sdl() const
// print a constant expression in sdl format.
{
#ifdef orig
	if (!this) return;
	switch(etag) {
	case EError: // undefined; do nothing
		break;
	case CName: 
	case Literal: 
		fprintf(bf," %s ",(char *)imm_value);
	break;
	case CDefault:
		fprintf(bf,"%s",exprop_to_string[etag]);
	break;
	// the following are binary or (maybe) unary exprs.
	case Plus: case Minus: case Mult: case Div:
	case ModA:
	case Or:
	case And:
	case Xor:
	case Complement:
	case LShift:
	case RShift:
		if (e1 != 0)
			e1->print_sdl();
		fprintf(bf,"%s",exprop_to_string[etag]);
		e2->print_sdl();
	break;
	}
#endif
}

void
sdlLitConst::print_sdl() const
// print a constant expression in sdl format.
{
	if (!this) return;
	fprintf(bf," %s ",(char *)imm_value);
}

void
sdlConstName::print_sdl() const
// print a constant expression in sdl format.
{
	if (!this) return;
	fprintf(bf," %s ",(char *)name); // dubious...
}

extern char* aqua_names[];
void
sdlArithOp::print_sdl() const
// print a constant expression in sdl format.
{
	if (!this) return;
	if (e1 != 0)
		e1->print_sdl();
	if (etag==Dot && aop != a_nil) // proxy for oql
		fprintf(bf,"aqua op %d",aop);
	else
		fprintf(bf,"%s",exprop_to_string[etag]);
	if (e2!=0)
		e2->print_sdl();
}

void
sdlSelectExpr::print_sdl() const
// print a constant expression in sdl format.
{
	if (!this) return;
	fprintf(bf,"select ");
	Ref<sdlDeclaration> d;
	for (d=ProjList; d!=0; d = d->next)
		d->print_sdl();

	if (RangeList != 0)
	{
		fprintf(bf,"\n\tfrom %s in ",RangeList->name.string());
		RangeList->expr->print_sdl();
		Ref<sdlConstDecl> more;
		more.assign(RangeList->next);
		while (more != 0)
		{
			fprintf(bf,", %s in ",more->name.string());
			more->expr->print_sdl();
			more.assign(more->next);
		}
	}
	
	if (Predicate != 0)
	{
		fprintf(bf,"\n\twhere ");
		Predicate->print_sdl();
	}
	fprintf(bf,";\n");

}

void
sdlFctExpr::print_sdl() const
{
	// ignore arg- body should be sufficient
	body->print_sdl();
}
void
sdlProjectExpr::print_sdl() const
{
	// type should be a struct; maybe;
	// initializers should be an expr list 
	fprintf(bf, " struct (");
	Ref<sdlDeclaration> mpt;
	Ref<sdlStructType> sref;
	sref = (Ref<sdlStructType> &)type;
	int i = 0;
	//for (i = 0, mpt = sref->members; 
		//i<initializers.get_size() && mpt!= 0; 
		//i++, mpt = mpt->next)
	Ref<sdlConstDecl> init;
	for (init = initializers; init != 0; init = (Ref<sdlConstDecl> &)(init->next))
	{
		//Ref<sdlExprNode> init = initializers.get_elt(i);
		// prepend , if i>0
		// fprintf(bf,"%c %s:",i?',':' ',init->name.string());
		init->expr->print_sdl();
		i = 1;
	}
	fprintf(bf, ")");
		
}



// C++ printing section. This works exactly analogous to print_sdl
// functions; in some cases we directly forward the call.
void
ExprNode::print_cxx() const
// currently, not distinguishable from print_sdl
{
	print_sdl();
}
	
void
sdlArithOp::print_cxx() const
// currently, not distinguishable from print_sdl
{
	print_sdl();
}
	
void
sdlLitConst::print_cxx() const
// currently, not distinguishable from print_sdl
{
	print_sdl();
}
	
void
sdlConstName::print_cxx() const
// currently, not distinguishable from print_sdl
{
	print_sdl();
}
	
void
sdlSelectExpr::print_cxx() const
// currently, not distinguishable from print_sdl
{
	print_sdl();
}
	

void
Declaration::print_cxx() const
// print a declaration in C++ format
// the subtypes of Declaration that are structurally different have
// explicit dispatch to a class - specific print_cxx here; we don't
// use virtual functions in order to make compiled-in initialization
// easier.
{
	switch(kind) {
	case Constant:
	// no differnece for c++/
		((ConstDecl *)this)->print_sdl();
	break;
	case TypeName:
	// first, check for struct/union/interface dcl
		if (type->name.string() && !strcmp((char *)name, (char *)type->name))
			type->print_sdl_dcl();
		else
		{
			fprintf(bf,"typedef ");
			type->print_sdl_var((char *)name);
		}
	break;
	case Member:  // structure member
		type->print_sdl_var((char *)name);
	break;
	case Arm: // union arm
		((ArmDecl *)this)->print_sdl();
	break;
	case Attribute: // interface attribute
		((AttrDecl *)this)->print_sdl();
	break;
	case Op:
		((OpDecl *)this)->print_sdl();
	break;
	case OpOverride:
		fprintf(bf,"override %s\n",(char *)name);
	break;
	case Param:
		((ParamDecl *)this)->print_sdl();
	break;
	case Mod:
		((ModDecl *)this)->print_sdl();
	break;
	case BaseType:
		fprintf(bf,"%s %s ",zone_to_string[zone],(char *)name); 
		// really need scoped name
		// always print zone.
	break;
	case EnumName:
		fprintf(bf,"%s ",(char *)name); // really need scoped name
	break;
	case Exception:
		; // ignore temporarily
	break;
	case InterfaceName:
		fprintf(bf,"interface %s ",(char *)name);
	break;
	default:
		fprintf(bf,"unknown/unhandled decl kind %d\n",kind);
		
	}
}

// the following section handles compiled-in support for swizzling.
long
sdlType::has_refs() const
// return 1/0 if the given type contains references that need to
// be swizzled.  This is a bit expensive
{
	Ref<Declaration> dpt;
	switch(tag) {
	case Sdl_ref:
		return 1;
	break;
	case Sdl_struct:
	{
		for( dpt= ((StructType *)this)->members; dpt != 0; dpt=dpt->next)
		{
			if (dpt->type->has_refs())
				return 1;
		}
		return 0;
	}
	break;
	case Sdl_array:
	{
		Ref<ArrayType> apt = (ArrayType *)this;
		return apt->elementType->has_refs();
	}
	break;
	case Sdl_interface:
	{
		Ref<InterfaceType> apt = (InterfaceType *)this;
		for (dpt = apt->Bases; dpt != 0; dpt= dpt->next)
			if (dpt->type->has_refs())
				return 1;
		for (dpt = apt->Decls; dpt != 0; dpt= dpt->next)
		{
			if (dpt->kind==Attribute && dpt->type->has_refs())
				return 1;
			if (dpt->kind == Relationship)
				return 1;
		}
		return 0;
	}
	// should do something about typedefs...
	default:
		return 0;
	}
	return 0;
}
			
// the following section handles compiled-in support for swizzling.
long
sdlType::has_heapsp() const
// return 1/0 if the given type contains references that need to
// be swizzled.  This is a bit expensive
{
	Ref<Declaration> dpt;
	switch(tag) {
	case Sdl_string:
	case Sdl_set:
	case Sdl_bag:
	case Sdl_text:
	case Sdl_Index:
	case Sdl_sequence:
		return 1;
	break;
	case Sdl_struct:
	{
		for( dpt= ((StructType *)this)->members; dpt != 0; dpt=dpt->next)
		{
			if (dpt->type->has_heapsp())
				return 1;
		}
		return 0;
	}
	break;
	case Sdl_array:
	{
		Ref<ArrayType> apt = (ArrayType *)this;
		return apt->elementType->has_heapsp();
	}
	break;
	case Sdl_interface:
	{
		Ref<InterfaceType> apt = (InterfaceType *)this;
		for (dpt = apt->Bases; dpt != 0; dpt= dpt->next)
			if (dpt->type->has_heapsp())
				return 1;
		for (dpt = apt->Decls; dpt != 0; dpt= dpt->next)
			if (dpt->kind==Attribute && dpt->type->has_heapsp())
				return 1;
		return 0;
	}
	case  Sdl_union: // always has heap sp.
		return 1;

	// should do something about typedefs...
	default:
		return 0;
	}
	return 0;
}
			
			
			
void
print_apply(CString vname, Ref<Type> tp)
// print an _apply thing for an interface/struct attribute.
// if the attribute is a ref, print the name directly;
// normally, just call __apply for the field, but we loop through
// array attributes here.
{
	char pref_buf[200];
	TypeTag t = tp->tag;
	switch(t)
	{
		case Sdl_ref:
		case Sdl_string:
		case Sdl_text:
		case Sdl_set:
		case Sdl_bag:
		case Sdl_Index:
		case Sdl_struct:
		case Sdl_sequence:
		case Sdl_union:
			fprintf(bf,"%s.__apply(op);\n",vname);
		break;
		// if other primitive types need this, do it here.
		break;
		case Sdl_array: 
		{
			// fairly bogus hack: make a fake Declaration
			// which has a name hacked in from the
			// index and inherits the element type.
			char ibuf[20];
			char dname[200];
			Ref<ArrayType> apt = (Ref<ArrayType> &)tp;
			sprintf(ibuf,"i%d",strlen(vname));
			sprintf(dname,"%s[%s]", vname,ibuf);
			// declare a loop to wrap the subapply into.
			// the following section prints a for loop header:
			// { int i10; for (i10=0;i10<dim;i10++) { ... }} 
			// where ... is replace by a recursif call to
			// print_attr apply with a suitable fake dec.
			fprintf(bf,"{ int %s; for (%s=0; %s<",
					ibuf,ibuf,ibuf);
			// temp. hack: print dim expr literally if it's
			// there; we should really demand folded expr
			// here.
			if (apt->dim_expr != 0)
				apt->dim_expr->print_sdl();
			else if (apt->dim)
				fprintf(bf,"%d",apt->dim);
			else 
				fprintf(bf,"0");
			fprintf(bf,";++%s) {\n", ibuf);
			print_apply(dname,apt->elementType);
			fprintf(bf,"}}\n");
		}
		break;
		// should do something with union here...
		default:
			;
		// nothing to to
	}
}
			
		      
	
// C++ printing section. This works exactly analogous to print_sdl
// functions; in some cases we directly forward the call.
void
InterfaceType::print_cxx_support() const
// print out support code for c++; this is largely macro-encapsulated
// but needs some type knowledge.
{
	Ref<Declaration> dpt; // used to iterated bases and decls.
	cur_interface_name = (char *)name; // el tacko, needed in RelDecl::..
	// fprintf(bf,"INTERFACE_CODEDEFS(%s,%d,%s)\n",(char *)name, numIndexes,
	//	Bases!=0? (char *)(Bases->name): "any"); // name - dependent portion
	// INTERFACE_CODEDEFS got too big, so print its components instead.
	sdl_string base_name;
	if (Bases!= 0)
		base_name = Bases->name;
	else
		base_name = "any";
	// ctor-related fcts.
	fprintf(bf,"SETUP_VTAB(%s,%d)\n",name.string(),numIndexes);
	fprintf(bf,"NEW_PERSISTENT(%s)\n",name.string());
	fprintf(bf,"CLASS_VIRTUALS(%s)\n",name.string());
	fprintf(bf,"template class BRef<%s,Ref<%s> >;\n",name.string(),base_name.string());
	fprintf(bf,"template class WRef<%s>;\n",name.string());
	fprintf(bf,"template class Apply<Ref<%s> >;\n",name.string());
	fprintf(bf,"template class RefPin<%s>;\n",name.string());
	fprintf(bf,"template class WRefPin<%s>;\n",name.string());
	fprintf(bf,"template class srt_type<%s>;\n",name.string());
	fprintf(bf,"TYPE(%s) TYPE_OBJECT(%s);\n",name.string(),name.string());

	// next, do the TYPE(T)::cast definiton
	fprintf(bf,"TYPE_CAST_DEF(%s)\n",(char *)name);
	// for each base class of this interface, need to defin
	// a case.
	for (dpt = Bases; dpt != 0; dpt = dpt->next)
		fprintf(bf,"TYPE_CAST_CASE(%s,%s)\n",
			(char *)name,(char *)dpt->name);
	fprintf(bf,"TYPE_CAST_END(%s)\n",(char *)name);

	// next, do the _apply definition: a little trickier.
	//fprintf(bf,"APPLY_DEF(%s)\n",(char *)name);
	if (has_refs() ||has_heapsp())
	{
		fprintf(bf,"void %s::__apply(HeapOps op) {\n",(char *)name);
		// again, for each base class of this interface, need to define
		// a call to the base instance.
		for (dpt = Bases; dpt != 0; dpt = dpt->next)
			if (dpt->type->has_refs() || dpt->type->has_heapsp())
			{
				fprintf(bf,"\t%s::__apply(op);\n",(char *)dpt->name);
			}
		
		// next, need to walk through the attributes printing
		// the calls.  For attributes that are refs, print the
		// apply call  directly; for structs and arrays, call
		// some support code.
		for (dpt = Decls; dpt != 0; dpt = dpt->next)
		{
			if((dpt->kind==Attribute ||dpt->kind==Relationship)
				&& (dpt->type->has_refs() || dpt->type->has_heapsp()) )
			{
				print_apply((char *)dpt->name,dpt->type);
			}
		}
		fprintf(bf,"} // end %s::__apply\n",(char *)name);
	}
	else // else, empty apply..
		fprintf(bf,"void %s::__apply(HeapOps ) {}\n",(char *)name);

	// next, do mini-heap managment.  This works simliar to ref_apply
	// for refs; 
	// next, we need to print out any relationship implementations.
	for (dpt = Decls; dpt != 0; dpt = dpt->next)
	{
		dpt->print_cxx_support();
	}
	sdlType::print_cxx_support(); // for other general stuff
}


// other print_cxx supports.
void
sdlDeclaration::print_cxx_support() const
// nothing here by default.
{
}

void 
sdlTypeDecl::print_cxx_support() const
// print type specific support here.
{
	if (type!=0)
		type->print_cxx_support();
}

void
sdlType::print_cxx_support() const
// nothing here by default now.
{ }

void
sdlRelDecl::print_cxx_support() const
{
	if (inverseDecl !=0) // need stuff.
	{
		char * mstr; // formating string for type of relation
		Ref<RefType> thisType = (Ref<RefType> &)(type);
		char * inv_tstr= 0;
		if (thisType->elementType->name.string() != NULL)
			inv_tstr = thisType->elementType->name;
		else // presumably, set<ref<t>>
		{
			inv_tstr = ((Ref<RefType> &)(thisType->elementType))->elementType->name;
		}
		// yuk

		TypeTag tptag = type->tag;
		switch(tptag)
		{
		case Sdl_ref:
			mstr = "REF_INV_IMPL(%s,%s,%s,%d)\n";
		break;
		case Sdl_set:
			mstr = "SET_INV_IMPL(%s,%s,%s,%d)\n";
		break;
		case Sdl_bag:
			mstr = "BAG_INV_IMPL(%s,%s,%s,%d)\n";
		break;
		}
		fprintf(bf, mstr // macro name + format
			,inv_tstr // type of inverse obj obj
			,(char *)inverseDecl->name // name of inv field
			,(char *)cur_interface_name // type name of this interface.
			,offset  // offset for set/bag if neccessary
		);
	}
}

void
sdlStructType::print_cxx_support() const
{
	// we need to print the definition of the static
	// external sdl_apply function here.
	// this does not handle scoped names properly...
	char dbuf[200];
	get_scoped_name(dbuf,myDecl);
	if (members==0)	// forward decl; don't do anything
		return;
	fprintf(bf,"\nvoid %s::sdl_apply(void *p,HeapOps op)\n",dbuf);
	fprintf(bf,"{ ((%s *)p)->__apply(op);};\n",(char *)name);
	fprintf(bf,"template class Apply<%s>;\n",dbuf);
	sdlType::print_cxx_support(); // for other general stuff
}

void
sdlUnionType::print_cxx_support() const
// there should be something here analagous to struct..
{
	// set the space pointer...
	char dbuf[200];
	get_scoped_name(dbuf,myDecl);
	fprintf(bf,"void %s::__apply(HeapOps op)\n{\n",dbuf);
	fprintf(bf,"sdl_heap_base::__apply(op);\n");
	fprintf(bf, "if (space != 0) switch(%s){\n",(char *)TagDecl->name); 
	// apply  the correct cast to the space elt, then call the elt
	// apply method if there is one. For now, we will end up
	// writing out the max space ever used for the union; we may
	// want to modify this to trim space in the future.
	Ref<ArmDecl> apt;
	for ( apt = ArmList; 
		apt != NULL; 
		apt = (Ref<sdlArmDecl> &) apt->next)
	{
		Ref<sdlExprNode> cpt = apt->CaseList;
		while (cpt != 0) // may be a list
		{
			Ref<sdlExprNode> ept;
			if (cpt->etag==Comma)
			{
				Ref<sdlArithOp> lpt = (Ref<sdlArithOp> &)cpt;
				ept = lpt->e1; // the actual expr
				cpt = lpt->e2;
			}
			else // end of list..
			{
				ept = cpt;
				cpt = 0;
			}
			fprintf(bf,"case ");
			ept->print_cxx();
			fprintf(bf,":\n");
			if (apt->type->has_refs() ||apt->type->has_heapsp())
			{
				if (apt->type->tag==Sdl_array)
				// special case this to handle pointers properly.
				{
					Ref<sdlArrayType> art = (Ref<sdlArrayType> &)apt->type;
					fprintf(bf,"{ ");
					art->elementType->print_sdl_var("(* tmp)");
					fprintf(bf," = (");
					art->elementType->print_sdl_var(" (*) ");
					fprintf(bf,")space;\n");
					print_apply("tmp",apt->type);
					fprintf(bf,"; return;}");
				}
				else // other types: just call apply directly on cast space.
				{
					fprintf(bf,"((");
					apt->type->print_sdl_var(" *  ");
					fprintf(bf,")space)->__apply(op); return ;\n");
				}
			}
			else
				fprintf(bf,"return;\n");
		}
	}
	fprintf(bf,"default: return;\n}}\n");

	// next print the _armi function
	// which returns an arm index corresponding to the current tag value.
	// at this point this may be simple enough to inline..
	fprintf(bf,"int %s::_armi( ) const\n{ switch(%s) {\n",
		dbuf, (char *)TagDecl->name);
	int i;
	for ( apt = ArmList, i=0; 
		apt != NULL; 
		apt = (Ref<sdlArmDecl> &) apt->next,++i)
	{
		Ref<sdlExprNode> cpt = apt->CaseList;

		while (cpt != 0) // may be a list
		{
			Ref<sdlExprNode> ept;
			if (cpt->etag==Comma)
			{
				Ref<sdlArithOp> lpt = (Ref<sdlArithOp> &)cpt;
				ept = lpt->e1; // the actual expr
				cpt = lpt->e2;
			}
			else // end of list..
			{
				ept = cpt;
				cpt = 0;
			}
			fprintf(bf,"case ");
			ept->print_cxx();
			fprintf(bf,":");
		}
		// for these values of tag, arm index is i.
		fprintf(bf,"return(%d);\n",i);
	}
	fprintf(bf,"default: return -1;}}\n");

	// now do assignment operator and ctor.
	char * uname = name;
	fprintf(bf,"const %s & %s::operator=(const %s  &arg ) {\n",
		dbuf, dbuf,uname);
	fprintf(bf,"set_utag(arg.get_utag());\n switch(%s) {\n",
		(char *)TagDecl->name);
	for ( apt = ArmList, i=0; 
		apt != NULL; 
		apt = (Ref<sdlArmDecl> &) apt->next,++i)
	{
		Ref<sdlExprNode> cpt = apt->CaseList;

		while (cpt != 0) // may be a list
		{
			Ref<sdlExprNode> ept;
			if (cpt->etag==Comma)
			{
				ept = ((Ref<sdlArithOp> &)cpt)->e1; // the actual expr
				cpt = ((Ref<sdlArithOp> &)cpt)->e2; // the actual expr
			}
			else // end of list..
			{
				ept = cpt;
				cpt = 0;
			}
			fprintf(bf,"case ");
			ept->print_cxx();
			fprintf(bf,":");
		}
		fprintf(bf,"set_%s() = arg.get_%s(); break;\n",(char *)apt->name,
			(char *)apt->name);
		// for these values of tag, arm index is i.
	}
	fprintf(bf,"default: sdl_heap_base::__clear();}\n");
	fprintf(bf,"return *this;}\n");
	fprintf(bf,"template class Apply<%s>;\n",dbuf);
	// t(t&) ctor just calls operator=, was inline.
	// sdlType::print_cxx_support(); // for other general stuff
}

void
sdlEnumType::print_cxx_support() const
// we need an Apply thing in case somebody does something silly
// (or for general usage of sets/perhaps.)
{
	char dbuf[200];
	get_scoped_name(dbuf,myDecl);
	//fprintf(bf,"NO_APPLY(%s)\n",dbuf);
	fprintf(bf,"template class noappIndVal<%s>;\n",dbuf);

}

void
sdlDeclaration::print_access_fcts() const
{
	// nothing for now
}

void
sdlArmDecl::print_access_fcts() const
// hack interface: make a set fct that returns a (C++) ref and
// resets the tag variable.  This is a bit bogus.
// things that need to be fixed:
// 1. figure out something inteligent to do with case list vs.
// single case.
// 2. type check case list.
{
	char buf[200];
	type->print_sdl_var(" "); //null print
	// this is somewhat bogus...
	fprintf(bf,"& set_%s() {\n",(char *)name);
	fprintf(bf,"tag_val = ");
	// fix to make this const..
	CaseList->print_cxx();
	fprintf(bf,";\n return (");
	type->print_sdl_var(" ");
	fprintf(bf," & ) %s;}\n",(char *)name);
}

	

	
// moved from node.C for want of a better place.
void
Declaration::ApplyBaseType(Ref<Type> tpt)
// The declaration may have no type set, or it may have an incomplete
// array type specification.  If it has no type, tpt becomes the type;
// if it's an array, tpt becomes the base type of the array; else.
// something is wrong.
{
	if (this->type == 0) // null
		this->type = tpt;
	else if (this->type->tag==Sdl_array)
	{
		Ref<ArrayType>  apt = (Ref<ArrayType> &)(this->type);
		while (apt->elementType!= 0 &&
		       apt->elementType->tag==Sdl_array)
		    apt = (Ref<ArrayType> &)(apt->elementType);
		apt.update()->elementType = tpt;
	}
	else
		abort(); // unexpected
}


// the following section defines  generic implementations of
// sdl_apply, based on interpretation of metatype objects
// describing the type of an object.  The metatype representation
// of an object type is traversed along with a memory pointer to
// an instance of the object type.

// traversal of all metatypes  is based on going down the list of declarations 
// describing  a particulare type  object; hence we begin with sdl_apply
//  ops for all forms of declarations.  Each Declartion::sdl_apply function
// has 2 parameters, one indicating the operation to be done and another
// giving the memory address of an instance of the object within the
// declaration applies.  The general action is to compute the address
// of the data a  particular declaration referes to and pass it on
// to the type-specific sdl_apply function for the type of that data.

void
Declaration::sdl_apply(HeapOps op, void *dpt) const
// most declaraions are subclasses  of decl, but interface base classes
// and struct members are currently not subclassed from sdlDeclartion;
// they are handled here.
{
	switch (kind) {
	case BaseType: case Member:
	{
		// compute an address for the data of the particular declaraion
		// relative to the incoming address dpt
		caddr_t cpt = (caddr_t)dpt;
		cpt += offset;
		type->sdl_apply(op,cpt);
		break;
	}
	default: // nothing to do
		;
	}
}

// interface attribute (data member) declarations.
void
AttrDecl::sdl_apply(HeapOps op, void *dpt) const
{
	// alway do type apply here.
	caddr_t cpt = (caddr_t)dpt;
	cpt += offset;
	type->sdl_apply(op,cpt);
}


void
RelDecl::sdl_apply(HeapOps op, void *dpt) const
// for now, I think we can get away with just apply against the regular type..
{
	caddr_t cpt = (caddr_t)dpt;
	cpt += offset;
	type->sdl_apply(op,cpt);
}

void
sdlType::sdl_apply(HeapOps op, void *dpt) const
{
}

void
sdlStructType::sdl_apply(HeapOps op, void *dpt) const
// here we walk down the elt list of the struct, applying to each dcl.
{
	Ref<Declaration> mpt;
	for (mpt = members; mpt !=0; mpt=mpt->next)
		mpt->sdl_apply(op,dpt);
}

void
CollectionType::sdl_apply(HeapOps op, void *dpt) const
{
	// this is not currently used...
}


void
sdlRefType::sdl_apply(HeapOps op, void *dpt) const
{
	// this actuall demultiplexes most things that have applies
	// into the underlying real runtime type.
	switch(tag)
	{
	case Sdl_ref:
		((OCRef *)dpt)->__apply(op);
	break;
	case Sdl_set:
	case Sdl_bag:
	// note: set,bag used to be special cased for refs, but not 
	// any more.
	case Sdl_sequence:
	{
		if (elementType->size==0)
			elementType.update()->compute_size();
		sdl_gen_set * hpt = (sdl_gen_set *)dpt;
		hpt->__apply(op);
		for (int i=0; i<hpt->num_elements; i++)
		{
			caddr_t elt_pt = hpt->space +(i*elementType->size);
			elementType->sdl_apply(op,elt_pt);
		}
	}
	break;
	}
	// any others here??
}

void
sdlArrayType::sdl_apply(HeapOps op, void *dpt) const
{
	caddr_t ar_pt = (caddr_t)dpt;
	switch (tag) {
	// for dubious reasons we've cheated and left strings as
	// an ArrayType.
	case Sdl_string:
	((sdl_string *)ar_pt)->__apply(op);
	break;
	case Sdl_text:
	((sdl_text *)ar_pt)->__apply(op);
	break;
	case Sdl_array:
	for (int i=0; i<dim; i++)
	{
		caddr_t elt_pt = ar_pt +(i*elementType->size);
		elementType->sdl_apply(op,elt_pt);
	}
	break;
	}
}


void
sdlSequenceType::sdl_apply(HeapOps op, void *dpt) const
// apply a
{
	switch(tag) {
	case Sdl_string:
	{
		((sdl_string *)dpt)->__apply(op);
		return;
	}
	case Sdl_text:
	{
		((sdl_text *)dpt)->__apply(op);
		return;
	}
	default:
	sdl_gen_set * hpt = (sdl_gen_set *)dpt;
	hpt->__apply(op);
	for (int i=0; i<hpt->num_elements; i++)
	{
		caddr_t elt_pt = hpt->space +(i*elementType->size);
		elementType->sdl_apply(op,elt_pt);
	}
	}
}


void 
InterfaceType::sdl_apply(HeapOps op, void *dpt) const
{
	Ref<Declaration> mpt;
	for (mpt = Bases; mpt !=0; mpt=mpt->next)
		mpt->sdl_apply(op,dpt);
	for (mpt = Decls; mpt !=0; mpt=mpt->next)
		mpt->sdl_apply(op,dpt);
}

void
IndexType::sdl_apply(HeapOps op, void *dpt) const
{
	((sdl_index_base *)dpt)->__apply(op);
}

void
sdlUnionType::sdl_apply(HeapOps op, void *dpt) const
// jeez, we should do unions some time...
{
}


// alternatively, use Apply(app_class * ) instead of sdl_apply...
void
Declaration::Apply(class app_class *arg) const
// most declaraions are subclasses  of decl, but interface base classes
// and struct members are currently not subclassed from sdlDeclartion;
// they are handled here.
{
#ifdef orig
	switch (kind) {
	case BaseType: case Member:
	{
		// compute an address for the data of the particular declaraion
		// relative to the incoming address dpt
		type->Apply(arg);
		break;
	}
	default: // nothing to do
		;
	}
#endif
	arg->action(this);
}

// interface attribute (data member) declarations.
void
AttrDecl::Apply(class app_class *arg) const
{
	arg->action(this);
}


void
RelDecl::Apply(class app_class *arg) const
// for now, I think we can get away with just apply against the regular type..
{
	arg->action(this);
}
void sdlModDecl::Apply(class app_class *arg) const { arg->action(this); }
// void sdlOpDecl::Apply(class app_class *arg) const { arg->action(this); }
void sdlParamDecl::Apply(class app_class *arg) const { arg->action(this); }
void sdlConstDecl::Apply(class app_class *arg) const { arg->action(this); }
void sdlTypeDecl::Apply(class app_class *arg) const { arg->action(this); }
void sdlExtTypeDecl::Apply(class app_class *arg) const { arg->action(this); }
void sdlArmDecl::Apply(class app_class *arg) const { arg->action(this); }
void sdlAliasDecl::Apply(class app_class *arg) const { arg->action(this); }

void
sdlType::Apply(class app_class *arg) const
{
	arg->action(this);
}

void
sdlStructType::Apply(class app_class *arg) const
// here we walk down the elt list of the struct, applying to each dcl.
{
#ifdef orig
	Ref<Declaration> mpt;
	for (mpt = members; mpt !=0; mpt=mpt->next)
		mpt->sdl_apply(op,dpt);
#endif
	arg->action(this);
}

void
CollectionType::Apply(class app_class *arg) const
{
	// this is not currently used...
	arg->action(this);
}


void
sdlRefType::Apply(class app_class *arg) const
{
#ifdef orig
	// this actuall demultiplexes most things that have applies
	// into the underlying real runtime type.
	switch(tag)
	{
	case Sdl_ref:
		((OCRef *)dpt)->__apply(op);
	break;
	case Sdl_set:
	case Sdl_bag:
	// note: set,bag used to be special cased for refs, but not 
	// any more.
	case Sdl_sequence:
	{
		if (elementType->size==0)
			elementType.update()->compute_size();
		sdl_gen_set * hpt = (sdl_gen_set *)dpt;
		hpt->__apply(op);
		for (int i=0; i<hpt->num_elements; i++)
		{
			caddr_t elt_pt = hpt->space +(i*elementType->size);
			elementType->sdl_apply(op,elt_pt);
		}
	}
	break;
	}
	// any others here??
#endif
	arg->action(this);
}

void
sdlArrayType::Apply(class app_class *arg) const
{
	arg->action(this);
#ifdef orig
	caddr_t ar_pt = (caddr_t)dpt;
	switch (tag) {
	// for dubious reasons we've cheated and left strings as
	// an ArrayType.
	case Sdl_string:
	((sdl_string *)ar_pt)->__apply(op);
	break;
	case Sdl_text:
	((sdl_text *)ar_pt)->__apply(op);
	break;
	case Sdl_array:
	for (int i=0; i<dim; i++)
	{
		caddr_t elt_pt = ar_pt +(i*elementType->size);
		elementType->sdl_apply(op,elt_pt);
	}
	break;
	}
#endif
}


void
sdlSequenceType::Apply(class app_class *arg) const
// apply a
{
	arg->action(this);
#ifdef orig
	switch(tag) {
	case Sdl_string:
	{
		((sdl_string *)dpt)->__apply(op);
		return;
	}
	case Sdl_text:
	{
		((sdl_text *)dpt)->__apply(op);
		return;
	}
	default:
	sdl_gen_set * hpt = (sdl_gen_set *)dpt;
	hpt->__apply(op);
	for (int i=0; i<hpt->num_elements; i++)
	{
		caddr_t elt_pt = hpt->space +(i*elementType->size);
		elementType->sdl_apply(op,elt_pt);
	}
	}
#endif
}


void 
InterfaceType::Apply(class app_class *arg) const
{
	arg->action(this);
#ifdef orig
	Ref<Declaration> mpt;
	for (mpt = Bases; mpt !=0; mpt=mpt->next)
		mpt->sdl_apply(op,dpt);
	for (mpt = Decls; mpt !=0; mpt=mpt->next)
		mpt->sdl_apply(op,dpt);
#endif
}

void
IndexType::Apply(class app_class *arg) const
{
	arg->action(this);
	//((sdl_index_base *)dpt)->__apply(op);
}

void
sdlUnionType::Apply(class app_class *arg) const
// jeez, we should do unions some time...
{
	arg->action(this);
}

void
Module::increment_refcount( long i)
{
	ref_count += i;
}


void print_decls(const Set<Ref<sdlModule> >   mods);
// print the C++ language binding code for some module.
void
sdlModule::print_cxx_binding() const
{

	int doing_metatypes = 0;
	Ref<sdlModule> mpt;
	mpt = get_ref();
	if (Unresolved.get_size()>0)
	{
		fprintf(stderr," can't print binding for %s: %d unresolved references\n",
			(char *)name, Unresolved.get_size());
	}
	//Set<Ref<sdlModule> > thisSet;
	//for (

	//thisSet.add(mpt);

	//Ref<sdlModule> dpt;
	//for (dpt= (Ref<sdlModDecl>&)(import_list); dpt != 0;
//			dpt= (Ref<sdlModDecl>&)(mpt->next))
//	{
//		if (dpt->dmodule==0) // ???
//			continue;
//		thisSet.add(dpt->dmodule);
//	}

//	print_decls(thisSet);
	// first get loid for the module
	LOID mod_oid;
	VolId Mod_volume;
	W_COERCE(mpt.get_primary_volid(Mod_volume));
	W_COERCE(mpt.get_primary_loid(mod_oid));

	if (!name.strcmp("metatypes"))
		doing_metatypes = 1;
	Ref<Declaration> dpt;
	fprintf(bf,"#ifndef %s_mod\n#define %s_mod 1\n",(char *)name,(char *)name);
	fprintf(bf,"#include \"ShoreApp.h\"\n");
	// print forward decls for interfaces
	// first, check for consistent versions by printing an external
	// variable name encodeing the serial # of the module, and making a
	// ref to it.
	fprintf(bf,"class sdlModule_%s 	{ public: virtual int oid_%d(); };\n",
		(char *)name,
		mod_oid.id.serial.guts._low);
	fprintf(bf,"static sdlModule_%s %s_header_version;\n", 
			(char *)name, (char *)name);

	for(dpt= decl_list; dpt!=0; dpt = dpt->next)
	{
		if (dpt->kind == TypeName 
			&& dpt->type->tag==Sdl_interface 
			// need to check if it's a typedef
			&&  (char *)dpt->type->name 
			&& !strcmp((char *)dpt->type->name,(char *)dpt->name))
		{
			// this used to be INTERFACE_PREDEFS, but forward class
			// decl is all thats left. Now reinstate.
			fprintf(bf,"INTERFACE_PREDEFS( %s);\n",(char *)dpt->name);
		}
	}
	// next, the class definitions themselves
	// lpt->print_sdl();
	for(dpt= decl_list; dpt!=0; dpt = dpt->next)
	{
		dpt->print_sdl();
		fprintf(bf,";\n");
	}

	// next, post-definition stuff
	for(dpt= decl_list; dpt!=0; dpt = dpt->next)
	{
		if (dpt->kind == TypeName && 
			dpt->type->tag==Sdl_interface
			// need to check if it's a typedef
			&&  (char *)dpt->type->name && !strcmp((char *)dpt->type->name,(char *)dpt->name))
		{
			// for want of a better place, print
			// out the "oid" here.:
			// metatype interfaces are registered objects; special
			// kludge for reserved names
			if (doing_metatypes)
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

	fprintf(bf,"struct rModule %s(\"%s\",%d,%d,%d );\n",
		(char *)name,(char *)name,
		Mod_volume.high,Mod_volume.low,
		mod_oid.id.serial.guts._low);
	fprintf(bf,"#define CUR_MOD %s\n",(char *)name);
	// define the per module version checking class.
	fprintf(bf,"int sdlModule_%s::oid_%d(){ return 0;};\n",
		(char *)name,
		mod_oid.id.serial.guts._low);
	// above is sleazy hack to get module name into other
	// macros.
	for(dpt= decl_list; dpt!=0; dpt = dpt->next)
	{
		dpt->print_cxx_support();
	}
	fprintf(bf,"#undef CUR_MOD %s\n",(char *)name);
	fprintf(bf,"\n#endif MODULE_CODE\n");
	fprintf(bf,"#endif %s\n",(char *)name);

}

extern int sdl_errors;

// error  msg handlers
// these are here to access printing statics.
void 
sdlNamedType::print_error_msg(sdl_string msg) const
// sdlNamedType is mostly used as a temporary standin 
{
		fprintf(stderr,"error in type %s, used at line %d:\n\t%s\n",name.string(),lineno,msg.string());
		++sdl_errors;
}
void
sdlType::print_error_msg(sdl_string msg) const
// just forward to the declaration.
{
	if (myDecl != 0)
		myDecl->print_error_msg(msg);
	else if (name.strlen() > 0)
	{
		fprintf(stderr,"error in type %s:  %s\n",name.string(),msg.string());
		++sdl_errors;
	}
	else
	// not much we can do... this should be a type of some kind,
	// since modules always have a decl, as do named types...
	{
		fprintf(stderr,"error in type %s:  %s\n",tag_to_string[tag],msg.string());
		++sdl_errors;
	}
}

void 
sdlDeclaration::print_error_msg(sdl_string msg) const
{
	FILE * sof = bf;
	bf = stderr;
	if (type != 0)
	{
		if (kind==TypeName)
			fprintf(stderr,"error in declaration of %s %s, ",
				tag_to_string[type->tag],name.string());
		else
			fprintf(stderr,"error in declaration of %s %s, ",
				DeclKind_desc[kind],name.string());
	}
	else
		fprintf(stderr,"error in  declaration of %s, ", name.string());
	Ref<sdlNameScope> scope_pt = scope;
	Ref<sdlModule> mod_pt = 0;
	while (scope_pt != 0)
	{
		if (scope_pt->myDecl == 0) break;
		if ( TYPE_OBJECT(sdlModule).isa(scope_pt)!= 0 )
		{
			mod_pt.assign(scope_pt);
			fprintf(stderr,"\n\tin module %s,src file %s, line %d\n",
				mod_pt->name.string(),mod_pt->src_file.string(),lineno);
			break;
		}
		fprintf(stderr,"\n\tin %s %s,",
			tag_to_string[scope_pt->myDecl->type->tag],
			scope_pt->myDecl->name.string());
		scope_pt = scope_pt->myDecl->scope;
	}
	if (mod_pt == 0)
	// couldn't get back to module, so print lineno anyway.
		fprintf(stderr,", line %d\n",lineno);
	if (type!= 0 )
	{	
		if (kind!=TypeName || name.strcmp(type->name))
		{
			fprintf(stderr,"declared as ");
			type->print_sdl_var(name.string());
		}
	}
	fprintf(stderr,"\n\t%s\n", msg.string());
	++sdl_errors;
	bf = sof;
}
void 
sdlExprNode::print_error_msg(sdl_string msg) const
{
	FILE * sof = bf;
	bf = stderr;
	fprintf(stderr,"error in expression at line %d: ",lineno);
	print_sdl();
	fprintf(stderr,"\n\t%s\n",msg.string());
	++sdl_errors;
	bf = sof;
}

void
sdlModule::print_error_msg(sdl_string msg) const
{
	FILE * sof = bf;
	bf = stderr;
	if (myDecl != 0)
		myDecl->print_error_msg(msg);
	else
		fprintf(stderr,"error in module %s %s\n",name.string(),  msg.string());
	bf = sof;
}
		
	
void
sdlType::print_val(ostream *opt,CString vpt) const
// print out a declaration for "this" type with the given name.
// this could be pushed down to the subtypes, but it's pretty
// simple to do it here.
{
	switch(tag)
	{
		// first, builtin types
		case Sdl_char:		*opt << 	*(	char *)vpt; break;
		case Sdl_short:		*opt <<		*(	short *)vpt; break;
		case Sdl_long:		*opt << 	*(	long *)vpt; break;
		case Sdl_float:		*opt << 	*(	float *)vpt; break;
		case Sdl_double:	*opt << 	*(	double *)vpt; break;
		case Sdl_boolean:	*opt << 	*(	bool *)vpt; break;
		case Sdl_octet:		*opt << 	*(	unsigned char *)vpt; break;
		case Sdl_unsigned_short:*opt <<	*(	unsigned short *)vpt; break;
		case Sdl_unsigned_long:*opt <<	*(	unsigned long *)vpt; break;
		// that's all by default?!
	}
}
void
sdlStructType::print_val(ostream *opt,CString valpt) const
// just run through things in sdl format.
{
	Ref<sdlDeclaration> dpt;
	opt->form("struct %s: values {\n",name.string());
	for (dpt = members; dpt != 0; dpt = dpt->next)
	{
		opt->form("%s:\t ",dpt->name.string());
		dpt->type->print_val(opt, valpt+dpt->offset);
		*opt << endl;
	}
	opt->form("} end struct %s\n",name.string());
}
void
sdlEnumType::print_val(ostream *opt,CString valpt) const
{
	int cval = *(int *)valpt; // enums are really just ints...
	int dnum = 0;
	
	if (consts==0) //forward decl..
	{
		// fprintf(bf,"enum %s;\n",(char *)name);
		// c++ forward decls aren't allowed...
		return;
	}
	Ref<sdlDeclaration> cpt;
	// get the n'th declarator
	for (cpt=consts, dnum=0; cpt != 0; cpt=cpt->next,dnum++)
	{
		// lame in the extreme.
		// enums don't seem to have a real value stored..
		if (dnum >= cval)
		{
			*opt << cpt->name.string();
			break;
		}
	}
}

void
sdlArrayType::print_val(ostream *opt,CString ar_pt) const
{
	switch (tag) {
	// for dubious reasons we've cheated and left strings as
	// an ArrayType.
	case Sdl_string:
	case Sdl_text:
		*opt << ((sdl_string *)ar_pt)->string();
	break;
	case Sdl_array:
	for (int i=0; i<dim; i++)
	{
		caddr_t elt_pt = ar_pt +(i*elementType->size);
		elementType->print_val(opt,elt_pt);
	}
	break;
	}
}
void
sdlSequenceType:: print_val(ostream *opt,CString dpt) const
// apply a
{
	switch(tag) {
	case Sdl_string:
	case Sdl_text:
		*opt << ((sdl_string *)dpt)->string();
		return;
	}
	sdl_gen_set * hpt = (sdl_gen_set *)dpt;
	for (int i=0; i<hpt->num_elements; i++)
	{
		caddr_t elt_pt = hpt->space +(i*elementType->size);
		elementType->print_val(opt,elt_pt);
	}
}
void
sdlCollectionType:: print_val(ostream *opt,CString dpt) const
{
	// 
}

void
sdlRefType::print_val(ostream *opt,CString dpt) const
{
	// oops, sdlRefType is still used for set/bag/list; this
	// should be fixed...
	if (dpt == 0) return;
	switch(tag) {
	case Sdl_set:
	case Sdl_bag:
	case Sdl_list:
	{
		sdl_gen_set * hpt = (sdl_gen_set *)dpt;
		if (elementType->size==0)
			elementType.update()->compute_size();
		*opt << "{ ";
		if (hpt != 0)
			for (int i=0; i<hpt->num_elements; i++)
			{
				caddr_t elt_pt = hpt->space +(i*elementType->size);
				elementType->print_val(opt,elt_pt);
				if (i+1 != hpt->num_elements)
					*opt << ",\n";
			}
		*opt << "}\n";
	}
	break;
	case Sdl_ref:
	{
		if (dpt != 0)
		{
			LOID loid;
			shrc rc = ((OCRef *)dpt)->simple_get_loid(loid);
			if (rc) rc.fatal();
			*opt << loid;
		}
	}
	break;
	}
}

void
sdlIndexType:: print_val(ostream *opt,CString dpt) const
{
	// TODO
}

void
sdlInterfaceType::print_val(ostream *opt,CString dpt) const
{
	Ref<Declaration> mpt;
	opt->form("interface  %s: {\n",name.string());
	if (Bases != 0)
	{
		*opt << " base ";
		for (mpt = Bases; mpt !=0; mpt=mpt->next)
			mpt->type->print_val(opt,dpt+ mpt->offset);
	}
	for (mpt = Decls; mpt !=0; mpt=mpt->next)
	{
		opt->form("%s:\t ",mpt->name.string());
		mpt->type->print_val(opt, dpt+mpt->offset);
		*opt << endl;
	}
	opt->form("} interface  %s\n",name.string());
}
void
sdlUnionType::print_val(ostream *opt,CString dpt) const
// jeez, we should do unions some time...
{
}
void sdlExprNode::Apply(app_class *arg) const 
{
	arg->action(this);
};
void sdlArithOp::Apply(app_class *arg) const
{
	arg->action(this);
};
void sdlConstName::Apply(app_class *arg) const
{
	arg->action(this);
};
void sdlLitConst::Apply(app_class *arg) const
{
	arg->action(this);
};
void sdlSelectExpr::Apply(app_class *arg) const
{
	arg->action(this);
};
void
sdlFctExpr::Apply(app_class *aarg) const
{
	aarg->action(this);
}
void 
sdlProjectExpr::Apply(app_class *arg) const
{
	arg->action(this);
}

