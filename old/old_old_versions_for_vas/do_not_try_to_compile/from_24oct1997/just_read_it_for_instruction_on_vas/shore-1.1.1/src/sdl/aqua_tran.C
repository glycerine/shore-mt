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

#ifdef STAND_ALONE
#define para_check assert
#endif STAND_ALONE

#include <oql_context.h>

#include <lalloc.h>
#ifdef USE_ALLOCATORS
void* aqua_t::operator new(long sz)
{
   return DefaultAllocator()->alloc(sz);
}

void* SymbolStack::operator new(long sz)
{
   return DefaultAllocator()->alloc(sz);
}

void* ContextStack::operator new(long sz)
{
   return DefaultAllocator()->alloc(sz);
}

void* SymbolStackItem::operator new(long sz)
{
   return DefaultAllocator()->alloc(sz);
}

void* ContextStackItem::operator new(long sz)
{
   return DefaultAllocator()->alloc(sz);
}

#endif USE_ALLOCATORS

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

char* translationState::delta()
{
   char s_buf[32];
   strstream	s(s_buf, sizeof(s_buf));

   s.form("__delta_%d", next_unique_delta++);
   s <<ends;
   return s.str();
}

char *translationState::lambda()
{
   char	s_buf[32]; 
   strstream	s(s_buf, sizeof(s_buf));

   s.form("__lambda_%d", next_unique++);
   s <<ends;
   return s.str();
}

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

aqua_t* update_lvalue(aqua_t* var, aqua_t* value, translationState& state);

ObjectType *AnyType = NULL ; 
RefType    *RefAny = NULL ; 

void
OQL_Initialize1(void)
{
   AnyType = new ObjectType("any") ; 	
   assert( AnyType ) ; 

   RefAny = new RefType("ref_any",AnyType) ;
   assert( RefAny ) ; 
}

extern OQL_Initialize2(void) ;

void
OQL_Initialize(void)
{
    OQL_Initialize1() ;
    OQL_Initialize2() ;
}


aqua_t *Ql_tree_node::convert_constant(translationState &state)
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
      return new aqua_constant_t(RefAny, id());
    default:
      OOPS(_type);
   }
   type = state.types().force_lookup(type_name);

   return new aqua_constant_t(type, id());
}

// Updates are sort of kludgy, 'coz I have to go thru the SET clause
//  and confirm that all of them are really updating only the attributes
//  of that particular object...

