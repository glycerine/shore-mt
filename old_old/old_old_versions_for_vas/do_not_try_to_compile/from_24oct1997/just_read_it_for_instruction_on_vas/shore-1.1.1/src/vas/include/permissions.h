/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __PERMISSIONS_H__
#define __PERMISSIONS_H__
class Permissions {
public:
	enum {
		SetUid	=04000, 
		SetGid	=02000,
		Sticky	=01000, /* save text image after execution */
		Rown	=00400,
		Wown	=00200,
		Xown	=00100, 
		Rgrp	=00040,
		Wgrp	=00020,
		Xgrp	=00010,
		Rpub	=00004,
		Wpub	=00002,
		Xpub	=00001
	};// unnamed - shorter than writing const for each one. 
	enum PermOps { op_none = 0, op_exec=X_OK, op_write=W_OK, op_read=R_OK,
		op_readwrite = (W_OK|R_OK), 
		op_search = op_exec,
		op_owner = 0x80, 
		};
};
#endif 
