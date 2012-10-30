/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/svas_base.C,v 1.33 1997/06/13 21:35:11 solomon Exp $
 * 
 * implementation of base class for client & server shore vas.
 */

#include <copyright.h>

#ifdef __GNUG__
#pragma implementation "svas_base.h"
#endif

#include "vas_internal.h"

#include <iostream.h>
#include <strstream.h>
#include <e_error.h>
#include <e_error.i>
#include "../client/svas_layer.h"

extern ErrLog *shorelog;
ErrLog *shorelog=0;

extern int sys_nerr;
extern char *sys_errlist[];

extern "C" void perrstop() {};

svas_base::svas_base(ErrLog *el): 
	errlog(el),
	_suppress_p_user_errors(false), 
#ifdef DEBUG
	failure_line(0),
#endif
	// transid set below
	_flags(0),
	// _cwd(lrid_t::null),
	_page_size(0),
	_num_page_bufs(0),
	_num_lg_bufs(0)
{
	transid = tid_t::null;
	_cwd = lrid_t::null;
	_oc_ptr = 0;
}
// goes to log.
void
svas_base::perr(
	const char *message,	// not printed if NULL
	int		 	line,  		// = -1,   not printed if <0
	const char *filename,	// = NULL, not printed if null
	error_type	etype		// = ET_VAS, not printed if ==ET_USER
) const
{
	FUNC(svas_base::perr);
	if(etype == ET_USER) {
		if (_suppress_p_user_errors || !ShoreVasLayer.pusererrs()) return;
	}

	LogPriority prio = log_info;

	switch(status.vasresult) {
		case SVAS_WARNING:
			prio = log_warning;
			break;

		case SVAS_FAILURE:
			prio = log_error;
			break;

		case SVAS_OK:
		case SVAS_ABORTED:
			prio = log_info;
			break;
	}
	if(etype & ET_FATAL) {
		// override
		prio = log_fatal;
	}

	// verbose logging... create an ostrstream,
	// put all the fancy info there, then send
	// that stuff to stderr 

	ostrstream out;
	char *save;

	perr(out, message, line, filename, etype);
	save = out.str();
	if(shorelog) {
		shorelog->log(prio,save);
	} else {
		cerr << save << endl << ends;
	}
	delete save;

	if(etype & ET_FATAL) {
		perrstop();
		abort(); 
	}
}

// called by higher layer
// so leave this as it is:
void		
svas_base::perr(
	ostream 	&out, 
	const char *message,	// not printed if NULL
	int		 	line,  		// = -1,   not printed if <0
	const char *filename,	// = NULL, not printed if null
	error_type	etype		// = ET_VAS, not printed if ==ET_USER
) const
{
	FUNC(svas_base::perr);
	char *kind=0;

	int neednl = 0;
#define UE		"USER ERROR  "
#define EE 		"ERROR       "
#define WE 		"WARNING     "
#define FE 		"FATAL ERROR "
#define AB 		"TX ABORTED  "

	if(etype & ET_FATAL) {
		kind = "FATAL ERROR ";
	} else switch(status.vasresult)  {
		case SVAS_FAILURE:
			kind = (etype==ET_USER)? UE: EE;
			break;
		case SVAS_ABORTED:
			kind = AB;
			break;
		case SVAS_WARNING:
			kind = WE;
			break;
		case SVAS_OK:
			break;
	}

	if(message) {
		out << message;
		neednl++;
	}
	if((line >=0) && (filename!=NULL)) {
		out << " at line " << line << ", " << filename << ":" ;
		neednl++;
	}
	if(neednl) {
		out << endl;
	}

	switch(status.vasresult) {
	case SVAS_FAILURE:
	case SVAS_WARNING:
	case SVAS_ABORTED: 
		{
			unsigned int	 e_num;

			out << "-->" << kind << ": ";
			if(status.vasreason == SVAS_SmFailure) {
				dassert(status.smresult != 0);
				
				if((status.smresult == fcOS)
						&& (status.osreason != 0)
						&& (status.osreason < sys_nerr))  {
					e_num = status.osreason;
					/*
					out << " this Unix OS error occurred: " 
						<< sys_errlist[status.osreason];
					*/
				} else {
					e_num = status.smreason;
				}
			} else {
				e_num = status.vasreason;
			}
			const char *module = w_error_t::module_name(e_num);
			out <<  module << " : " ;
			if(strlen(module)>20) {
				out << endl ;
			}
			out << w_error_t::error_string(e_num);
		}
		break;

	case SVAS_OK:
		break;
	}

#ifdef DEBUG
	out << "\n\t\t" <<
		" (svas="
		<< dec << status.vasresult << "/0x" <<  hex << status.vasreason
		<< " sm="
		<< dec << status.smresult << "/" <<  dec << status.smreason
		<< " unix="
		<< dec << status.osreason <<")"
		<< endl<<ends; 
#else
	out << endl << ends;
#endif /* DEBUG */

}

