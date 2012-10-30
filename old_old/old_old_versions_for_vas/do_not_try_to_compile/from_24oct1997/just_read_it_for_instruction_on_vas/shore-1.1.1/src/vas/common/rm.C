/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <stream.h>
#include <lid_t.h>
#include <reserved_oids.h>
#include <vas.h>
#include <string_t.h>
#include "debug.h"

#define VERR(e)\
		status.vasresult = SVAS_FAILURE;\
		status.vasreason = e;\
		perr(cerr, _fname_debug_, __LINE__, __FILE__);

static
rmde( Tcl_Interp* ip, int amt, lsflags flags)
{
	FUNC(gde);
    VASResult   res;
	Cookie 		cookie;
	char 		*Dirent = NULL, *dirent;
	int			nentries=0, nbytes=1000;
	SysProps	sysprops;
	lrid_t		temp;

	// uses the dir that is "."
	// there might not be a legit lrid_t for "."

	CHECKCONNECTED;

/*NEW*/Dirent = dirent = new char[amt]; 
	if(!Dirent) {
		Tcl_AppendResult(ip, "cannot calloc a buffer that big.",0);
		return TCL_ERROR;
	}

	DBG(
		<< "current working directory is: " << Vas->cwd()
	)
	if(Vas->cwd() == ReservedOid::_nil) {
		Tcl_AppendResult(ip, "\".\": No such directory.", 0);
		return TCL_ERROR;
	}

	res = SVAS_OK;
	cookie = (Cookie)0;
	do {
		dirent = Dirent;
		// re-user dirent space
		res = Vas->getDirEntries(Vas->cwd(), 
			dirent,  nbytes, &nentries, &cookie);

	DBG(
		<< "Vas->getDirEntries returned cookie" << hex((u_long)cookie) << ", Res" << res
	);
		if(res== SVAS_OK) {
			int 	i;
			char 	*p = dirent;
			int 	pathlen;

	DBG(
		<< "Vas->getDirEntries returned nentries" << nentries << 
		", nbytes" << nbytes
	);
			if(nentries == 0) {
				cout << "<end of directory>." << endl;
				break;
			}
			cout << endl;

			for(p = dirent, i=0; i< nentries; i++, dirent = p) {
				pathlen = strlen(p) + 1;
				// TODO: what about byte swapping?
				bcopy((p+pathlen), (char *)&temp, sizeof(temp));

#ifdef DEBUG
	if(_debug.flag_on(_fname_debug_,__FILE__)) {
				_debug.memdump(p, pathlen + sizeof(temp) + 2);
	}
#endif
				DBG(
					<< "-->LS: " << p << " : " << temp 
				) 

				// By stat-ing a path, we follow xrefs and symlinks.
				// What we really need to do is stat the object itself

				if(Vas->sysprops(temp, &sysprops) != SVAS_OK) break;

				p += pathlen;
				p += sizeof(temp);

				// TODO: ask here
				lsitem(dirent, temp, sysprops, flags);
				// TODO: if match, and response==yes, 
				// remove it with the proper remove func
			}
		}  
	} while((res == SVAS_OK)  && (cookie != (Cookie)0));

/*DEL*/ 
	delete [] Dirent;
	REPLY(res);
}
