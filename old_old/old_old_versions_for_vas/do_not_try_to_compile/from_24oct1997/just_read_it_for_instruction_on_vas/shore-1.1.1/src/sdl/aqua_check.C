/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stdio.h>
#include <ostream.h>

#include <symbol.h>
#include <types.h>
#include <typedb.h>
#include <aqua.h>
#include <oql_context.h>

#include <sstack.h>

class typecheckState {
 public:
   oqlContext	&oql;
   SymbolStack	current;
   typecheckState(oqlContext &_oql)
      : oql(_oql), current("typecheck symbol stack") { }

   TypeDB		&types() { return oql.db.types; }
   ExtentDB	&extents()  { return oql.db.extents; }
   SymbolTable	&defines()  { return oql.defines; }
};

const char* DeltaNames[2] = {"delta0", "delta1"};
typedef Type* TypeP;

/* Emacs indent mode screws up with {} case stmts */
#define	BEGIN	{/*void*/
#define	END	} break

// Crude-hack-begin
aqua_t* resolve(SymbolStack& sstack, const char* fieldName)
{
   SymbolStack_i ssi(sstack);
   SymbolTable* st;
   Symbol* sym;
   Type* t;
   int j;
   char* subname[2];
   for (; st = *ssi; ++ssi)
   {
      for (j=0; j < 2; j++)
      {
	 if (!(sym = st->lookup(DeltaNames[j]))) continue;
	 sym = st->lookup((char *)sym->data());
	 t = (Type *)sym->data();
	 if (t->isRef()) 
	    t = t->isRef()->of();
	 if (t->isContainer())
	 {
	    if (!t->isTuple())
	    {
	       if (t->isContainer()->member(fieldName, 
					    ContainerType::EnforceProtection)) 
		  return new aqua_var_t(sym->name());
	    }
	    else
	    {
	       // It is a tuple
	       t->isTuple()->isField(fieldName, subname);
	       if (!subname[0]) continue;
	       if (subname[0] && subname[1])
	       {
		  errstream() << "Ambiguous use of <" << fieldName 
		     << "> . Could belong to either range variable <" 
			<< subname[0] << "> or <" << subname[1] << ">\n";
		  return 0;
	       }
	       // All is well...
	       return new aqua_get_t(new aqua_var_t(sym->name()), 
				     subname[0]);
	    }	 // end of is this a tuple?
	 }
      }
   }
   return 0;
}   

Type *aqua_t::typecheck(oqlContext &context)
{
   typecheckState state(context);
   return gen(state);
}

Type *aqua_type_t::gen(typecheckState &state)
{
   return _type = state.types().force_lookup(_name);
}

Type *aqua_dbvar_t::gen(typecheckState &state)
{
   return _type = _extent;
}

Type	*aqua_var_t::gen(typecheckState &state)
{
   Symbol	*sym;
   sym = state.current.lookup_all(_name);
   if (!sym) {
      errstream() << "aqua_var_t(" << _name << ") not found!\n";
      return new ErrorType;
   }
   _type = (Type *)(sym->data());
   // cout << "Var(" << _name << ") : " << *_type << '\n';
   return _type;
}

Type *aqua_get_t::gen(typecheckState &state)
{
   ContainerType *it;
   RefType *ref;
   MethodType *method;

   // Murali.muck_up 3/3 .
   if (!_what)
   {
      // Try to resolve the name in the current context...
      if (!(_what = resolve(state.current, _name.str())))
      {
	 errstream() << "Could not resolve <" << _name.str() << ">\n";
	 _what = new aqua_error_t();
	 return _type = new ErrorType;
      }
   }
   // Murali.muck_up end

   aqua_unary_t::gen(state);
   
   /* assumes only 1 ref */
   ref = _type->isRef();
   if (ref)
      _type = ref->of();

   it = _type->isContainer();
   if (!it)
      return _type = new ErrorType;
   // This is where we have to respect privacy and the like
   _type = it->member(_name, ContainerType::EnforceProtection);
   if (!_type)
   {
      errstream() << "No non-private member " << _name 
	   << " in type " << it->name() << endl;
      _type = new ErrorType;
   }
   
#if 1
   /* XXX rewrite of node needed */
   if ((method = _type->isMethod())  &&  _op == a_get) {
      errstream() << "get() -> method() rewrite needed!\n";
      /* XXX should check that method is Method(void) */
      _type = method->returns();
   }
#endif

   return _type;
}

