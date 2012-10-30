/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stdlib.h>
#include <ostream.h>
#include <strstream.h>
#include <assert.h>

#include <types.h>
#include <typedb.h>
#include <tree.h>
#include <defns.h>

#include <aqua.h>
#include <sstack.h>
#include <cstack.h>
#include "metatypes.sdl.h"
#include "type_globals.h"


#include <oql_context.h>

#include <lalloc.h>

/*
   How to expand lambda variables when they are found
   _normal = just use the variable
   _select = extract (get) the named member from the lambda variable
   _get	   = get the named member from the variable
   _self   = make a variable of the same name
   */  

#define	AQUA_GROUP	"group"
#define	AQUA_PARTITION	"partition"

class translationState {
 public:
   enum { lambda_normal=0, lambda_select, lambda_group, lambda_self };

   oqlContext	&oql;

   /* symbols/names available in a stacked search order */
   SymbolStack	symbols;

   /* transformation context for current construct */
   ContextStack	transform;

   int		next_unique;
   int next_unique_delta;
   translationState(oqlContext &_oql)
      : oql(_oql), symbols("symbol stack"), transform("context stack") {
	 next_unique = 0; next_unique_delta = 0;
      }

   char	*lambda();
   char* delta();

   TypeDB		&types() const { return oql.db.types; }
   ExtentDB	&extents() const { return oql.db.extents; }
   SymbolTable	&defines() const { return oql.defines; }
};

extern "C" char *strdup(const char *s);
extern RefType    *RefAny;



static char *sortkey(int at)
{
   char	s_buf[1024];
   strstream	s(s_buf, sizeof(s_buf));

   s.form("k%d", at);
   s << ends;
   return s.str();
}

static char *argid(int at)
{
   char	s_buf[1024];
   strstream	s(s_buf, sizeof(s_buf));

   s.form("[%d]", at);
   s << ends;
   return s.str();
}

static void oops_func(char *filename, int linenumber, enum node_type nt)
{
   errstream().form("%s:%d: warning unresolved node_type %s in switch\n",
	     filename, linenumber,
	     node_name(nt));
}

#define	OOPS(nt)	oops_func(__FILE__, __LINE__, nt); break


Ref<sdlExprNode>
new_shore_get_t( Ref<sdlExprNode> b, const char *f)
// a ref expr (e.g. a.b.c) ; b is base of ref, f is field
{
	// note that we need to ditch this ArithOp crap and
	// put in a proper ref node in metatypes.
	WRef<sdlArithOp> ref = NEW_T sdlArithOp;
	WRef<sdlLitConst> field = NEW_T sdlLitConst;
	field->imm_value = f;
	field->type = 0;
	ref->e1 = b;
	ref->e2 = field;
	ref->type = 0;
	ref->etag = Dot;
	ref->aop= a_nil;
	return ref;
}

extern Ref<sdlType> get_shore_type(Type * tt,const char *ename = 0);
Ref<sdlExprNode>
new_shore_constant_t(Type *t, const char *val)
{
	sdlLitConst *ept = NEW_T sdlLitConst;
	ept->imm_value = val;
	ept->type = get_shore_type(t);
	ept->etag = Literal;
	return ept;
}

Ref<sdlExprNode>
// not sure how this comes up??
// another case where constname should be new type exprname.
new_shore_var_t(char *s)
{
	WRef<sdlConstName> bref = NEW_T sdlConstName;
	bref->name = s;
	bref->type = 0;
	return bref;
}

Ref<sdlExprNode>
new_shore_binary_t(aqua_op op, Ref<sdlExprNode> left, Ref<sdlExprNode> right)
{
	WRef<sdlArithOp> ref = NEW_T sdlArithOp;
	ref->e1 = left;
	ref->e2 = right;
	ref->type = 0;
	ref->etag = Dot;
	ref->aop = op;
	return ref;
}
Ref<sdlExprNode>
new_shore_modify_binary_t(aqua_op op, Ref<sdlExprNode> left, Ref<sdlExprNode> right)
{
	return new_shore_binary_t(op,left,right);
}