aqua_t* Ql_tree_node::convert_update(translationState& state)
{
   aqua_t* modifier;
   aqua_t* what;
   aqua_t* selector;
   aqua_var_t* lambda_var;
   auto_string lambda_name;
   Symbol* sym;
   aqua_t* result;
   char* s;

   state.symbols.push("a update");
   lambda_name = state.lambda();
   lambda_var = new aqua_var_t(lambda_name);
   sym = state.symbols.enter("lambda", (char *)lambda_var);
   sym->setFlags(translationState::lambda_normal);
   
   // Convert the FROM list
   // Easy now, 'coz there's one 1 entry in the list
   what = _kids[0]->convert(state);

   // Enter the range variable if necessary
   if (_nkids > 2 && _kids[2]->_type == id_n)
      state.symbols.enter(_kids[2]->id());

   // Convert the WHERE clause if any exists...
   selector = (_type = ufw_n) ? _kids[_nkids - 1]->convert(state)
                              : new aqua_zary_t(a_true);
   selector = new aqua_function_t(lambda_var, selector);

   // Convert the SET clause
   modifier = _kids[1]->convert(state);
   modifier = new aqua_function_t(lambda_var, modifier);
   // create a new aqua node
   result = new aqua_update_t(what, (aqua_function_t *)selector,
			      (aqua_function_t *)modifier);
   state.symbols.pop();
   result = update_lvalue(what, result, state);
   return result;
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
aqua_t* Ql_tree_node::convert_delete(translationState& state)
{
   aqua_t* what;
   aqua_t* where;
   aqua_t* result;
   aqua_function_t* selector;
   aqua_var_t* lambda_var;
   auto_string lambda_name;
   Symbol* sym;
   char* s;

   state.symbols.push("a delete");
   lambda_name = state.lambda();
   lambda_var = new aqua_var_t(lambda_name);
   sym = state.symbols.enter("lambda", (char *)lambda_var);
   sym->setFlags(translationState::lambda_normal);

   // Enter the range variable into the symbol stack if necessary
   if (_nkids > 1 &&_kids[1]->_type == id_n)
      state.symbols.enter(_kids[1]->id());
   
   // Convert the FROM list
   // Easy now, 'coz there's one 1 entry in the list
   what = _kids[0]->convert(state);

   // Convert the WHERE clause if any exists...
   where = (_type == dfw_n) ? _kids[_nkids - 1]->convert(state)
                            : new aqua_zary_t(a_true);
   selector = new aqua_function_t(lambda_var, where);

   result = new aqua_delete_t(what, selector);
   state.symbols.pop();
   result = update_lvalue(what, result, state);
   return result;
}

// INSERT by itself is pretty simple, but lets' consider the following...
//   INSERT <> INTO X.Y.Z
// must be Xlated to an update on X, Y

aqua_t* Ql_tree_node::convert_insert(translationState& state)
{
   aqua_t* into;
   aqua_t* what;
   aqua_t* result;

   // No need to do any stack pushing and popping..
   into = _kids[1]->convert(state);
   what = _kids[0]->convert(state);
   result = new aqua_insert_t(into, what);
   result = update_lvalue(into, result, state);
   return result;
}

aqua_t* update_lvalue(aqua_t* var, aqua_t* value, translationState& state)
{
   aqua_binary_t* at_op;
   aqua_get_t* get_op;
   Symbol* sym;

   switch(var->_op)
   {
    case a_get:
      get_op = (aqua_get_t *)var;
      if (!get_op->_what)
      {
	 sym = state.symbols.lookup("lambda");
//	 assert(sym);
// 
	 if (!sym)
	 {
	    errstream() << "Could not resolve identifier <" << get_op->_name
	         << ">" << endl;
	    return new aqua_error_t();
	 }
	 get_op->_what = (aqua_var_t *)sym->data();
      }
      return update_lvalue(get_op->_what, 
	       new aqua_modify_tuple_t(get_op->_what, get_op->_name, value),
	       state);
    case a_at:
      at_op = (aqua_binary_t *)var;
      return update_lvalue(at_op->_left, 
		   new aqua_modify_array_t(at_op->_left, at_op->_right, value),
			   state);
    case a_var:
      return value;
//      return new aqua_set_t(this, value);
    case a_dbvar:
      return value;
// My reasoning goes this way...
//  For top-level relations, this is obviously ok
//  If on the other hand, I have a Ref<Set<O>>, then Ref<Set<derivedclass<O>>>
//    is not a valid value for it. Therefore, that case shd not occur
// I dont know if I'm making any sense here
      return value;
#ifdef TYPED_EXTENTS
    case a_closure:
      return value;
#endif TYPED_EXTENTS
    default:
      errstream() << "Invalid lvalue in SET clause\n";
      return new aqua_error_t();
   }
}

// 
// _kids[0] == Constructor call
// _kids[1] == FROM clause
// _kids[2] == WHERE clause
//
aqua_t* Ql_tree_node::convert_create(translationState& state)
{
   aqua_t* into;
   aqua_t* result;
   const char* extent_name;
   Type* extent;

   switch(_kids[0]->_type)
   {
    case function_or_create_n:
	case object_constructor_n:
      _kids[0]->_type = object_constructor_n;
      break;
    case tuple_obj_n:
      _kids[0]->_type = labelled_object_constructor_n;
      break;
    default:
      errstream() << "Invalid call in CREATE statement. Only constructors"
	   << " can be called" << endl;
      return new aqua_error_t();
   }
   extent_name = _kids[0]->_kids[0]->id();
   result = (_nkids > 1) ? this->convert_select(state)
                         : _kids[0]->convert(state);

      // Now what do I do...
      // Insert this stuff into the appropriate extent...
      // Does this extent exist at all ?
   extent = state.extents().lookup(extent_name);
   if (!extent)
   {
      errstream() << "Cannot find extent " << extent_name << endl;
      return new aqua_error_t();
   }
   into = new aqua_dbvar_t(extent_name, extent);
   result = new aqua_insert_t(into, result);
   return result;
}

/**
aqua_t* Ql_tree_node::convert_constructor(translationState& state, 
					  int update_statement)
{
   if (!update_statement)
   {
      errstream() << "Attempt to call object constructor in query statement" 
	   << endl;
      return new aqua_error_t();
   }
}
**/
   
aqua_t* Ql_tree_node::convert_assign(translationState& state)
{
   aqua_t* rvalue;
   aqua_t* lvalue;
   aqua_t* result;

   rvalue = _kids[1]->convert(state);
   lvalue = _kids[0]->convert(state);
   switch(_type)
   {
    case plus_equal_n:
      result = new aqua_modify_binary_t(a_modify_add, lvalue, rvalue);
      break;
    case minus_equal_n:
      result = new aqua_modify_binary_t(a_modify_subtract, lvalue, rvalue);
      break;
    case assign_n:
      result = rvalue;
      break;
    default:
      // Shd never reach here...
      assert(0);
   }
   result = update_lvalue(lvalue, result, state);
   return result;
}

// How do I ensure that all attributes that get updated in the 
//  SET clause belong to this object
// 

aqua_t* Ql_tree_node::convert_update_expr_list(translationState& state)
{
   aqua_function_t* modifier;
   aqua_var_t* lambda_var;
   auto_string lambda_name;
   aqua_t* update_expr;
   aqua_t* to;
   Symbol* sym;
   int i;

   sym = state.symbols.lookup("lambda");
   to = (aqua_var_t *)(sym->data());
   for (i = 0; i < _nkids; i++)
   {
      lambda_name = state.lambda();
      lambda_var = new aqua_var_t(lambda_name);
      sym->setData((char *)lambda_var);
      update_expr = _kids[i]->convert(state);
// Some rudimentary checking...
      switch (update_expr->_op)
      {
       case a_modify_tuple:
       case a_modify_array:
	   {
	 aqua_modify_t* modif = (aqua_modify_t *)update_expr;
		 if (modif->_what != lambda_var)
		 {
			errstream() << "Attempt to update a non-owned attribute\n";
			return new aqua_error_t();
		 }
	 }
	 break;
       default:
	 break;
	 // the whole tuple is being given a new value
	 // something like
	 // UPDATE StringSet using s set s = "pink"
	 // No problems in this case...
      }
//
      modifier = new aqua_function_t(lambda_var, update_expr);
      to = new aqua_eval_t(modifier, to);
   }
   return to;
}

/*
   Are we having fun yet?

   This is magic
   */
#ifndef DEPENDENT
aqua_t *Ql_tree_node::convert_select(translationState &state)
{
   aqua_t *list, *from, *where;
   aqua_t *result;
   Symbol *sym;
   aqua_t *apply;
   aqua_t *select;
   aqua_var_t *lambda_var;
   auto_string lambda_name;

   state.symbols.push("a select");

   lambda_name = state.lambda();
   lambda_var = new aqua_var_t(lambda_name);
   sym = state.symbols.enter("lambda", (char *) lambda_var);
   sym->setFlags(translationState::lambda_select);

   /* Convert the FROM clause 1st; this fills the symbol table */
   state.transform.push(a_tuple_join, a_nil);
   from = _kids[1]->convert(state);
   state.transform.pop();

   /* Convert the SELECT clause; uses the symbols */
   lambda_var = new aqua_var_t(state.lambda() /*_name*/);
   sym->setData((char *) lambda_var);
   state.transform.push(a_tuple_concat, a_nil);
   list = _kids[0]->convert(state);
   state.transform.pop();

   /* turn the output list into a function */
   list = new aqua_function_t(lambda_var, list);

   /* another var instance needed for select predicate */
   lambda_var = new aqua_var_t(state.lambda() /*_name*/);
   sym->setData((char *) lambda_var);
   /* Convert the WHERE clause; uses the symbols */
   where = _nkids > 2
      ? _kids[2]->convert(state) : new aqua_zary_t(a_true);

   /*
      I can use the same lambda variable without confusion, since
      the two expressions are independent of each other
      */  

   /* ... and the selection predicates */
#if 0
   if (_nkids > 2)
#endif
      /* always need function of 1 arg for aqua select */
      where = new aqua_function_t(lambda_var, where);

   select = new aqua_apply_t(a_select, where, from);
   apply = new aqua_apply_t(a_apply, list, select);

   switch (_type) {
    case usfw_n:
    case usf_n:
      result = new aqua_unary_t(a_multiset_to_set, apply);
      break;
    default:
      result = apply;
      break;
   }


   state.symbols.pop();

   return result;
}
#else /* DEPENDENT */
aqua_t *Ql_tree_node::convert_select(translationState &state)
{
   int	op;
   aqua_t	*list, *from, *where;
   aqua_t	*result;
   SymbolTable	*table;
   Symbol *sym;
   aqua_t	*apply;
   aqua_t	*select;
   aqua_var_t	*lambda_var;
   auto_string	lambda_name;
   aqua_t	*predicate, *extractor, *pred_extr;

   state.symbols.push("a select");
#if 1
   lambda_name = state.lambda();
   lambda_var = new aqua_var_t(lambda_name);
   sym = state.symbols.enter("lambda", (char *) lambda_var);
   sym->setFlags(translationState::lambda_self);
#endif

   /*  I.    Fill the query's symbol table  */
   _kids[1]->enter_symbols(state);

   /* IIa.    Build the predicate */
   if (_nkids > 2) {
      predicate = _kids[2]->convert(state);
      predicate = new aqua_function_t(new aqua_var_t("unused"),
				      predicate);
   }
   else
      predicate = new aqua_function_t(new aqua_var_t("unused"),
				      new aqua_zary_t(a_true));
   
   
   
   /* IIb.    Build the extractor */
   extractor = _kids[0]->convert(state);
   extractor = new aqua_unary_t(a_multiset, extractor);

   /* combine them into the predicate extractor */
   pred_extr = new aqua_apply_t(a_select, predicate, extractor);

   /* III.    Build the nested  flatten(apply()()) tree */
   state.transform.push(a_build_join, a_build_join);
   state.transform.setPE(pred_extr);
   result = _kids[1]->convert(state);
   state.transform.pop();

#ifdef old	
   /* Convert the FROM clause 1st; this fills the symbol table */
   state.transform.push(a_tuple_join, a_nil);
   from = _kids[1]->convert(state);
   state.transform.pop();

   /* Convert the SELECT clause; uses the symbols */
   lambda_var = new aqua_var_t(state.lambda() /*_name*/);
   sym->setData((char *) lambda_var);
   state.transform.push(a_tuple_concat, a_nil);
   list = _kids[0]->convert(state);
   state.transform.pop();

   /* turn the output list into a function */
   list = new aqua_function_t(lambda_var, list);

   /* another var instance needed for select predicate */
   lambda_var = new aqua_var_t(state.lambda() /*_name*/);
   sym->setData((char *) lambda_var);
   /* Convert the WHERE clause; uses the symbols */
   where = _nkids > 2 ? _kids[2]->convert(state) : new aqua_zary_t(a_true);

   /*
      I can use the same lambda variable without confusion, since
      the two expressions are independent of each other
      */  

   /* ... and the selection predicates */
#if 0
   if (_nkids > 2)
#endif
      /* always need function of 1 arg for aqua select */
      where = new aqua_function_t(lambda_var, where);

   select = new aqua_apply_t(a_select, where, from);
   apply = new aqua_apply_t(a_apply, list, select);
#endif

   switch (_type) {
    case usfw_n:
    case usf_n:
      result = new aqua_unary_t(a_multiset_to_set, result);
      break;
    default:
      break;
   }


   state.symbols.pop();

   return result;
}
#endif /* DEPENDENT */

/* hmm this may require more derivations hmmm */
aqua_t *Ql_tree_node::convert_sort(translationState &state)
{
   Symbol	*sym;
   aqua_t	*result;
   aqua_t	*t;
   aqua_t	*keys;	// sort keys
   aqua_t	*inner;	// inner apply/sort
   aqua_t	*outer;	// outer apply	
   int	i;
   Type	*string;
   auto_string	lambda_name;
   aqua_var_t	*lambda_var;
   aqua_t	*source, *by;

   state.symbols.push("a sort");
   lambda_name = _kids[0]->id();
   lambda_var = new aqua_var_t(lambda_name);
   sym = state.symbols.enter("lambda", (char *) lambda_var);
   state.symbols.enter(lambda_name);

   source = _kids[1]->convert(state);

   state.transform.push(a_tuple_concat, a_sort);
   by = _kids[2]->convert(state);

   /*
      build the lambda function for the apply into sort;;
      First we add the tuple to the sort keys, the produce
      the function
      */  
   t = new aqua_tuple_t(lambda_name, lambda_var);
   by = new aqua_binary_t(a_tuple_concat, t, by);
   t = new aqua_function_t(lambda_var, by);
   inner = new aqua_apply_t(a_apply, t, source);

   /* build the list of sort keys */
   keys = (aqua_t *)0;
   string = state.types().force_lookup("string");
   for (i = state.transform.top().cardinality-1; i >= 0; i--) {
      t = new aqua_constant_t(string, sortkey(i));
      t = new aqua_unary_t(a_list, t);
      keys = keys ? new aqua_binary_t(a_list_concat, t, keys) : t;
   }

   /* build the sort */
   inner = new aqua_binary_t(a_sort, keys, inner);

   /* Now build the extractor function */
   t = new aqua_get_t(lambda_var, lambda_name);
   outer = new aqua_function_t(lambda_var, t);

   result = new aqua_apply_t(a_apply, outer, inner);

   state.transform.pop();
   state.symbols.pop();

   return result;
}

/* hmm this may require more derivations hmmm */
aqua_t *Ql_tree_node::convert_group(translationState &state)
{
   Symbol	*lambda_sym, *sym;
   aqua_t	*result;
   aqua_t	*t;
   aqua_t	*group;
   aqua_t	*outer;	// outer apply
   aqua_t	*with;
   auto_string	lambda_name;
   char	*group_name, *part_name;
   aqua_var_t	*lambda_var;
   aqua_t	*source, *by;

   state.symbols.push(AQUA_GROUP);

   lambda_name = state.lambda(); //_kids[0]->steal();
   lambda_var = new aqua_var_t(lambda_name);
   lambda_sym = state.symbols.enter("lambda", (char *) lambda_var);
   //	state.symbols.enter(lambda_name);
   state.symbols.enter(_kids[0]->id());

   source = _kids[1]->convert(state);

   state.transform.push(a_tuple_concat, a_nil);
   by = _kids[2]->convert(state);

   /*  build the lambda function + the GROUP */
   t = new aqua_function_t(lambda_var, by);
   group = new aqua_apply_t(a_group, t, source);

   /* new lambda var instance needed for the extractor */
   lambda_var = new aqua_var_t(lambda_name);

   /* Now build the extractor */
   switch (_nkids) {
    case 3:		/* group x in y by (...) */
      /* extract the group: tuple and relabel it as partition: */

      t = new aqua_get_t(lambda_var, AQUA_GROUP);
      outer = new aqua_tuple_t(AQUA_PARTITION, t);
      break;

    case 4:		/* group x in y by (...) with (...) */
      /* relabel group: as partition: and extract it */
      
      group_name = AQUA_GROUP;
      part_name = AQUA_PARTITION;

      /* change the lambda for the partition */
      state.symbols.remove(_kids[0]->id());
      lambda_sym->setData((char *)0);

      /* XXX duplication problems */
      // part_var = new aqua_get_t(lambda_var, group_name);
      lambda_sym->setData((char *) lambda_var);
      lambda_sym->setFlags(translationState::lambda_group);	/* expand to get */
      
      sym = state.symbols.enter(part_name);
      with = _kids[3]->convert(state);
      outer = with;
      break;
   }

   t = new aqua_get_t(lambda_var, "attr");
   outer = new aqua_binary_t(a_tuple_concat, t, outer);
   
   // t = new aqua_get_t(lambda_var, lambda_name);
   outer = new aqua_function_t(lambda_var, outer);

   result = new aqua_apply_t(a_apply, outer, group);
   /* group returns set, aqua apply returns bag */
   result = new aqua_unary_t(a_multiset_to_set, result);

   state.transform.pop();
   state.symbols.pop();

   return result;
}

/* XXX i'm not sure about the semantics of this */
aqua_t *Ql_tree_node::convert_existential(translationState &state)
{
   Symbol	*sym;
   aqua_var_t	*lambda_var;
   auto_string	lambda_name;
   aqua_op	op;
   aqua_t	*source;
   aqua_t	*predicate;
   aqua_t	*result;

   state.symbols.push("exists/forall");

   lambda_name = _kids[0]->id();
   lambda_var = new aqua_var_t(lambda_name);
   sym = state.symbols.enter("lambda", (char *) lambda_var);
   /* sym->setFlags(lambda_normal); */

   switch (_type) {
    case forall_n:
      op = a_forall;
      break;
    case exists_n:
      op = a_exists;
      break;
    default:
      OOPS(_type);
   }
   state.symbols.enter(lambda_name);	/* XXX reuse */
   source = _kids[1]->convert(state);
   predicate = _kids[2]->convert(state);
   predicate = new aqua_function_t(lambda_var, predicate);

   result = new aqua_apply_t(op, predicate, source);

   state.symbols.pop();

   return result;
}

aqua_t *Ql_tree_node::convert_trinary(translationState &state)
{
   aqua_op	op;

   switch (_type) {
    case sublist_n:
      op = a_subrange;
      break;
    default:
      OOPS(_type);
   }
   return new aqua_trinary_t(op, _kids[0]->convert(state),
			     _kids[1]->convert(state),
			     _kids[2]->convert(state));
}

aqua_t *Ql_tree_node::convert_binary(translationState &state)
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

   return new aqua_binary_t(op, _kids[0]->convert(state),
			    _kids[1]->convert(state));
}