Type* aqua_update_t::gen(typecheckState& state)
{
   Type* selector_type;
   Type* modifier_type;
   Type* input_type;
   Type* input_element;

   input_type = _what->gen(state);
   if (!input_type->isCollection())
   {
      errstream() << "Updates for now only on collections\n";
      return _type = new ErrorType;
   }
   input_element = input_type->isCollection()->of();

   selector_type = _selector->gen(state, input_element);
   if (!selector_type->isBoolean()) return _type = new ErrorType;

   modifier_type = _modifier->gen(state, input_element);
   if (modifier_type != input_element) return _type = new ErrorType;

   return _type = input_type;
   //   return _type = new ErrorType;
}

Type* aqua_modify_tuple_t::gen(typecheckState& state)
{
   Type* tuple_type;
   Type* field_type;
   Type* rvalue_type;

   tuple_type = _what->gen(state);
   if (tuple_type->me() == ODL_Error) return _type = new ErrorType;

   // IS THIS NECESSARY ?
   // Check if such a field exists
   {
      int offset;
      odlFlags props;
    
      field_type = tuple_type->isContainer()?
	 tuple_type->isContainer()->member(_name.str(), props, 
					   ContainerType::EnforceProtection)
	    : (Type *)0;
      if (!field_type)
      {
	 errstream() << "No non-private member " << _name.str() 
	      << " in type " << tuple_type->name() << endl;
	 return _type = new ErrorType;
      }
      if (props & ODL_readonly == ODL_readonly)
      {
	 errstream() << _name.str() << " is a CONST attribute of " 
	      << tuple_type->name() << endl;
	 return _type = new ErrorType;
      }
   }
   // Check the to-be-assigned-value
   rvalue_type = _value->gen(state);
   if (rvalue_type->me() == ODL_Error) return _type = new ErrorType;
   if (field_type->match(rvalue_type, Type::R_assign_attribute))
      return _type = tuple_type;

   // Other cases... for now JUST CRASH
   errstream() << "Type mismatch. lvalue is of type ";
   field_type->print(errstream());
   errstream() << " and rvalue is of type ";
   rvalue_type->print(errstream());
   errstream() << "\n";
   return _type = new ErrorType;
}

Type* aqua_modify_array_t::gen(typecheckState& state)
{
   Type* array_type = _what->gen(state);
   Type* index_type;
   Type* rvalue_type;
   Type* array_element_type;

   if (!array_type->isIndexedCollection)
   {
      errstream() << "Cannot index into type ";
      array_type->print(errstream()) << endl;
      return _type = new ErrorType;
   }

   index_type = _at->gen(state);
   if (!index_type->isInteger())
   {
      errstream() << "Type mismatch. Index must be of type Integer. Found type ";
      index_type->print(errstream()) << " instead \n";
      return _type = new ErrorType;
   }

   array_element_type = array_type->isCollection()->of();
   rvalue_type = _value->gen(state);

   if (array_element_type->match(rvalue_type, Type::R_assign_attribute))
      return _type = array_type;

   // Other cases... for now JUST CRASH
   errstream() << "Type mismatch. lvalue is of type ";
   array_element_type->print(errstream());
   errstream() << " and rvalue is of type ";
   rvalue_type->print(errstream());
   errstream() << "\n";
   return _type = new ErrorType;
}

Type *aqua_method_t::gen(typecheckState &state)
{
   MethodType *it;
   Type* arg_type;

   it = (aqua_get_t::gen(state))->isMethod();
   if (!it)
      return _type = new ErrorType;
   arg_type = _args->gen(state);
   if (!arg_type)
      return _type = new ErrorType;

   Type* cast_type;
   Type* base = _what->gen(state);
   MethodType* ret = base->lookup_method(_name.str(), arg_type, cast_type);
   if (!ret)
   {
      errstream() << "Type mismatch in arguments to method " << _name.str() << endl;
      return _type = new ErrorType;
   }
   if ((ret->isConst() != ODL_readonly) && _inquery)
   {
      errstream() << "Cannot call a non-constant method within a query..." << endl;
      return _type = new ErrorType;
   }
   return (_type = ret->returns());
}

