/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifdef SOLARIS2
#undef clnt_control
#define clnt_control(cl, rq, in) \
	((*(cl)->cl_ops->cl_control)(cl, rq, (char *)in))
#undef CLNT_CONTROL
#define CLNT_CONTROL(cl, rq, in) \
	 ((*(cl)->cl_ops->cl_control)(cl, rq, (char *)in))

#undef clnt_freeres
#define clnt_freeres(rh, xres, resp) \
			((*(rh)->cl_ops->cl_freeres)(rh, xres, (char *)resp))
#undef CLNT_FREERES
#define CLNT_FREERES(rh, xres, resp) \
		((*(rh)->cl_ops->cl_freeres)(rh, xres, (char *)resp))

#endif



