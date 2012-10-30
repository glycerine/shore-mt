/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "metatypes.sdl.h"
#include "app_class.h"
#include "expr_eval.h"
#include "sdl_gen_set.h"
missing_fct();
expr_ebase * 
new_subeval(Ref<sdlExprNode> eref, expr_ebase *ppt)
{
	if (eref == 0)
		return 0;
	get_subeval tmp(eref,ppt);
	return tmp.ept; // duh.
}
	
expr_eval<sdlConstDecl> * 
new_subeval(Ref<sdlConstDecl> eref, expr_ebase *ppt)
{
   	// get_subeval tmp(eref,ppt);
	// return (expr_eval<sdlConstDecl> *)tmp.ept; // duh.
	return new expr_eval<sdlConstDecl> (eref,ppt);
}
	

expr_eval<sdlConstDecl> *
expr_ebase::lookup_var(Ref<sdlConstDecl> s)
{
	// by default, just look up in parent
	if (parent!=0 && parent!= this)
		return parent->lookup_var(s);
	else
		return 0;
}

expr_eval<sdlConstDecl> *
expr_eval<sdlSelectExpr>::lookup_var(Ref<sdlConstDecl> s)
{
	// go down the range list...
	// oops, we used to just check rangelist; now we can 
	// do a recursive lookup, dubiously...
	expr_eval<sdlConstDecl> *rpt;
	for (rpt=range_eval; rpt != 0; rpt = rpt->next)
		if (rpt->my_ref()==s)
			return rpt;
	if (parent)
		return parent->lookup_var(s);
}
// note: this is wrong.
expr_eval<sdlConstDecl> *
expr_eval<sdlConstDecl>::lookup_var(Ref<sdlConstDecl> s)
{
	if (my_ref()==s)
	 	return this;
	if (next)
	 	return next->lookup_var(s);
	if (parent)
		return parent->lookup_var(s);
	return 0;
}

void get_subeval::action(const sdlExprNode * pt)
{
	ept = new expr_eval<sdlExprNode>(pt,ppt);
}
void get_subeval::action(const sdlLitConst * pt)
{
	ept = new expr_eval<sdlLitConst>(pt,ppt);
}
void get_subeval::action(const sdlConstName * pt)
{
	ept = new expr_eval<sdlConstName>(pt,ppt);
}

