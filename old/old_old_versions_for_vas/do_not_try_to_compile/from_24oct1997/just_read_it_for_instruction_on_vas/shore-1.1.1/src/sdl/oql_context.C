/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef STAND_ALONE
#include <macros.h>
//#include <pthread.h>
#include <globals.h>
#endif STAND_ALONE

#include <defns.h>
#include <lalloc.h>

// A temporary hack...
//Allocator GlobalAllocator;
#ifdef USE_ALLOCATORS
Allocator* globalAllocatorPtr = (Allocator *)0;
#endif USE_ALLOCATORS

#include <base_types.h>
#include <oql_tsi.h>

#ifndef NORET
#define NORET
#endif

/** Define the global types **/
TypePtr TypeLongRef, TypeUlongRef, TypeShortRef, TypeUshortRef;
TypePtr TypeDoubleRef, TypeFloatRef, TypeBoolRef, TypeCharRef;
TypePtr TypeVoidRef, TypeStringRef, TypeAnyRef, TypeOctetRef;

/** Bolo additions...**/
TypePtr TypeNilRef, TypeErrorRef;

/** Paradise ADTs **/
TypePtr TypeRealRef, TypeIntRef, TypePointRef, TypePolygonRef;
TypePtr TypePolylineRef, TypeRasterRef, TypeVideoRef, TypeCircleRef;

     // Defined in ../oql/oql.c
void OqlInit();

#ifndef STAND_ALONE
     // Are defined in gis/common/comm/src/server.c
int setupOpenDb(const char* dbName);
int setupCloseDb();
int setupDropDb(const char* dbName);
#endif STAND_ALONE

oql_rc_t CreateBaseTypes();
oql_rc_t CreateADTs();
NORET  oqlDatabase::oqlDatabase(const char* name = 0)
#ifdef USE_ALLOCATORS
: extents(&GlobalAllocator, "extents"), types(&GlobalAllocator, "types")
#else
: extents( "extents"), types( "types")
#endif
{
  _name = strdup(name ? name : "");
#ifndef STAND_ALONE
#ifdef NO_SDL
  t_array = 0;
#endif 
#endif STAND_ALONE
  CreateBaseTypes();
  OqlInit();
}

NORET oqlDatabase::~oqlDatabase()
{
  free(_name);
}

#ifdef NO_SDL
#define PR(s) PrimitiveType::s
#else
#define PR(s) s
#endif

oql_rc_t CreateBaseTypes()
{
   TypeCharRef = new PrimitiveType("char",  PR(pt_char), 
				   sizeof(char));
   TypeFloatRef = new PrimitiveType("real", PR(pt_real), 
				    sizeof(float));
   TypeBoolRef = new PrimitiveType("boolean", PR(pt_boolean),
				   sizeof(char));
   TypeStringRef= new PrimitiveType("string", PR(pt_string));
   TypeErrorRef= new ErrorType("*ERROR*");
   TypeNilRef = new ObjectType("nil");
   TypeVoidRef = new PrimitiveType("void", PR(pt_void));

   // Set the rest of the types to 0
   TypeShortRef = TypeUshortRef = TypeUlongRef = TypeLongRef = 0;
   TypeDoubleRef = 0;
   TypeOctetRef = TypeAnyRef = 0;
   TypeShortRef = new PrimitiveType("short",Sdl_short);
   TypeUlongRef = new PrimitiveType("unsigned",Sdl_unsigned_long);
   TypeDoubleRef = new PrimitiveType("double",Sdl_double);

   return OQL_OK;
}



oql_rc_t oqlDatabase::AddBaseTypes()
{
   types.add(TypeLongRef);
   types.add(TypeShortRef);
   types.add(TypeUlongRef);
   types.add(TypeUshortRef);

   types.add(TypeFloatRef);
   types.add(TypeDoubleRef);

   types.add(TypeStringRef);
   types.add(TypeBoolRef);
   types.add(TypeCharRef);
   
   types.add(TypeOctetRef);
   types.add(TypeVoidRef);
   types.add(TypeAnyRef);

    // Bolo additions
   types.add(TypeErrorRef);
   types.add(TypeNilRef);

   return OQL_OK;
}

oql_rc_t oqlDatabase::AddADTs()
{
   types.add(TypeIntRef);
   types.add("int", TypeIntRef);
   types.add(TypePointRef);
   types.add(TypePolygonRef);
   types.add(TypePolylineRef);
   types.add(TypeCircleRef);
   types.add(TypeRasterRef);
   types.add(TypeVideoRef);

   return OQL_OK;
}

oql_rc_t oqlDatabase::LoadAtomicTypes()
{
   AddBaseTypes();
   AddADTs();

   return OQL_OK;
}