aqua_t	*Ql_tree_node::convert_unary(translationState &state)
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

   return new aqua_unary_t(op, _kids[0]->convert(state));
}

aqua_t	*Ql_tree_node::convert_zary(translationState &state)
{
   aqua_op	op;

   switch (_type) {
    case nil_n:	op = a_nil;	break;
	      case true_n:	op = a_true;	break;
	      case false_n:	op = a_false;	break;
	      default:
		OOPS(_type);
	     }

   return new aqua_zary_t(op);
}

#ifdef DEPENDENT
void Ql_tree_node::enter_symbols(translationState &state)
{
   int	i;
   char	*name;
   Symbol	*sym;
   Ql_tree_node	*scan;	
   Ql_tree_node	*id;
   auto_string s;

   assert(_type == from_n);
   for (i = 0; i < _nkids; i++) {
      scan = _kids[i];
      assert(scan->_type == one_scan_var_n);
      id = (scan->_nkids > 1)? scan->_kids[1]
	                     : new Ql_tree_node(id_n, state.delta);
      assert(id->_type == id_n);
      name = id->id();
      sym = state.symbols.lookup(name);
      if (sym)
	 errstream() << "Multiply defined scan var " << name << '\n';
      state.symbols.enter(name);
   }
}
#endif

#ifndef DEPENDENT
/* XXX I think this can use the current context's lambda variable */
aqua_t *Ql_tree_node::convert_one_scan_var(translationState &state)
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

   /* generate an apply + lambda for this expression */
   sym = state.symbols.lookup("lambda");
   assert(sym);
   v = (aqua_var_t *) sym->data();

   l = new aqua_tuple_t(name, v);
   l = new aqua_function_t(v, l);
   /* XXX slight multiple-link problem here */

   return new aqua_apply_t(a_apply, l, _kids[0]->convert(state));
}