extern TypeTag reduce_type1(TypeTag val);
extern TypeTag reduce_type2(TypeTag val, TypeTag v2);
expr_ebase *
get_bin_expr(const sdlArithOp * pt,expr_ebase *ppt)
// get an operator and type specific instance...
{
	TypeTag etype;
	if (pt->e1 != 0 && pt->e2 != 0)
		etype = reduce_type2(pt->e1->type->tag,pt->e2->type->tag);
	else
		etype = NO_Type;
	switch(etype) {
	case Sdl_long:
	// cheat on unsigned for the moment; we don't have this right
	case Sdl_unsigned_long:
		switch (pt->aop) {
		case a_eq:	return new bin_eval_expr<long, a_eq>(pt,ppt);
		case a_ne:	return new bin_eval_expr<long, a_ne>(pt,ppt);
		case a_lt:	return new bin_eval_expr<long, a_lt>(pt,ppt);
		case a_gt:	return new bin_eval_expr<long, a_gt>(pt,ppt);
		case a_ge:	return new bin_eval_expr<long, a_ge>(pt,ppt);
		case a_le:	return new bin_eval_expr<long, a_le>(pt,ppt);
		case a_add:	return new bin_eval_expr<long, a_add>(pt,ppt);
		case a_subtract:	return new bin_eval_expr<long, a_subtract>(pt,ppt);
		case a_multiply:	return new bin_eval_expr<long, a_multiply>(pt,ppt);
		case a_divide:	return new bin_eval_expr<long, a_divide>(pt,ppt);
		case a_mod:	return new bin_eval_expr<long, a_mod>(pt,ppt);
		case a_and:	return new bin_eval_expr<long, a_and>(pt,ppt);
		case a_or:	return new bin_eval_expr<long, a_or>(pt,ppt);
		//case a_count:	return new bin_eval_expr<long, a_count>(pt,ppt);
		//case a_sum:	return new bin_eval_expr<long, a_sum>(pt,ppt);
		}
		break;
	case Sdl_double:
		switch (pt->aop) {
		case a_eq:	return new bin_eval_expr<double, a_eq>(pt,ppt);
		case a_ne:	return new bin_eval_expr<double, a_ne>(pt,ppt);
		case a_lt:	return new bin_eval_expr<double, a_lt>(pt,ppt);
		case a_gt:	return new bin_eval_expr<double, a_gt>(pt,ppt);
		case a_ge:	return new bin_eval_expr<double, a_ge>(pt,ppt);
		case a_le:	return new bin_eval_expr<double, a_le>(pt,ppt);
		case a_add:	return new bin_eval_expr<double, a_add>(pt,ppt);
		case a_subtract:	return new bin_eval_expr<double, a_subtract>(pt,ppt);
		case a_multiply:	return new bin_eval_expr<double, a_multiply>(pt,ppt);
		case a_divide:	return new bin_eval_expr<double, a_divide>(pt,ppt);
		case a_mod:	return new bin_eval_expr<double, a_mod>(pt,ppt);
		case a_and:	return new bin_eval_expr<double, a_and>(pt,ppt);
		case a_or:	return new bin_eval_expr<double, a_or>(pt,ppt);
		//case a_count:	return new bin_eval_expr<double, a_count>(pt,ppt);
		//case a_sum:	return new bin_eval_expr<double, a_sum>(pt,ppt);
		}
		break;
	}
	return new expr_eval_aqua(pt,ppt);
}
void get_subeval::action(const sdlArithOp * pt)
{
	if (pt->etag==Dot )
	{
		if(  pt->aop==a_nil)
	// this is really a Ref op; allocate a special case.
			ept = new expr_eval_ref(pt,ppt);
		else if (pt->aop == a_union)
			ept = new expr_eval_aqua_union(pt,ppt);
		else
			ept = get_bin_expr(pt,ppt);

			//ept = new expr_eval_aqua(pt,ppt);
	}
	else if (pt->etag==CName) // an aggregate expr
	{
		ept = new expr_eval_aqua(pt,ppt);
	}
	else // seems to be a native shore const expr; not used?/
		ept = new expr_eval<sdlArithOp>(pt,ppt);
}
void get_subeval::action(const sdlSelectExpr * pt)
{
	ept = new expr_eval<sdlSelectExpr>(pt,ppt);
}
void get_subeval::action(const sdlFctExpr * pt)
{
	// ignore the arg and use the body.
	ept = new_subeval(pt->body,ppt);
	// ept = new expr_eval<sdlFctExpr>(pt,ppt);
}
void get_subeval::action(const sdlConstDecl * pt)
{
	ept = new expr_eval<sdlConstDecl>(pt,ppt);
}


Ref<sdlType> element_of( Ref<sdlType> cont)
// check if cont is a "container" (set/bag/sequence/array)
// and if so, if elt is the element type of the container.
// should probably be moved into metatype system...
{
	switch(cont->tag){
	case Sdl_set:
	case Sdl_bag:
	case Sdl_sequence:
	case Sdl_array:
		return ((Ref<sdlEType> &)cont)->elementType;
	}
	return 0;
}

class get_eltpt : public app_class {
public:
	char * base_ptr;
	int index;
	char *elt_pt;
	s_value rval;
	virtual void action(const sdlArrayType *);
	virtual void action(const sdlRefType *);
	virtual void action(const sdlSequenceType *);
	virtual void action(const sdlStructType *);
	get_eltpt(char *b, int idx) { base_ptr = b; index = idx; elt_pt = 0; }
};
void
get_eltpt::action(const sdlArrayType * type)
{
	// note: may still be string or text; ignore for now.
	if (type->tag!= Sdl_array) abort();
	if (index>= type->dim)
		elt_pt = 0;
	else
		elt_pt = base_ptr + (index * type->elementType->size);
}

void
get_eltpt::action(const sdlRefType *type)
// may be ref,sequence,bag,set???
{
	switch(type->tag) {
	case Sdl_sequence:
	case Sdl_bag:
	case Sdl_set:
		sdl_gen_set *hpt =(sdl_gen_set *)base_ptr;
		if (index >= hpt->num_elements)
			elt_pt = 0;
		else
			elt_pt = hpt->space +(index*type->elementType->size);
	}
}
void
get_eltpt::action(const sdlSequenceType *type)
// may be ref,sequence,bag,set???
{
	sdl_gen_set *hpt =(sdl_gen_set *)base_ptr;
	if (index >= hpt->num_elements)
		elt_pt = 0;
	else
		elt_pt = hpt->space +(index*type->elementType->size);
}
void
get_eltpt::action(const sdlStructType *type)
{
	// this is moderately dubious; return pointer to the ith field.
	int i;
	Ref<sdlDeclaration> fref = type->members;
	for (i=0; i<index ; ++i)
	{
		if (fref == 0)
		{
			elt_pt = 0;
			return;
		}
		fref=fref->next;
	}
	elt_pt = base_ptr + fref->offset;
}


		

