#ifndef _TYPES_HH_
#define _TYPES_HH_
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <iostream.h>

#include <auto_string.h>
#include <symbol.h>
//#include <defns.h>

#include <tsi.h>

// odd def seems to be needed.
#define ADT class
//#include <adt.h>


class TypeDB;
class ExtentDB;

class SystemType;
class Type;
class PrimitiveType;
class ContainerType;
class TupleType;
class CollectionType;
class IndexedCollectionType;
class ObjectType;
class StructType;
class ListType;
class ArrayType;
class MethodType;
class SetType;
class BagType;
class RefType;
class InterfaceType;
class ModuleType;
class ScopeType;

class Descriptor;
class MethodTable;

#ifdef STAND_ALONE
class SystemType
{
 public:
#ifdef USE_ALLOCATORS
   void* operator new(long sz);
   void  operator delete(void *) {}
#endif USE_ALLOCATORS
};
#else STAND_ALONE
#include <system_types.h>
#endif STAND_ALONE

#include <lalloc.h>
class Allocator* CurrentTypesAllocator();

class Type: public SystemType
{
 public:
   enum Reason {R_param_in, R_param_out, R_assign_attribute, R_assign};
   friend ostream &operator<<(ostream &s, Type &type);
 protected:
   TypeType _me;
   auto_string _name;
   bool _saved; 
   virtual void _save() {}
   virtual Type* _match(Type* t, Reason why);
   virtual Type* _constructor(TupleType* arg) {return (Type *)0;}
 public: /* protected: */
   Type(const char *name = 0, TypeType i = ODL_Type, bool saved = false)
#ifdef USE_ALLOCATORS
      : _name(CurrentTypesAllocator(), name), _saved(saved), _me(i) {}
#else  USE_ALLOCATORS
      : _name(name), _saved(saved), _me(i) {}
#endif USE_ALLOCATORS
   TypeType me() {return _me;}

   bool isNamed() {return _name.str()[0] != 0;}
   virtual const char *name();
   void setName(const char *name) {_name.set(name);}

   bool isSaved() {return _saved;} 
   void setSaved() {_saved = true;}
   void unsetSaved() {_saved = false;}

   virtual int isForward() {return 0;}

   virtual int isa(Type* t) {return this == t;}
   int isa(Type& t) {return isa(&t);}

   int operator ==(Type& t) {return *this == &t;}
   virtual int operator ==(Type* t) {return this == t;}

      // Returns the type of the CAST if any to be done...
   Type* match(Type* t, Reason why) {
      return _match(t, why);
   }
   Type* cast(Type* outp);

   /* Downcasts */ 
   virtual PrimitiveType *isPrimitive() {return 0;}

   virtual ScopeType  *isScope() {return 0;}
   virtual ContainerType *isContainer() {return 0;}
   virtual TupleType *isTuple() {return 0;}
   virtual ObjectType *isObject() {return 0;}
   virtual StructType *isStruct() {return 0;}

   virtual MethodType *isMethod() {return 0;}

   virtual CollectionType *isCollection() {return 0;}
   virtual IndexedCollectionType *isIndexedCollection() {return 0;}
   virtual ArrayType *isArray() {return 0;}
   virtual ListType *isList() {return 0;}
   virtual SetType *isSet() {return 0;}
   virtual BagType *isBag() {return 0;}

   virtual RefType *isRef() {return 0;}

   virtual InterfaceType *isInterface() {return 0;}
   virtual ModuleType *isModule() {return 0;}

   /* psuedo-Downcasts */
   virtual int isInteger() {return 0;}
   virtual int isBoolean() {return 0;}
   virtual int isVoid()    {return 0;}

   /* arithmetic and object compatabilities */
   virtual Type *canCompareWith(Type *it) {return 0;}
   virtual Type *canDoMathWith(Type *it) {return 0;}
   virtual Type *canDoMath() {return 0;}
   virtual Type *lcs(Type *it) {return 0;}

   virtual ostream &print(ostream &s) {
      return s << "<Type " << Type::name() << ">";
   }

   /* methods for realizing types */
   virtual void commit() {}
   virtual int size() {return 0;}
   virtual int isVarLength() {return 0;}
   virtual int inline_size() {return size();}
   virtual int alignment() {return inline_size();}

   /* Default save action is to not do anything */
   void save();
   void save_typedef(const char* name);

   virtual MethodType* lookup_method(const char* name, Type* arg = 0) {
      return (MethodType *)0;
   }
   virtual MethodType* lookup_method(const char* name, Type* arg, Type*& cast){
      return (MethodType *)0;
   }
   Type* constructor(Type* arg);
};