void	
svas_base::set_error_info(
	int verr, 
	int res, 
	error_type ekind,
	const w_rc_t &smerrorrc,
	const char *msg,
	int	 line,
	const char *file
)
{
	FUNC(svas_base::set_error_info);
    if(smerrorrc) { 
		status.smresult = -1;
        status.smreason = (int)smerrorrc.err_num(); 
        status.osreason = (int)smerrorrc.sys_err_num(); 
#ifdef DEBUG
		if( ekind != ET_USER ||
			( ShoreVasLayer.pusererrs() && !_suppress_p_user_errors )
		) {
			cerr 
				<< "{ line " << __LINE__
				<< " file " << __FILE__
				<< ": for -DDEBUG only, ekind = "  << ekind  << ":\n"
				<< smerrorrc 
				<< "\n}"
				<< endl;
		}
#endif
		//
		// TRANSLATE CERTAIN ERRORS HERE:
		//
		bool translated=true;
		switch(status.smreason) {
		case eOUTOFSPACE:
		case eDEVICEVOLFULL:
			verr =  OS_NoSpaceLeft;
			break;

		default:
			translated = false;
			break;
		}
		if(translated) {
			status.osreason = status.smresult = status.smreason = 0;
#ifdef DEBUG
			cerr << "TRANSLATED TO " << verr << endl;
#endif
		}
    }/* else -- not an SM error */ else {
		status.osreason = status.smresult = status.smreason = 0;
	}
    status.vasreason = verr;
    status.vasresult = res;
    perr(msg, line, file, ekind);
	perrstop();
}

VASResult
svas_base::trans( OUT(tid_t)    tid)
{ 
	FUNC(svas_base::trans);
	if(tid) *tid = this->transid; 
	if(this->transid==null_tid) {
		return SVAS_FAILURE;
	} else {
		return SVAS_OK;
	}
}

#undef __VAS_STATUS_H__
#define	__COMMON__ERRORS_C__

#include <svas_error_def.h>
#include <svas_einfo.i>
#include <os_error_def.h>
#include <os_einfo.i>

#include "svas_einfo_bakw.i"
#include "os_einfo_bakw.i"

// converts error code (integer) to full error string
const char *
svas_base::err_msg(unsigned int x)
{
	FUNC(svas_base::err_msg);
	return w_error_t::error_string(x);
}


// converts error name to full error string
const char *
svas_base::err_msg(const char *str)
{
	FUNC(svas_base::err_msg);
	bool			found;
	unsigned int	code;

	return w_error_t::error_string(err_code(str));
}

// converts error name to code
// for OS* and SVAS* ONLY
unsigned int
svas_base::err_code(const char *x)
{
	FUNC(svas_base::err_code);
	bool			found = false;
	w_error_info_t	*v = svas_error_info_bakw;
	int				j = SVAS_ERRMIN;

	while( (v != 0) && j++ < SVAS_ERRMAX ) {
		if(strcmp(v->errstr,x)==0) {
		  	return v->err_num;
		}
		v++;
	}
	// try OS group
	v = os_error_info_bakw;
	j = OS_ERRMIN;

	while( (v != 0) && j++ < OS_ERRMAX ) {
		if(strcmp(v->errstr,x)==0) {
			return v->err_num;
		}
		v++;
	}
	return fcNOSUCHERROR;
}