Type	*aqua_function_t::gen(typecheckState &state, Type *arg0, Type *arg1)
{
   Type	*result;

   if (arg0 && !_one  ||  !arg0 && _one)
      return new ErrorType;
   if (arg1 && !_two  ||  !arg1 && _two)
      return new ErrorType;
#if 0
   /* This is good enough if we use the duplicated aqua_var_t */
   if (arg0)
      _one->_type = arg0;
   if (arg1)
      _two->_type = arg1;
#else
   /*
      We must set both the embedded type and the symbol stack
      types, for both types of vars are put in the tree
      by aqua_tran (For now at least)

      BZZT wrong; only need the symbol stack entries.
      */  
   state.current.push("function");
   if (arg0) {
      _one->_type = arg0;
      state.current.enter(_one->_name, (char *)arg0);
      // cout << "Function(" << _one->_name << ", " << *arg0 << ")\n";  
      state.current.enter("delta0", _one->_name);
   }
   if (arg1) {
      _two->_type = arg1;
      state.current.enter(_two->_name, (char *)arg1);
      state.current.enter("delta1", _two->_name);
   }
#endif
   result = _body->gen(state);
#if 1
   state.current.pop();
#endif
   return _type = result;
}

Type	*aqua_use_t::gen(typecheckState &state)
{
   Symbol *sym;
   aqua_t *define;

   sym = state.defines().lookup(_name);
   if (!sym)
      return _type = new ErrorType;
   define = (aqua_t *) sym->data();
   
   return define->type();
}

Type* aqua_eval_t::gen(typecheckState& state)
{
   Type* input;
   Type* results_in;

   input = _to->gen(state);
   results_in = _what->gen(state, input);
   return _type = results_in;
}


// Murali. 3/6
// \begin{kludge mode}
/**
  I want to disallow inserting already existing objects into sets.
  So, whenever I iterate over a collection of objects, I return a ref<Obj>
  rather than the object itself. I have ensured that a Ref<O> is not 
  acceptable where a <O> is expected
  **/
/* XXX ick what about double-variable functions ??? */
Type	*aqua_apply_t::gen(typecheckState &state)
{
   Type	*element;
   Type	*input;
   CollectionType *work_on;
   Type	*results_in;
   TupleType *group;

   input = _to->gen(state);
   /* major league conversion needed here from collection->object, etc */

   work_on = input->isCollection();
   if (!work_on) {
      // cout << "Apply must be to a collection\n";
      return _type = new ErrorType;
   }
   element = work_on->of();
   if (element->isObject())
   {
      switch(_op)
      {
       case a_select:
       case a_apply:
// Murali. comment out 4/15
// Dont need this stuff any more...
//	 element = new RefType(element);
// 4/23
// Yes, I do...
	 element = new RefType(element);
	 break;
       case a_group:
	 break;
	 // I'm not sure what to do in these cases...
      }
   }
   
   // cout << "Apply working on " << *element << '\n';
   results_in = _what->gen(state, element);

   switch (_op) {
    case a_select:
      /* select just filters the input elements */
      if (results_in->isBoolean())
	 results_in = input;
      else
	 results_in = new ErrorType;
      break;
    case a_apply:
      /* Sets turn into bags when an apply happens  */
      if (work_on->isSet())
	 results_in = new BagType(results_in);
      else
	 results_in = work_on->rebuild(results_in);
      break;
    case a_group:
      /* group produces set<struct<"attr":x, "group":set<input>>> */
      group = new TupleType();
      group->add("attr", results_in);
      group->add("group", new SetType(element));
      results_in = new SetType(group);
      break;
    case a_exists:
    case a_forall:
      if (!results_in->isBoolean())
	 results_in = new ErrorType;
      break;
   }

#if 0
   /* Can't guarantee a set remains that way */ 
   if (resuls_in->kind() == tt_set) {
      results_in = Type::_rebuild(tt_bag, results_in->of);
      delete results_in;
   }
#endif

   return _type = results_in;
}

Type	*aqua_join_t::gen(typecheckState &state)
{
   Type	*input1, *input2;
   CollectionType *tmp;
   TupleType	*tuple;
   Type	*t;
   

   input2 = _other->gen(state);
   input1 = _to->gen(state);

   /* convert inputs from collections to items */
   tmp = input1->isCollection();
   if (!tmp)
      return _type = new ErrorType;
   input1 = tmp->of();
   if (!input1->isTuple())
      return _type = new ErrorType;

   tmp = input2->isCollection();
   if (!tmp)
      return _type = new ErrorType;
   input2 = tmp->of();
   if (!input2->isTuple())
      return _type = new ErrorType;

   t = _what->gen(state, input1, input2);
   if (!t->isBoolean())
      return _type = new ErrorType;

   tuple = new TupleType();
   tuple->concat(*input1->isTuple());
   tuple->concat(*input2->isTuple());

   /* Bag, List, ??? */ 
   t = new BagType(tuple);
   
   return _type = t;
}