#ifndef STAND_ALONE
#ifdef NO_SDL
oql_rc_t oqlDatabase::attach(Catalog *cat)
{
  if (!cat)
    return oqlATTACH_NULL_CAT;
  _cat = cat;
  return OQL_OK;
}
#endif

oql_rc_t oqlContext::create_db(const char *dbname)
{
#ifdef NO_SDL
  Catalog* dummy = new Catalog;
  dummy->createDatabase(dbname);
//  if (dummy->createDatabase(dbname) != 0)
//    OQL_FATAL(oqlCREATE_DB);
   delete dummy;
#endif
  return OQL_OK;
}

oql_rc_t oqlContext::open_db(const char *dbname)
{
  free(db._name);
  db._name = strdup(dbname);
//  if (_cat->openDb(dbname) != 0)
//   if (setupOpenDb(dbname) != 0)
//    OQL_FATAL(oqlOPEN_DB);
#ifdef NO_SDL
  INT_PARA_CALL(setupOpenDb(dbname));

// Now attach this to our context
  attach(catalog);
  reset(dbname);
#endif
  
  // now load the catalog info of db into our type system
  OQL_DO(db.LoadAtomicTypes());
  OQL_DO(db.LoadOtherTypes());

  _open = 1;
  return OQL_OK;
}

oql_rc_t oqlContext::close_db()
{
//  if (_cat->closeDb() != 0)
//    OQL_FATAL(oqlCLOSE_DB);
   if (!_open)
   {
      errstream() << "No database currently open. Open one first..." << endl;
      return OQL_OK;
   }
   if (setupCloseDb() != 0)
      OQL_FATAL(oqlCLOSE_DB);
   db.reset();
   
   _open = 0;
   return OQL_OK;
}

oql_rc_t oqlContext::drop_db(const char* dbname)
{
   errstream() << "WARNING: destroy db not implemented!\n";
   return OQL_OK;
}

oql_rc_t oqlContext::drop_extent(const char* dbname)
{
   errstream() << "WARNING: destroy extent not implemented!\n";
   return OQL_OK;
}

oql_rc_t oqlContext::drop_index(const char* dbname)
{
   errstream() << "WARNING: destroy index not implemented!\n";
   return OQL_OK;
}

#ifdef NO_SDL
oqlContext::attach(Catalog* cat)
{
   _cat = cat;
   db.attach(cat);
}
#endif

#endif STAND_ALONE

oqlContext::oqlContext(const char *name)

: db(name)
#ifdef USE_ALLOCATORS
, defines(&GlobalAllocator, "defines")
#endif
, _tmpTypes(true)
{
#ifndef STAND_ALONE
#ifdef NO_SDL
   _cat = 0;
#endif
#endif STAND_ALONE
   scope.reset(&universe);
   universe.reset(&db.types, &db.extents);
   scope.tscope.push(&universe);
   _open = 0;

#ifdef USE_ALLOCATORS

   // Set the current types allocator to be the Eternal types allocator
   setEternalTypes();
   //

   db.types.setAllocator(&_types_alloc);
   db.extents.setAllocator(&_types_alloc);
   defines.setAllocator(&_types_alloc);

#endif USE_ALLOCATORS
}

oqlContext::~oqlContext()
{
  // delete _cat;
}

oqlContext& oqlContext::reset(char* name)
{
   db.reset();
   scope.reset(&universe);
   universe.reset(&db.types, &db.extents);
   scope.tscope.push(&universe);
#ifdef USE_ALLOCATORS
   _default_alloc.rewind();
   _other_alloc.rewind();
   _types_alloc.rewind();
   _current_types_alloc = &_types_alloc;
   // The last is so that loading from the database proceeds properly...
#endif USE_ALLOCATORS
}

#ifdef USE_ALLOCATORS

Allocator* globalAllocator()
{
   if (!globalAllocatorPtr)
      globalAllocatorPtr = new Allocator;
   return globalAllocatorPtr;
}

Allocator* DefaultAllocator()
{
   if (oql_context())
      return oql_context()->DefaultAllocator();
   return globalAllocator();
}

Allocator* OtherAllocator()
{
   if (oql_context())
      return oql_context()->OtherAllocator();
   return globalAllocator();
}

Allocator* TypesAllocator()
{
   if (oql_context())
      return oql_context()->TypesAllocator();
   return globalAllocator();
}

Allocator* CurrentTypesAllocator()
{
   if (oql_context())
      return oql_context()->CurrentTypesAllocator();
   return globalAllocator();
}

#endif USE_ALLOCATORS

  // Massive kludges follow...
oqlContext* new_oqlContext()
{
   return new oqlContext();
}

void delete_oqlContext(oqlContext* o)
{
   delete o;
}