#else /* DEPENDENT */

/* XXX I think this can use the current context's lambda variable */
aqua_t *Ql_tree_node::convert_one_scan_var(translationState &state)
{
   aqua_t	*l;
   aqua_var_t	*var;
   char	*name;
   Symbol	*sym;
   aqua_t	*over;
   aqua_t	*body;
   aqua_function_t	*func;
   aqua_t	*result;
   auto_string  s;

   name = (_nkids > 1)? _kids[1]->id(): state.delta();
   var = new aqua_var_t(name);

   over = _kids[0]->convert(state);

   body = state.transform.PE();

   func = new aqua_function_t(var, body);

   result = new aqua_apply_t(a_apply, func, over);
   result = new aqua_unary_t(a_flatten, result);

   return result;

#ifdef old
   name = _kids[0]->id();
   /* assert(state.symbols.top());	/* sanity check */
   sym = state.symbols.lookup(name);
   if (sym)
      errstream() << "Multiply defined scan variable " << name << "\n";
   state.symbols.enter(name);

   /* generate an apply + lambda for this expression */
   sym = state.symbols.lookup("lambda");
   assert(sym);
   v = (aqua_var_t *) sym->data();

   l = new aqua_tuple_t(name, v);
   l = new aqua_function_t(v, l);
   /* XXX slight multiple-link problem here */
   return new aqua_apply_t(a_apply, l, _kids[1]->convert(state));
#endif
}
#endif /* DEPENDENT */