// returns error name given error code
// return false if the error code is not in SVAS_* or OS_*
bool
svas_base::err_name(unsigned int x,	const char *&res)
{
	FUNC(svas_base::err_name);
	bool			found = false;
	w_error_info_t	*v = svas_error_info_bakw;
	int				j = SVAS_ERRMIN;

	while( (v != 0) && j++ < SVAS_ERRMAX ) {
		if(x == v->err_num) {
			res =  v->errstr;
			return true; // found
		}
		v++;
	}
	// try OS group
	v = os_error_info_bakw;
	j = OS_ERRMIN;

	while( (v != 0) && j++ < OS_ERRMAX ) {
		if(x == v->err_num) {
			res =  v->errstr;
			return true; // found
		}
		v++;
	}
	return false; // not found
}

// it's virtual but gcc seems to want
// us to define it, and it doesn't get
// exported from this implementation .o file
// if it's defined in the class defn.
// (Gcc isn't so smart about inline funcs and
// #pragma implementation I guess)

svas_base::~svas_base() {}

VASResult
svas_base::option_value(
	const char *name, 
	const char**val
) 
{
	FUNC(svas_base::option_value);
	w_rc_t e = svas_layer::option_value(name, val);
	if(e) {
		this->set_error_info(e.err_num(),
			SVAS_FAILURE, ET_USER, RCOK, _fname_debug_, __LINE__,
			__FILE__);
		return  SVAS_FAILURE;
	}
	return SVAS_OK;
}
VASResult 	
svas_base::version_match(int x) 
{
	FUNC(svas_base::version_match);
	if(_version != x) {
		this->set_error_info(SVAS_ConfigMismatch,
			SVAS_FAILURE, ET_USER, RCOK, _fname_debug_, __LINE__,
			__FILE__);
		return  SVAS_FAILURE;
	}
	return SVAS_OK;
}

VASResult		
svas_base::access(
	const Path 			name,	// Shore path --
	AccessMode			mode,
	OUT(bool)			haveaccess
)
{
	FUNC(svas_base::access);
	bool found;
	lrid_t target;
	VASResult res;


	if(name==0) {
		// VERR(SVAS_BadParam1);
		set_error_info(SVAS_BadParam1,
			SVAS_FAILURE,svas_base::ET_USER, RCOK,
			_fname_debug_, __LINE__, __FILE__);
		return SVAS_FAILURE;
	}

	if(haveaccess==0) {
		// VERR(SVAS_BadParam3);
		set_error_info(SVAS_BadParam3,
			SVAS_FAILURE,svas_base::ET_USER, RCOK,
			_fname_debug_, __LINE__, __FILE__);
		return SVAS_FAILURE;
	}

	res = this->lookup(name, &target, &found, 
			((Permissions::PermOps) (mode & (F_OK|R_OK|X_OK|W_OK))),
			true);

	if(res == SVAS_OK) {
		if(!found) { 
			set_error_info(SVAS_NotFound,
				SVAS_FAILURE,svas_base::ET_USER, RCOK,
				_fname_debug_, __LINE__, __FILE__);
			res = SVAS_FAILURE;
		} else {
			*haveaccess = true;
			// res is still ok
		}
	} else {
		// requst failed-- determine why
		if(status.vasreason == OS_PermissionDenied) {
			*haveaccess = false;
			clr_error_info();
			res = SVAS_OK;
		}
	}
	return res;
}

// UPDATE any time something
// might make a client incompatible with an old server, or vice versa
const	int svas_base::_version = 5;   // this will be
	// sent on an init, and also added to RPC's CLIENT_PROGRAM

// static functions that are the same for both sides
void
// option_t *& is ref to ptr
svas_layer_init::init_Boolean(bool &var, option_t *& opt, bool defaultval) 
{
	bool	bad;
	var = defaultval;
	if(opt) {
		var = option_t::str_to_bool(opt->value(),bad);
		if(bad) var = defaultval;
	} 
}

// option_group_t *svas_layer_init::option_group()  { return options; }
// is defined in each subordinate svas_layer.C