class ErrorType : public Type 
{
 public:
   ErrorType(const char *name = 0); // : Type(name, ODL_Error) {}
   // make this external so we can break on it.
};

extern ErrorType* TypeError;

class MethodTable
{
 private:
   SymbolTable _methods;
   MethodType* _lookup(const char* name);
   MethodType* _lookup(const char* name, TupleType* arg, TupleType*& castinto);
 public:
#ifdef USE_ALLOCATORS
   MethodTable() : _methods(CurrentTypesAllocator()) {}
#else
   MethodTable() {}
#endif
   int add(MethodType* m, odlFlags = ODL_public);
   MethodType* lookup(const char* name, Type* arg, Type*& cast_into);
   MethodType* lookup(const char* name, Type* arg = 0);
   int nmethods() {return _methods.size();}
};

#include "primitive_types.h"
class PrimitiveType : public Type {
 public:
 typedef TypeTag PrimitiveTypes; 
 private:
   enum PrimitiveTypes _primitive;
   MethodTable methods;
   int _size;
   int _isVarLen;
   void _save();
 public: /*protected:*/
   PrimitiveType(const char* name, PrimitiveTypes prim, int sz)
      : Type(name, ODL_Primitive), _size(sz), _primitive(prim), 
        _isVarLen(0) {save();}
   PrimitiveType(const char* name, PrimitiveTypes prim)
      : Type(name, ODL_Primitive), _size(sizeof(int)), _primitive(prim),
        _isVarLen(1) {save();}
 public:
   virtual PrimitiveType *isPrimitive() {return this;}
   enum PrimitiveTypes primitive() {return _primitive;}
   Type *lcs(Type *it);
   Type *canCompareWith(Type *it);
   Type *canDoMathWith(Type *it);
   Type *canDoMath();
   int isInteger(); // {return _primitive == pt_integer;}
   int isBoolean() {return _primitive == pt_boolean;}
   int isVoid()    {return _primitive == pt_void;}
   ostream &print(ostream &s) {return Type::print(s);}
   
   int add(MethodType* m);
   MethodType* lookup_method(const char* name, Type* argType = 0){
      return methods.lookup(name, argType);
   }
   MethodType* lookup_method(const char* name, Type* argType, Type*& cast_type){
      return methods.lookup(name, argType, cast_type);
   }

   Type* _match(Type* t, Reason why);

   int isa(Type* t);

   int size()     {return _size;}
   int isVarLen() {return _isVarLen;}
   int inline_size();

   void extract(Descriptor &d, int offset, void *);
};

extern PrimitiveType* TypeVoid;

/*
   Base type for types containing named members
   This is really a reusable interface class for container objects.
   */

class TableEntry {
 public:
   Type *type;
   int offset;
   odlFlags properties;

   TableEntry(Type* t, odlFlags props = ODL_private | ODL_readwrite,
	      int _offset = 0)
      : offset(_offset), type(t), properties(props) {}
#ifdef USE_ALLOCATORS
   void* operator new(long sz);
   void  operator delete(void *) {}
#endif USE_ALLOCATORS
};

class ContainerType_i;

class ContainerType : public Type {
   friend class ContainerType_i;
 protected: /* private: */
   SymbolTable members;
   int _size;
   int _align;
   bool _commitStarted;
   int _isVarLen;

   virtual void _commit();
   void _save() {
      _save(name());
   }
   virtual void _save(const char* name);
 public: /* protected: */
   enum prot_modes {EnforceProtection, Open};

   ContainerType(const char *name, TypeType i = ODL_Container)
      : Type(name, i), _size(-1), _isVarLen(0), _commitStarted(false)
#ifdef USE_ALLOCATORS
        ,members(CurrentTypesAllocator()) 
#endif
			{}
   /* This is only visible for tuples in real life */

   int add(const char* name, Type* type, 
	   odlFlags flags = ODL_public | ODL_readwrite);
   int add(Type* type, odlFlags flags = ODL_public | ODL_readwrite) {
      return add(type->name(), type, flags);
   }
   ostream &_print(ostream &s, const char *type_name);
   virtual TableEntry *_member(const char *name, prot_modes p = Open);
   virtual Type* _constructor(TupleType* tuple);
   int _constructor(ContainerType_i& tuple, TupleType& casttype);
   Type* _labelledConstructor(TupleType* arg);
 public:
   int nmembers() {return members.size();}
   virtual int nattrs() {return nmembers();}

