/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "sdl_internal.h"
#include "node.h"
#include "metatype_ext.h"
#include "sdl_ext.h"
#include "sdl-gram.tab.h"
#include <assert.h>
#include <debug.h>
#include "type_globals.h"
#include "string.h"
#include "metatypes.sdl.h"
extern int scheck_only;
extern int overwrite_module;
static int doing_metatypes; 
extern int lineno;
extern char * cur_src;
extern char *src_version_string;
template class UnRef<sdlDeclaration>;
template class UnRef<sdlType>;
template class UnRef<sdlExprNode>;
node::node(short c, node *inf, node *p2) 
{	
	code = c; 
	info = inf; 
	next = p2; 
}

// declaration node default field routine.  Most declarations have
// set the fields name,type,kind,zone,and lineno. zone defaults to
// public and lineno is set from the current scanned line.
void
set_decl(WRef<sdlDeclaration> dref, const char *name, Ref<sdlType> type, DeclKind kind, int lineno = 0, Zone z=Public)
{
	dref->name = name;
	dref->kind = kind;
	dref->type = type;
	dref->zone = z;
	if (lineno)
		dref->lineno = lineno;
	else
		dref->lineno = ::lineno;
	dref->next = 0;
}
	
// a name, type, declaration kind

// #ifdef oldcode
// need to redo this for sdl/ref based impl.
// for the time being, use pointers to refs.
extern "C"
node *
list_append(node *list, node *lastinfo)
{

	if (scheck_only) return 0;
	node *new_node = new node(LIST,lastinfo,0);
	if (list)
	{
		node * tail = list;
		while (tail->next)
			tail = tail->next;
		tail->next = new_node;
	}
	else
		list = new_node;
	return list;
}

extern "C" 
decl_pt
decl_list_append(decl_pt hpt, decl_pt apt)
// append apt to list headed by hpt if hpt non-null; else, just
// return apt which becomes the head of the list
{
	if (scheck_only) return 0;
	if (hpt != 0 && (apt != 0) &&  (hpt) != 0)
	{
		hpt->update()->ListAppend(apt);
		return hpt;
	}
	else if (apt==0)
		return hpt;
	else
		return apt;
}

extern "C"
node *
type_list_append(node *list, type_pt lastinfo)
// use the node-style linked list, add on lastinfo, just casting
// the Type to a node.  THIS NEEDS WORK.
{
	if (scheck_only) return 0;
	return list_append(list, (node *)new Ref<sdlType>(lastinfo));
}

extern "C"
expr_pt
expr_list_append(expr_pt list, expr_pt lastinfo)
// use the node-style linked list, add on lastinfo, just casting
// the ExprNode to a node.  THIS NEEDS WORK.
{
	// new expr list: persistent, looks like a comma expr. e.g. the
	// list 1,2 become  comma_expr(1,2),
	// and 1,2,3 becomes comma_expr(1,comma_expr(2,3)
	// oops, should 1 be comma_expr(1,nii) or 1?
	// for simplicity, do the latter.
	if (scheck_only) return 0;
	if (list == 0) // just return the tail (new elt)
		return lastinfo;
	if ( (list)->etag==Comma)
	// replace e2, recursively?
	{
		Ref<sdlArithOp> l = (Ref<sdlArithOp> &)list;
		l->update()->e2 = expr_list_append((l->update())->e2,lastinfo);
		return l;
	}
	else // *list is a simple expr
	{
		sdlArithOp * rpt = NEW_T sdlArithOp;
		rpt->etag = Comma;
		rpt->e1 = list;
		rpt->e2 = lastinfo;
		return (rpt);
	}
}
// #endif

extern "C" 
void
check_module_name(node *id)
// check if this is a metatype module.
{
	if (!strcmp("metatypes",(CString)id->info))
	// we are recompiling metatype module
		doing_metatypes = 1;
	else
		doing_metatypes = 0;
}

extern "C"
decl_pt
sdl_module (decl_pt  mdpt, decl_pt export_list, decl_pt  import_list, decl_pt def_list)
// the module struct initially just has three declaration lists: the import
// list, which defines what external names may be referenced; the export list,
// which defines what name defined with this module may be referenced outside
// of it, and the declaration list defining everything.
{

	WRef<sdlModule> mpt;
	WRef<sdlModDecl> dpt;
	if (scheck_only) return 0;
	if (mdpt)
	{
		dpt = (WRef<sdlModDecl> &)mdpt;
	
		mpt = dpt->dmodule;
		mpt->myDecl = dpt;
		dpt->scope = 0; // no containing scope for modules.
	}
	else
		return 0;

	// export/import away node reimplementation; they are
	// some kind of list but not yet done.
	if (export_list)
		mpt->export_list = export_list;
	if (import_list)
		mpt->import_list = import_list;
	if (def_list)
		mpt->decl_list = def_list;
	return mdpt;
}