char * get_ith_element_of(char *data_ptr,Ref<sdlType> type,int i)
{
	get_eltpt retr(data_ptr,i);
	type->Apply(&retr);
	return retr.elt_pt;
}

// note that the const decl get_first_elt should never return a value.
// it always leaves the value attached to itself.
bool expr_eval<sdlConstDecl>::get_first_elt()  // dest ignored..
{
	// this is a range expression. 
	// or a range list..
	// execute the subexpression to get the next value.
	iter_count =0;
	if (type == element_of(range_expr->type)) //should always be true...
		if (range_expr->get_first_elt(cur_val))
		{
			++iter_count;
			if (next) // bogus nested loops init...
				return (next->get_first_elt());
			return true;
		}
		else
			return false;
	else
		return false;
}

bool expr_eval<sdlConstDecl>::get_next_elt( )
{
	// this is a range expression. 
	// execute the subexpression to get the next value.
	// to handle next field we have problems..
	// ok, if there is a next, try to exhaust its iterator first...
	if ( next && next->get_next_elt())
	{
		++ iter_count;
		return true; // still looping on inner variable...
	}
	if (range_expr->get_next_elt(cur_val))
	{
		++iter_count;
		if (next) // need inner loop on inner range variable.
			return next->get_first_elt();
		return true;
	}
	return false;
}




expr_eval<sdlSelectExpr>:: expr_eval(Ref<sdlSelectExpr> aop, expr_ebase *ppt) : expr_ebase(aop,ppt)
{
	// be careful about range eval list; iterate instead of recursion.
	expr_eval<sdlConstDecl> *range_end = 0;
	Ref<sdlDeclaration> rpt;
	range_eval = 0;
	range_end = range_eval =  new_subeval(aop->RangeList,this);
	for (rpt = aop->RangeList->next; rpt != 0;  rpt = rpt->next)
	{
		range_end->next = new_subeval((Ref<sdlConstDecl> &)rpt,this);
		range_end = range_end->next;
	}
	pred_eval = new_subeval(aop->Predicate,this);
	proj_eval = new expr_eval_project(aop,this);
}
bool expr_eval<sdlSelectExpr>::get_next_elt(s_value & dest)
{
	while ( range_eval->get_next_elt())
		if (pred_eval->get_int_val())
		{
			// note that might be subject to refinement, but I
			// think that if the project is set valued, the
			// type of the select expr will be set<set<..>>
			proj_eval->get_val(dest);
			++iter_count;
			return true;
		}
	return false;
}
bool expr_eval<sdlSelectExpr>::get_first_elt(s_value & dest) 
// select is being used as an iterator..
{
	// note: in order to handle iteration here
	// iterate over range; apply predicate;
	// set value from project.
	//
	iter_count = 0;
	if (range_eval->get_first_elt() )
	{
		if ( pred_eval->get_int_val())
		{
			proj_eval->get_val(dest);
			iter_count++;
			return true;
		}
		else
			return get_next_elt(dest);
	}
	else
		return false;
}

void expr_eval<sdlSelectExpr>::get_val(s_value & dest)
// get the <set-valued> value of the whole iteration...
{
	sdl_gen_set *spt = new sdl_gen_set;
	s_value elt_v;
	Ref<sdlType> etype = element_of(type);

	if (get_first_elt(elt_v))
	{
		spt->add(elt_v.get_ptr(),etype);
		while (get_next_elt(elt_v))
			spt->add(elt_v.get_ptr(),etype);
	}
	dest.set_temp_val(type,(char *)spt);
}
			