aqua_t	*Ql_tree_node::convert_object(translationState &state)
{
   const char *type_name = _kids[0]->id();
   //	cout << "type_name=" << type_name << '\n';
   aqua_t	*type = new aqua_type_t(type_name);
   aqua_t	*init;
   aqua_op	op;

   /* Only change the conversion for object creations */
   switch (_type) {
    case tuple_obj_n:
    case function_or_create_n:	/* XXX ambiguity */
    case object_constructor_n:
    case labelled_object_constructor_n:
	{
      Type* t = state.types().lookup(type_name);
      if (!t)
      {
	 errstream() << type_name << " unknown. Cannot construct it" << endl;
	 return new aqua_error_t();
      }
      if (t && t->isObject())
	 switch(_type)
	 {
	  case object_constructor_n:
	  case labelled_object_constructor_n:
	    break;
	  default:
	    errstream() << "Cannot call object constructors within queries" << endl;
	    return new aqua_error_t();
	 }
      op = a_mkobj;
      aqua_op el_conv;
      switch(_type)
      {
       case function_or_create_n:
       case object_constructor_n:
	 el_conv = a_arg;
	 break;
       default:
	 el_conv = a_nil;
      }
      if (_nkids > 1)
	 state.transform.push(a_tuple_concat, el_conv);	
             /* XXX default xlation ? */
      break;
	}
    case cast_n:
      op = a_cast;
      break;
    default:
      OOPS(_type);
   }

   init = (_nkids > 1) ? _kids[1]->convert(state) : new aqua_zary_t(a_void);

   if (op == a_mkobj && _nkids > 1)
      state.transform.pop();

   return new aqua_binary_t(op, type, init);
}