extern "C"
decl_pt
module_tag (node *id)
// now, create the module when the name is encountered; fill in the decl_list
// fields later.
{
	// before fixing new, we hack this to delete any existing
	// object with the same name. 
	if (scheck_only) return 0;

	// If either of the module or the pool objects exists,
	// we behave as if the entire thing is there.
	bool	module_exists=false;

	Ref<sdlModule> ExistingM; // is null already
	shrc rc;
	char pool_name[200];
	check_module_name(id);
	sprintf(pool_name,"%s_pool",id->info);

	{
		// zero out references
		Ref<Pool> nilref;
		CurTypes = nilref;
	}
	rc = ExistingM.lookup((CString)id->info,ExistingM);
	if (!rc && ExistingM != 0)
	{
		module_exists = true;
		DBG(<<"Module " << id->info << " exists and has " 
			<< ExistingM->ref_count << " references " );
	}
	rc = Ref<Pool>::lookup(pool_name,CurTypes);
	if (!rc && CurTypes != 0)
	{
		module_exists = true;
	} 
	// rc.reset(); this seems bogus.
	if(module_exists) 
	{
		if (!overwrite_module)
		{
			fprintf(stderr,"cannot overwrite existing module %s\n",id->info);
			exit(-1);
		} 
		if (ExistingM != 0  && ExistingM->ref_count)
		{
			fprintf(stderr,"module %s has %d instances\n",
				id->info,ExistingM->ref_count);
			exit(-1);
		} 
		fprintf(stderr,"deleting existing module %s\n",id->info);
		if (ExistingM != 0)
		{
			SH_DO( Shore::unlink(CString(id->info)));
		}
		if (CurTypes != 0)
		{
			SH_DO(CurTypes.destroy_contents());
			SH_DO( Shore::unlink(pool_name));
		}
	}

	WRef<sdlModule> mpt;
	SH_DO(mpt.new_persistent((char *)id->info,0644));
	SH_DO(REF(Pool)::create(pool_name,0644,CurTypes));
	mpt->my_pool = CurTypes;
	if (doing_metatypes)
		mpt->src_file = src_version_string;
	else
		mpt->src_file = cur_src;
	WRef<ModDecl> dpt;
	SH_DO(dpt.new_persistent(mpt->my_pool));
	dpt->name = (CString)id->info;
	dpt->type = (Type *)0; // note: type is now some node thing.
	dpt->kind = Mod;
	// export/import away node reimplementation; they are
	// some kind of list but not yet done.
	mpt->name = (CString)id->info;
	dpt->dmodule = mpt;
	return dpt;
}

// another import thing to be redone
extern "C"
decl_pt
mod_import(short use_or_imp,expr_pt n, node * alias)
{
	sdl_string name_str;
	if (n->etag==Literal) // a string for the module name...
	{
		Ref<sdlLitConst> e;
		e.assign(n);
		name_str.set(e->imm_value.string()+1,0,e->imm_value.strlen()-2);
		name_str.set(e->imm_value.strlen()-1,0); // truncate the str.
	}
	else if (n->etag==CName)
	// 
	{
		Ref<sdlConstName> nn;
		nn.assign(n);
		name_str = nn->name;
	}
	// no other possibilities?
	if (scheck_only) return 0;
	sdlModDecl * dpt;
	dpt = NEW_T sdlModDecl;
	switch(use_or_imp){
	case USE:
		if (alias) // add in module node with real expected name.
		{
			set_decl(dpt,(CString)alias->info,0,UseMod,alias->lineno);
			// alias is the name of the decl.
			sdlModule *mpt;
			mpt = NEW_T sdlModule;
			// use name is the real name of the module
			mpt->name = name_str;;
			dpt->dmodule = mpt;
		}
		else
			set_decl(dpt,name_str,0,UseMod);
	break;
	case IMPORT:
		set_decl(dpt,name_str,0,ImportMod);
		dpt->dmodule = 0; // no mod node needed here.
	break;
	}

		
	return (dpt);
}




