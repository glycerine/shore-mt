/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "metatypes.sdl.h"
#include <ostream.h>
#include <strstream.h>
#include <symbol.h>
#include <types.h>
#include "typedb.h"


//#include <defns.h>
#include <assert.h>

PrimitiveType* TypeVoid = NULL ; 
ErrorType* TypeError = NULL ; 
ObjectType* TypeNil = NULL ;

void
OQL_Initialize2(void)
{
   TypeVoid = new PrimitiveType("void", /* PrimitiveType:: */  pt_void);
   //para_check( TypeVoid ) ;
   TypeError = new ErrorType("*ERROR*");
   // para_check( TypeError ) ;
   TypeNil = new ObjectType("nil") ; 
   //para_check( TypeNil ) ; 
}

#ifdef USE_ALLOCATORS
void* SystemType::operator new(long sz)
{
   return (SystemType *)(CurrentTypesAllocator()->alloc(sz));
}

void* TableEntry::operator new(long sz)
{
   return CurrentTypesAllocator()->alloc(sz);
}
#endif USE_ALLOCATORS

InterfaceType::InterfaceType(const char* name, odlFlags forward)
: ObjectType(name), forwardInstantiated(0),
  currentAccessSpec(ODL_public), fwd(forward), ScopeType(0)
{
   _types = new TypeDB(
#ifdef USE_ALLOCATORS
   CurrentTypesAllocator(), 
#endif
   "interface_types");
}

ModuleType::ModuleType(const char* name)
: ContainerType(name), ScopeType(0)
{
   _types = new TypeDB(
#ifdef USE_ALLOCATORS
   CurrentTypesAllocator(), 
#endif
   "module_types");
}

ostream &operator<<(ostream &s, Type &type)
{
   return type.print(s);
}

#if 0
#include "primitive_names.h"
ostream &PrimitiveType::print(ostream &s)
{
   return s << '<' << PrimitiveNames[_primitive] << '>';
}
#endif

int PrimitiveType::add(MethodType* m)
{
   return methods.add(m, (ODL_public | ODL_readonly));
}

ostream &MethodType::print(ostream &s)
{
   int	i;
   Symbol	*sym;
   SymbolTable_i st(_args);

   s << "<Method " << name() << '(';
   for (i = 0; sym = *st; ++st ) {
      s << " ";
      s << sym->name() << ':' << *(Type *)(sym->data());
   }
   return s << " ) : " << _returns->name() << ">";
}


ostream &ContainerType::_print(ostream &s, const char *type_name)
{
   int	token = 0;
   Symbol	*sym;
   Type	*type;
   TableEntry	*entry;

   if (isNamed()) {
      /* kludge for objects */
      if (type_name)
	 return s << '<' << type_name << '<' << _name << ">>";
      else
	 return s << '<' << _name << '>';
   }

   s << '<' << type_name << " <";

   SymbolTable_i it(members);
   for (; sym = *it; ++it)
   {
      s << sym->name() << ':';
      entry = (TableEntry *) (sym->data());
      type = entry->type;
      s << type->name();
      if (token >= 0)
	 s << ' ';
   }
   s << ">>";
   return s;
}

ostream &CollectionType::_print(ostream &s, const char *type_name)
{
   return s << '<' << type_name <<  *_of << ">";
}

Type	*RefType::rebuild(Type *of)
{
   return new RefType(of);
}

Type	*RefType::lcs(Type *_it)
{
   RefType *it = _it->isRef();
   Type	*of_lcs;

   if (!it)
      return 0;

   of_lcs = _of->lcs(it->_of);
   if (!of_lcs)
      return (Type *)0;

   return rebuild(of_lcs);
}

Type	*RefType::canCompareWith(Type *_it)
{
   return lcs(_it);
}

ostream &RefType::print(ostream &s)
{
   return s << "<Ref" << *_of << '>';
}

