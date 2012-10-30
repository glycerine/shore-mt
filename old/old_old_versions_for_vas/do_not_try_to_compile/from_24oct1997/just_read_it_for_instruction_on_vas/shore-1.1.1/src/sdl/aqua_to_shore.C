/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <ostream.h>
#include <strstream.h>

#include <symbol.h>
#include <types.h>
#include <aqua.h>

#include <metatypes.sdl.h>
#include <type_globals.h> // new_t ???
#include "app_class.h"
#include <sdl_ext.h>
void
set_decl(WRef<sdlDeclaration> dref, const char *name, Ref<sdlType> type, 
	DeclKind kind, int lineno = 0, Zone z=Public);

Ref<sdlExprNode>
aqua_t::toShoreExpr()
{
	// hm..
	abort(); return 0;
}
extern "C" Ref<sdlType> primitive_type(TypeTag code);

// try to convert an aqua/bolo type to an sdl type.  This is not
// fully general.
Ref<sdlType> get_shore_type(Type * tt,const char *ename = 0)
{
	Ref<sdlType> rtype;
	if (rtype=tt->realType())
		return rtype;
	else if (tt->me()==ODL_Primitive)
	{
		rtype = primitive_type( tt->isPrimitive()->primitive());
	}
	else if (tt->isTuple() )
	{
		TupleType * rtup = tt->isTuple();
		if (rtup== 0)
			abort(); // multiple range vars==join, later.
		if ( rtup->nmembers() >1) // multiple range vars-> a join somewhere.
		{
			if (ename == 0)
			// hmm - return the tuple as a struct type.
			// this may lead to other problems...
				rtup->commit();
				return rtup->realType();
				// abort(); // need a name in this context.
			
		}
		if (ename==0) // only 1 elt
		{
			char *vname;
			ContainerType_i mems(*rtup);
			return get_shore_type(mems.curr(vname));
		}
		else // look up type of member indicated by ename.
		{
			return get_shore_type(rtup->member(ename));
		}
	}
	else if(tt->isRef())
	{
		RefType *rt = tt->isRef();
		if (rt->of()->realType() == 0)
			rt->of()->setRealType(get_shore_type(rt->of()));
		if (rt->of()->realType() != 0)
			rt->setRealType(get_shore_type(rt->of())->get_dtype(Sdl_ref));
		rtype = tt->realType();
	}
	else if (tt->isSet())
	{
		SetType *rt = tt->isSet();
		if (rt->of()->realType() == 0)
			rt->of()->setRealType(get_shore_type(rt->of()));
		if (rt->of()->realType() != 0)
			rt->setRealType(get_shore_type(rt->of())->get_dtype(Sdl_set));
		rtype = tt->realType();
	}
	else if (tt->isBag())
	{
		BagType *rt = tt->isBag();
		if (rt->of()->realType() == 0)
			rt->of()->setRealType(get_shore_type(rt->of()));
		if (rt->of()->realType() != 0)
			rt->setRealType(get_shore_type(rt->of())->get_dtype(Sdl_bag));
		rtype = tt->realType();
	}
	else if (tt->isArray())
	{
		ArrayType *rt = tt->isArray();
		if (rt->of()->realType() == 0)
			rt->of()->setRealType(get_shore_type(rt->of()));
		if (rt->of()->realType() != 0)
			rt->setRealType(get_shore_type(rt->of())->get_dtype(Sdl_sequence));
		rtype = tt->realType();
	}
	if (rtype==0)
	{
		tt->commit();
		rtype = tt->realType();
	}
	if (rtype == 0)
		abort();
	return rtype;
}