w_rc_t 
svas_layer::option_value(const char *name, const char**val)
{
	if(!val) {
		return RC(SVAS_BadParam2);
	}
	if(ShoreVasLayer.option_group()) {
		w_rc_t 		rc;
		option_t	*opt;
		char		temp[strlen(name) + 10];

		temp[0]='\0';
		if(strchr(name,'.')) {
			strcat(temp, "*.");
		}
		strcat(temp, name);

		DBG(<<"lookup " << name);
		if(rc =  ShoreVasLayer.option_group()->lookup(name, true, opt))  {
			DBG(<<"lookup " << temp);
			if(rc =  ShoreVasLayer.option_group()->lookup(temp, true, opt)) return rc;
		}
		if(!opt) {
			return RC(SVAS_NotFound);
		}
		*val = opt->value();
		return RCOK;
	} else {
		return RC(SVAS_NotInitialized);
	}
}


w_statistics_t &operator<<(w_statistics_t&w, svas_base &s) 
{
	s.pstats(w); return w;
}

w_rc_t 
svas_base::setup_options(option_group_t *t)
{
	return svas_layer::setup_options(t);
}

int 
svas_base::unix_error()
{
	int e = this->status.vasreason;
	if(e== SVAS_SmFailure) {
		e = status.smresult;
		if(e==fcOS) {
			e = status.osreason;
			if(e != 0 && e < sys_nerr) {
				return e;
			} else return fcOS; // oh, well
		}
	} 
	int converted =  unix_error(e);
	if(converted == NoSuchError) {
		errlog->log(log_error, 
		"error code 0x%x/%d does not translate to UNIX error",
			e,e);
	}
	return converted;
}
	
int 
svas_base::unix_error(w_rc_t rc)
{
	if(rc) {
		w_rc_i	iter(rc);
		w_base_t::int4_t e;

		e = iter.next_errnum();
		if(e== SVAS_SmFailure) {
			e = iter.next_errnum();
			if(e==fcOS) {
				e = iter.next_errnum();
			}
		} 
		return unix_error((int)e);
	} else return 0;
}

int 
svas_base::unix_error(int reason)
{
	/////////////////////////////////////////////
	// here's where the real conversions are done
	/////////////////////////////////////////////
	if(reason >= OS_ERRMIN && reason <= OS_ERRMAX) {
		return (int)(reason-OS_ERRMIN);
	} else switch (reason) {
		case SVAS_OK:
			return 0;

		case SVAS_ABORTED:
		case SVAS_FAILURE:
		case SVAS_SmFailure:
		case fcOS:
			assert(0); // should not happen
			break;

		case eBADPID:
		case eBADSTID:
		case eBADSTART:
		case eBADLENGTH:
		case eBADAPPEND:
			DBG(<<"ESTALE");
			return ESTALE; // stale nfs handle

		case fcNOTFOUND:
		case eBADLOGICALID:
			DBG(<<"ENOENT");
			return ENOENT; 

		case eBADVOL:
			DBG(<<"ENXIO");
			return ENXIO; // no such device or addr

		case eOUTOFSPACE:
		case eDEVICEVOLFULL:
			DBG(<<"ENOSPC");
			return ENOSPC;

		case fcOUTOFMEMORY:
			DBG(<<"ENOMEM");
			return ENOMEM;

		case eRECWONTFIT:
		case eRECUPDATESIZE:
		case eVOLTOOLARGE:
		case eNVOL:
			DBG(<<"EFBIG");
			return EFBIG;

		case eLOCKTIMEOUT:
			DBG(<<"ETIMEDOUT");
			return ETIMEDOUT; 	

		case SVAS_NotFound:
			return ENOENT;

		case SVAS_BadParam1:
		case SVAS_BadParam2:
		case SVAS_BadParam3:
		case SVAS_BadParam4:
		case SVAS_BadParam5:
		case SVAS_BadParam6:
		case SVAS_BadParam7:
		case SVAS_BadParam8:
		case SVAS_BadParam9:
			return EINVAL;

		case SVAS_MallocFailure:
			return ENOMEM;

		case SVAS_BadRange:
			return ENXIO; // bad address

		case SVAS_WrongObjectKind:
			return EACCES; //permission denied
				// can get this when trying to do something
				// to an anonymous object through an xref
			
		default:
			return NoSuchError;
	}
	// to keep compiler quiet
	return 0;
}

