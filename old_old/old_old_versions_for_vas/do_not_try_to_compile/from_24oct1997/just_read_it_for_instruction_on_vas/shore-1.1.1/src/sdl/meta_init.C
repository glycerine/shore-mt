/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "ShoreApp.h"
#include "sdl_internal.h"
#include "sdl_UnixFile.h"
// initialization module for type metaobjects.
// metaobj_init creates the objects initial
// metaobj_bind binds to existing metaobjects.
#define DEF_VASHOST   "localhost"
#define VASHOST               "VASHOST"
#define TYPE_DIR "/types" 
#define BUILTIN_SUBDIR "builtin"
char *dirname = TYPE_DIR;
char *builtin_dir = "primitive_types";
char *metatype_dir = "metatype_objs";
char *vashost = DEF_VASHOST;
extern char * metatype_version;
// pretty bogus

#include <debug.h>

/// this is a dummy; thees need to be really defined somewhere
REF(Type) ShortIntegerTypeRef;
REF(Type) LongIntegerTypeRef;
REF(Type) CharacterTypeRef;
REF(Type) BooleanTypeRef;
REF(Type) UnsignedLongIntegerTypeRef;
REF(Type) UnsignedShortIntegerTypeRef;
REF(Type) UnsignedCharacterTypeRef;
REF(Type) FloatingPointTypeRef;
REF(Type) DoublePrecisionTypeRef;
REF(Type) AnyTypeRef;
REF(Type) VoidTypeRef;
REF(Type) PoolTypeRef;
// REF(Type) StringTypeRef;
REF(sdlSequenceType) StringTypeRef;
REF(SdlUnixFile) metaVersionRef;

// for now, always allocate out of a fixed pool.
REF(Pool) CurTypes;

// a hack initialization for metatype objects: run this off
// of a dummy ctor. 