extern TypeTag reduce_type2 (TypeTag val1,TypeTag val2);

#include "base_types.h"
Type *Primitive_lcs( TypeTag val1,TypeTag val2)
{
	TypeTag lcs_tag = reduce_type2(val1,val2);
	switch(lcs_tag) {
	case Sdl_double: return TypeDoubleRef;
	case Sdl_unsigned_long: return TypeUlongRef;
	case Sdl_long: return TypeIntRef;
	}
	return 0;
}


Type	*PrimitiveType::lcs(Type *_it)
{
   PrimitiveType *it = _it->isPrimitive();

   if (!it)
      return (Type *)0;

   if (_primitive == it->_primitive)
      return this;
   if ((_primitive == pt_real || _primitive == pt_integer) &&
       (it->_primitive == pt_real  ||  it->_primitive == pt_integer)) {
      /* return an existing type, don't look it up in db! */
      if (_primitive == pt_real)
	 return this;
      else
	 return it;
   }
   return Primitive_lcs(_primitive,it->_primitive);
   // return (Type *)0;
}



int PrimitiveType::isa(Type* t)
{
   PrimitiveType* p = t->isPrimitive();
   if (!p) return 0;
   if (this == p) return 1;
   if (_primitive == pt_real && p->_primitive == pt_integer) return 1;
   if (p->_primitive == pt_real && _primitive == pt_integer) return 1;
   return 0;
}

int CollectionType::isa(Type* t)
{
   return (me() == t->me() && *of() == *(t->isCollection()->of()));
}

int SetType::isa(Type* t)
{
   return (t->isBag() && *of() == *(t->isBag()->of()));
}

int RefType::isa(Type* t)
{
   return (t->isRef() && of()->isa(t->isRef()->of()));
}

int RefType::operator == (Type* t)
{
   return (t->isRef() && *of() == *(t->isRef()->of()));
}

Type* PrimitiveType::_match(Type* t, Reason why)
{
   PrimitiveType * t2;
   if (this == t) return this;
   if (!(t2=t->isPrimitive())) return 0;
   if (why != Type::R_param_out)
   {
      if (primitive() == pt_real 
	  && t2->primitive() == pt_integer)
	 return this;
      if (primitive() == pt_integer 
	  && t2->primitive() == pt_real)
	 return this;
   }
//
// Include any other dependencies here...
//
	// really lame is this.
	if (primitive() == t2->primitive())
		return this;
	// need to do promotions here...
   return 0;
}

Type* CollectionType::_match(Type* t, Reason why)
{
   CollectionType* x = t->isCollection();
   if (!x) return 0;
   if (!x->of()->isa(of())) return 0;

   switch(why)
   {
    case R_param_out:
      return (t->isa(this)) ? this : 0;
    case R_param_in:
      if (!(*x->of() == *of())) return 0;
    case R_assign_attribute:
    case R_assign:
      if (isIndexedCollection())
	 return (me() == t->me()) ? t : 0;
      else
	 return this;
   }
}

extern RefType* RefAny;
Type* RefType::_match(Type* t, Reason why)
{
   if (t == RefAny)
      return this;
   if (t->isRef() && t->isRef()->of()->isa(of())) return this;
   if (t->isa(of())) return this;
   return (Type *)0;
}

Type* ObjectType::_match(Type* t, Reason why)
{
   switch(why)
   {
#ifdef PARADISE
    case R_assign:
#endif PARADISE
    case R_assign_attribute:
      // Corresponds to the ML notion of shallow equivalence...
      // Not that its ever going to arise...
      if (this == t) return this;
      if (t->isRef() && t->isRef()->of() == this) return this;
      return 0;
    case R_param_in:
    case R_param_out:
#ifndef PARADISE
    case R_assign:
#endif PARADISE
      if (t->isa(this)) return this;
      if (t->isRef() && t->isRef()->of()->isa(this)) return this;
      return 0;
   }
}