// Murali.add 3/4/95

Type* aqua_delete_t::gen(typecheckState& state)
{
   TypeP input, results_in, element;
   CollectionType* work_on;

   input = _from->gen(state);      // What are we deleting from
   work_on = input->isCollection();
   if (!work_on)
   {
      errstream() << "<delete> must operate on a collection\n";
      return _type = new ErrorType;
   }
   element = work_on->of();
   // Now to check if this is a collection that exists in the DB
   // In other words, can I delete something from this, AND expect
   // the change to be reflected in the database...
   // NULL for now

   results_in = _selector->gen(state, element);
   if (results_in->isBoolean())
      results_in = input;
   else
      results_in = new ErrorType;
   
   return _type = results_in;
}

// Some general rules about inserting and replaceability
/***
  1. You should NOT be able to insert pre-existing objects into a set
  of objects...
  (ie)
  insert into Boats (select b from b in Boats)
  must be disallowed.
  The way to go about doing this is by making the select statement
  return a set<ref<boat>>. Then disallow Ref<T> as a suitable candidate
  for insertion into collection<T>

  2. T1, T2: T2 derives from T1
  a) T2 cannot be used where T1 is expected, if 
  1) T1 is an attribute of a container class
  2) T1 is an element of a Paradise-style collection...
  b) Ref<T2> can be used where Ref<T1> is expected 
  c) T2 can be used where Ref<T2> is expected
  NB: So Ref(CreateObject(T2)) shd be used there...
  ***/


Type* aqua_insert_t::gen(typecheckState& state)
{
   CollectionType* into;
   Type* what;
   Type* into_element;
   Type* what_element;

   into = _into->gen(state)->isCollection();
   if (!into)
   {
      errstream() << "<insert> must operate on a collection\n";
      return _type = new ErrorType;
   }
   into_element = into->of();
   what = _what->gen(state);
   what_element = what->isCollection()? what->isCollection()->of(): (Type *)0;

   if (into_element->match(what, Type::R_assign))
      return _type = into;
   if (what_element && into_element->match(what_element, Type::R_assign))
      return _type = into;
   return _type = new ErrorType;
}

Type *aqua_tuple_t::gen(typecheckState &state)
{
   Type *it = _value->gen(state);
   TupleType *tuple;

   _type = tuple = new TupleType();
   tuple->add(_name, it);

   return _type;
}

Type *aqua_assign_t::gen(typecheckState &state)
{
   Type *to = _left->gen(state);
   Type *from = _right->gen(state);

   _type = from;
   switch (_left->op()) {
    case a_dbvar:
      /* type incompatibility is ok; runtime changes dbvar type */
      _type = from;
    default:
      if (! to->lcs(from))
	 _type = new ErrorType;
      break;
   }
   return _type;
}

Type *aqua_zary_t::gen(typecheckState &state)
{
   switch (_op) {
    case a_true:
    case a_false:
      _type = state.types().force_lookup("boolean");
      break;
    case a_nil:
      _type = state.types().force_lookup("nil");
      break;
    case a_void:
// Murali. change 4/4
      _type = state.types().force_lookup("void");
      break;
   }
   return _type;
}