ExprOp
code_to_exprop(short code)
{	
	switch(code)
	{
		case '+':	return Plus;
		case '-':	return Minus;
		case '*':	return Mult;
		case '/':	return Div;
		case '%':	return ModA;
		case '|':	return Or;
		case '&':	return And;
		case '^':	return Xor;
		case '~':	return Complement;
		case LSHIFT:	return LShift;
		case RSHIFT:	return RShift;
	}
	return EError;
}
extern "C"
decl_pt
// this slaps a public/private/protected indicatore in front of a
// (now) type node.
// ok, plan b: make it a declaration.
// allocate a new node
access_scoped_name(Zone access, type_pt sname)
{
	if (scheck_only) return 0;
	Declaration *bpt;
	bpt = NEW_T Declaration;
	set_decl(bpt,(sname)->name,sname,BaseType,0,access);
	return (bpt);
}

extern "C"
decl_pt
// path in the zone specification to each declaration
export_access_spec(Zone access, decl_pt sname)
{
	if (scheck_only) return 0;
	Ref<Declaration> app_pt;
	if (sname)
	{
		for (app_pt = sname; app_pt!=0;app_pt = app_pt->next)
			app_pt.update()->zone = access;
	}
	return sname;
		
}

// count the number of elements in the cons-cell style list "this".
int
node::list_count()
//
{
	node * cpt = this;
	int count = 0;
	for (cpt = this; cpt; cpt = cpt->next)
		count++;
	return count;
}

char *
// currently only used for ID's.
node::get_string()
{
	switch (code) {
	case ID:
		return (char *)info;
	}
	return 0;
}
typedef void * voidpt;

	

extern "C"
decl_pt
interface_dcl(node * header, decl_pt body)
// header  contains tag and base class list; body is
// a list of declarations.
{
	if (scheck_only) return 0;
	sdlTypeDecl * dpt; // treat as type name declaration
	InterfaceType * ipt;
	dpt = NEW_T sdlTypeDecl;
	if (doing_metatypes) 
	// special case: make registerd objects for metatypes.
	// and delete old object if it exists.
	{
		char iname[200];
		sprintf(iname,"primitive_types/%s", (CString)header->info->info);
		Ref<sdlInterfaceType> old_i;
		shrc rc;
		rc = Ref<sdlInterfaceType>::lookup(iname,old_i);
		if (!rc && old_i != 0)
		{
			fprintf(stderr,"deleting existing module %s\n",iname);
			rc = Shore::unlink(iname);
			if (rc)
				rc.fatal();
			// does unlink really destroy:
		}
		ipt = new(iname , 0644) sdlInterfaceType;
	}
	else
		ipt = NEW_T InterfaceType;
	set_decl(dpt,(CString)header->info->info,ipt,TypeName,header->info->lineno);
	ipt->name = dpt->name;
	ipt->tag = Sdl_interface;
	ipt->myDecl = dpt;
	// ipt->Bases = (Declaration *)(header->next);
	// ok, this is pretty ugly, should fix up the old node code,
	// but for now, header->nexd is a decl_pt (e.g Ref<Declaration> *;
	// cast it properly.
	if (header->next)
		ipt->Bases = *(decl_pt *)(header->next);
	else
		ipt->Bases = 0;
	if (body)
		ipt->Decls = body;
	else
		ipt->Decls = 0;

	// next go though the inheritance list
	return (dpt);

}

extern "C"
decl_pt
forward_dcl(node *id)
// forward decl is node wit
// info = id;
// next = null
{
	if (scheck_only) return 0;
	Declaration * fpt;
	fpt = NEW_T sdlTypeDecl;
	set_decl(fpt,(CString)id->info,0,InterfaceName,id->lineno);
	return (fpt);
}

extern "C"
node *
interface_header(node *id,decl_pt inherit)
// intnerface header is a node with
// info = id of class
// next = inheritance list (or null)
// inheritance list is a list of scoped names, which is
// a list of list of names.
{
	node * ihead;
	ihead = new node(INTERFACE,id,(node *)(new Ref<sdlDeclaration>(inherit)));
	ihead->lineno = id->lineno;
	return ihead;
}

extern "C"
node *
scoped_name(node * left, node * right)
// scoped name is, basicly, left::right.  If there is no
// qualification, the ID is passed through as is; 
// ::id is inidicated with left= 0;
// scoped name is a list of id's but we give it a special node
// class.  We may have to punt on this.no
// as is, scoped name can pass through as ID; this may be
// disturbing.

{
	if (scheck_only) return 0;
	node *scopenode;
	scopenode = new node(SCOPED_NAME,left,right);
	scopenode->lineno = right->lineno;
	return scopenode;
}