// for now treat bin & unary uniformly.
Ref<sdlExprNode>
new_shore_unary_t(aqua_op op,  Ref<sdlExprNode> right)
{
	return new_shore_binary_t(op,0,right);
}

Ref<sdlExprNode>
new_shore_zary_t(aqua_op op)
{
	switch(op) {
	case a_nil: // nothing?
		return 0;
	case a_true: // return true value??
	WRef<sdlLitConst> cpt = NEW_T sdlLitConst;
	cpt->type = BooleanTypeRef;
	cpt->imm_value = "true";
	cpt->resolve_names(0,0); // set the value.
	return cpt;
	}
	abort(); // not implemented.
	return 0;
}

Ref<sdlExprNode>
new_shore_function_t(Ref<sdlExprNode> arg, Ref<sdlExprNode> body)
{
	return body;
	// this should never be called?
}
Ref<sdlConstDecl>
new_shore_tuple_t(const char *n, Ref<sdlExprNode> e)
{
// I don't know about this...
// in conflict with aqua_trans, convert to const decl.
// note that we need to fix the expr/const problem sometime...
	WRef<sdlConstDecl> t_decl = NEW_T sdlConstDecl;
	t_decl->name = n;
	t_decl->expr = e;
	return t_decl;
}
Ref<sdlConstDecl>
new_shore_tuple_t(const char *n, Ref<sdlConstDecl> n2)
{
// I don't know about this...
// in conflict with aqua_trans, convert to const decl.
// note that we need to fix the expr/const problem sometime...
	WRef<sdlConstDecl> t_decl = NEW_T sdlConstDecl;
	t_decl->name = n;
	t_decl->expr = n2->expr;
	return t_decl;
}
Ref<sdlConstDecl>
check_range(Ref<sdlExprNode> arg)
{
	return (Ref<sdlConstDecl> &)arg;
	// should do some checking here...
}

Ref<sdlExprNode>
convert_range(Ref<sdlConstDecl> arg)
{ // the inverse of check_range, yuck.
	return (Ref<sdlExprNode> &)arg;
}

Ref<sdlConstDecl>
shore_append_list(Ref<sdlConstDecl> first, NodeType op, aqua_op comb, Ref<sdlConstDecl> elt)
{
	switch(op) {
	case from_n:
	case fields_n: // range expr, elt & last are actually sdlConstDecl.
	// I'm not sure about other ops??
	default:
		if (first==0)
			return elt;
		Ref<sdlDeclaration> n_list = first;
		while (n_list->next != 0)
			n_list = n_list->next;
		n_list.update() ->next = elt;
		return first;
	break;
	}
		abort(); //???
}

		
Ref<sdlExprNode>
update_lvalue(Ref<sdlExprNode> var, Ref<sdlExprNode> value, translationState& state);



Ref<sdlExprNode>
Ql_tree_node::sconvert_constant(translationState &state)
{
   char *type_name;
   Type *type;
   
   if (_nkids)
      errstream() << form("XXX constants don't have children??? (%d)\n",
		   _nkids);
   switch (_type) {
    case float_n:
      type_name = "real";
      break;
    case int_n:
      type_name = "integer";
      break;
    case char_n:
      type_name = "char";
      break;
    case string_n:
      type_name = "string";
      break;
    case refconst_n:
      return new_shore_constant_t(RefAny, id());
    default:
      OOPS(_type);
   }
   type = state.types().force_lookup(type_name);

   return new_shore_constant_t(type, id());
}

// Updates are sort of kludgy, 'coz I have to go thru the SET clause
//  and confirm that all of them are really updating only the attributes
//  of that particular object...

Ref<sdlExprNode>
Ql_tree_node::sconvert_update(translationState& state)
{
	abort(); return 0;
}