// put this here so we can call get_shore_type
void ContainerType::_commit()
{
   Symbol *sym;
   Type  *t;
   TableEntry  *entry;
   Ref<sdlDeclaration> dlist;
   SymbolTable_i  it(members);

   _align = 1; /* byte alignment */

   /**
     To handle ObjectType as well. For objects, i have to compute
     the sizes of the base classes before its own members; therefore
     the _size field is already filled by ObjectType::_commit.
     I shouldn't override this field...
   **/
   if (realType() != 0) return;
   if (_size < 0)
      _size = 0;
   for (; sym = *it; ++it)
   {
      entry = (TableEntry *) (sym->data());
      t = entry->type;
	  Ref<sdlType> st = t->realType();
      if (st==0)
      {
		st = get_shore_type(t);
	  }
	  if (st==0)
	  {
		cerr<<"cannot create type";
		_size = 0;
		return;
      }
      // mimic the sdl parser node.C routines.
      sdlDeclaration *dpt;
      dpt = NEW_T sdlDeclaration;
      set_decl(dpt,sym->name(),st,Member,0);
      dlist = decl_list_append(dlist,dpt);

	

       t->commit(); // hmm.
	  // we  need to create an sdl th

      /* meta information; a method in this case */
      /** Methods are of zero size. **/
      if (t->inline_size() == 0)
         continue;
      
      /** 
	  If the field is variable length, then the container is
	  also variable length
      **/
      if (t->isVarLength())
	 _isVarLen = 1;

/** All this is fine
   But it 'aint what Paradise does...
   So lets comment it out for now...
**/
/**
      if ((_size % t->align()) != 0)
         _size += t->align() - (_size % t->align());
**/
      entry->offset = _size;
      _size += t->inline_size();

      /* alignment is most restrictive of all subtypes */
      //if (t->align() > _align)
       //  _align = t->align();
   }
   // now, fix up the sdl type.
   Ref<sdlType> st;
   st = struct_type(0,dlist);
   // continue struct_type stuff.
   st->resolve_names(0,0);
   st.update()->compute_size();
   setRealType(st);
}
// function is a lambda-expr; build some kind of fuct. expr tree?
// but functions aren't named so what? 
Ref<sdlExprNode> aqua_function_t::toShoreExpr() 
{
	// check for some patterns..
	if ((_body->op() == a_tuple_concat) 
	 || (_body->op()==a_nil && _body->_type->isTuple()))
		// toSHoreProject now called directly, so we don't expect this.
		abort();

		// return this->toShoreProject(); // dubious,
		// but initially we assume that this is a projection
		// function.
	// hm, bogus.  Give up and make a real fct.
	// :f
#if 0
	WRef<sdlFctExpr> new_fct;
	new_fct = NEW_T sdlFctExpr;
	new_fct->body = _body->toShoreExpr();
	return new_fct;
#endif
	// it seems we should just use the body??
	return _body->toShoreExpr();
	// new_fct->arg = _one->toShoreExpr();
	// another hack; we don't really know what the arg is here...
	// so use the trusty litconst node.
	// oops, we never used the arg.  get_shore_type doesn't probler
	// translate tuple type, but since we never use it here, ignore it.
#ifdef oldcode
	{
		char * vname = _one->_name;
		WRef<sdlConstName> rvar = NEW_T sdlConstName;
		rvar->type = get_shore_type(_body->_type);
		rvar->name = vname;
		new_fct->arg = rvar;
	}
#endif

}