extern "C"
type_pt
type_name(node *scoped_node)
// flag a node (from scoped_name) as being used as a type name;
// this is done by interposing a TYPE_NAME node ahead of the
// scoped_node
{
	// node * tn_node;
	// tn_node = new node(TYPE_ID,scoped_node,0);
	// return tn_node;
	// we should really try to resolve this first...
	if (scheck_only) return 0;
	sdlNamedType * tpt;
	tpt = NEW_T sdlNamedType;
	tpt->tag = Sdl_NamedType;
	if (scoped_node->code==ID)
	{
		if (!strcmp((CString)(scoped_node->info),"external"))//gross
			tpt->tag = Sdl_ExternType;
			// name set later.
		else
			tpt->name = (CString)(scoped_node->info);
		tpt->lineno = scoped_node->lineno;
		tpt->scope = 0;

	}
	else if (scoped_node->code==SCOPED_NAME)
	{
		tpt->name = (CString)(scoped_node->next->info);
		tpt->lineno = scoped_node->next->lineno;
		tpt->scope = id_declarator(scoped_node->info);
	}
	else // ???
		abort();
	return (tpt);
}

extern "C"
decl_pt
const_dcl(type_pt type,node *id,expr_pt expr)
//  create a const declaration node, with proper string and expression
// field set. This should perhaps be handled with ctors.
{
	if (scheck_only) return 0;
	WRef<ConstDecl> cpt = NEW_T ConstDecl;
	set_decl(cpt,(CString)id->info,type,Constant,id->lineno);
	cpt->expr = expr;        // note: expr is now some node thing
	// additional processing needed: convert type & expr to
	// something reasonable.
	return cpt;
}

extern "C"
expr_pt
expr(expr_pt left,short op,expr_pt right)
// expr's are cons up with
// code = op;
// info = lhs of expr;
// next = rhs of expr
{
	if (scheck_only) return 0;
	WRef<sdlArithOp>  rpt = NEW_T sdlArithOp;
	rpt->etag = code_to_exprop(op);
	if (left)
	{
		rpt->e1 = left;
		rpt->lineno = (left)->lineno;
	}
	else
	{	if (right)
			rpt->lineno = (right)->lineno;
		else 
			rpt->lineno = ::lineno;
	}
	if (right) 
		rpt->e2 = right;
	return rpt;
}

extern "C"
expr_pt
const_name(node_pt  npt)
// here, we want to convert a scoped name to an expression.
// we cheat somewhat.
{
	if (scheck_only) return 0;
	sdlConstName *ept = NEW_T sdlConstName;
	ept->update();
	ept->etag = CName;
	if (npt->code==SCOPED_NAME)
	{
		// ept->scope = (Ref<sdlConstName> &)(const_name(npt->info));
		ept->scope.assign(const_name(npt->info));
		ept->name = (char *)npt->next->info;
	}
	else if (npt->code ==ID)
	{
		ept->name = (char*)npt->info;
		ept->scope = 0;
	}
	else // bummer.
		npt->error("unexpected const name node");
	ept->lineno = ::lineno;
	return ept;
}
	
extern "C"
decl_pt
type_declarator(type_pt spec, decl_pt decls)
// type declarators are composed of a spec and a list of decls;
// the spec is a "basetype" of sorts and the decls are a list
// of either id's or "array_decls", eg. ID + dimenssions.
// they seem to show up only as typedefs.
// code = TYPE_DCL;
// info = spec
// list = list of declarators (IDs and arrays )
// Plan B: we now turn the list of decls into either declartions
// creating array types as appropriate.
// what this is supposed to look like: the decl list has Type
// pointers that are either null or are array types without a base
// type.  Set the type field from spec if the pointer is null, set
// the element type of the array if the decl is for an array
//  

//:e g
{
	if (scheck_only) return 0;
	Ref<Declaration> dpt;
	for  (dpt = decls; dpt != 0; dpt = dpt->next)
	{
		dpt.update()->kind = TypeName;
		dpt.update()->ApplyBaseType(spec);
	}
	return decls;
}

extern "C"
decl_pt
id_declarator(node * p)
// get the string out of the node & stuff in a new declarator.:371
// we may want to do more work here; for now just a dummy.
{
	if (scheck_only) return 0;
	Declaration * dpt;
	dpt = NEW_T Declaration;
	set_decl(dpt,p->get_string(),0,ERROR,p->lineno);
	return dpt;
}

extern "C"
decl_pt
export_declarator(node * p)
// get the string out of the node & stuff in a new declarator.:371
// this is special cased for export.
{
	if (scheck_only) return 0;
	Declaration * dpt;
	dpt = NEW_T sdlDeclaration;
	if (p == 0 )
		set_decl(dpt,0,0,ExportAll,0);
	else
		set_decl(dpt,p->get_string(),0,ExportName,p->lineno);
	return dpt;
}