expr_eval_project::expr_eval_project(Ref<sdlSelectExpr> aop, expr_ebase *ppt) 
	: expr_ebase(aop->ProjList,ppt)
{
	Ref<sdlConstDecl> init;
	type = element_of(aop->type); 

	n_inits = 0;
	for (init = aop->ProjList; init!=0; init.assign(  init->next))
		++n_inits;
	init_exprs = new (expr_ebase *)(n_inits);
	int i = 0;
	for (init = aop->ProjList; init!=0; init.assign(  init->next))
	{
		Ref<sdlExprNode> ie = init->expr;
			init_exprs[i] =  new_subeval(ie,this);
			++i;

	}
}

// project is a mess. we have to walk through the
// project type and copy out the elements of the init expr.
// can we do better?
void expr_eval_project::get_val(s_value &dest)
{
	// this is of course wrong...
	int i;
	char *data;
	s_value init_dest[n_inits]; //???
	// this is pretty sucky; we have neither a copy operator
	// for values nor a temp operator at this point.
	data = new char[type->size];

	dest.set_temp_val(type,data);

	// dest->type = type;
	for (i=0; i<n_inits; i++)
	{
		init_exprs[i]->get_val(init_dest[i]);
		char * rpt;
		if (i==0)
			rpt = data;
		else
			rpt = get_ith_element_of(data,type,i);
		memcpy(rpt,init_dest[i].get_ptr(),(unsigned)init_exprs[i]->type->size);
	}
}

expr_eval<sdlConstDecl>::expr_eval(Ref<sdlConstDecl> aop, expr_ebase *ppt)
	: expr_ebase(aop,ppt)
{
	// wrong; this is always a range variable.
	// maybe?? watch out for real consts. - should have a real
	// range decl type.
	next = 0;
	range_expr = new_subeval(aop->expr,this);
}

expr_eval<sdlConstName>::expr_eval(Ref<sdlConstName> aop, expr_ebase *ppt) : expr_ebase(aop,ppt)
{
	// the following should vector up to a parent containing
	// the variable decl...
	if (aop->dpt != 0)
	{
		ept = ppt->lookup_var(aop->dpt);
		if (ept == 0) // range var lookup failed;
			abort();
	}
	else // if no dpt, this should be a db variable.
	{
		// try to look up the name and get a ref for it from shore.
		ept = 0;
		shrc rc;
		rc = Ref<any>::lookup(my_ref()->name,bref);
		// should set a type here???
		if (rc != RCOK)  
			abort();
	}
}

Ref<any>
expr_eval<sdlConstName>::get_ref_val()
{
	if (ept)
		return ept->get_ref_val();
	else
		return bref;
}

void
expr_eval<sdlConstName>::get_val(s_value &dest)
{
	if (ept)
		ept->get_val(dest);
	else
		abort();
}

char *
expr_eval<sdlConstName>::deref()
{
	if (ept)
		return ept->deref();
	else
		abort();
}
char *
expr_eval<sdlLitConst>::deref()
{
	return my_ref()->tvalue.space;
}

Ref<any>
expr_eval<sdlConstDecl>::get_ref_val()
{
	// either a range v or a db var.
	// this is problematic in the new systme
	if (cur_val.byvalue)
		return cur_val.refval;
	else
		return (*(Ref<any> *)(cur_val.get_ptr()));
	return 0;
}

char *
expr_eval<sdlConstDecl>::deref()
{
	// either a range v or a db var.
	return cur_val.get_ptr();
}

void
expr_eval<sdlConstDecl>::get_val(s_value &dest)
{
	dest = cur_val;
}

expr_eval_ref::expr_eval_ref(Ref<sdlArithOp> aop, expr_ebase *ppt) 
	: expr_ebase(aop,ppt)
{
// this is a ref.  we need to get a field decl and offset
// for the lhs.
// note that the arithop usage for dot is odd, and the field
// is stored as a literal constant string.. this should be
// fixed in the metatype expressions.
	Ref<sdlLitConst> fconst;
	Ref<sdlType> btype;
	base = new_subeval(aop->e1,ppt);
	fconst.assign(aop->e2);
	char *fn = fconst->imm_value.string();
	// get the sdl type of the lhs
	// somewhat bogusly, we need to get the subtype we want..
	// could do a swith here; instead,
	// just try all possibilities..
	if (aop->e1->type->tag==Sdl_ref)
		btype = ((Ref<sdlEType> &)(aop->e1->type))->elementType;
	else
		btype = aop->e1->type;
	field = btype->lookup_name(fn,Attribute);
	if (field==0)
		field= btype->lookup_name(fn,Relationship);
	if (field==0)
		field= btype->lookup_name(fn,Member);
	if (field==0)
	// last chance.
		field = btype->lookup_name(fn,Arm);
	foffset = field->offset;

}