Type* Type::_match(Type* t, Reason why)
{
   return (t->isa(this)) ? this : 0;
}

Type* Type::cast(Type* out)
{
   return this->isa(out) ? this : out;
}

Type	*PrimitiveType::canCompareWith(Type *_it)
{
   return lcs(_it);
}

Type	*PrimitiveType::canDoMathWith(Type *_it)
{
   return lcs(_it);
}

Type	*PrimitiveType::canDoMath()
{
   return (_primitive == pt_real ||  _primitive == pt_integer) ?
      this : 0;
}

Type	*ContainerType::canCompareWith(Type *_it)
{
   return lcs(_it);
}

int	ObjectType::isa(Type* it)
{
   ObjectType* o_it = it->isObject();
   if (!o_it) return 0;
   if (o_it == this) return 1;

   SymbolTable_i bases_iter(base_classes);
   Symbol* sym;
   for (; sym = *bases_iter; ++bases_iter)
   {
      if (sym->flags() != ODL_public) 
	 continue;
      if (((ObjectType *)(sym->data()))->isa(o_it))
	 return 1;
   }
   return 0;
}

int ObjectType::add(const char* name, Type* type, odlFlags as, odlFlags is_const)
{
   if (type->isMethod())
   {
      is_const = type->isMethod()->isConst();
      if (!methods.add(type->isMethod(), as))
	 return 0;
   }
   return ContainerType::add(name, type, as | is_const);
}

int MethodType::add(const char* name, Type* type, odlFlags mode)
{
   _args.enter(name, type)->setFlags((int)mode);
   return 1;
}

int InterfaceType::add(const char* name, Type* t, odlFlags readOnly)
{
   if (fwd == ODL_forward)
   {
      fwd = ODL_notforward;
      forwardInstantiated = 1;
      unsetSaved();
   }
   return ObjectType::add(name, t, currentAccessSpec, readOnly);
}

int ObjectType::AddBaseClass(ObjectType* new_base, odlFlags as)
{
   if (base_classes.lookup(new_base->name()))
      return 0;
   base_classes.enter(new_base->name(), new_base)->setFlags((int)as);
   new_base->derived_classes.enter(name(), this)->setFlags((int)as);
}

// To handle virtual base classes...
int ObjectType::_newsz()
{
/**
   m_list_t<ObjectType> *c_list = new m_list_t<ObjectType>;
   m_list_i<ObjectType> c_list_i;
   ObjectType* t;
   _size = 0;
   classlist(c_list);
   c_list_i.rewind();
   while (t = c_list_i.next()) _size += t->ContainerType::size();
   delete c_list;
   return _size;
**/
   return size();
}

void ObjectType::classlist(ObjectType **list, int &where, int& number_left)
{
   SymbolTable_i b_iter(base_classes);
   ObjectType* b;

   assert(number_left > 0);
   list[where++] = this;
   number_left--;
   Symbol* sym;
   for (; sym = *b_iter; ++b_iter)
   {
	
   		ObjectType * b = (ObjectType *)((*b_iter)->data());
      b->classlist(list, where, number_left); 
	}
}

// Lets put protection on top of the whole thing...
TableEntry* ObjectType::_member(const char* name, ContainerType::prot_modes prot)
{
   TableEntry* e;
   SymbolTable_i base_i(base_classes);
   Symbol* s;
   
   e = ContainerType::_member(name);
   if (e)
      return ((prot == Open) || 
	      ((e->properties & ODL_public) == ODL_public)) ? e: 0;

   for (; s = *base_i, !base_i.eof(); ++base_i)
   {
      if ((prot == EnforceProtection) && 
	  (((odlFlags)(s->flags()))!= ODL_public))
	 continue;
      e = ((ObjectType *)(s->data()))->_member(name, prot);
      if (e) return e;
   }
   return 0;
}