// Syntax of a delete statement is
//            DELETE FROM query [WHERE query]
// *this looks like
//            _type = df_n, _kids[0] = 
// Murali.change 3/8
// Delete could now be
//  delete <x> in <X> ... OR delete <X> ...
// , so   _kids[0] = id, if any, _kids[1] = collection, _kids[2] = where
//
// TO DO
//   Take care of ALL keywords
Ref<sdlExprNode>
Ql_tree_node::sconvert_delete(translationState& state)
{
	abort(); return 0;
}

// INSERT by itself is pretty simple, but lets' consider the following...
//   INSERT <> INTO X.Y.Z
// must be Xlated to an update on X, Y

Ref<sdlExprNode>
Ql_tree_node::sconvert_insert(translationState& state)
{
	abort(); return 0;
}


// 
// _kids[0] == Constructor call
// _kids[1] == FROM clause
// _kids[2] == WHERE clause
//
Ref<sdlExprNode>
Ql_tree_node::sconvert_create(translationState& state)
{
	abort(); return 0;
}

Ref<sdlExprNode>
Ql_tree_node::sconvert_assign(translationState& state)
{
   Ref<sdlExprNode> rvalue;
   Ref<sdlExprNode> lvalue;
   Ref<sdlExprNode> result;

   rvalue = _kids[1]->sconvert(state);
   lvalue = _kids[0]->sconvert(state);
   switch(_type)
   {
    case plus_equal_n:
      result = new_shore_modify_binary_t(a_modify_add, lvalue, rvalue);
      break;
    case minus_equal_n:
      result = new_shore_modify_binary_t(a_modify_subtract, lvalue, rvalue);
      break;
    case assign_n:
      result = rvalue;
      break;
    default:
      // Shd never reach here...
      assert(0);
   }
   // result = update_lvalue(lvalue, result, state);
   return result;
}

// How do I ensure that all attributes that get updated in the 
//  SET clause belong to this object
// 

Ref<sdlExprNode>
Ql_tree_node::sconvert_update_expr_list(translationState& state)
{
	abort(); return 0;
}

/*
   Are we having fun yet?

   This is magic
   */
Ref<sdlExprNode>
Ql_tree_node::sconvert_select(translationState &state)
{
   Ref<sdlConstDecl> list, from;
   Ref<sdlExprNode> where;
   Ref<sdlExprNode> result;
   Symbol *sym;
   Ref<sdlExprNode> apply;
   Ref<sdlExprNode> select;

   state.symbols.push("a select");


   /* Convert the FROM clause 1st; this fills the symbol table */
   state.transform.push(a_tuple_join, a_nil);
   from = _kids[1]->sconvert_dlist(state);
   state.transform.pop();

   state.transform.push(a_tuple_concat, a_nil);
   // list = _kids[0]->sconvert(state);
   list = _kids[0]->sconvert_dlist(state);
   state.transform.pop();


   /* Convert the WHERE clause; uses the symbols */
   where = _nkids > 2
      ? _kids[2]->sconvert(state) : new_shore_zary_t(a_true);

   WRef<sdlSelectExpr> s;
   s = NEW_T sdlSelectExpr;
   s->ProjList = list;
   s->Predicate = where;
   s->RangeList = from; 

   switch (_type) {
    case usfw_n:
    case usf_n:
      result = new_shore_unary_t(a_multiset_to_set, s);
      break;
    default:
      result = s;
      break;
   }


   state.symbols.pop();

   return result;
}

/* hmm this may require more derivations hmmm */
Ref<sdlExprNode>
Ql_tree_node::sconvert_sort(translationState &state)
{
	abort(); return 0;
}

/* hmm this may require more derivations hmmm */
Ref<sdlExprNode>
Ql_tree_node::sconvert_group(translationState &state)
{
	abort(); return 0;
}

/* XXX i'm not sure about the semantics of this */
Ref<sdlExprNode>
Ql_tree_node::sconvert_existential(translationState &state)
{
	abort(); return 0;
}