aqua_t *Ql_tree_node::expand_lambda(Symbol *sym)
{
   SymbolTable *table;
   Symbol *lambda_sym;
   aqua_t *lambda;
   aqua_t *expr;
   const char	*s = id();

   table = sym->table();
   lambda_sym = table->lookup("lambda");
   assert(lambda_sym);

   lambda = (aqua_t *) lambda_sym->data();

   switch (lambda_sym->flags()) {
      /* XXX copy problem */
    case translationState::lambda_normal:
      expr = lambda;
      break;
      /* XXX copy problem */
    case translationState::lambda_select:
      expr = new aqua_get_t(lambda, s);
      break;
      /* XXX copy problem */
    case translationState::lambda_group:
      expr = new aqua_get_t(lambda, AQUA_GROUP);
      break;
    case translationState::lambda_self:
      /* egads, something that isn't a problem */
      expr = new aqua_var_t(s);
      break;
   }

   return expr;
}

aqua_t	*Ql_tree_node::convert_id(translationState &state)
{
   Type	*extent;
   Symbol	*sym;
   const char	*s;

   sym = state.symbols.lookup_all(id());
   if (sym)
      return expand_lambda(sym);

   s = id();

   sym = state.defines().lookup(s);
   if (sym)
      return new aqua_use_t(s);

// Actually, this should be done later...
// So i'm moving this out to where aqua_get_t::check is actually called
// Naah, I'll do it later...
   extent = state.extents().lookup(s);
   if (extent)
      return new aqua_dbvar_t(s, extent);

//   errstream() << "Extent "  << s << " not found\n";

   // Murali.crude-hack
   return new aqua_get_t(0, s);
   // I'm just assuming that this is a field of some range variable...
   return new aqua_error_t;
}

// This function returns the union of all the relations that are derived
//  from this relation
// This is really necessary only for a typed-extent system

