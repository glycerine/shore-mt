/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ssh.h,v 1.14 1997/05/02 22:23:46 nhall Exp $
 */
#ifndef SSH_H
#define SSH_H

#define SSH_ERROR(err) \
    {                                                           \
        cerr << "ssh error (" << err << ") at file " << __FILE__\
             << ", line " << __LINE__ << endl;                  \
        W_FATAL(fcINTERNAL);                                    \
    }

#undef DO

#define DO(x)                                                   \
    {                                                           \
        int err = x;                                            \
        if (err)  { SSH_ERROR(err); }                           \
    }

#define COMM_ERROR(err) \
        cerr << "communication error: code " << (err) \
             << ", file " << __FILE__ << ", line " << __LINE__

#define COMM_FATAL(err) { COMM_ERROR(err); W_FATAL(fcINTERNAL); }

//
// Start and stop client communication listening thread
//
extern rc_t stop_comm();
extern rc_t start_comm();

struct linked_vars {
    int sm_page_sz;
    int sm_max_exts;
    int sm_max_vols;
    int sm_max_xcts;
    int sm_max_servers;
    int sm_max_keycomp;
    int sm_max_dir_cache;
    int sm_max_rec_len;
    int sm_srvid_map_sz;
    int verbose_flag;
} ;
extern linked_vars linked;


#ifdef USE_SSMTEST
/* defined in bf.c */
extern "C" {
    void simulate_preemption(bool);
    bool preemption_simulated();
}
#endif

#endif /* SSH_H */