Type	*ObjectType::lcs(Type *_it)
{
   ObjectType *it;
   ObjectType **my_classes;
   ObjectType **its_classes;
   int	my_n, its_n;
   int	i, j;
   ObjectType *result;
   int remaining = 100;

   it = _it->isObject();
   if (!it)
      return (Type *)0;
   
   my_classes = new ObjectType *[100];
   my_n = 0;
   classlist(my_classes, my_n, remaining);
   its_classes = new ObjectType *[100];
   its_n = 0;
   remaining = 100;
   it->classlist(its_classes, its_n, remaining);

   for (i = 0; i < my_n; i++)
      for (j = 0; j < its_n; j++)
	 if (my_classes[i] == its_classes[j]) {
	    result = my_classes[i];
	    delete my_classes;
	    delete its_classes;
	    return result;
	 }
   delete my_classes;
   delete its_classes;
   return (ObjectType *)0;
}

Type* ContainerType::immediate_member(const char* name, prot_modes p)
{
   TableEntry* e = ContainerType::_member(name);
   if (!e) return 0;
   if ( ((e->properties & ODL_public) != ODL_public)
       && (p == EnforceProtection))
      return 0;
   return e->type;
}

/* Add the tuple entries in the 'from' tuple to our tuple */
void	TupleType::concat(TupleType &from)
{
   Symbol	*sym;
   SymbolTable_i it(from.members);
   TableEntry	*entry;

   /* XXX assumes no collisions */
   for (;sym = *it; ++it)
   {
      entry = (TableEntry *) (sym->data());
      add(sym->name(), entry->type);
   }
}

/*
   The LCS of two tuples, is the intersection of
   all attributes of the same name which have types which
   have LCSs
   */
Type	*TupleType::lcs(Type *_it)
{
   TupleType	*it = _it->isTuple();
   Type	*one,*two;
   int	i, j;
   Symbol	*is, *js;
   TupleType	*result = (TupleType *)0;
   Type	*lcs;

   if (!it)
      return (Type *)0;

   i = 0;
   SymbolTable_i outer(members);
   for (; is = *outer; ++outer) {
      j = 0;
      SymbolTable_i inner(it->members);
      for (; js = *inner; ++inner) {
	 if (strcmp(is->name(), js->name()) != 0)
	    continue;
	 one = (Type *)(is->data());
	 two = (Type *)(js->data());
#if 0
	 lcs = ((Type *)(is->data()))->lcs(((Type *)(js->data())));
#else
	 lcs = one->lcs(two);
#endif
	 if (lcs) {
	    cout << "Combine " << *one <<
	       ", " << *two <<'\n';
	    cout << "\tInto " << *lcs << '\n'; 
	    if (!result)
	       result = new TupleType();
	    result->add(is->name(), lcs);
	 }
      }
   }
   return result;
}

Type	*CollectionType::_lcs(CollectionType *it)
{
   Type	*of_lcs;

   /* CollectionType match already done by downwards lcs()s */

   of_lcs = _of->lcs(it->_of);
   if (!of_lcs)
      return (Type *)0;

   return rebuild(of_lcs);
}

Type	*BagType::lcs(Type *_it)
{
   BagType *it = _it->isBag();

   if (!it)
      return 0;

   return CollectionType::_lcs(it);
}

Type	*SetType::lcs(Type *_it)
{
   SetType *it = _it->isSet();

   /* allow set->bag downgrades, needed because of l->rebuild(r)  */
   if (!it && _it->isBag())
      return _it->lcs(this);

   return BagType::lcs(it);
}

Type	*ListType::lcs(Type *_it)
{
   ListType *it = _it->isList();

   if (!it)
      return 0;
   return CollectionType::_lcs(it);
}

Type	*ArrayType::lcs(Type *_it)
{
   ArrayType *it = _it->isArray();

   if (!it)
      return 0;
   return CollectionType::_lcs(it);
}


