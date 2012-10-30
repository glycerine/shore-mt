/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef __oql_error_h__
#define __oql_error_h__

/* DO NOT EDIT --- GENERATED from oql_err.dat by errors.pl */

enum { 
    oqlASSERT               = 0x800000,
    oqlMEMORY               = 0x800001,
    oqlCLOSE_DB             = 0x800002,
    oqlOPEN_DB              = 0x800003,
    oqlCREATE_DB            = 0x800004,
    oqlDROP_EXTENT          = 0x800005,
    oqlGET_EXTENT_NAMES     = 0x800006,
    oqlGET_EXTENT_REC       = 0x800007,
    oqlGET_SUPER_INFO       = 0x800008,
    oqlGET_ATTR_INFO        = 0x800009,
    oqlATTACH_NULL_CAT      = 0x80000a,
    oqlCREATE_EXTENT        = 0x80000b,
    oqlTYPE_EXISTS          = 0x80000c,
    oqlNO_SUCH_TYPE         = 0x80000d,
    oqlNO_SUCH_EXTENT       = 0x80000e,
    oqlNO_SUCH_MEMBER       = 0x80000f,
    oqlTYPE_IN_USE          = 0x800010,
    oqlSUPER_EXIST          = 0x800011,
    oqlMEMBER_EXIST         = 0x800012,
};

enum {
    oqlERRMIN = 0x800000,
    oqlERRMAX = 0x800012
};

#endif /*__oql_error_h__*/