extern "C"
decl_pt
enum_declarator(node * p)
// get the string out of the node & stuff in a new declarator.:371
// this is different from id_declarator in the type of decl allocated.
// we may want to do more work here; for now just a dummy.
{
	if (scheck_only) return 0;
	ConstDecl * dpt;
	dpt = NEW_T ConstDecl;
	set_decl(dpt,p->get_string(),0,EnumName,p->lineno);
	return (dpt);
}

extern "C"
type_pt
primitive_type(TypeTag code)
// again, just a dummy.
// we temporarily return addresses of static objects
// NOTE: if we collect garabage this is really bad.
{
	if (scheck_only) return 0;
	switch (code)
	{
		case Sdl_char: return CharacterTypeRef;
		case Sdl_boolean: return BooleanTypeRef;
		case Sdl_octet:	return UnsignedCharacterTypeRef;
		case Sdl_any:	return AnyTypeRef;
		case Sdl_float:	return FloatingPointTypeRef;
		case Sdl_double:	return DoublePrecisionTypeRef;
		case Sdl_long:	return LongIntegerTypeRef;
		case Sdl_short:	return ShortIntegerTypeRef;
		case Sdl_void:	return VoidTypeRef; // ops only.
		case Sdl_pool:	return PoolTypeRef;
		case Sdl_unsigned_short: return UnsignedShortIntegerTypeRef;
		case Sdl_unsigned_long:  return UnsignedLongIntegerTypeRef;
		case Sdl_string: // bogus
			return string_type(0);
	}
	return 0;
}

decl_pt
check_for_typedecl(type_pt spec,decl_pt decls)
{
	// within structs and unions, 
	// we need to check for declarations of form "struct s{...} svar;
	// if we see such a decl, need to convert the type_pt to a decl
	// and insert at head of list.
	switch((spec)->tag)
	{
		case Sdl_struct:
		case Sdl_union:
		case Sdl_enum:
		{
			decl_pt tdecl;
			tdecl = get_dcl_for_type(spec);
			tdecl->update()->next = decls;
			return tdecl;
		}
	}
	return decls;
}

extern "C"
decl_pt
typedef_dcl(decl_pt p)
// here we just change the value of the code for p.
// we walk down the list of declarations and set the kind
// field for each.
{
	if (scheck_only) return 0;
	Ref<Declaration> dpt;
	for (dpt= p; dpt!= 0; dpt=dpt->next)
	{
		dpt.update()->kind = TypeName;
		if (dpt->type != 0 && dpt->type->tag==Sdl_ExternType)
			dpt->type.update()->name = dpt->name;
	}
	// need to check for typedef struct s1 { ... } s2; & variants.
	Ref<sdlType> dtype = (p)->type;
	return check_for_typedecl(dtype,p);
}

extern "C"
decl_pt
get_dcl_for_type(type_pt tpt)
//  tpt points to either a struct, interface,or union type; add
// in a declaration node, as a TypeName, and pass back the
// node.
{
	if (scheck_only) return 0;
	Ref<sdlTypeDecl> dpt = NEW_T sdlTypeDecl;
	WRef<sdlType> tref = tpt;
	tref->myDecl = dpt;
	set_decl(dpt,(tpt)->name,tpt,TypeName);
	return ( dpt);
}

// return an externally declared type pointer.
extern "C"
decl_pt
get_extern_type(TypeTag tkind,node_pt id)
{
	if (scheck_only) return 0;
	sdlExtTypeDecl  * dpt;
	CString tname = id->get_string();
	dpt = NEW_T sdlExtTypeDecl;
	switch(tkind) {
	case Sdl_union:	
		dpt->type = union_type(id,0,0,0);
		set_decl(dpt,tname,union_type(id,0,0,0),TypeName,id->lineno);
		dpt->type.update()->tag = Sdl_CUnion;
	break;
	case Sdl_struct:
		set_decl(dpt,tname,struct_type(id,0),TypeName,id->lineno);
	break;
	case Sdl_Class:
		set_decl(dpt,tname,class_type(id),TypeName,id->lineno);
	break;
	case Sdl_enum:
		set_decl(dpt,tname,enum_type(id,0),TypeName,id->lineno);
	break;
	case Sdl_ExternType:
	{
		sdlNamedType * tpt;
		tpt = NEW_T sdlNamedType;
		set_decl(dpt,tname,tpt,TypeName,id->lineno);
		tpt->tag = Sdl_ExternType;
		tpt->name = dpt->name;
	}
	break;
	}
	return (dpt);
}