// get_ptr always returns pointer to data refered to by an expr.
char * expr_eval_ref::deref()
// what? return a poiter to what this expression points to.
{
	// minimal checking here.
	if (base->type->tag==Sdl_ref)
		return base->get_ref_val().quick_eval() + foffset;
	else // composed ref, e.g. a.b.c and e1 is a.b.
		return base->deref() + foffset;
}
Ref<any>  expr_eval_ref::get_ref_val()
// this is a ref valued dereference; return the ref.
{
	return *(Ref<any> *)deref();
}

// 
bool
expr_eval_ref::get_first_elt(s_value &dest)
{
	Ref<sdlType> tref = element_of(type);
	iter_count = 0;
	if (tref==0)
		return false;
	// the type of this elt is set/array/something; we need to extract
	// the value somehow. Set the data ptr if not set??
	char *data_ptr = deref();
	char *rptr = get_ith_element_of(data_ptr,type,iter_count);
	// this is dubious.
	dest.set_ptr_val(tref,rptr);
	if (rptr ==0)
		return false;
	++iter_count;
	return true;
}
bool
expr_eval_ref::get_next_elt(s_value &dest)
{
	// the type of this elt is set/array/something; we need to extract
	// the value somehow. Set the data ptr if not set??
	char *data_ptr = deref();
	char *rptr = get_ith_element_of(data_ptr,type,iter_count);
	if (rptr ==0)
		return false;
	++iter_count;
	dest.set_ptr_val( element_of(type),rptr);
	return true;
}

// dummy base virtuals..
long expr_ebase::get_int_val()
{
	abort(); return 0;
}; // extract an integer & return it.
Ref<any> expr_ebase::get_ref_val()
{
	abort(); return 0;
}
char *expr_ebase::deref()
{
	abort(); return 0;
}
void expr_ebase::get_val(s_value &dest )
{
	abort();
}
bool 
expr_ebase::get_first_elt(s_value & dest )
{
	abort(); return false;
}
bool 
expr_ebase::get_next_elt(s_value &dest )
{
	abort(); return false;
}


void
expr_eval_ref::get_val(s_value &dest)
{
	dest.set_ptr_val( type, deref());
}

long
expr_eval_ref::get_int_val()
{
	// bogus...
	// ok fix this slightly...
	switch(type->tag){
	case Sdl_unsigned_long:
	case Sdl_long: return *(long *)deref();
	case Sdl_unsigned_short:
	case Sdl_short: return *(short *)deref();
	case Sdl_float: return long( *(float *)deref());
	case Sdl_double: return  long(*(double *)deref());
	case Sdl_boolean:	return *(bool *)deref();
	}

}

long 
expr_eval<sdlLitConst>::get_int_val()
{
	// moderately bogus call to sdl evaluation routing
	return my_ref()->fold();
}
extern TypeTag reduce_type1(TypeTag val);
extern TypeTag reduce_type2(TypeTag val, TypeTag v2);

template class Apply<Ref<any> >;
template class Sequence< Ref< any> >;
template class Set< Ref< any> >;
template class Sequence< sdl_string >;
template class Set< sdl_string >;
long
other_op(expr_eval_aqua * e)
// an arith?operator on a non-numberic thing....
{ 
	// for now we just do strings...
	if (e->e1->type->tag==Sdl_string && e->e2->type->tag==Sdl_string)
	{
		sdl_string *s1 = (sdl_string *)(e->e1->deref());
		sdl_string *s2 = (sdl_string *)(e->e2->deref());
		int cmpval = s1->strcmp(*s2);
		switch(e->atag) {
		case a_eq:
			return cmpval == 0;
		case a_ne:
			return cmpval != 0;
		case a_lt:
			return cmpval < 0;
		case a_gt:
			return cmpval > 0;
		case a_ge:
			return cmpval >= 0;
		case a_le:
			return cmpval <= 0;
		}
	}
	if (e->atag==a_member)
	{
		// assume e2 = set of e1, somehow...
		// only refs for now...
		if (e->e1->type->tag == Sdl_ref)
		{
			Ref<any> * eltpt = (Ref<any> *)(e->e1->deref());
			Set<Ref<any> > * setpt = (Set<Ref<any> > *)(e->e2->deref);
			return setpt->member( *eltpt);
		}
		if (e->e1->type->tag==Sdl_string) // set of strings. gosh.
		{
			sdl_string * eltpt = (sdl_string*)(e->e1->deref());
			Set<sdl_string > * setpt = (Set<sdl_string > *)(e->e2->deref());
			return setpt->member( *eltpt);
		}
	}
	abort(); // need to generalize comparison on arb. types...
}