Ref<sdlConstDecl>
aqua_function_t::toShoreProject()
{
	// body is tuple_concat; we need to 
	// a) convert the tuple type to a shore struct.
	// b) create an initializer list of some sort...
	// c) stuff it all into an appr. sdl expr.
#if 0
	TupleType *p_type = type()->isTuple();
	assert (p_type!=0);
	if (p_type->realType()==0)
		p_type->commit();
#endif
	Ref<sdlConstDecl> first = 0;
	// we need to iterate through the tuple expr...
	// we make the dangerous assumption that tuple concat
	// corresponds directly to order in struct; if not,
	// the project expr will need to have a seq. of
	// (decl,expr) pairs instead of just exprs. This
	// should be checkable though.
	aqua_t  * plist = _body;
	Ref<sdlExprNode> init;
	WRef<sdlConstDecl> init_tail;
	while(plist)
	{
		aqua_t * init_elt;
		const char *fname = 0;
		if (plist->op() == a_tuple_concat) {
			init_elt = (aqua_tuple_t *)((aqua_binary_t *)plist)->_left;
			plist = ((aqua_binary_t *)plist)->_right;
		}
		else {
			init_elt=plist;
			plist = 0;
		}
		if (init_elt->op()==a_tuple) // aqua_tuple_t operator???
		{
			init = ((aqua_tuple_t *)(init_elt))->_value->toShoreExpr();
			fname = ((aqua_tuple_t *)(init_elt))->_name;

		}
		else
			init = init_elt->toShoreExpr();
		WRef<sdlConstDecl> init_decl = NEW_T sdlConstDecl;
		init_decl->expr = init;
		init_decl->type = init->type;
		// probably wrong...
		//init_decl->name = init_elt->id();
		init_decl->name = fname;
		if (init_tail != 0)
			init_tail->next = init_decl;
		else
			first = init_decl;
		init_tail = init_decl;
	}
	return first;
}




// aqua_join_t and aqua_apply have two shore translation methods.
// toShoreExpr returns a select expression; toshorerange returns
// a range variable decl list.
Ref<sdlExprNode> 
aqua_join_t::toShoreExpr()
// the join operator is a result of range variable declarations; 
// at this level, create a select expr with the proper range list.
{
	WRef<sdlSelectExpr>  range_e = NEW_T sdlSelectExpr;
	range_e->RangeList = toShoreRange();
	return range_e;
}

Ref<sdlConstDecl> aqua_join_t::toShoreRange() 
// for the simple case, an aqua_join_t seems to be the result of a
// select with multiple range variables, i.e.
// select ... from a in A, b in B ...
// we just get the shore expressions for the 2 subexprs, 
// which should be of type sdlConstDecl, and chain them together.
// Note: this leaves us with some inheritance problems...
// they should be fixed by making a common base for decl, exprnode.
{
	Ref<sdlConstDecl> r1 = _to->toShoreRange();
	WRef<sdlDeclaration> last = r1;
	if (r1==0)
		abort();
	while (last->next != 0) // by constr
		last = last->next;
	last->next = _other->toShoreRange();
	return r1;
}

// default aqua ops can't be converted to range decls.
Ref<sdlConstDecl> aqua_t::toShoreRange()  { abort(); return 0; }

Ref<sdlConstDecl> aqua_apply_t::toShoreRange() 
// by context, this apply expr was generated by the range part
// of a select statement; return the proper range decl.
{
	// WRef<sdlArithOp>  range_e = NEW_T sdlArithOp;
	// WRef<sdlSelectExpr>  range_e = NEW_T sdlSelectExpr;
	WRef<sdlConstDecl> rvar = NEW_T sdlConstDecl;
	TupleType * rtup = _what->_type->isTuple();
	if (rtup->nmembers() >1)
		abort(); // multiple range vars==join, later.
	ContainerType_i mems(*rtup);
	// get type and name of range v.
	char *vname;
	Type *vtype = mems.curr(vname);
	rvar->name = vname;
	rvar->type = get_shore_type(vtype);
	rvar->expr = _to->toShoreExpr();
	return rvar;
}
// apply a function (_what) to elts of a set (_to)
Ref<sdlExprNode> aqua_apply_t::toShoreExpr() 
{
// evaluate an expression??
// 2 ops: apply and select.
	WRef<sdlSelectExpr> sel = 0;
	if (op()==a_select )
	// implies select without projection
	{
		sel = (Ref<sdlSelectExpr> &)_to->toShoreExpr();
		// we used to create the selectExpr from the rangelist,
		// but that doesn't work well for joins.  Or maybe it
		// does.
		// rangelist set elsewhere.
		sel->ProjList = 0; // projlist set in parent??
		// will this work for a_group?
		sel->Predicate = _what->toShoreExpr();
		sel->type= get_shore_type(_type);
		return sel;
	}
	else if (_to->op()==a_select || _to->op()==a_group) // project out of a select..
	{
		sel = (Ref<sdlSelectExpr> &)(_to->toShoreExpr()); // as above
		// now, allocate select here
		sel->ProjList = _what->toShoreProject();
		sel->type= get_shore_type(_type);
		return sel;
	}

	else if ((op()==a_group 
		|| (_what->_body->op()==a_tuple && _what->_type->isTuple()))
		&& _to->type()->isCollection())
	// note that the above conditional may need refinement; it
	// _to is a tuple constructor function, and "this" is
	// (a_apply (tuple constructor) (range description)).  the
	// a_nil for what.body is dubious??
	// this is a range expression, binding a range variable
	// to iterate over something..
	// hmm maybe not..
	// yet another expression type we need here...
	// initially this is just single range var.
	// ok, turn this around-> create a decl for the range
	// var; stuff the expr into the decl; and create the select 
	// expr here.
	// modification: return the range expr, not the select.
	// range_exprs will be chained to make joins.
	// for a_group op, we plug in the fct as a predicate.
	{
		WRef<sdlSelectExpr>  range_e = NEW_T sdlSelectExpr;
		range_e->RangeList = toShoreRange();
		if (op()==a_group)
		range_e->Predicate = _what->toShoreExpr();	
		return range_e;
	}
	else // debugging mode......

		abort();
}

