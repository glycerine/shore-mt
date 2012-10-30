#ifndef __oql_error_i__
#define __oql_error_i__

/* DO NOT EDIT --- GENERATED from oql_err.dat by errors.pl */

static char* oql_errmsg[] = {
/* oqlASSERT             */ "Assertion failed",
/* oqlMEMORY             */ "Out of memory",
/* oqlCLOSE_DB           */ "Close DB failed",
/* oqlOPEN_DB            */ "Open DB failed",
/* oqlCREATE_DB          */ "Create DB failed",
/* oqlDROP_EXTENT        */ "Drop extent failed",
/* oqlGET_EXTENT_NAMES   */ "Couldn't obtain extent names",
/* oqlGET_EXTENT_REC     */ "Couldn't obtain the information of the extent",
/* oqlGET_SUPER_INFO     */ "Couldn't obtain infomation of super class",
/* oqlGET_ATTR_INFO      */ "Couldn't obtain attribute information",
/* oqlATTACH_NULL_CAT    */ "Trying to attach to null catalog",
/* oqlCREATE_EXTENT      */ "Create extent failed",
/* oqlTYPE_EXISTS        */ "Type or extent already exists",
/* oqlNO_SUCH_TYPE       */ "No such type",
/* oqlNO_SUCH_EXTENT     */ "No such extent",
/* oqlNO_SUCH_MEMBER     */ "No such member in the given type.",
/* oqlTYPE_IN_USE        */ "Type is still being referenced and/or have subtypes.",
/* oqlSUPER_EXIST        */ "Type is already subtype of the given super type.",
/* oqlMEMBER_EXIST       */ "Member named already exist.",
	"dummy error code"
};
const oql_msg_size = 18;

#endif /*__oql_error_i__*/