long
expr_eval_aqua::get_int_val()
{
	// we should maybe redo this with subtypes.
	// need to handle strings, and perhaps other stuff.
		
	TypeTag etype;
	if (e1==0) 
		etype = reduce_type1(type->tag);
	else
		etype= reduce_type2(e1->type->tag,e2->type->tag);
	if (etype==NO_Type)
		return other_op(this);

	switch(atag){
		case a_add:
			return e1->get_int_val()+e2->get_int_val();
		case a_subtract:
			return e1->get_int_val()-e2->get_int_val();
		case a_multiply:
			return e1->get_int_val()*e2->get_int_val();
		case a_divide:
			return e1->get_int_val()/e2->get_int_val();
		case a_mod:
			return e1->get_int_val()%e2->get_int_val();
		case a_eq:
			return e1->get_int_val()==e2->get_int_val();
		case a_ne:
			return e1->get_int_val()!=e2->get_int_val();
		case a_lt:
			return e1->get_int_val()<e2->get_int_val();
		case a_le:
			return e1->get_int_val()<=e2->get_int_val();
		case a_gt:
			return e1->get_int_val()>e2->get_int_val();
		case a_ge:
			return e1->get_int_val()>=e2->get_int_val();
		case a_and:
			return e1->get_int_val() && e2->get_int_val();
		case a_or:
			return e1->get_int_val() || e2->get_int_val();
		// aggregates:
		case a_count:
		{
			int count = 0;
			s_value tmp;
			if (e2->get_first_elt(tmp))
			{	++count;
				while (e2->get_next_elt(tmp))
					++count;
			}
			return count;
		}
		case a_sum:
		// this needs to be done.
		{
			abort();
			return 0;
		}
		default:
			return 0;
	}
}

