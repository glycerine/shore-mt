/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: tcl_thread.h,v 1.26 1997/05/27 13:10:47 kupsch Exp $
 */
#ifndef TCL_THREAD_H
#define TCL_THREAD_H

#ifdef __GNUG__
#pragma interface
#endif

class ss_m;
extern ss_m* sm;
extern int t_co_retire(Tcl_Interp* , int , char*[]);
extern void copy_interp(Tcl_Interp *ip, Tcl_Interp *pip);

class tcl_thread_t : public smthread_t  {
public:
    NORET			tcl_thread_t(
	int 			    ac, 
	char* 			    av[], 
	Tcl_Interp* 		    parent,
	bool			    block_immediate = false,		     
	bool			    auto_delete = false);
    NORET			~tcl_thread_t();

    Tcl_Interp*			get_ip() { return ip; }

    static w_list_t<tcl_thread_t> list;
    sevsem_t			sync_point;
    scond_t			proceed;
    smutex_t			lock;
    w_link_t 			link;

    static void			initialize(Tcl_Interp* ip, const char* lib_dir);
    static void 		sync_other(unsigned long id);
    static void 		sync();
    static void 		join(unsigned long id);

    // This is a pointer to a global variable that all
    // threads can use to pass info between them.
    // It is linked to a TCL variable via the
    // link_to_inter_thread_comm_buffer command.
    static char*		inter_thread_comm_buffer;

protected:
    virtual void	 	run();

protected:
    char*			args;
    Tcl_Interp*			ip;
    smutex_t   			thread_mutex;

    static int 			count;
public:
    static bool 		allow_remote_command;

private:
    tcl_thread_t(const tcl_thread_t&);
    tcl_thread_t& operator=(const tcl_thread_t&);
};

// for gcc template instantiation
typedef w_list_i<tcl_thread_t>             tcl_thread_t_list_i;

#endif /*TCL_THREAD_H*/