extern "C"
type_pt
struct_type(node *id, decl_pt members)
// return a pointer to a ref to a new structType instance;
{
	if (scheck_only) return 0;
	node *stnode;
	StructType *spt;
	spt = NEW_T StructType;
	spt->tag = Sdl_struct;
	if (members)
		spt->members = members;
	if (id != 0)
		spt->name = id->get_string();
	return (spt);
}

extern "C" 
type_pt
class_type( node *id)
// return a pointer to a ref to a new classType instance;
// right now, classes are strictly externally defined and have
// no known internal structure.
{
	if (scheck_only) return 0;
	node *stnode;
	sdlClassType *spt;
	spt = NEW_T sdlClassType;
	spt->tag = Sdl_Class;
	spt->name = id->get_string();
	return (spt);
}


extern "C"
decl_pt
member(type_pt spec,decl_pt decls)
// struct members look like type declarators and 
// are composed of a spec and a list of decls;
// the spec is a "basetype" of sorts and the decls are a list
// of either id's or "array_decls", eg. ID + dimenssions.
// Make sure the type is set right on each decl, and set the "kind"
// field apropriately
// new declar
{
	if (scheck_only) return 0;
	Ref<Declaration> dpt;
	for  (dpt = decls; dpt!=0; dpt = dpt->next)
	{
		dpt.update()->kind = Member;
		dpt.update()->ApplyBaseType(spec);
	}
	return check_for_typedecl(spec,decls);
}

extern "C"
type_pt
union_type(node *id, type_pt desc, node *tagid,decl_pt body)
// create a union type specifier, given the union tag id,
// the type of the discriminator, and the list of union arms.
{
	if (scheck_only) return 0;
	node * switchnode;
	node * unode;
	UnionType *rpt = NEW_T UnionType;
	// this needs to be redone.
	rpt->tag =Sdl_union;
	if (desc ) // nill for forward/extern decl
	{
		if (tagid)
		{
			rpt->TagDecl = id_declarator(tagid);
			rpt->TagDecl.update()->type = desc;
		}
		else
		{
			sdlDeclaration * dpt = NEW_T sdlDeclaration;
			set_decl(dpt,"i_tag_val",desc,Member); // Member is dubious...
			rpt->TagDecl =  dpt;
		}
		rpt->ArmList = (Ref<ArmDecl> &)(body);
	}
	rpt->name = id->get_string();
	return (rpt);
}

extern "C"
decl_pt
case_elt(expr_pt clist,decl_pt elem)
// creat a union arm decl, given a list of constants and
// a name/type declaration for the arm data element.
{
	// we chose this point to transform the generic Declaration * elem
	// into an ArmDecl.
	if (scheck_only) return 0;
	ArmDecl * apt  = NEW_T ArmDecl;
	set_decl(apt,(elem)->name,(elem)->type,Arm, (elem)->lineno);
	// apt->CaseList = clist; // INCORRECT
	// leave null for now
	if (clist)
		apt->CaseList = clist;
	return (apt);
}

extern "C"
decl_pt 
element_spec(type_pt spec, decl_pt decl)
// looks type type_decl
// this is always a union arm declaration, and should be a plain declaration.
// we could chose to transform this to an arm decl here.
{
	if (scheck_only) return 0;
	decl->update()->ApplyBaseType(spec);
	return check_for_typedecl(spec,decl);
}

extern "C"
type_pt
enum_type(node *tag, decl_pt elist)
{
	if (scheck_only) return 0;
	EnumType * ept;
	ept = NEW_T EnumType;
	ept->tag = Sdl_enum;
	if (elist)
		ept->consts = elist;
	ept->tag_decl = id_declarator(tag);
	ept->name = ept->tag_decl->name;
	// patch the type into the enum decl's
	Ref<Declaration> dpt = 0;
	if (elist)
		for (dpt = elist; dpt !=0; dpt = dpt->next)
			dpt.update()->type = ept;
	return (ept);
}

extern"C"
type_pt 
sequence_type(type_pt tparm, expr_pt bound)
// for now, treat this like array type with a diffent tag.
{
	if (scheck_only) return 0;
	sdlSequenceType *spt = NEW_T sdlSequenceType;
	spt->tag= Sdl_sequence;
	spt->elementType = tparm;
	if (bound)
		spt->dim_expr = bound;
	else
		spt->dim_expr =0;
	return (spt);
}