Ref<sdlExprNode>
Ql_tree_node::sconvert_trinary(translationState &state)
{
	abort(); return 0;
}

Ref<sdlExprNode>
Ql_tree_node::sconvert_binary(translationState &state)
{
   aqua_op	op;

   switch (_type) {
    case plus_n:
      op = a_add;	break;
    case minus_n:
      op = a_subtract;	break;
    case mult_n:
      op = a_multiply;	break;
    case div_n:
      op = a_divide;	break;
    case mod_n:
      op = a_mod;	break;
    case and_n:
      op = a_and;	break;
    case or_n:
      op = a_or;	break;
    case inf_n:
      op = a_lt;	break;
    case inf_equal_n:
      op = a_le;	break;
    case equal_n:
      op = a_eq;	break;
    case diff_n:
      op = a_ne;	break;
    case ith_n:
      op = a_at;	break;
    case in_n:
      op = a_member;	break;
    case intersect_n:
      op = a_intersect;	break;
    case union_n:
      op = a_union;	break;
    case except_n:
      op = a_diff;	break;
    default:
      OOPS(_type);
   }

   return new_shore_binary_t(op, _kids[0]->sconvert(state),
			    _kids[1]->sconvert(state));
}

Ref<sdlExprNode>
Ql_tree_node::sconvert_unary(translationState &state)
{
   aqua_op	op;

   switch (_type) {
    case unary_minus_n:
      op = a_negate;	break;
    case not_n:
      op = a_not;	break;
    case abs_n:
      op = a_abs;	break;
    case first_n:
      op = a_first;	break;
    case last_n:
      op = a_last;	break;
    case element_n:
      op = a_choose;	break;
    case listoset_n:
      op = a_list_to_set;	break;
    case flatten_n:
      op = a_flatten;	break;	/* XXX temporary */
    default:
      OOPS(_type);
   }

   return new_shore_unary_t(op, _kids[0]->sconvert(state));
}

Ref<sdlExprNode>
Ql_tree_node::sconvert_zary(translationState &state)
{
   aqua_op	op;

   switch (_type) {
    case nil_n:	op = a_nil;	break;
	      case true_n:	op = a_true;	break;
	      case false_n:	op = a_false;	break;
	      default:
		OOPS(_type);
	     }

   return new_shore_zary_t(op);
}


Ref<sdlConstDecl>
Ql_tree_node::sconvert_one_scan_var(translationState &state)
{
   aqua_t	*l;
   aqua_var_t	*v;
   const char	*name;
   Symbol	*sym;

   name = (_nkids > 1)? _kids[1]->id(): state.delta();
   /* assert(symbols.top());	/* sanity check */
   sym = state.symbols.lookup(name);
   if (sym)
      errstream() << "Multiply defined scan variable " << name << "\n";
   state.symbols.enter(name); 

   // for shore, just create a variable name and attach the expr.
   WRef<sdlConstDecl> s_v = NEW_T sdlConstDecl;
   s_v->name = name;
   s_v->expr = _kids[0]->sconvert(state);
   return s_v;

}


Ref<sdlExprNode>
Ql_tree_node::sconvert_object(translationState &state)
{
	abort(); return 0;
}


Ref<sdlExprNode>
Ql_tree_node::sconvert_id(translationState &state)
{
	// for shore, just return the all-purpose const
	// name node here.
	WRef<sdlConstName> bref = NEW_T sdlConstName;
	bref->name = id();
	return bref;
}

// This function returns the union of all the relations that are derived
//  from this relation
// This is really necessary only for a typed-extent system

Ref<sdlExprNode>
Ql_tree_node::sconvert_closure(translationState& state)
{
   Ref<sdlExprNode> kid_type = _kids[0]->sconvert(state);
   return kid_type;
   // another hack; review this.

}

Ref<sdlExprNode>
Ql_tree_node::sconvert_define(translationState &state)
{
	abort(); return 0;
}