Type *aqua_unary_t::gen(typecheckState &state)
{
   _type = _what->gen(state);

   switch (_op) {
    case a_abs:
    case a_negate:
      /* The type must be a numeric type */
      if (!_type->canDoMath())
	 _type = new ErrorType;
      break;
    case a_not:
      /* The type must be boolean */
      if (!_type->isBoolean())
	 _type = new ErrorType;
      break;
    case a_first:
    case a_last:
    case a_choose:
      /* the type must be a list, array, or collection */
      BEGIN;
      CollectionType *colxion = _type->isCollection();
      _type = colxion ? colxion->of() : new ErrorType;
      END;
    case a_multiset_to_set:
      BEGIN;
      BagType *bag = _type->isBag();
      CollectionType *colxion;
#if 0
      _type = bag ? new SetType(bag->of()) : new ErrorType;
#else
      if (bag)
	 _type = new SetType(bag->of());
      else if (colxion = _type->isCollection()) {
	 _type = new SetType(colxion->of());
	 /* still needs rewrite */
      }
      else
	 _type = new ErrorType;
      
#endif
      END;
    case a_list_to_set:
      BEGIN;
      ListType *list = _type->isList();
      _type = list ? new SetType(list->of()) : new ErrorType;
      END;

    case a_flatten:
      BEGIN;
      CollectionType *outer = _type->isCollection();
      CollectionType *inner = outer ? outer->of()->isCollection() :0;
      BagType *bag;
      IndexedCollectionType *ixc, *ixc1;
      if (inner  &&  outer) {
	 if (bag = inner->isBag()) {
	    cout << "Flatten xx(bag) -> bag\n";
	    _type = bag;
	    break;
	 }
	 if ((bag = outer->isBag()) &&
	     (ixc = inner->isIndexedCollection())) {
	    cout << "Flatten set(list) -> set\n";
	    _type = new SetType(ixc->of());
	    break;
	 }
	 if ((ixc = outer->isIndexedCollection()) &&
	     (ixc1 = inner->isIndexedCollection())) {
	    cout << "Flatten list(list) -> list\n";
	    _type = ixc1;
	    break;
	 }
	 cout << "Flatten ????\n";
	 _type = new ErrorType;
      }
      END;
      
    case a_array:
      _type = new ArrayType(_type);
      break;
    case a_list:
      _type = new ListType(_type);
      break;
    case a_set:
      _type = new SetType(_type);
      break;
    case a_multiset:
      _type = new BagType(_type);
      break;

    default:
      /* do nothing, just pass the type along */
      break;
   }
   return _type;
}

Type* aqua_modify_binary_t::gen(typecheckState& state)
{
   Type *one, *two, *result;
   one = _left->gen(state);
   two = _right->gen(state);

   _type = TypeError;	/* default value */
   switch (_op)
   {
    case a_modify_add:
    case a_modify_subtract:
      // Right now, both the LHS must be a collection
      // For now only collections are supported...
      if (!one->isCollection())
	 return _type = new ErrorType;
//
// Murali.change 4/5
// I'm restricting the LHS to be a collection of anything but OBJECTS...
//
      Type* one_element;
      one_element = one->isCollection()->of();
      if (one_element->isObject())
      {
	 errstream() << "Cannot use operators += and -= on collections of objects"
	      << endl;
	 return _type = new ErrorType;
      }
      if (one_element->match(two, Type::R_assign))
	 return _type = one;
      if (!two->isCollection())
	 return _type = new ErrorType;
      if (one_element->match(two->isCollection()->of(), Type::R_assign))
	 return _type = one;
      return _type = new ErrorType;
    default:
      errstream() << "Unused binary " << *this << '\n';
      break;
   }
   return _type;
}

