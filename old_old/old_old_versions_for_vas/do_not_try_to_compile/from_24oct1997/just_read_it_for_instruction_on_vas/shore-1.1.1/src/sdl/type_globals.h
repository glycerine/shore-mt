/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

// Ref to current module
extern Ref<sdlModule> CurModule;
// Ref to pool contained in current module
extern Ref<Pool> CurTypes;
// new macro used for allocating type objects
#define NEW_T new(CurTypes)

// note: it seems there should be size in here someplace...
extern Ref<sdlType> ShortIntegerTypeRef;
extern Ref<sdlType> LongIntegerTypeRef;
extern Ref<sdlType> CharacterTypeRef;
extern Ref<sdlType> BooleanTypeRef;
extern Ref<sdlType> UnsignedLongIntegerTypeRef;
extern Ref<sdlType> UnsignedShortIntegerTypeRef;
extern Ref<sdlType> UnsignedCharacterTypeRef;
extern Ref<sdlType> FloatingPointTypeRef;
extern Ref<sdlType> DoublePrecisionTypeRef;
extern Ref<sdlType> AnyTypeRef;
extern Ref<sdlType> VoidTypeRef;
extern Ref<sdlType> StringLiteralTypeRef;
extern Ref<sdlType> PoolTypeRef;

// defined in tresolve.C, used in 2 files.
extern Ref<sdlModule> lookup_module(const char *mname);
// defined in get_type_oid.C
extern shrc get_type_oid(const char *mpath, char  *iname,
								LOID		&result);