   Type* member(const char *name, prot_modes p = Open);
   Type* immediate_member(const char* name, prot_modes p = Open);
   Type* member(const char* name, odlFlags& props, prot_modes p = Open);
   int   member(const char* name, Type*& type);

   virtual ostream &print(ostream &s) = 0;
   virtual ContainerType *isContainer() {return this;}
   virtual Type *canCompareWith(Type *of);

   virtual int size() {
      commit();
      return _size;
   }
   virtual int isVarLen() {
      commit();
      return _isVarLen;
   }
   int alignment();

      // Forces this type to go to the store...
   void force_save(const char* name) {
      _save(name);
   }

   void commit() {
      if (_commitStarted == false) 
	 _commit();
      _commitStarted = true;
   }
   virtual ContainerType_i *iterator(); // over members
};

class ObjectType : public ContainerType {
 protected:
   SymbolTable base_classes;
   SymbolTable derived_classes;
   MethodTable methods;

   void classlist(ObjectType **list, int &where, int& remaining); 

   int _newsz(); // The right algo to find the size...

   Type* _match(Type* t, Reason why);

   virtual Type* _constructor(TupleType* tuple);
   int _constructor(ContainerType_i& tuple, TupleType& casttype);

   virtual void _commit();
   virtual void _save(const char* name) {}
 public: /*protected:*/
   ObjectType(const char *name, TypeType i = ODL_Object) 
      : ContainerType(name, i)
#ifdef USE_ALLOCATORS
	  , base_classes(CurrentTypesAllocator()),
        derived_classes(CurrentTypesAllocator()) 
#endif
	{}

      // Overrides the _member() from ContainerType
   TableEntry* _member(const char* name, prot_modes prot = Open);
 public:
   int add(const char* name, Type* type, 
	   odlFlags = ODL_public, odlFlags is_const = ODL_readonly);
   int isa(Type *it);
   Type *lcs(Type *it);
   virtual ObjectType *isObject() {return this;}
   ostream &print(ostream &s) {return ContainerType::_print(s, 0);}

   int nattrs();

   MethodType* lookup_method(const char* name, Type* arg, Type*& cast_into);
   MethodType* lookup_method(const char* name, Type* arg = TypeVoid);

   int AddBaseClass(ObjectType* new_base, odlFlags as = ODL_public);
   SymbolTable& bases() {return base_classes;}

   int inline_size() {return sizeof(void *);}
   int alignment() {return inline_size();}
};

class StructType : public ContainerType {
private:
public: /* protected: */
  StructType(const char *name = 0): ContainerType(name, ODL_Struct) {};
public:
  virtual StructType *isStruct() {return this;}
  ostream &print(ostream &s) {
     return ContainerType::_print(s, "Struct");
  }
};

class TupleType : public ContainerType {
private:
   int _labelled;
public: /* protected: */
  TupleType(const char *name = 0) 
     : ContainerType(name, ODL_Tuple), _labelled(0) {};
public:
   int add(const char* name, Type* t);
   int add(Type* t);
   int isLabelled() {return _labelled;}
   void concat(TupleType &from);
   const char *name();
   Type *lcs(Type *it);
   virtual TupleType *isTuple() {return this;}
   ostream &print(ostream &s) {return ContainerType::_print(s, "Tuple");}
      // Purely for OQL processing...
   void isField(const char* fieldName, char* names[]);
};

class MethodType : public Type 
{
friend class MethodType_i;
private:
  Type *_returns;
  odlFlags _const;
  SymbolTable _args;
public: /* protected: */
  MethodType(const char *name, Type *returns = TypeVoid, 
	     odlFlags is_const = ODL_readwrite)
   : Type(name, ODL_Method), _const(is_const), _returns(returns)
#ifdef USE_ALLOCATORS
     ,_args(CurrentTypesAllocator()) 
#endif
	{}
  MethodType(Type *returns = TypeVoid, odlFlags is_const = ODL_readwrite)
   : Type(0, ODL_Method), _returns(returns), _const(is_const)
#ifdef USE_ALLOCATORS
   , _args(CurrentTypesAllocator()) 
#endif
   {}

  void setReturns(Type *type) {_returns = type;}
  int nargs() {return _args.size();}
  
  // Some functions to make life easy for me later
  MethodType(const char* name, Type* returns, 
	     const char* name1, Type* arg1, odlFlags = ODL_in, 
	     odlFlags is_const = ODL_readwrite);
  MethodType(const char* name, Type* returns, 
	     const char* name1, Type* arg1, odlFlags m1,
	     const char* name2, Type* arg2, odlFlags m2,
	     odlFlags is_const = ODL_readonly);
public:
  odlFlags isConst() {return _const;}
  Type *returns() {return _returns;}
  Type *argument(const char *name) {
     Symbol *s = _args.lookup(name);
     return s ? (Type *)(s->data()) : (Type *)0;
  }