#if 1
const char	*TupleType::name()
{
#if 0
   //	return Type::name();
   char buf[1024];
   char buf2[128];
   Symbol *sym;
   int	token = 0;
   SymbolTable_i it(members);

   strcpy(buf, "<Tuple<");

   for (; sym = *it; ++it) {
      sprintf(buf2, "%s:%s%s",
	      sym->name(),
	      ((Type *)(sym->data()))->name(),
	      token >= 0 ? " " : "");
      strcat(buf, buf2);
   }
   return strdup(strcat(buf, ">>"));	/* XXX leak */
#else
   ostrstream buf;
   buf << *this;
   return buf.str();	/* XXX leak */
#endif
}
#endif

const char	*Type::name()
{
   return _name;
#if 0
   return strdup(form("#%#x", (int)this));
#else
   //	cout << "Type::name() using tricks\n";
   ostrstream buf;
   buf << *this;
   return buf.str();	/* XXX leak */
#endif
}

/***
int	PrimitiveType::size()
{
   int	s;

   switch (_primitive) {
    case pt_integer:
      s = sizeof(int);
      break;
    case pt_real:
      s = sizeof(double);
      break;
    case pt_boolean:
      s = sizeof(bool);
      break;
    case pt_char:
      s = sizeof(char);
      break;
    case pt_string:
      s = 0;
      break;
    case pt_void:
      s = 0;
      break;
   }

   return s;
}
***/

int	PrimitiveType::inline_size()
{
   int	s;

   switch (_primitive) {
    case pt_string:
      s = sizeof(char *);
      break;
    default:
      s = size();
      break;
   }
   return s;
}

int	ContainerType::alignment()
{
   switch (_size) {
    case 1:
    case 2:
    case 4:
    case 8:
      return _size;
      break;
    case 3:
      return 4;
      break;
    case 5:
    case 6:
    case 7:
      return 8;
      break;
    default:
      return sizeof(double);
      break;
   }
}

/* XXX for multiple inheritance, we want to compute offsets
   w/in each chunk seperately

   an object then looks like a bunch of structs ina big struct

   aka
   object FUBAR : CRUD {
   struct CRUD crud;
   struct FUBAR fubar;
   }

   alignment is confusing here
   is it the alignment of the total object,
   wor the alignment of this section ???

   Size is also confusing, since we may need to
   pad from the end of the parent;
   */


ContainerType_i	*ContainerType::iterator()
{
   return new ContainerType_i(*this);
}

MethodType_i	*MethodType::iterator()
{
   return new MethodType_i(*this);
}

MethodType::MethodType(const char* name, Type* returns, const char* name1, Type* arg1,
		       odlFlags m1, odlFlags is_const)
   : Type(name, ODL_Method), _const(is_const), _returns(returns)
#ifdef USE_ALLOCATORS
   , _args(CurrentTypesAllocator()) 
#endif
{
   add(name1, arg1, m1);
}

MethodType::MethodType(const char* name, Type* returns, 
		       const char* name1, Type* arg1, odlFlags m1, 
		       const char* name2, Type* arg2, odlFlags m2, 
		       odlFlags is_const)
  : Type(name, ODL_Method), _const(is_const), _returns(returns)
#ifdef USE_ALLOCATORS
  , _args(CurrentTypesAllocator()) 
#endif
{
   add(name1, arg1, m1);
   add(name2, arg2, m2);
}

int MethodType::clash(MethodType* m)
{
   if (nargs() != m->nargs()) 
      return 0;
   MethodType_i it1(*this);
   MethodType_i it2(*m);

   for (;(!it1.eof() && it1.curr()->match(it2.curr(), Type::R_param_in));
	++it1, ++it2);
   return it1.eof();
}

