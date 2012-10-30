
/*
COMMENTS:

	rpcgen does not preserve case, and since we have to post-process
		its output to make it C++, we *MUST* use only lower-case names
		here.

	Also because of postprocessing (for C++), we *MUST* use the convention
		that a function x's argument is "x_arg", and its reply is, for some
		type t, "t_reply".

*/

struct path2loid_arg {
	Path 	path;
};

struct openfile_arg {
	lrid_t	file;
};

typedef void_arg vzero_arg;

struct shutdown1_arg {
	small_bool_t	 crash;
	small_bool_t	 wait;
	u_short	 dummy;
};

#ifdef RPC_HDR 
%BEGIN_EXTERNCLIST
#endif 
program VAS_PROGRAM {
    version VAS_VERSION {
		void_reply 			vzero(vzero_arg) = 0;
		lrid_t_reply 		path2loid(path2loid_arg) = __LINE__;
		void_reply 			openfile(openfile_arg) 	= __LINE__;
		void_reply 			shutdown1(shutdown1_arg) 	= __LINE__;
    } = 1;
} = 0x20000001;

#ifdef RPC_HDR 
%END_EXTERNCLIST
#endif 