aqua_t* Ql_tree_node::convert_closure(translationState& state)
{
   aqua_t* kid_type = _kids[0]->convert(state);

#ifndef TYPED_EXTENTS
   if (_type != default_closure_n)
      errstream() << "Invalid use of ALL/ONLY keywords. You cant do that!\n";
   return kid_type;
#endif

   switch (_type)
   {
    case only_n:
      return kid_type;
    case default_closure_n:
    case closure_n:
      if (kid_type->_op != a_dbvar && _type == closure_n)
      {
	  // I'm not sure why closure even comes up here.
	  // errstream() << "Invalid use of keyword ALL."
	   //    << "I forgive you this time, but...\n";
	 // Doesnt make sense to have closures on non-(db extents)
// And why not?
// Let me look at it this way. Suppose I have an attribute that is a 
// Ref<Set<O>>; Ref<Set<derivedClass<O>>> is NOT a replacement for it
//  And therefore, it cannot have a closure as we define it.

	 return kid_type;
      }
#ifdef TYPED_EXTENTS
      return new aqua_closure_t(kid_type);
#endif
    default:
      return new aqua_error_t();
   }
   return 0; // Keep g++ happy
}

aqua_t	*Ql_tree_node::convert_define(translationState &state)
{
   aqua_t *t;
   const char	*name;

   name = _kids[0]->id();
   t = _kids[1]->convert(state);

   /* enter t into symbol table */
   state.defines().enter(name, (char *)t, 1);

   //	cout << "[define " << _kids[0]->id() << *t << '\n';
   
   return new aqua_zary_t(a_nil);	/* XXX */
}

aqua_t *Ql_tree_node::convert_parameters(translationState &state)
{
   aqua_op	combine_op = state.transform.combiner();
   aqua_op	element_op = state.transform.element();
   aqua_t	*last = (aqua_t *)0;
   aqua_t	*t;
   int	i;

   state.transform.top().cardinality = _nkids;

// Murali.change 4/4
//   for (i = _nkids-1; i >= 0; i--) {
     for (i = 0; i < _nkids; i++) {
      state.transform.top().at = i;
      t = _kids[i]->convert(state);

      switch (element_op) {
       case a_list:
       case a_union:
       case a_additive_union:
       case a_array:
       case a_set:
       case a_multiset:
	 t = new aqua_unary_t(element_op, t);
	 break;

       case a_sort:
	 t = new aqua_tuple_t(sortkey(state.transform.top().at), t);
	 break;

       case a_arg:
//	 t = new aqua_tuple_t(argid(state.transform.top().at), t);
	 t = new aqua_tuple_t(0, t);
	 break;

       case a_build_join:
	 state.transform.setPE(t);
	 break;

       case a_nil:	/* Explicit don't touch */
       default:
	 /* leave it alone */
	 break;
      }

      if (!last) {
	 last = t;
	 continue;
      }
      
      switch  (combine_op) {
       case a_tuple_join: {
	 aqua_function_t *pred = new aqua_function_t(
						     new aqua_var_t("unused1"),
						     new aqua_var_t("unused2"),
						     new aqua_zary_t(a_true));
	 last = new aqua_join_t(pred, t, last);
	 }break;
       case a_build_join:
	 last = t;
	 break;
       default:
	 last = new aqua_binary_t(combine_op, t, last);
	 break;
      }
   }

   return last;
}

aqua_t *Ql_tree_node::convert_collection(translationState &state)
{
   aqua_op	combine_op;
   aqua_op	element_op;
   aqua_t	*result;

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
      result = new aqua_unary_t(element_op,
				new aqua_zary_t(a_nil)); // XXX
      break;
    default:
      state.transform.push(combine_op, element_op);
      result = _kids[0]->convert(state);
      state.transform.pop();
      break;
   }

   return result;
}

/* XXX ooops, this is a 4 arg operator.  Hmmmm */ 

aqua_t	*Ql_tree_node::convert_folded(translationState &state)
{
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
      sum = convert_folded(state);
      _type = count_n;
      count = convert_folded(state);
      _type = avg_n;
      return new aqua_binary_t(a_divide, sum, count);
   }

   /* Need two lambda variables for the fold function + 1 for extract */
   x_lambda = new aqua_var_t(s = state.lambda());
   f_lambda1 = new aqua_var_t(s);	/* reuse name */
   f_lambda2 = new aqua_var_t(state.lambda());

   integer = state.types().force_lookup("integer");

   /* Initial value */
   switch (_type) {
    case count_n:
    case sum_n:
      t = new aqua_constant_t(integer, "0");
      break;
    case max_n:
    case min_n:
      /* XXX disgusting; duplication of tree (but is type safe) */
      t = new aqua_unary_t(a_choose, _kids[0]->convert(state));
      break;
    default:
      t = new aqua_error_t;
      break;
   }
   fold_to = t;

   /* Extractor */
   switch (_type) {
    case count_n:
      /* just add 1 */
      t = new aqua_constant_t(integer, "1");
      break;
    default:
      /* default extractor just gets whatever is there */
      t = x_lambda;
      break;
   }
   extractor = new aqua_function_t(x_lambda, t);

   /* Folder */
   switch (_type) {
    case count_n:
    case sum_n:
      t = new aqua_binary_t(a_add, f_lambda1, f_lambda2);
      break;
    case min_n:
      t = new aqua_binary_t(a_min, f_lambda1, f_lambda2);
      break;
    case max_n:
      t = new aqua_binary_t(a_max, f_lambda1, f_lambda2);
      break;
    default:
      t = new aqua_error_t;
      break;
   }
   folder = new aqua_function_t(f_lambda1, f_lambda2, t);

   source = _kids[0]->convert(state);

   return new aqua_fold_t(fold_to, extractor, folder, source);
}