double expr_ebase::get_float_val()
{
	abort(); return 0;
}
long bin_eval_expr<long,a_add>::get_int_val() {
			return e1->get_int_val()+e2->get_int_val();
}
long bin_eval_expr<long,a_subtract>::get_int_val() {
			return e1->get_int_val()-e2->get_int_val();
}
long bin_eval_expr<long,a_multiply>::get_int_val() {
			return e1->get_int_val()*e2->get_int_val();
}
long bin_eval_expr<long,a_divide>::get_int_val() {
			return e1->get_int_val()/e2->get_int_val();
}
long bin_eval_expr<long,a_mod>::get_int_val() {
			return e1->get_int_val()%e2->get_int_val();
}
long bin_eval_expr<long,a_eq>::get_int_val() {
			return e1->get_int_val()==e2->get_int_val();
}
long bin_eval_expr<long,a_ne>::get_int_val() {
			return e1->get_int_val()!=e2->get_int_val();
}
long bin_eval_expr<long,a_lt>::get_int_val() {
			return e1->get_int_val()<e2->get_int_val();
}
long bin_eval_expr<long,a_le>::get_int_val() {
			return e1->get_int_val()<=e2->get_int_val();
}
long bin_eval_expr<long,a_gt>::get_int_val() {
			return e1->get_int_val()>e2->get_int_val();
}
long bin_eval_expr<long,a_ge>::get_int_val() {
			return e1->get_int_val()>=e2->get_int_val();
}
long bin_eval_expr<long,a_and>::get_int_val() {
			return e1->get_int_val() && e2->get_int_val();
}
long bin_eval_expr<long,a_or>::get_int_val() {
			return e1->get_int_val() || e2->get_int_val();
}
double bin_eval_expr<long,a_add>::get_float_val() {
			return e1->get_float_val()+e2->get_float_val();
}
double bin_eval_expr<long,a_subtract>::get_float_val() {
			return e1->get_float_val()-e2->get_float_val();
}
double bin_eval_expr<long,a_multiply>::get_float_val() {
			return e1->get_float_val()*e2->get_float_val();
}
double bin_eval_expr<long,a_divide>::get_float_val() {
			return e1->get_float_val()/e2->get_float_val();
}
double bin_eval_expr<long,a_mod>::get_float_val() {
			return e1->get_int_val()%e2->get_int_val();
}
double bin_eval_expr<long,a_eq>::get_float_val() {
			return e1->get_float_val()==e2->get_float_val();
}
double bin_eval_expr<long,a_ne>::get_float_val() {
			return e1->get_float_val()!=e2->get_float_val();
}
double bin_eval_expr<long,a_lt>::get_float_val() {
			return e1->get_float_val()<e2->get_float_val();
}
double bin_eval_expr<long,a_le>::get_float_val() {
			return e1->get_float_val()<=e2->get_float_val();
}
double bin_eval_expr<long,a_gt>::get_float_val() {
			return e1->get_float_val()>e2->get_float_val();
}
double bin_eval_expr<long,a_ge>::get_float_val() {
			return e1->get_float_val()>=e2->get_float_val();
}
double bin_eval_expr<long,a_and>::get_float_val() {
			return e1->get_float_val() && e2->get_float_val();
}
double bin_eval_expr<long,a_or>::get_float_val() {
			return e1->get_float_val() || e2->get_float_val();
}
long bin_eval_expr<double,a_add>::get_int_val() {
			return e1->get_int_val()+e2->get_int_val();
}
long bin_eval_expr<double,a_subtract>::get_int_val() {
			return e1->get_int_val()-e2->get_int_val();
}
long bin_eval_expr<double,a_multiply>::get_int_val() {
			return e1->get_int_val()*e2->get_int_val();
}
long bin_eval_expr<double,a_divide>::get_int_val() {
			return e1->get_int_val()/e2->get_int_val();
}
long bin_eval_expr<double,a_mod>::get_int_val() {
			return e1->get_int_val()%e2->get_int_val();
}
long bin_eval_expr<double,a_eq>::get_int_val() {
			return e1->get_int_val()==e2->get_int_val();
}
long bin_eval_expr<double,a_ne>::get_int_val() {
			return e1->get_int_val()!=e2->get_int_val();
}
long bin_eval_expr<double,a_lt>::get_int_val() {
			return e1->get_int_val()<e2->get_int_val();
}
long bin_eval_expr<double,a_le>::get_int_val() {
			return e1->get_int_val()<=e2->get_int_val();
}
long bin_eval_expr<double,a_gt>::get_int_val() {
			return e1->get_int_val()>e2->get_int_val();
}
long bin_eval_expr<double,a_ge>::get_int_val() {
			return e1->get_int_val()>=e2->get_int_val();
}
long bin_eval_expr<double,a_and>::get_int_val() {
			return e1->get_int_val() && e2->get_int_val();
}
long bin_eval_expr<double,a_or>::get_int_val() {
			return e1->get_int_val() || e2->get_int_val();
}
double bin_eval_expr<double,a_add>::get_float_val() {
			return e1->get_float_val()+e2->get_float_val();
}
double bin_eval_expr<double,a_subtract>::get_float_val() {
			return e1->get_float_val()-e2->get_float_val();
}
double bin_eval_expr<double,a_multiply>::get_float_val() {
			return e1->get_float_val()*e2->get_float_val();
}
double bin_eval_expr<double,a_divide>::get_float_val() {
			return e1->get_float_val()/e2->get_float_val();
}
double bin_eval_expr<double,a_mod>::get_float_val() {
			return e1->get_int_val()%e2->get_int_val();
}
double bin_eval_expr<double,a_eq>::get_float_val() {
			return e1->get_float_val()==e2->get_float_val();
}
double bin_eval_expr<double,a_ne>::get_float_val() {
			return e1->get_float_val()!=e2->get_float_val();
}
double bin_eval_expr<double,a_lt>::get_float_val() {
			return e1->get_float_val()<e2->get_float_val();
}
double bin_eval_expr<double,a_le>::get_float_val() {
			return e1->get_float_val()<=e2->get_float_val();
}
double bin_eval_expr<double,a_gt>::get_float_val() {
			return e1->get_float_val()>e2->get_float_val();
}
double bin_eval_expr<double,a_ge>::get_float_val() {
			return e1->get_float_val()>=e2->get_float_val();
}
double bin_eval_expr<double,a_and>::get_float_val() {
			return e1->get_float_val() && e2->get_float_val();
}
double bin_eval_expr<double,a_or>::get_float_val() {
			return e1->get_float_val() || e2->get_float_val();
}
Ref<any>
expr_eval_aqua::get_ref_val()
{
	abort(); return 0;
}
char *
expr_eval_aqua::deref()
{
	abort(); return 0;
}