Ref<sdlConstDecl>
Ql_tree_node::sconvert_parameters(translationState &state)
{
   aqua_op	combine_op = state.transform.combiner();
   aqua_op	element_op = state.transform.element();
   Ref<sdlConstDecl> last = 0;
   Ref<sdlConstDecl> t;
   int	i;

   state.transform.top().cardinality = _nkids;

     for (i = 0; i < _nkids; i++) {
      state.transform.top().at = i;
      t = _kids[i]->sconvert_decl(state);

      switch (element_op) {
       case a_list:
       case a_union:
       case a_additive_union:
       case a_array:
       case a_set:
       case a_multiset:
	   t.update()->expr = new_shore_unary_t(element_op,t->expr);
	   // abort(); // for the moment- need to 
	 // t = new_shore_unary_t(element_op, t);
	 break;

       case a_sort:
	 t = new_shore_tuple_t(sortkey(state.transform.top().at), t->expr);
	 break;

       case a_arg:
//	 t = new_shore_tuple_t(argid(state.transform.top().at), t->expr);
	 t = new_shore_tuple_t(0, t->expr);
	 break;

       case a_build_join:
#if 0
	 // who knows what?
	 state.transform.setPE(t);
#endif
	 break;

       case a_nil:	/* Explicit don't touch */
       default:
	 /* leave it alone */
	 break;
      }

      last = shore_append_list(last,type(),combine_op,t);
   }

   return last;
}

Ref<sdlConstDecl>
Ql_tree_node::sconvert_dlist(translationState &state)
{
   aqua_op	combine_op = state.transform.combiner();
   aqua_op	element_op = state.transform.element();
   Ref<sdlConstDecl> first = 0;
   WRef<sdlConstDecl> n;
   Ref<sdlConstDecl> t;
   int	i;

   state.transform.top().cardinality = _nkids;

     for (i = 0; i < _nkids; i++) {
      state.transform.top().at = i;
      n = _kids[i]->sconvert_decl(state);


	  first = shore_append_list(first,type(),combine_op,n);
   }

   return first;
}

Ref<sdlExprNode>
Ql_tree_node::sconvert_collection(translationState &state)
{
   aqua_op	combine_op;
   aqua_op	element_op;
   Ref<sdlExprNode> result;

   /*
      We can just set a "conversion" for
      "actual_parameters_n" to convert on its children!

      This might work for other "gathering" operators too!
      */
   switch (_type) {
    case list_n:
    case empty_list_n:
      combine_op = a_list_concat;
      element_op = a_list;
      break;
    case bag_n:
    case empty_bag_n:
      combine_op = a_additive_union;
      element_op = a_multiset;
      break;
    case set_n:
    case empty_set_n:
      combine_op = a_union;
      element_op = a_set;
      break;
    case array_n:
    case empty_array_n:
      combine_op = a_array_concat;
      element_op = a_array;
      break;
    case tuple_n:
      combine_op = a_tuple_concat;
      element_op = a_nil;
      break;
    default:
      OOPS(_type);
   }

   switch (_type) {
    case empty_set_n:
    case empty_list_n:
    case empty_bag_n:
    case empty_array_n:
      result = new_shore_unary_t(element_op,
				new_shore_zary_t(a_nil)); // XXX
      break;
    default:
      state.transform.push(combine_op, element_op);
      result = _kids[0]->sconvert(state);
      state.transform.pop();
      break;
   }

   return result;
}

/* XXX ooops, this is a 4 arg operator.  Hmmmm */ 