Type* MethodType::ok(TupleType* arg, TupleType*& cast_into)
{
   cast_into = new TupleType;
   if (nargs() != arg->nmembers())
      return 0;
   MethodType_i m_i(*this);
   ContainerType_i arg_i(*arg);
   Type* t1, *t2;
   Type* lcast;
   char* name;
   odlFlags flags;

   for (; t1 = m_i.curr(flags), t2 = arg_i.curr(name); ++m_i, ++arg_i)
   {
      lcast = t1->match(t2, ((flags == ODL_in) ? R_param_in : R_param_out));
      if (!lcast) 
	 return (Type *)0;
      // cast_into->add(t2->cast(lcast));
   }
   return returns();
}

Type* MethodType::ok()
{
   return (nargs() == 0) ? returns() : (Type *)0;
}

void TupleType::isField(const char* fieldName, char* names[])
{
   ContainerType_i ci(*(this->isContainer()));
   Type* t;
   int j = 0;
   names[0] = names[1] = 0;
   for (; t = ci.curr(names[j]); ++ci)
   {
      if (t->isRef()) 
	 t = t->isRef()->of();
      if (!t->isContainer()) continue;
      if (t->isContainer()->member(fieldName, 
				   ContainerType::EnforceProtection) 
	  && ++j == 2) return;
   }
   names[j] = 0;
   return;
}

void Type::save()
{
   if (_saved == false)
   {
      _saved = true;
      _save();
   }
   return;
}
      
int ContainerType::add(const char* name, Type* type, odlFlags flags)
{
   TableEntry* e;
   e = new TableEntry(type, flags);
   members.enter(name, e);
   return 1;
}

TableEntry* ContainerType::_member(const char* name, prot_modes p)
{
   Symbol* s = members.lookup(name);
   return s ? (TableEntry *)(s->data()) : (TableEntry *)0;
}

Type* ContainerType::member(const char* name, prot_modes p)
{
   TableEntry* e = _member(name, p);
   return e ? e->type : (Type *)0;
}

int ContainerType::member(const char* name, Type*& type)
{
   commit();
   TableEntry* e = _member(name);
   if (!e) 
      return -1;
   type = e->type;
   return e->offset;
}
   
Type* ContainerType::member(const char* name, odlFlags& props, prot_modes p)
{
   TableEntry* e = _member(name, p);
   return e ? (props = e->properties, e->type) : (Type *)0;
}

MethodType* ObjectType::lookup_method(const char* name, Type* arg)
{
   Type* cast;
   return lookup_method(name, arg, cast);
}

MethodType* ObjectType::lookup_method(const char* name, Type* arg, Type*& cast)
{
   MethodType* t = methods.lookup(name, arg, cast);
   if (t) 
      return t;

   SymbolTable_i bases_i(base_classes);
   for (; !bases_i.eof(); ++bases_i)
   {
      t = ((ObjectType *)(*bases_i)->data())->lookup_method(name, arg, cast);
      if (t) return t;
   }
   return (MethodType *)0;
}

Type* MethodType_i::curr()
{
   if (eof()) return 0;
   return (Type *)((*_args)->data());
}

Type* MethodType_i::curr(const char*& name)
{
   if (eof()) return 0;
   Symbol* s = *_args;
   name = s->name();
   return (Type *)(s->data());
}

Type* MethodType_i::curr(odlFlags& flags)
{
   if (eof()) return 0;
   Symbol* s = *_args;
   flags = s->flags();
   return (Type *)(s->data());
}

TableEntry* ContainerType_i::_curr(const char*& name)
{
   if (eof()) return 0;
   Symbol* s = *_members;
   name = s->name();
   return (TableEntry *)(s->data());
}
 
Type* ContainerType_i::curr(const char*& name)
{
   return eof() ? (Type *)0 : _curr(name)->type;
}

Type* ContainerType_i::curr(const char*& name, int& offset, odlFlags& flags)
{
   TableEntry* e;
   if (eof())
      return (Type *)0;
   e = _curr(name);
   offset = e->offset;
   flags = e->properties;
   return e->type;
}