aqua_t *Ql_tree_node::convert_method(translationState &state)
{
   /* XXX consistency problem */
   aqua_t	*args;
   aqua_op	el_conv;

   if (_nkids > 2) {
      el_conv = ((_type == method_call_n) || (_type == fake_method_call_n)) 
	 ? a_arg : a_nil;
      state.transform.push(a_tuple_concat, el_conv);
      args = _kids[2]->convert(state);
      state.transform.pop();
   }
   else
      args = new aqua_zary_t(a_void);

   return new aqua_method_t(_kids[0]->convert(state),
			    _kids[1]->id(),
			    args, 
			    ((_type == method_call_n) 
			     || (_type == named_method_call_n)));
}

aqua_t *Ql_tree_node::convert(translationState &state)
{
   switch (_type) {
    case int_n:
    case float_n:
    case char_n:
    case string_n:
    case refconst_n:
      return convert_constant(state);
      break;

      /* "loose" id's are database variables (for now) */
    case id_n:
      return convert_id(state);
      break;

    case one_field_n:
      return new aqua_tuple_t(_kids[0]->id(),
			      _kids[1]->convert(state));
      break;

    case extract_attribute_n:
      return new aqua_get_t(_kids[0]->convert(state),
			    _kids[1]->id());
      break;

    case tuple_obj_n:
    case cast_n:
    case function_or_create_n:
    case object_constructor_n:
    case labelled_object_constructor_n:
      return convert_object(state);
      break;

    case one_scan_var_n:
      return convert_one_scan_var(state);
      break;

    case sf_n:
    case usf_n:
    case sfw_n:
    case usfw_n:
      return convert_select(state);
      break;

    case df_n:
    case dfw_n:
      return convert_delete(state);

    case iiw_n:
      return convert_insert(state);

    case uf_n:
    case ufw_n:
      return convert_update(state);

    case cfw_n:
      return convert_create(state);

    case update_expr_n:
      return convert_update_expr_list(state);
    case plus_equal_n:
    case minus_equal_n:
    case assign_n:
      return convert_assign(state);

    case only_n:
    case closure_n:
      return convert_closure(state);

    case sublist_n:
      return convert_trinary(state);
      break;

    case define_n:
      return convert_define(state);
      break;

//    case assign_n:
//      return new aqua_assign_t(_kids[0]->convert(state),
//			       _kids[1]->convert(state));
//      break;

    case forall_n:
    case exists_n:
      return convert_existential(state);
      break;

    case method_call_n:
    case named_method_call_n:
      return convert_method(state);
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
      return convert_collection(state);
      break;

    case from_n:
    case fields_n:
    case actual_parameters_n:
      return convert_parameters(state);
      break;

    case sort_n:
      return convert_sort(state);
      break;

    case group_n:
    case group_with_n:
      return convert_group(state);
      break;

    case count_n:
    case min_n:
    case max_n:
    case sum_n:
    case avg_n:
      return convert_folded(state);
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
      return convert_binary(state);
      break;

    case unary_minus_n:
    case not_n:
    case abs_n:
    case first_n:
    case last_n:
    case element_n:
    case listoset_n:
      return convert_unary(state);
      break;

    case flatten_n:
      return convert_unary(state);	/* XXX cheap shot */
      break;

    case nil_n:
    case true_n:
    case false_n:
      return convert_zary(state);
      break;
    default:
      OOPS(_type);
   }
   errstream() << "unimplemented conversion for " << *this << '\n';
   return new aqua_error_t;
}

aqua_t *Ql_tree_node::aqua_convert(oqlContext& context)
{
   aqua_t	*result;
   translationState state(context);

#if 0
   next_unique = 0;

   Types = &oql_transform.db.types;
   Extents = &oql_transform.db.extents;
   Defines = &oql_transform.defines;
#endif

   state.transform.push(a_nil, a_nil);	/* default context */
   result = convert(state);
   state.transform.pop();
   if (!state.symbols.empty())
      errstream() << "OOPS: symbol stack not empty!\n";
   if (!state.transform.empty())
      errstream() << "OOPS: context stack not empty!\n";
   return result;
}
