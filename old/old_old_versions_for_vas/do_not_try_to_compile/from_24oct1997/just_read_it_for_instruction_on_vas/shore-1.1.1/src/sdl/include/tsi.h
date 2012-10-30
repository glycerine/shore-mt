#ifndef TSI_H
#define TSI_H
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

   //  in odl/

enum	SdlZones {Sdl_public, Sdl_private, Sdl_protected};
enum	SdlModes {Sdl_in, Sdl_out, Sdl_inout};
enum 	SdlBool {Sdl_true, Sdl_false};
typedef SdlZones Sdl_AccessSpec;

enum  TypeType {ODL_Type, ODL_Container, ODL_Primitive, ODL_Method, 
		ODL_Struct, ODL_Tuple, ODL_Object, ODL_Interface, 
		ODL_Collection, ODL_IndexedCollection,
		ODL_Bag, ODL_Set, ODL_List, ODL_Array,
		ODL_Any, ODL_Error,
		ODL_Void, ODL_Ref,
		ODL_Typedef,
		ODL_Module};

typedef TypeType ConstructorType;

class oqlDatabase;
class oqlContext;
class Type;

const int ODL_private   = 0x0000; // 0b00000000
const int ODL_protected = 0x0001; // 0b00000001
const int ODL_public    = 0x0002; // 0b00000010
const int ODL_access    = 0x0003; // 0b00000011

const int ODL_in        = 0x0000; // 0b00000000
const int ODL_inout     = 0x0004; // 0b00000100
const int ODL_out       = 0x0008; // 0b00001000
const int ODL_param_mode= 0x000B; // 0b00001100

const int ODL_readonly  = 0x0000; // 0b00000000;
const int ODL_const     = 0x0000; // 0b00000000;
const int ODL_readwrite = 0x0010; // 0b00010000;

const int ODL_persistent= 0x0020; // 0b00100000;
const int ODL_transient = 0x0000; // 0b00000000;

const int ODL_indexable = 0x0040; // 0b01000000;
const int ODL_noindex   = 0x0000; // 0b00000000;
const int ODL_unindexable= 0x0000; // 0b00000000;

const int ODL_oneway    = 0x0080; // 0b01000000;
const int ODL_notoneway = 0x0000; // 0b00000000;

const int ODL_direct    = 0x0100; // 0b100000000;

const int ODL_forward   = 0x0200; // 0b1000000000;
const int ODL_notforward= 0x0000; // 0b0000000000;

const int ODL_fixedLen  = 0x0000; // 0b10000000000;
const int ODL_varLen    = 0x0400; // 0b00000000000;

typedef int odlFlags;

typedef unsigned long TypeId;

#endif TSI_H