  Type* member(const char* name) {return argument(name);}
  Type* immediate_member(const char* name) {return member(name);}

  virtual MethodType *isMethod() {return this;}
  int add(const char* name, Type* type, odlFlags mode = ODL_in);
  MethodType_i *iterator(); // over args

  int clash(MethodType*);
  Type* ok(TupleType* arg, TupleType*& cast_into);
  Type* ok();

  ostream &print(ostream &s);
  void _save();
};


/*
  XXXX
  
  Reference share the interface that Collections use,
  but a reference isn't a collection, so it shouldn't be
  part of that class tree.
  */ 

class RefType : public Type {
private:
  Type *_of;
  void _save();
  
public: /* protected: */
  RefType(const char *name, Type *of) : Type(name, ODL_Ref), _of(of) {}
  RefType(Type *of) : Type(0, ODL_Ref), _of(of) {}

  void setOf(Type *of) {_of = of;}
public:
  Type *of() {return _of;}
  virtual RefType *isRef() {return this;}
  Type *canCompareWith(Type *of);
  Type *rebuild(Type *of);

  Type *lcs(Type *it);
  
  int  size();
  // inline_size() will remain the same...
  // isVarLen() is False, as it must be...
  
  // Type *operator->() {return _of;}
  Type *operator*() {return _of;}

  int isa(Type* t);
  int operator == (Type* t);
  Type* _match(Type* t, Reason why);

  MethodType* lookup_method(const char* name, Type* arg = TypeVoid) {
     return of()->lookup_method(name, arg);
  }
  MethodType* lookup_method(const char* name, Type* arg, Type*& cast_into) {
     return of()->lookup_method(name, arg, cast_into);
  }
  ostream &print(ostream &s);
};

#if 0
class NilRefType : public RefType {
};
#endif

/*
  Collections of similar types

  As with the container, this class is a reusable interface class
  */

class CollectionAccessor;

class CollectionType : public Type {
private:
  Type *_of;
  void _save();
public: /* protected: */
  CollectionType(const char *name, Type *of, TypeType i = ODL_Collection)
     : Type(name, i), _of(of) {}
  CollectionType(Type *of, TypeType i = ODL_Collection)
     : Type((const char *)0, i), _of(of) {}

  void setOf(Type *of) {_of = of;}
  ostream &_print(ostream &s, const char *type_name);
public:
  virtual Type *rebuild(Type *of) = 0;
  Type *of() {return _of;}
  virtual CollectionType *isCollection() {return this;}

  virtual Type *lcs(Type *it) = 0;
  Type *_lcs(CollectionType *it);
  
  virtual ostream &print(ostream &s) = 0;

  int inline_size() {return sizeof(int) + sizeof(void *);}
  int alignment() {return sizeof(int);}
  int isVarLength() {return 1;}
    // Collections are variable length by default...

  virtual int isa(Type* t);
  int operator == (Type* t) {return ContainerType::isa(t);}
  virtual Type* _match(Type* t, Reason why);

  CollectionAccessor *accessor(Descriptor &d, int offset = 0);
};

class BagType : public CollectionType {
public:
  virtual BagType *isBag() {return this;}
  BagType(const char *name, Type *of, TypeType i = ODL_Bag)
   : CollectionType(name, of, i) {}
  BagType(Type *of, TypeType i = ODL_Bag)
   : CollectionType(of, i) {}
  virtual Type *rebuild(Type *of) {return new BagType(of);}
  virtual Type *lcs(Type *it);
  virtual int inline_size() { return size(); } ; 
  virtual int size(); // { return sizeof(sdl_string) ; } ; 
  ostream &print(ostream &s) {return CollectionType::_print(s, "Bag");}
};

class SetType : public BagType {
public:
  virtual SetType *isSet() {return this;}
  SetType(const char *name, Type *of): BagType(name, of, ODL_Set) {}
  SetType(Type *of): BagType(of, ODL_Set) {}
  virtual Type *rebuild(Type *of) {return new SetType(of);}
  Type *lcs(Type *it);
  ostream &print(ostream &s) {return CollectionType::_print(s, "Set");}
  int isa(Type* t);
  virtual int inline_size() { return size() ; } ; 
  virtual int size(); // { return sizeof(sdl_string) ; } ; 
};