Type *aqua_binary_t::gen(typecheckState &state)
{
   Type *one, *two, *result;
   one = _left->gen(state);
   two = _right->gen(state);

   _type = TypeError;	/* default value */
   switch (_op) {
    case a_tuple_concat:
      BEGIN;
      TupleType *l, *r;
      l = one->isTuple();
      r = two->isTuple();
      if (l && r) {
	 TupleType *me;
	 me = new TupleType;
	 me->concat(*l);
	 me->concat(*r);
	 _type = me;
      }
      END;
    case a_array_concat:
    case a_list_concat:
      BEGIN;
      IndexedCollectionType *l, *r;
      Type	*lcs;
      l = one->isIndexedCollection();
      r = two->isIndexedCollection();
      if (l && r && (lcs = l->lcs(r)))
	 _type = lcs;
      END;
    case a_union:
    case a_additive_union:
    case a_diff:
    case a_intersect:
      /*
	 unions resulting in a Bag should be additive
	 additive unions should be done on bags
	 */
      BEGIN;
      BagType *l, *r;
      Type	*lcs;
      l = one->isBag();
      r = two->isBag();
      if (l && r && (lcs = l->lcs(r)))
	 _type = lcs;
      END;
    case a_eq:
    case a_ne:
      /*
	 Different from relative compares, as objects
	 can be compared for equality
	 XXX sort of disgusting
	 */  
      /* coercions may be needed */
      if (result = one->canCompareWith(two))
	 _type = state.types().force_lookup("boolean");
      break;
    case a_le:
    case a_lt:
    case a_gt:
    case a_ge:
      /* coercions may be needed */
      /* XXX relative compare method needed */
      if (result = one->canDoMathWith(two))
	 _type = state.types().force_lookup("boolean");
      break;
    case a_add:
    case a_subtract:
    case a_multiply:
    case a_divide:
    case a_max:
    case a_min:
      if (result = one->canDoMathWith(two))
	 _type = result;
      else if (_op == a_add) {
	 /* Is it really list/array concatenation ? */
	 IndexedCollectionType *l, *r;
	 Type	*lcs;
	 l = one->isIndexedCollection();
	 r = two->isIndexedCollection();
	 if (l && r && (lcs = l->lcs(r))) {
	    _type = lcs;
	    /* disgusting, should new /delete stuff */
	    _op = lcs->isArray()
	       ? a_array_concat
		  : a_list_concat;	
	 }
      }
      break;
    case a_mod:
      if (one->isInteger()  &&  two->isInteger())
	 _type = state.types().force_lookup("integer");
      break;
    case a_and:
    case a_or:
      if (one->isBoolean()  &&  two->isBoolean())
	 _type = state.types().force_lookup("boolean");
      break;
    case a_at:
      BEGIN;
      IndexedCollectionType *array = one->isIndexedCollection();
      int	indexOk = two->isInteger();
      if (array && indexOk)
	 _type = array->of();
      END;
    case a_cast:
      /* This catches most things except ... */
      /* XXX and the bloody ref problem shows up again */
      if (one->lcs(two))
	 _type = one;
      else {
	 /* This catches down-classing */
	 ObjectType *type = one->isObject();
	 ObjectType *it = two->isObject();
	 if (type && it && (type->isa(it)))
	    _type = type;
	 else
	    cout << *two << " is not a " << *one << '\n';
      }
      break;
    case a_sort:
      BEGIN;
      ListType *keys = one->isList();
      CollectionType *source = two->isCollection();
      /* sort output is always a list */
      if (keys && source)
	 _type = new ListType(source->of());
      END;
    case a_member:
      BEGIN;
      CollectionType *colxion = two->isCollection();
      if (colxion && one->lcs(colxion->of()))
	 _type = state.types().force_lookup("boolean");
      END;
    case a_mkobj:{
      // Massive kludge for now...
      Type* cast_type = one->constructor(two);
      if (!cast_type)
	 return _type = new ErrorType;
      return _type = one;
	}
    default:
      errstream() << "Unused binary " << *this << '\n';
      break;
   }

   return _type;
}

Type *aqua_trinary_t::gen(typecheckState &state)
{
   Type *one, *two, *three;
   IndexedCollectionType *array;

   /* there is only 1 trinary  type at this point */
   one = _left->gen(state);
   two = _middle->gen(state);
   three = _right->gen(state);

   /* range can return one or more elements, so type is same as src */
   array = one->isIndexedCollection();
   if (array  &&  two->isInteger()  &&  three->isInteger())
      _type = array;
   else
      _type = new ErrorType;
   return _type;
}

Type *aqua_fold_t::gen(typecheckState &state)
{
   CollectionType *source;		/* source of whatevers */
   Type	*element;		/* individual whatever */
   Type	*extract;		/* extract from whatever */
   Type	*initial;		/* initial value */
   Type	*folded;		/* folded result */

   /* there is only 1 cubary  type at this point */
   _type = TypeError;

   source = _to->gen(state)->isCollection();
   if (!source)
      return _type;

   element = source->of();
   extract = _what->gen(state, element);
   initial = _initial->gen(state);
   folded = _folder->gen(state, initial, extract);

   return _type = folded;
}
/**
  Type* constructor(Type* t, aqua_t* arg, typecheckState& state)
  {
  Type* arg_type;
  TupleType* tuple_type;
  if ((arg_type = arg->gen(state))->me() == ODL_Error) 
  return new ErrorType;
  if ((tuple_type = arg_type->isTuple()) == 0)
  return new ErrorType;
  // Now scan thru the tuple
  // Is the tuple named?
  if (tuple_type->name())
  {
  // It is...
  // So lets just scan thru the tuple, and thru the object
  ContainerType* c = t->isContainer();
  if (!c) return new ErrorType;
  }
  }
  **/
#ifdef TYPED_EXTENTS
Type* aqua_closure_t::gen(typecheckState& state)
{
   return _type = _what->gen(state);
}
#endif TYPED_EXTENTS