extern"C"
type_pt
string_type( expr_pt bound)
// for now, treat this as a sequence of char.
{
	if (scheck_only) return 0;
	Ref<Type> spt = sequence_type(CharacterTypeRef,bound);
	spt.update()->tag =Sdl_string;
	return (spt);

}
extern"C"
type_pt
text_type( expr_pt bound)
// text type is a minor varant of string type.
{
	if (scheck_only) return 0;
	Ref<Type> spt = sequence_type(CharacterTypeRef,bound);
	spt.update()->tag =Sdl_text;
	return (spt);

}

extern "C"
type_pt
relation_type(TypeTag  kind, type_pt ctype)
{
// this is not yet well defined
// oops this is for refs etc.
// IMPORTANT TO FIX THIS.
	if (scheck_only) return 0;
	RefType * rpt = NEW_T RefType;
	rpt->tag = (kind);
	rpt->elementType = ctype;
	return (rpt);
}

// a hacked verion of relation type.k
extern "C"
type_pt
index_type(type_pt ktype, type_pt vtype)
{
// this is not yet well defined
// oops this is for refs etc.
// IMPORTANT TO FIX THIS.
	if (scheck_only) return 0;
	IndexType * rpt = NEW_T IndexType;
	rpt->tag = Sdl_Index; 
	rpt->keyType = ktype;
	rpt->elementType = vtype;
	return (rpt);
}


// multidimensional arrays are treated as arrays of arrays.
extern"C"
decl_pt
array_declarator(node *id, expr_pt size_e)
{
	if (scheck_only) return 0;
	node * anode;
	Ref<Declaration> apt = id_declarator(id);
	ArrayType *Atype = NEW_T ArrayType;
	ArrayType *Ptype = 0;
	ArrayType *Rtype = 0;

	Ref<sdlExprNode> sizelist;
	if (size_e)
		sizelist = size_e;
	else 
		sizelist = 0;
	while(sizelist != 0)
	{
		Atype = NEW_T ArrayType;
		Atype->tag = Sdl_array;
		if (sizelist->etag==Comma)
		{
			Ref<sdlArithOp> lpt;
			lpt.assign(sizelist);
			Atype->dim_expr = lpt->e1;
			sizelist = lpt->e2;
		}
		else
		{
			Atype->dim_expr = sizelist;
			sizelist = 0;
		}
		Atype->dim =0;
		if (Ptype)
			Ptype->elementType = Atype;
		else // set the initial type & Ptype
		{
			Ptype = Rtype  = Atype;
		}
		Ptype = Atype;
	}
	apt.update()->type = Rtype;
	return (apt);
}

extern"C"
decl_pt
attr_dcl(short index_n,type_pt spec,decl_pt decls)
//  for the moment ignore readonly and pragmas
{
	// The primary glitch here is copying the decls into AttrDecls.
	// hmm, the only thing we have to add to decls is Readonly.
	if (scheck_only) return 0;
	AttrDecl * atdcls = 0;
	boolean indexable;
	//if (index_n==INDEXABLE)
	if (index_n!=0)
		indexable = true;
	else
		indexable = false;
	Ref<Declaration> dpt;
	for (dpt = decls; dpt!=0; dpt = dpt->next)
	{
		AttrDecl * atp = NEW_T AttrDecl;
		set_decl(atp,dpt->name,dpt->type,Attribute,dpt->lineno,dpt->zone);
		atp->Indexable = indexable;
		atp->ApplyBaseType(spec);
		decl_list_append( atdcls, atp);
		if (!atdcls)
			atdcls = atp;
	}
	return (atdcls);
}

extern "C"
decl_pt
op_dcl(type_pt type,node *id,decl_pt parms,node *exception,short context)
{
	if (scheck_only) return 0;
	OpDecl *opt = NEW_T OpDecl;
	set_decl(opt,id->get_string(),type,Op,id->lineno);
	if (parms)
		opt->parameters = (Ref<ParamDecl> &)parms;
	else
		opt->parameters = 0;
	// note: need to handle something other than const here.
	opt->isConst = context?true:false;
	return (opt);
}

extern "C"
decl_pt
parm_dcl(Mode  attr, type_pt spec, decl_pt dcl)
// attribute is either tIN, tOUT or tINOUT
// code = attr->code
// infor = spec
// next = declarator
{
	if (scheck_only) return 0;
	ParamDecl *ppt = NEW_T ParamDecl;
	set_decl(ppt,(dcl)->name,(dcl)->type,Param,(dcl)->lineno,(dcl)->zone);
	ppt->ApplyBaseType(spec);
	ppt->mode = (attr);
	return (ppt);
}
	

	
extern "C"
node *
raises_expr(node * slist)
{
	if (scheck_only) return 0;
	node *cnode;
	// no need for code, comes from context.
	cnode = new node(0,slist,0);
	return cnode;
}