Ref<sdlExprNode> aqua_fold_t::toShoreExpr() 
{
	// for at least some stuff, fold is used to implement aggregate
	// fcts; we try to recognize this.
	// Although the generated aqua is quite complex, there are
	// only a few operators implemented, so we stuff this into
	// our old reliable sdlArithOp.
	// we will cheat for now and recyle CName as the expr tag, and
	// use the aqua op file to indicate the operation.
	WRef<sdlArithOp> agg_expr = NEW_T sdlArithOp;
	agg_expr->etag = CName;
	agg_expr->e1=0;
	agg_expr->e2 = _to->toShoreExpr();
	switch(_folder->_body->op())
	{
		case a_add: // need to distinguish between count and sum.
			if (_what->_body->op()==a_constant) // this is count.
				agg_expr->aop = a_count;
			else
				agg_expr->aop = a_sum;
		break;
		case a_min:
		case a_max:
			agg_expr->aop = _folder->_body->op();
		break;
		default:
			abort();
	}
	agg_expr->type = get_shore_type(_type);
	return agg_expr;
}




Ref<sdlExprNode> aqua_eval_t::toShoreExpr() { abort(); return 0; }
// logical/arith. operator

#if 0
aqua_to_shore(aqua_op ao)
{ 
	switch(ao) {
	case a_at: 		return Dot;
	case a_add: 		return Plus;
	case a_subtract: 		return Minus;
	case a_multiply: 		return Mult;
	case a_divide: 		return Div;
	case a_mod: 		return ModA;
	case a_eq: 		return jjjjjjjjk
	case a_ne: 		return
	case a_lt: 		return
	case a_le: 		return
	case a_gt: 		return
	case a_ge: 		return
	case a_and: 		return
	case a_or: 		return
	case a_tuple_concat: 		return
	case a_array_concat: 		return
	case a_list_concat: 		return
	case a_forall: 		return
	case a_exists: 		return
	case a_union: 		return
	case a_additive_union: 		return
	case a_cast: 		return
	case a_sort: 		return
	case a_min: 		return
	case a_max: 		return
	case a_intersect: 		return
	case a_diff: 		return
	case a_modify_add: 		return
	case a_modify_subtract: 		return
	case a_invoke: 		return
	case a_define: 		return
	case a_function: 		return
	case a_mkobj: 		return
	case a_program: 		return
	case a_select: 		return
	case a_group: 		return
	case a_subrange: 		return
	case a_tuple_join: 		return
	case a_fold: 		return
	case a_apply: 		return
	case a_method: 		return
	case a_arg: 		return
	case a_arg_concat: 		return
	case a_build_join: 		return
	case a_modify: 		return
	case a_modify_tuple: 		return
	case a_modify_array: 		return
	case a_update: 		return
	case a_delete: 		return
	case a_insert: 		return
	case a_eval: 		return
	case a_closure: 		return
}; /* enum aqua_op */

#endif
// we need to do something about missing thing.
Ref<sdlExprNode> aqua_binary_t::toShoreExpr() 
{
	WRef<sdlArithOp> exp;
	exp = NEW_T sdlArithOp;
	exp->e1 = _left->toShoreExpr();
	exp->e2 = _right->toShoreExpr();
	exp->etag = Dot; // proxy for aqua expr.
	exp->aop = op();
	exp->type = get_shore_type(_type);
	return exp;
}

// logical/arith op
// pass through unary ops as with binary, e2=0.
Ref<sdlExprNode> aqua_unary_t::toShoreExpr() 
{
// access to arg or name based db elt.
	WRef<sdlArithOp> exp;
	exp = NEW_T sdlArithOp;
	exp->e2 = _what->toShoreExpr();
	exp->e1 = 0;
	exp->etag = Dot; // proxy for aqua expr.
	exp->aop = op();
	exp->type = get_shore_type(_type);
	return exp;
}
Ref<sdlExprNode> aqua_get_t::toShoreExpr() 
{ 
	switch (_what->op()) {
	case a_var: // a range variable: translate directly to  sdlConstName.
	// this effectively collapses out the lambda name.
	{
		const char * vname = ((aqua_var_t *)_what)->_name;
		WRef<sdlConstName> bref = NEW_T sdlConstName;
		bref->name = _name;
		bref->type = get_shore_type(_what->_type,_name);
		// oops, need to collapse out tuple type...
		return bref;
	}
	break;
	case a_dbvar:  case a_get:
	// usage: dbvar contains database variable name, string
	// contains field.
	// if _what is a_get, _what is either another ref ore
	// e.g. _what is a db var, _what._name = name of dbvar,
	// _name = name of field in dbvar.
	// we should get a new exprnode for this, but temporise for now.
	{
		WRef<sdlArithOp> refnode;
		refnode = NEW_T sdlArithOp;
		if (_what->op() == a_dbvar ) // this should be subtyped,
		// but for now aqua_dbvar_t and aqua_var_t doesn't have a toSHoreExpr, so
		// do it by hand. This is bogus anyway, since we don't have
		// a proper implm. for these vars yet..
		{
			const char *vname;
			vname = ((aqua_dbvar_t *)_what)->_name;
			// dump it into litconst temp., like file stuff.
			WRef<sdlConstName> bref = NEW_T sdlConstName;
			bref->name = vname;
			bref->type = get_shore_type(_what->_type);
			// a temporary hole... realType doesn't exist!
			refnode->e1 =bref;
			
		}
		else // a_get will handle multilevel deref, etc.
			refnode->e1 = _what->toShoreExpr();
		// refnode should have a place for a decl,instead stuff 
		// a string for now...
		WRef<sdlLitConst> field = NEW_T sdlLitConst;
		field->imm_value = _name;
		field->type = _type->realType();
		refnode->e2 = field;
		refnode->type = _type->realType();
		refnode->etag = Dot;
		refnode->aop = a_nil;
		return refnode;
	}

	}
		abort();
}

Ref<sdlExprNode> aqua_constant_t::toShoreExpr()
{
	WRef<sdlLitConst> cpt = NEW_T sdlLitConst;
	cpt->type = get_shore_type(_type);
	cpt->imm_value = _rep;
	cpt->resolve_names(0,0); // set the value.
	return cpt;
}

Ref<sdlExprNode> aqua_zary_t::toShoreExpr()
{
	switch(op()) {
	case a_nil: // nothing?
		return 0;
	case a_true: // return true value??
	WRef<sdlLitConst> cpt = NEW_T sdlLitConst;
	cpt->type = get_shore_type(_type);
	cpt->imm_value = "true";
	cpt->resolve_names(0,0); // set the value.
	return cpt;
	}
	abort(); // not implemented.
	return 0;
}
// unknown.
Ref<sdlExprNode> aqua_assign_t::toShoreExpr() { abort(); return 0; }
Ref<sdlExprNode> aqua_method_t::toShoreExpr() { abort(); return 0; }

template class Sequence<Ref<sdlConstName> >;
class bind_expr : public app_class 
// descend an exprnode, depth first??
// how much context?
{
public:
	Ref<sdlExprNode> descend_base;
	Sequence<Ref<sdlConstDecl> > range_vars;
	// override the expr actions
	virtual void action(const sdlExprNode *);
	virtual void action(const sdlLitConst *);
	virtual void action(const sdlConstName *);
	virtual void action(const sdlArithOp *);
	virtual void action(const sdlSelectExpr *);
	virtual void action(const sdlProjectExpr *);
	virtual void action(const sdlFctExpr *);
	Ref<sdlConstDecl> lookup_var(const char *);
};

Ref<sdlConstDecl>
bind_expr::lookup_var(const char *s)
// walk through the arg list in reverse & see if any matches.
{
	int nelts = range_vars.get_size();
	for (int i = nelts-1; i>=0; i--)
	{
		Ref<sdlConstDecl> arg = range_vars.get_elt(i);
		if (arg->name.strcmp(s)==0)
			return arg;
	}
	return 0;
}

void bind_expr::action(const sdlFctExpr *fct)
{
	// add arg to current arg list; bind body via recursif call
	fct->body->Apply(this);
}

void bind_expr::action(const sdlExprNode *)
{
	
}
void bind_expr::action(const sdlLitConst *)
{
}
void bind_expr::action(const sdlConstName  *arg)
{
	// try to match this up with a fct arg lexically.
	if (arg->dpt==0)// not yet bound
	{
		Ref<sdlConstDecl> res = lookup_var(arg->name);
		if (res !=0)
		{
			arg->update()->dpt = res;
			arg->update()->type = res->type;
		}
		
	}
}

void bind_expr::action(const sdlArithOp *arg)
{
	switch(arg->etag) {
	}
	if (arg->e1 != 0)
		arg->e1->Apply(this);
	if (arg->e2 != 0)
		arg->e2->Apply(this);
}
void bind_expr::action(const sdlSelectExpr * spt)
{
	//  this is where we really bind in fct??
	// go through the range expr list; insert each into 
	// arg list??
	int orig_size = range_vars.get_size();
	Ref<sdlConstDecl> rv = spt->RangeList;
	while (rv != 0)
	{
		range_vars.append_elt(rv);
		if( rv->expr!=0) 
		// bind the range expression.	
			rv->expr->Apply(this);
		rv.assign(rv->next);
	}
	for (rv = spt->ProjList; rv!= 0; rv.assign(rv->next))
		rv->expr->Apply(this);
	if (spt->Predicate != 0)
		spt->Predicate->Apply(this);
	while (range_vars.get_size()>orig_size)
		range_vars.delete_elt(range_vars.get_size()-1);

}
void bind_expr::action(const sdlProjectExpr *pexp)
{
	Ref<sdlDeclaration> init;
	for (init = pexp->initializers; init!=0; init = init->next)
	{
		Ref<sdlExprNode> ie = ((Ref<sdlConstDecl> &)(init))->expr;
			ie->Apply(this);
	}
}
	
#include "expr_ebase.h"
void do_shore_q(Ref<sdlExprNode> q_expr)
// evaluate the query rooted at q_expr.  we need to do some binding
// before this works.. (esp.  args...)
{
	bind_expr q_bind; // instance of binding class
	s_value val;
	q_expr->resolve_names(0,0); // complete type checking???
	q_expr->Apply(&q_bind);
	expr_ebase * eval_pt;
	eval_pt = new_subeval(q_expr,0);

	eval_pt->get_val(val);

	eval_pt->type->print_val(&cout,val.get_ptr());
}