Ref<sdlExprNode>
Ql_tree_node::sconvert_folded(translationState &state)
{
	abort(); return 0;
	// look at a_count, a_sum things in aqua_to_shore.C
	// to see what should be done here...
#if 0
   aqua_t	*fold_to;
   aqua_function_t	*extractor;
   aqua_function_t	*folder;
   aqua_t	*source;
   aqua_t	*t;
   aqua_var_t	*f_lambda1, *f_lambda2, *x_lambda;
   Type	*integer;
   char	*s;

   /* This is truly disgusting avg(x) == sum(x) / count(x) */
   if (_type == avg_n) {
      aqua_t	*sum, *count;
      _type = sum_n;
      sum = sconvert_folded(state);
      _type = count_n;
      count = sconvert_folded(state);
      _type = avg_n;
      return new_shore_binary_t(a_divide, sum, count);
   }

   /* Need two lambda variables for the fold function + 1 for extract */
   x_lambda = new_shore_var_t(s = state.lambda());
   f_lambda1 = new_shore_var_t(s);	/* reuse name */
   f_lambda2 = new_shore_var_t(state.lambda());

   integer = state.types().force_lookup("integer");

   /* Initial value */
   switch (_type) {
    case count_n:
    case sum_n:
      t = new_shore_constant_t(integer, "0");
      break;
    case max_n:
    case min_n:
      /* XXX disgusting; duplication of tree (but is type safe) */
      t = new_shore_unary_t(a_choose, _kids[0]->sconvert(state));
      break;
    default:
      t = new_shore_error_t;
      break;
   }
   fold_to = t;

   /* Extractor */
   switch (_type) {
    case count_n:
      /* just add 1 */
      t = new_shore_constant_t(integer, "1");
      break;
    default:
      /* default extractor just gets whatever is there */
      t = x_lambda;
      break;
   }
   extractor = new_shore_function_t(x_lambda, t);

   /* Folder */
   switch (_type) {
    case count_n:
    case sum_n:
      t = new_shore_binary_t(a_add, f_lambda1, f_lambda2);
      break;
    case min_n:
      t = new_shore_binary_t(a_min, f_lambda1, f_lambda2);
      break;
    case max_n:
      t = new_shore_binary_t(a_max, f_lambda1, f_lambda2);
      break;
    default:
      t = new_shore_error_t;
      break;
   }
   folder = new_shore_function_t(f_lambda1, f_lambda2, t);

   source = _kids[0]->sconvert(state);

   return new_shore_fold_t(fold_to, extractor, folder, source);
#endif
}

Ref<sdlExprNode>
Ql_tree_node::sconvert_method(translationState &state)
{
	abort();
	return 0;
#if 0
   /* XXX consistency problem */
   aqua_t	*args;
   aqua_op	el_conv;

   if (_nkids > 2) {
      el_conv = ((_type == method_call_n) || (_type == fake_method_call_n)) 
	 ? a_arg : a_nil;
      state.transform.push(a_tuple_concat, el_conv);
      args = _kids[2]->sconvert(state);
      state.transform.pop();
   }
   else
      args = new_shore_zary_t(a_void);

   return new_shore_method_t(_kids[0]->sconvert(state),
			    _kids[1]->id(),
			    args, 
			    ((_type == method_call_n) 
			     || (_type == named_method_call_n)));
#endif
}