Type* ContainerType_i::curr(const char*& name, odlFlags& flags)
{
   TableEntry* e;
   return eof() ? (Type *)0 
                : (e = _curr(name), flags = e->properties, e->type);
}

int MethodTable::add(MethodType* m, odlFlags flags)
{
   const char* name = m->name();
   flags |= m->isConst();

   // Check to see if a similar method exists already ?
   SymbolTable_i meth_i(_methods, m->name());
   Symbol* s;
   for (; s = *meth_i; ++meth_i)
      if (((MethodType *)(s->data()))->clash(m))
	 return 0;

   // Add this
   _methods.enter(name, m)->setFlags(flags);
   return 1;
}

MethodType* MethodTable::lookup(const char* name, Type* arg, Type*& cast)
{
   MethodType* m;
   if (!arg || arg->isVoid())
   {
      m = _lookup(name);
      if (m) 
	 cast = arg;
      return m;
   }
   if (!arg->isTuple())
      return (MethodType *)0;
   TupleType* castinto = new TupleType;
   m = _lookup(name, arg->isTuple(), castinto);
   cast = (Type *)castinto;
   return m;
}

MethodType* MethodTable::lookup(const char* name, Type* arg)
{
   TupleType* castinto = new TupleType;
   if (!arg || arg->isVoid())
      return _lookup(name);
   if (!arg->isTuple())
      return (MethodType *)0;
   return _lookup(name, arg->isTuple(), castinto);
}

MethodType* MethodTable::_lookup(const char* mname, TupleType* args, 
				TupleType*& cast_type)
{
   SymbolTable_i iter(_methods, mname);
   TupleType* casts[2];
   MethodType* meths[2];
   int where = 0;
   Symbol* s;
   const char* name;
   MethodType* m;
   odlFlags flags;
   
   for (; (where < 2) && (s = *iter); ++iter)
   {
      m = (MethodType *)(s->data());
      flags = s->flags();
      if ((flags & ODL_public) != ODL_public)
	 continue;
      if (m->ok(args, casts[where]))
	 meths[where++] = m;
   }
   switch(where)
   {
    case 0:
      // Not found...
      return 0;
    case 2:
      // Ambiguous...
      return 0;
    default:
      cast_type = casts[0];
      return meths[0];
   }
}

// Is there a method with no parameters...
MethodType* MethodTable::_lookup(const char* name)
{
   SymbolTable_i m_i(_methods, name);
   Symbol* s;
   MethodType* m;
   odlFlags flags;
   
   for (; s = *m_i; ++m_i)
   {
      flags = ((odlFlags)(s->flags()));
      if ((flags & ODL_public) != ODL_public)
	 continue;
      m = (MethodType *)(s->data());
      if (m->nargs() == 0)
	 return m;   
   }
   return 0;
}
   
Type* ScopeType::lookup(const char* name)
{
   return _types->lookup(name);
}

void ScopeType::AddType(const char* name, Type* t)
{
   _types->add(name, t);
}

int ScopeType::RemoveType(const char* name)
{
   return _types->remove(name);
}

Type* ContainerType::_constructor(TupleType* tuple)
{
   if (tuple->isLabelled())
      return _labelledConstructor(tuple);
   TupleType* casttype;
//   if (nmembers() != tuple->nmembers())
//      return (Type *)0;
   ContainerType_i t_i(*tuple);
   casttype = new TupleType;
   if (!ContainerType::_constructor(t_i, *casttype))
      return (Type *)0;
   if (!t_i.eof())
      return (Type *)0;
   return (Type *)casttype;
}

