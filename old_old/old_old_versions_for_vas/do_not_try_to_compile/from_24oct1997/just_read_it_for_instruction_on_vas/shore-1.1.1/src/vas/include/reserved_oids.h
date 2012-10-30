/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef _RESERVED_OIDS_H__
#define _RESERVED_OIDS_H__

/*
// if you haven't already, you might have to
// #include <lid_t.h>

//
// Each volume has a set of reserved serial #s to
// represent the reserved types.
//
// Logical oids also have the same set of reserved ids
// so that we can represent these reserved oids in both
// serial form and in full logical oid form.
// 
*/

class ReservedOid: public lrid_t {
public:

#ifndef OBJECT_CACHE
	ReservedOid(uint4 val) : 
			// serial_t(val, false) creates non-remote serial#
		lrid_t(lvid_t::null, serial_t(val, false)) {};
#endif

	static const ReservedOid 		_nil;
#ifdef notdef
	// Might delete these...
	static const ReservedOid		_char;
	static const ReservedOid		_u_char;
	static const ReservedOid		_short;
	static const ReservedOid		_u_short;
	static const ReservedOid		_int;
	static const ReservedOid		_u_int;
#endif
	// for VAS:
	static const ReservedOid		MaxReserved;
	static const ReservedOid		MaxReservedFS;
	static const ReservedOid		MaxReservedSDL;
	static const ReservedOid		_RootDir;
	static const ReservedOid		_Directory;
	static const ReservedOid		_Xref;
	static const ReservedOid		_Symlink;
	static const ReservedOid		_Pool;
	static const ReservedOid		_UnixFile;

	// THIS WILL GO AWAY -- it's there only for shore_vas testing
	static const ReservedOid		_UserDefined;

	// for the type system:
	static const ReservedOid 	_sdlExprNode;
	static const ReservedOid 	_sdlDeclaration;
	static const ReservedOid 	_sdlTypeDecl;
	static const ReservedOid 	_sdlConstDecl;
	static const ReservedOid 	_sdlArmDecl;
	static const ReservedOid 	_sdlAttrDecl;
	static const ReservedOid 	_sdlRelDecl;
	static const ReservedOid 	_sdlParamDecl;
	static const ReservedOid 	_sdlModDecl;
	static const ReservedOid 	_sdlType;
	static const ReservedOid 	_sdlNamedType;
	static const ReservedOid 	_sdlStructType;
	static const ReservedOid 	_sdlEnumType;
	static const ReservedOid 	_sdlArrayType;
	static const ReservedOid 	_sdlSequenceType;
	static const ReservedOid 	_sdlCollectionType;
	static const ReservedOid 	_sdlRefType;
	static const ReservedOid 	_sdlIndexType;
	static const ReservedOid 	_sdlOpDecl;
	static const ReservedOid 	_sdlInterfaceType;
	static const ReservedOid 	_sdlModule;
	static const ReservedOid 	_sdlUnionType;
	static const ReservedOid 	_sdlClassType;
	static const ReservedOid 	_sdlExtTypeDecl;
	static const ReservedOid 	_sdlEType;
	static const ReservedOid 	_sdlAliasDecl;
	static const ReservedOid 	_sdlNameScope;
};

class ReservedSerial: public serial_t {
public:

#ifndef OBJECT_CACHE
	ReservedSerial(uint4 val) : serial_t(val, 0/* not remote*/) {}
		// not remote just because it's easier to read the value
		// during debugging this way.
#endif

	//  is any reserved type
	static bool is_reserved(const serial_t &s) {
		return (!s.is_remote() && (s <= MaxReserved));
		// TODO: handle remote oids as well, but have to
		// do something about the comparison, since we
		// cannot compare a remote serial with a local serial
	}

	static bool is_reserved_sdl(const serial_t &s) {
		return (!s.is_remote() && 
			(s > MaxReservedFS) &&
			(s <= MaxReservedSDL));
		// TODO: handle remote oids as well, but have to
		// do something about the comparison, since we
		// cannot compare a remote serial with a local serial
	}

	//  is reserved file system type
	static bool is_reserved_fs(const serial_t &s) {
		return (!s.is_remote() && (s <= MaxReservedFS));
	}

	// returns true if type is one that user cannot
	// instantiate with mkRegistered or mkAnonymous
	static bool is_protected(const serial_t &s) {
		if( is_reserved_fs(s) 
			&& s != ReservedSerial::_UnixFile) 
			return true;
		return false;
	}

	static const ReservedSerial 	_nil;
#ifdef notdef
	static const ReservedSerial		_char;
	static const ReservedSerial		_u_char;
	static const ReservedSerial		_short;
	static const ReservedSerial		_u_short;
	static const ReservedSerial		_int;
	static const ReservedSerial		_u_int;
#endif
	//
	static const ReservedSerial		MaxReservedFS;
	static const ReservedSerial		MaxReservedSDL;
	static const ReservedSerial		MaxReserved;
	static const ReservedSerial		_RootDir;
	static const ReservedSerial		_Directory;
	static const ReservedSerial		_Xref;
	static const ReservedSerial		_Symlink;
	static const ReservedSerial		_Pool;
	static const ReservedSerial		_UnixFile;

	// THIS WILL GO AWAY -- it's there only for shore_vas testing
	static const ReservedSerial		_UserDefined;
	//
//#ifdef notdef
	static const ReservedSerial 	_sdlExprNode;
	static const ReservedSerial 	_sdlDeclaration;
	static const ReservedSerial 	_sdlTypeDecl;
	static const ReservedSerial 	_sdlConstDecl;
	static const ReservedSerial 	_sdlArmDecl;
	static const ReservedSerial 	_sdlAttrDecl;
	static const ReservedSerial 	_sdlRelDecl;
	static const ReservedSerial 	_sdlParamDecl;
	static const ReservedSerial 	_sdlModDecl;
	static const ReservedSerial 	_sdlType;
	static const ReservedSerial 	_sdlNamedType;
	static const ReservedSerial 	_sdlStructType;
	static const ReservedSerial 	_sdlEnumType;
	static const ReservedSerial 	_sdlArrayType;
	static const ReservedSerial 	_sdlSequenceType;
	static const ReservedSerial 	_sdlCollectionType;
	static const ReservedSerial 	_sdlRefType;
	static const ReservedSerial 	_sdlIndexType;
	static const ReservedSerial 	_sdlOpDecl;
	static const ReservedSerial 	_sdlInterfaceType;
	static const ReservedSerial 	_sdlModule;
	static const ReservedSerial 	_sdlUnionType;
	static const ReservedSerial 	_sdlClassType;
	static const ReservedSerial 	_sdlExtTypeDecl;
	static const ReservedSerial 	_sdlEType;
	static const ReservedSerial 	_sdlAliasDecl;
	static const ReservedSerial		_sdlNameScope;

//#endif /*notdef*/
};
#endif