Ref<sdlExprNode>
Ql_tree_node::sconvert(translationState &state)
{
   switch (_type) {
    case int_n:
    case float_n:
    case char_n:
    case string_n:
    case refconst_n:
      return sconvert_constant(state);
      break;

      /* "loose" id's are database variables (for now) */
    case id_n:
      return sconvert_id(state);
      break;


    case extract_attribute_n:
      return new_shore_get_t(_kids[0]->sconvert(state),
			    _kids[1]->id());
      break;

    case tuple_obj_n:
    case cast_n:
    case function_or_create_n:
    case object_constructor_n:
    case labelled_object_constructor_n:
      return sconvert_object(state);
      break;


    case sf_n:
    case usf_n:
    case sfw_n:
    case usfw_n:
      return sconvert_select(state);
      break;

    case df_n:
    case dfw_n:
      return sconvert_delete(state);

    case iiw_n:
      return sconvert_insert(state);

    case uf_n:
    case ufw_n:
      return sconvert_update(state);

    case cfw_n:
      return sconvert_create(state);

    case update_expr_n:
      return sconvert_update_expr_list(state);
    case plus_equal_n:
    case minus_equal_n:
    case assign_n:
      return sconvert_assign(state);

    case only_n:
    case closure_n:
      return sconvert_closure(state);

    case sublist_n:
      return sconvert_trinary(state);
      break;

    case define_n:
      return sconvert_define(state);
      break;

//    case assign_n:
//      return new_shore_assign_t(_kids[0]->sconvert(state),
//			       _kids[1]->sconvert(state));
//      break;

    case forall_n:
    case exists_n:
      return sconvert_existential(state);
      break;

    case method_call_n:
    case named_method_call_n:
      return sconvert_method(state);
      break;

    case bag_n:
    case set_n:
    case list_n:
    case array_n:
    case empty_set_n:
    case empty_list_n:
    case empty_bag_n:
    case empty_array_n:
    case tuple_n:
      return sconvert_collection(state);
      break;

    case sort_n:
      return sconvert_sort(state);
      break;

    case group_n:
    case group_with_n:
      return sconvert_group(state);
      break;

    case count_n:
    case min_n:
    case max_n:
    case sum_n:
    case avg_n:
      return sconvert_folded(state);
      break;

    case plus_n:
    case minus_n:
    case mult_n:
    case div_n:
    case mod_n:
    case and_n:
    case or_n:
    case inf_n:
    case inf_equal_n:
    case equal_n:
    case diff_n:
    case ith_n:
    case in_n:
    case union_n:
    case except_n:
    case intersect_n:
      return sconvert_binary(state);
      break;

    case unary_minus_n:
    case not_n:
    case abs_n:
    case first_n:
    case last_n:
    case element_n:
    case listoset_n:
      return sconvert_unary(state);
      break;

    case flatten_n:
      return sconvert_unary(state);	/* XXX cheap shot */
      break;

    case nil_n:
    case true_n:
    case false_n:
      return sconvert_zary(state);
      break;
    default:
      OOPS(_type);
   }
   errstream() << "unimplemented conversion for " << *this << '\n';
   return 0;
}

Ref<sdlConstDecl>
Ql_tree_node::sconvert_decl(translationState &state)
{
	WRef<sdlConstDecl> t_decl;
   switch (_type) {

      /* "loose" id's are database variables (for now) */
    case id_n:
      // return sconvert_id(state);
		t_decl = NEW_T sdlConstDecl;
		t_decl->name = id();
		return t_decl;
	  // make into decl..
      break;

    case one_field_n:
      return new_shore_tuple_t(_kids[0]->id(),
			      _kids[1]->sconvert(state));
      break;


    case one_scan_var_n:
      return sconvert_one_scan_var(state);
      break;



    case from_n:
    case fields_n:
    case actual_parameters_n:
      return sconvert_parameters(state);
      break;



    default:
	// convert an expr node to a decl.
		t_decl = NEW_T sdlConstDecl;
		t_decl->expr = sconvert(state);
		// we may sometimes have an id?
		t_decl->name = id();
		return t_decl;
      // OOPS(_type);
   }
   errstream() << "unimplemented conversion for " << *this << '\n';
   return 0;
}

Ref<sdlExprNode>
Ql_tree_node::shore_convert(oqlContext& context)
{
   Ref<sdlExprNode>	result;
   translationState state(context);

#if 0
   next_unique = 0;

   Types = &oql_transform.db.types;
   Extents = &oql_transform.db.extents;
   Defines = &oql_transform.defines;
#endif

   state.transform.push(a_nil, a_nil);	/* default context */
   result = sconvert(state);
   state.transform.pop();
   if (!state.symbols.empty())
      errstream() << "OOPS: symbol stack not empty!\n";
   if (!state.transform.empty())
      errstream() << "OOPS: context stack not empty!\n";
   return result;
}