extern "C"
decl_pt
rel_dcl(type_pt type,decl_pt decls,node * rel_pragma, decl_pt rel_declarator)
// another relatively complex one.
// defered for now.
{
	return 0;
}

extern "C"
type_pt
collection_type_spec(type_pt ctype)
// this is the old odl-style relation collection spec; we
// pass the node through as an sdl-style set type.
{
	return 0;
}

extern "C"
decl_pt
except_dcl(node *id,decl_pt mlist)
{
	return 0;
}


extern "C"
decl_pt
// overide declaration id list header
override_dcl(node *lpt)
{
	if (scheck_only) return 0;
	OpDecl * opt = NEW_T OpDecl;
	set_decl(opt,lpt->get_string(),0,OpOverride,lpt->lineno);
	return (opt);
}

	
/* line # handling and error recovery */
extern "C" 
int inc_linecount()
{
	lineno++; // maybe we should just do this in lex 
	return lineno;
}

node * cur_filename = 0;
extern int sdl_errors;
extern "C"
int check_linecount(char * line_txt)
// line_txt is presumably the # lineno "filename" output that comes
// out of cpp
{
	int new_ln;
	int scount;
	char namebuf[200];
	scount = sscanf(line_txt,"# %d %s",&new_ln,namebuf);
	if (scount == 2)  /* was in expected format */
	{
		lineno = new_ln;
		cur_filename = get_string_node(namebuf);
	}
	else
		lineno++;

	// scount = sscanf(line_txt,"# $Header: %s %s");
	if (!strncmp(line_txt,"# $Header:",10))
		src_version_string = strdup(line_txt+3);
	return lineno;
	// that's it, for now.
}

extern char *yytext;
extern "C"
yyerror()
{
	
  sdl_errors++;
  if (cur_filename)
  	fprintf(stderr,"file %s line %d: syntax error at token \"%\"\n",
		(char *)(cur_filename->info),lineno, yytext);
  else
  	fprintf(stderr,"line %d: syntax error at token \"%s\"\n",lineno,
		yytext);
  return -1;
}

void
node::error(char * str)
// some error has been detected. This will need refinement.
{
  sdl_errors++;
	fprintf(stderr,"error detected, error was %s, construct %d\n",str,code);
	abort();
}


// Declaration * g_module_list;
decl_pt g_module_list;

void
set_module_list(decl_pt  mlist)
{
	g_module_list = mlist;
}

expr_pt
get_literal_expr(TypeTag code, char *stval)
{
// convert some kind of literal to an expression node.
	if (scheck_only) return 0;
	sdlLitConst *ept = NEW_T sdlLitConst;
	ept->lineno = ::lineno;
	ept->imm_value = stval;
	ept->etag = Literal;
	switch(code) {
	case Sdl_long: 
		ept->type = LongIntegerTypeRef;
	break;
	case Sdl_string: 
		ept->type = StringTypeRef;
	break;
	case Sdl_char:
		ept->type = CharacterTypeRef;
	break;
	case Sdl_float:
	case Sdl_double:
		ept->type = DoublePrecisionTypeRef;
	break;
	case Sdl_boolean:
		ept->type  = BooleanTypeRef;
	break;
	case Sdl_void:
		ept->type = 0;
		ept->etag = CDefault;
	break;
	}
	return (ept);
}

extern"C"
decl_pt
relationship_dcl(type_pt spec,decl_pt dpt,node_pt inverse_name, node_pt ordered_name)
{
	// The primary glitch here is copying the decls into AttrDecls.
	// hmm, the only thing we have to add to decls is Readonly.
	if (scheck_only) return 0;
	RelDecl * atp = NEW_T RelDecl;
	set_decl(atp,(dpt)->name,spec,Relationship,(dpt)->lineno,(dpt)->zone);
	atp->lineno = (dpt)->lineno;
	atp->readOnly = false;
	if (inverse_name)
	{
		RelDecl * rtp2 = NEW_T RelDecl;
		set_decl(rtp2,inverse_name->get_string(),0,UnboundRelationship,inverse_name->lineno);
		// note: inverse name is a scoped name but we do
		// not handle scoping yet.
		atp->inverseDecl = rtp2;
	}
	else
		atp->inverseDecl = 0;
		

	return (atp);
}