void meta_bind(bool createversion)
// look up existing type names, versa creating new ones.
{
	FUNC(meta_bind);
	{
		shrc rc;
		rc = REF(SdlUnixFile)::lookup("metatypeVersion", metaVersionRef);
		if(rc  && rc.err_num() != SH_NotFound){
			rc.fatal();
		}
		if(rc.err_num() == SH_NotFound){
			if(createversion) {
				DBG(<<"creating metatypeVersion");
				rc = REF(SdlUnixFile)::new_persistent("metatypeVersion", 0644, metaVersionRef);
				if(rc ) {
					cerr << "Cannot create version object, metatypeVersion" << endl; 
					rc.fatal();
				}
			} else {
				cerr<< "Metatypes database was created by an unidentifiable version of sdl." << endl;
				cerr << "Current version is "
					<< metatype_version 
					<< endl;
				cerr << "You must remove /types and start over." << endl;
				exit(1);
			}
		}else if (metaVersionRef->UnixText.strcmp(metatype_version) != 0) {
			cerr<< "Metatypes database was created under version " 
				<< (char *)metaVersionRef->UnixText
				<< ";" << endl;
			cerr<< "Current version is "
				<< metatype_version 
				<< endl;

			cerr << "You must remove /types and start over." << endl;
			exit(1);
		}
		DBG(<< "metaVersion =" << (char *)metaVersionRef->UnixText);
	}

	{
		shrc rc;

		DBG(<<"creating new primitive types");

#define _MAKETYPE_(T,x) \
		if( x##Ref== 0) { \
			DBG(<<"creating " << #x); \
			rc = REF(T)::new_persistent(#x, 0644, x##Ref );\
			if(rc  && rc.err_num() != EEXIST){\
				cerr << "Cannot create " << #x << endl; \
				rc.fatal();\
			} else if(x##Ref== 0) { \
				cerr << "ERR: Created object but ref is not valid!" << endl; \
				cerr << rc << endl; \
			}\
		} dassert(x##Ref != 0);
#define MAKETYPE(x)  _MAKETYPE_(Type,x)

		// ShortIntegerTypeRef = new("ShortIntegerType",0644) Type;
		// LongIntegerTypeRef = new("LongIntegerType",0644) Type;
		// CharacterTypeRef = new("CharacterType",0644) Type;
		// BooleanTypeRef = new("BooleanType",0644) Type;
		// UnsignedLongIntegerTypeRef = new("UnsignedLongType",0644) Type;
		// UnsignedShortIntegerTypeRef = new("UnsignedShortType",0644) Type;
		// UnsignedCharacterTypeRef = new("UnsignedCharacterType",0644) Type;
		// FloatingPointTypeRef = new("FloatingPointType",0644) Type;
		// DoublePrecisionTypeRef = new("DoublePrecisionType",0644) Type;
		// AnyTypeRef = new("AnyType",0644) Type;
		// VoidTypeRef = new("VoidType",0644) Type;

		W_IGNORE(REF(Type)::lookup("ShortIntegerType", ShortIntegerTypeRef));
		W_IGNORE(REF(Type)::lookup("LongIntegerType", LongIntegerTypeRef));
		W_IGNORE(REF(Type)::lookup("CharacterType", CharacterTypeRef));
		W_IGNORE(REF(Type)::lookup("BooleanType", BooleanTypeRef));
		W_IGNORE(REF(Type)::lookup("UnsignedLongIntegerType", UnsignedLongIntegerTypeRef));
		W_IGNORE(REF(Type)::lookup("UnsignedShortIntegerType", UnsignedShortIntegerTypeRef));
		W_IGNORE(REF(Type)::lookup("UnsignedCharacterType", UnsignedCharacterTypeRef));
		W_IGNORE(REF(Type)::lookup("FloatingPointType", FloatingPointTypeRef));
		W_IGNORE(REF(Type)::lookup("DoublePrecisionType", DoublePrecisionTypeRef));
		W_IGNORE(REF(Type)::lookup("AnyType", AnyTypeRef));
		W_IGNORE(REF(Type)::lookup("VoidType", VoidTypeRef));
		W_IGNORE(REF(Type)::lookup("PoolType", PoolTypeRef));
		W_IGNORE(REF(sdlSequenceType)::lookup("StringType", StringTypeRef));

		MAKETYPE(ShortIntegerType);
		MAKETYPE(LongIntegerType);
		MAKETYPE(CharacterType);
		MAKETYPE(BooleanType);
		MAKETYPE(UnsignedLongIntegerType);
		MAKETYPE(UnsignedShortIntegerType);
		MAKETYPE(UnsignedCharacterType);
		MAKETYPE(FloatingPointType);
		MAKETYPE(DoublePrecisionType);
		MAKETYPE(AnyType);
		MAKETYPE(VoidType);
		MAKETYPE(PoolType);
		_MAKETYPE_(sdlSequenceType,StringType);

		DBG(<<"updating types...");

		ShortIntegerTypeRef.update()->tag = Sdl_short;
		LongIntegerTypeRef.update()->tag = Sdl_long;
		CharacterTypeRef.update()->tag = Sdl_char;
		BooleanTypeRef.update()->tag = Sdl_boolean;
		UnsignedLongIntegerTypeRef.update()->tag = Sdl_unsigned_long;
		UnsignedShortIntegerTypeRef.update()->tag = Sdl_unsigned_short;
		UnsignedCharacterTypeRef.update()->tag = Sdl_octet;
		FloatingPointTypeRef.update()->tag = Sdl_float;
		DoublePrecisionTypeRef.update()->tag = Sdl_double;
		AnyTypeRef.update()->tag = Sdl_any;
		VoidTypeRef.update()->tag = Sdl_void;
		PoolTypeRef.update()->tag = Sdl_pool;
		StringTypeRef.update()->tag = Sdl_string;

		DBG(<<"setting version to " << metatype_version);
		metaVersionRef.update()->UnixText = metatype_version;

	}

}
void meta_destroy()
// unlink existing type objects 
{
	FUNC(meta_destroy);
	DBG(<<"destroying primitive types");

    W_IGNORE(Shore::unlink("ShortIntegerType"));
    W_IGNORE(Shore::unlink("LongIntegerType"));
    W_IGNORE(Shore::unlink("CharacterType"));
    W_IGNORE(Shore::unlink("BooleanType"));
    W_IGNORE(Shore::unlink("UnsignedLongType"));
    W_IGNORE(Shore::unlink("UnsignedShortType"));
    W_IGNORE(Shore::unlink("UnsignedCharacterType"));
    W_IGNORE(Shore::unlink("FloatingPointType"));
    W_IGNORE(Shore::unlink("DoublePrecisionType"));
    W_IGNORE(Shore::unlink("AnyType"));
    W_IGNORE(Shore::unlink("VoidType"));
    W_IGNORE(Shore::unlink("PoolType"));
    W_IGNORE(Shore::unlink("StringType"));
	// Don't destroy this!
    // W_IGNORE(Shore::unlink("metatypeVersion"));
}
void
metaobj_init(int argc, char *argv[])
{
	FUNC(metaobj_init);
	bool	starting_fresh = false;
	shrc rc;
	option_group_t *options;
	int bound = 0;
	rc = Shore::process_options(argc, argv,
		"client",
		0,  // argv[0]
		Shore::default_config_file,
	   "bad sdl options", 
	   0, &options); // default last argument means
	   // read the command line and process -h, -v

	if(rc){
		cerr << "Cannot process options:" << rc << endl;
		exit (1);
	}

	W_COERCE(Shore::init());
	W_COERCE(Shore::begin_transaction(3));
	rc = Shore::chdir(dirname);
	if (rc)
	{
		if (rc.err_num() != SH_NotFound)
			rc.fatal(); //give up
		SH_DO(Shore::mkdir(dirname,0755));
		SH_DO(Shore::chdir(dirname));
		starting_fresh = true;
	}
	rc = Shore::chdir(builtin_dir);

	if (rc)
	{
		if (rc.err_num() != SH_NotFound)
			rc.fatal(); //give up
		SH_DO(Shore::mkdir(builtin_dir,0755));
		SH_DO(Shore::chdir(builtin_dir));
	}

	rc = REF(Pool)::lookup("TypePool", CurTypes);
	if(rc  && rc.err_num() != SH_NotFound){
	    rc.fatal();
	}

	if (CurTypes == 0) {
		W_COERCE(REF(Pool)::create("TypePool",0644, CurTypes));
	}
	if (!bound)  {
		meta_bind(starting_fresh); bound = 1;
	}
	// change back to original types dir.
	SH_DO(Shore::chdir(dirname));

	W_COERCE(Shore::commit_transaction());
}