int ContainerType::_constructor(ContainerType_i& tuple, TupleType& casttype)
{
   ContainerType_i me(*this);
   Type* t1, *t2, *lcast;
   const char* name;

   while (!tuple.eof() && !me.eof())
   {
      t1 = me.curr(name);
      t2 = tuple.curr(name);
      if (t1->isMethod())
      {
	 ++me;
	 continue;
      }
      lcast = t1->match(t2, R_param_in);
      if (!lcast)
	 return 0;
      casttype.add(t2->cast(lcast));
      ++me;
      ++tuple;
   }
   while (!me.eof())
   {
      t1 = me.curr(name);
      if (!t1->isMethod())
	 break;
      ++me;
   }
   if (tuple.eof() && !me.eof())
      return 0;
   return 1;
}

int ObjectType::_constructor(ContainerType_i& tuple, TupleType& casttype)
{
   // First iterate thru the base classes
   SymbolTable_i b_i(base_classes);
   ObjectType* b;
   Symbol* s;

   for (; s = *b_i; ++b_i)
   {
      b = (ObjectType *)(s->data());
      if (!b->_constructor(tuple, casttype))
	 return 0;
   }

   // Now iterate over myself...
   return ContainerType::_constructor(tuple, casttype);
}

Type* ContainerType::_labelledConstructor(TupleType* arg)
{
   TupleType* casttype = new TupleType;
   Type* t1, *t2, *lcast;
   const char* name;
   if (nattrs() != arg->nmembers())
      return (Type *)0;
   ContainerType_i tuple_i(*arg);
   
   for (; !tuple_i.eof(); ++tuple_i)
   {
      t2 = tuple_i.curr(name);
      t1 = member(name);
      if (!t1)
	 return 0;
      lcast = t1->match(t2, R_param_in);
      casttype->add(t2->cast(lcast));
   }
   return casttype;
}

int ObjectType::nattrs()
{
   // First iterate thru the base classes
   SymbolTable_i b_i(base_classes);
   ObjectType* b;
   Symbol* s;
   int n = 0;

   for (; s = *b_i; ++b_i)
   {
      b = (ObjectType *)(s->data());
      n += b->nattrs();
   }
   n += nmembers();
   n -= methods.nmethods();
   return n;
}

Type* ObjectType::_constructor(TupleType* arg)
{
   // Iterate...
   if (arg->isLabelled())
      return _labelledConstructor(arg);
   ContainerType_i tuple_i(*arg);
   TupleType* casttype = new TupleType;
   if (!_constructor(tuple_i, *casttype))
      return 0;
   if (!tuple_i.eof())
      return 0;
   return casttype;
}

int TupleType::add(Type* t)
{
   _labelled = 0;
   return ContainerType::add("", t);
}

int TupleType::add(const char* name, Type* t)
{
   if (!name || !name[0])
      return add(t);
   if (ContainerType::member(name))
      return 0;
   if (strlen(name)>0)
	   _labelled = 1;
	else
		_labelled=0;
   return ContainerType::add(name, t);
}

Type* Type::constructor(Type* arg)
{
   TupleType* tuple = arg->isTuple();
   if (!tuple && !arg->isVoid())
      return (Type *)0;
   if (!tuple)
      tuple = new TupleType;
   Type* casttype = new TupleType;
   if (isObject()  && (tuple->nmembers()==1)
	&& tuple->member("") && tuple->member("")->isTuple())
   // yucko
	tuple = tuple->member("")->isTuple();
   if (me() != ODL_Ref && me() !=ODL_Interface)
      if (lookup_method(name(), tuple, casttype))
	 return casttype;
   return _constructor(tuple);
}





// a bunch of type stuff from oql, currenty not used but virtuall..
void ObjectType::_commit(){}
// void ContainerType::_commit(){}
// implement this so we can have dynamic types...
void MethodType::_save(void) {}
void PrimitiveType::_save(void) {}
void CollectionType::_save(void) {}
void RefType::_save(void) {}
void ContainerType::_save(const char *) {}
void InterfaceType::_save(const char *) {}


BagType::size(void) { return 0;}
RefType::size(void) { return 0;}
SetType::size(void) { return 0;}
ListType::size(void) { return 0;}
ArrayType::size(void) { return 0;}