class IndexedCollectionType : public CollectionType {
  int _cardinality;
public:
  IndexedCollectionType(const char *name, Type *of, TypeType i = ODL_IndexedCollection)
     : CollectionType(name, of, i), _cardinality(0) {}
  IndexedCollectionType(Type *of, TypeType i = ODL_IndexedCollection)
     : CollectionType(of, i), _cardinality(0) {}
  virtual IndexedCollectionType *isIndexedCollection() {return this;}
  int cardinality() {return _cardinality;}
  void setCardinality(int newcard) {_cardinality = newcard;}
  virtual Type *rebuild(Type *of) = 0;
  virtual Type *lcs(Type *it) = 0;
  virtual ostream &print(ostream &s) = 0;
};

class ListType : public IndexedCollectionType {
public:
  virtual ListType *isList() {return this;}
  ListType(const char *name, Type *of)
   : IndexedCollectionType(name, of, ODL_List) {}
  ListType(Type *of)
   : IndexedCollectionType(of, ODL_List) {}
  virtual Type *rebuild(Type *of) {return new ListType(of);}
  Type *lcs(Type *it);
  ostream &print(ostream &s) {return CollectionType::_print(s, "List");}
  virtual int inline_size() { return size() ; } ; 
  virtual int size(); // { return sizeof(sdl_string) ; } ; 
};

class ArrayType : public IndexedCollectionType {
public:
  virtual ArrayType *isArray() {return this;}
  ArrayType(const char *name, Type *of)
   : IndexedCollectionType(name, of, ODL_Array) {}
  ArrayType(Type *of)
   : IndexedCollectionType(of, ODL_Array) {}
  virtual Type *rebuild(Type *of) {return new ArrayType(of);}
  Type *lcs(Type *it);
  ostream &print(ostream &s) {return CollectionType::_print(s, "Array");}
  virtual int inline_size() { return size() ; } ; 
  virtual int size(); // { return (sizeof(sdl_string) + sizeof(sdl_set)) ; } ; 
};

extern ObjectType* TypeNil; 

class ContainerType_i {
  SymbolTable_i _members;
  TableEntry* _curr(const char*& name);
public:
  ContainerType_i(ContainerType &it) : _members(it.members) {}
  int eof() {return _members.eof();}
  ContainerType_i& operator ++() {return ++_members, *this;}
  Type* curr(const char*& name);
  Type* curr(const char*& name, odlFlags& flags);
  Type* curr(const char*& name, int& offset, odlFlags& flags);
};

class MethodType_i {
  SymbolTable_i _args;
public:
  MethodType_i(MethodType &it) : _args(it._args) {}
  Type* curr();
  Type* curr(const char*& name);
  Type* curr(odlFlags& flags);
  MethodType_i& operator ++() {++_args; return *this;}
  int eof() {return _args.eof();}
};

class CollectionAccessor {
public:
  virtual int count() = 0;
  virtual Type &of() = 0;
  virtual bool at(int i, Descriptor &d) = 0;
  virtual bool next(Descriptor &d) = 0 ;
  virtual bool more() = 0;
};

class ScopeType
{
public:
  TypeDB* _types;

  ScopeType(TypeDB* t) : _types(t) {}
  virtual ScopeType* isScope() {return this;}
  Type* lookup(const char* name);
  void AddType(const char* name, Type* t);
  int  RemoveType(const char* name);
};

class InterfaceType: public ObjectType, public ScopeType
{
public:
  odlFlags currentAccessSpec;
  odlFlags fwd;
  int forwardInstantiated;

  InterfaceType(const char* name, odlFlags fwd = ODL_notforward);

  InterfaceType* isInterface() {return this;}
  ScopeType* isScope() {return ScopeType::isScope();}
      // A dirty hack, but seems justified...
   // Hack for now...
  int isForward() {return fwd == ODL_forward;}
  int add(const char* name, Type* t, odlFlags = ODL_readwrite);
  void _save(const char* name);
};

class ModuleType: public ContainerType, public ScopeType
{
public:
  ModuleType(const char* name);

  ModuleType* isModule() {return this;}
  ScopeType* isScope() {return ScopeType::isScope();}
     // A dirty hack, but seems justified...
  ostream &print(ostream &s) {return ContainerType::_print(s, "Module");}
  void _save() {}
};

/** This is the universe. container class for all types and extents... **/
class DataBase: public ScopeType
{
public:
  ExtentDB* _extents;

  DataBase(TypeDB* t = 0, ExtentDB* ex = 0)
   : ScopeType(t), _extents(ex) {}
  void reset(TypeDB* t, ExtentDB* ex) {
     _types = t; 
     _extents = ex;
  }
};

#endif /* _TYPES_HH_ */
