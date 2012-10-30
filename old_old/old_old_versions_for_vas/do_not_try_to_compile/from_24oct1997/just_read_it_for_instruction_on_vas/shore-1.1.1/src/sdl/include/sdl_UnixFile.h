/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifdef __GNUG__
#pragma interface
#endif

#include "ShoreApp.h"
#ifndef unix_support
#define unix_support 1
INTERFACE_PREDEFS(SdlUnixFile);
class SdlUnixFile {
COMMON_FCT_DECLS(SdlUnixFile)
public:
sdl_text UnixText ;
};
//const int SdlUnixFile_OID = ReservedSerial::_UnixFile;
#define SdlUnixFile_OID ReservedSerial::_UnixFile.guts._low
INTERFACE_POSTDEFS(SdlUnixFile)

#ifdef MODULE_CODE
#define CUR_MOD dummy_m
rModule dummy_m  ( "metatypes",0,0 ,0);
INTERFACE_CODEDEFS(SdlUnixFile,0,any)
TYPE_CAST_DEF(SdlUnixFile)
TYPE_CAST_END(SdlUnixFile)
APPLY_DEF(SdlUnixFile)
UnixText.__apply(op);
END_APPLY_DEF(SdlUnixFile)

#undef CUR_MOD
#endif MODULE_CODE
#endif unix_support