void 
expr_eval_aqua::get_val(s_value &dest)
{
	switch(reduce_type1(type->tag))
	{
		case Sdl_long:
		case Sdl_unsigned_long:
			dest.byvalue = true;
			dest.vu.intval = get_int_val();
			return;
		break;
		case NO_Type:
			break;
		case Sdl_double:	abort();
	}
	// some non-primitve type ...
	switch (atag) {
		case a_multiset_to_set:
			e2->get_val(dest);
			return;
		default:
			abort();
	}
}

bool expr_eval_aqua::get_first_elt(s_value &dest)
{	missing_fct(); return false;}
bool expr_eval_aqua::get_next_elt(s_value &dest)
{	missing_fct(); return false;}


void expr_eval_aqua_union::get_val(s_value &dest)
// for now, do a simple concatenation of the 2 sets.
{	missing_fct(); }

// on the fly union; always a bag for now..
bool expr_eval_aqua_union::get_first_elt(s_value & dest)
{	
	iter_count =0;
	e2_active = false;
	if (e1->get_first_elt(dest)) {
		++iter_count;
		return true;
	}
	else {
		e2_active = true;
		if (e2->get_first_elt(dest))
		{
			++iter_count;
			return true;
		}
		else
			return false;
	}
}


bool expr_eval_aqua_union::get_next_elt(s_value & dest)
// oddly, this looks just like get_first_elt-> note that this
// is a bit inefficient but correct.
{	
	if (e2_active) // we have  finished e1 & started e2.
	{
		if (e2->get_next_elt(dest))
		{
			++iter_count;
			return true;
		}
		else
			return false;
	}
	else if ( e1->get_next_elt(dest))
	{
		++iter_count;
		return true;
	}
	else // e2_active false, e1 empty -> activate e2.
	{
		e2_active = true;
		if (e2->get_first_elt(dest))
		{
			++iter_count;
			return true;
		}
		else
			return false;
	}
}
s_value::s_value()
{
	bypointer = byref = byvalue = tempspace = false;
	vu.space = 0;
	refval = 0;
	vt = 0;
}
s_value::~s_value()
{
	if (tempspace)
		delete vu.space;
}
void
s_value::operator=(s_value &rhs)
{
	// this of course needs work.
	byref =rhs.byref;
	bypointer = rhs.bypointer;
	byvalue = rhs.byvalue;
	tempspace = false;
	refval = rhs.refval;
	if(bypointer)
		vu.space = rhs.vu.space;
	else if (byvalue)
	{
		vu.floatval = rhs.vu.floatval;
	}
	else if (byref)
	{
		vu.intval = rhs.vu.intval;
	}
}

template class bin_eval_expr<long, a_eq>;
template class bin_eval_expr<long, a_ne>;
template class bin_eval_expr<long, a_lt>;
template class bin_eval_expr<long, a_gt>;
template class bin_eval_expr<long, a_ge>;
template class bin_eval_expr<long, a_le>;
template class bin_eval_expr<long, a_add>;
template class bin_eval_expr<long, a_subtract>;
template class bin_eval_expr<long, a_multiply>;
template class bin_eval_expr<long, a_divide>;
template class bin_eval_expr<long, a_mod>;
template class bin_eval_expr<long, a_and>;
template class bin_eval_expr<long, a_or>;
//template class bin_eval_expr<long, a_count>;
//template class bin_eval_expr<long, a_sum>;
template class bin_eval_expr<double, a_eq>;
template class bin_eval_expr<double, a_ne>;
template class bin_eval_expr<double, a_lt>;
template class bin_eval_expr<double, a_gt>;
template class bin_eval_expr<double, a_ge>;
template class bin_eval_expr<double, a_le>;
template class bin_eval_expr<double, a_add>;
template class bin_eval_expr<double, a_subtract>;
template class bin_eval_expr<double, a_multiply>;
template class bin_eval_expr<double, a_divide>;
template class bin_eval_expr<double, a_mod>;
template class bin_eval_expr<double, a_and>;
template class bin_eval_expr<double, a_or>;
//template class bin_eval_expr<double, a_count>;
//template class bin_eval_expr<double, a_sum>;
