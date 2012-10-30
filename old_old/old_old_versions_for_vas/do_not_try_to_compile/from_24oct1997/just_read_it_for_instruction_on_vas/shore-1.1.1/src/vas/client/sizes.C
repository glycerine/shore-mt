/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <stdio.h>
#include <debug.h>
#include <stdlib.h>
#include "msg.h"
#include <svas_client.h>
#include <svas_layer.h>
#include <option.h>

main()
{
#define S(x) cout << #x << ":\t\t" << sizeof(x) << endl;

	S(uid_t);
	S(gid_t);
	S(mode_t);
	S(time_t);
	S(AnonProps);
	S(RegProps);
	S(_anon_sysprops);
	S(_reg_sysprops);
	S(_reg_sysprops_withtextandindex); //the largest

	int largest=0;
	int smallest=10000;
	int largest_arg=0;
	int smallest_arg=10000;
	int largest_reply=0;
	int smallest_reply=10000;
#undef S
#define S(x)\
	cout << #x << ":\t\t" << sizeof(x) << endl;\
	if(sizeof(x)>largest) largest=sizeof(x); \
	if(sizeof(x)<smallest) smallest=sizeof(x);

	S(timeval_t);
	S(VASResult);
	S(LockMode);
//	S(Path);
	S(IndexKind);
	S(CompareOp);
	S(Vote);
	S(RequestMode);
	S(LockEvent);
	S(ShoreStatus);
	S(FSDATA);
	S(opaque_t);
	S(gtid_t);
//	S(CoordHandle);
	S(POID);
	S(Cookie);
	S(tid_t);
	S(IndexId);
	S(sm_du_stats_t);
	S(class svas_base);
	S(class svas_client);
	S(class svas_layer_init);
	S(class option_t);
	S(class option_group_t);

/***********************/
#define SA(x)\
	S(x)\
	if(sizeof(x)>largest_arg) largest_arg=sizeof(x); \
	if(sizeof(x)<smallest_arg) smallest_arg=sizeof(x);

#define SR(x)\
	S(x)\
	if(sizeof(x)>largest_reply) largest_reply=sizeof(x); \
	if(sizeof(x)<smallest_reply) smallest_reply=sizeof(x);

SR(void_reply);
SR(voidref_reply);
SR(lvid_t_reply);
SR(int_reply);
SR(short_reply);
SR(statindex_reply);
SR(u_int_reply);
SR(u_short_reply);
SR(bool_reply);
SR(char_reply);
SR(u_char_reply);
SR(lrid_t_reply);
SR(lookup_reply);
SR(mkanon_reply);
SR(init_reply);
SR(POID_reply);
SR(Path_reply);
SR(Cookie_reply);
SR(c_mode_t_reply);
SR(c_uid_t_reply);
SR(c_gid_t_reply);
SR(IndexId_reply);
SR(tid_t_reply);
SR(Vote_reply);
//SR(CoordHandle_reply);
SR(statfs1_reply);
SR(diskusage_reply);
SR(getvol_reply);
SR(getmnt_reply);
SR(getdirentries1_reply);
SR(nextelem_reply);
SR(common_objmsg_reply);
SR(rpcSysProps_reply);
SR(readobj_reply);
SR(nextpoolscan1_reply);
SR(nextpoolscan2_reply);
SR(rmlink1_reply);
SR(find_reply);
SR(readsymlink_reply);
SR(nextindexscan_reply);
SR(getreuid1_reply);
SR(getregid1_reply);
SR(batched_req_reply);
SR(gather_stats_reply);

S(stats_types);
S(stat_values);
S(stats_module);

SA(void_arg);
SA(client_init_arg);
SA(v_format_arg);
SA(v_mkfs_arg);
SA(v_rmfs_arg);
SA(v_serve_arg);
SA(v_unserve_arg);
SA(v_dismount_arg);
SA(lvid_t_arg);
SA(v_volroot_arg);
SA(setroot_arg);
SA(getroot_arg);
SA(czero_arg);
SA(v_newvid_arg);
SA(getvol_arg);
SA(getmnt_arg);
SA(lookup1_arg);
SA(lookup2_arg);
SA(statfs1_arg);
SA(mklink_arg);
SA(rename1_arg);
SA(mksymlink_arg);
SA(readsymlink_arg);
SA(mkxref_arg);
SA(readxref_arg);
SA(mkregistered2_arg);
S(shmdata);
SA(mkregistered_arg);
SA(mkanonymous2_arg);
SA(mkanonymous3_arg);
SA(mkanonymous4_arg);
SA(mkanonymous5_arg);
SA(getdirentries1_arg);
SA(mkdir1_arg);
SA(mkpool_arg);
SA(rm_arg);
SA(rmpool_arg);
SA(rmdir1_arg);
SA(rmlink1_arg);
SA(rmlink2_arg);
SA(rmanonymous_arg);
SA(fileof1_arg);
SA(fileof2_arg);
SA(openpoolscan1_arg);
SA(openpoolscan2_arg);
SA(common_objmsg_arg);
SA(stat1_arg);
SA(stat2_arg);
SA(readobj_arg);
SA(nextpoolscan2_arg);
SA(nextpoolscan1_arg);
SA(cookie_arg);
SA(closepoolscan_arg);
SA(inserta_arg);
SA(remove1a_arg);
SA(remove2a_arg);
SA(finda_arg);
SA(openindexscan2_arg);
SA(nextindexscan_arg);
SA(closeindexscan_arg);
SA(chroot1_arg);
SA(chdir1_arg);
SA(getdir_arg);
SA(setumask_arg);
SA(getumask_arg);
SA(setreuid1_arg);
SA(getreuid1_arg);
SA(setregid1_arg);
SA(getregid1_arg);
SA(chmod1_arg);
SA(chown1_arg);
SA(utimes2_arg);
SA(utimes1_arg);
SA(addindex1_arg);
SA(dropindex1_arg);
SA(statindex1_arg);
SA(fetchelem_arg);
SA(insertelem_arg);
SA(removeelem_arg);
SA(incelem_arg);
SA(decelem_arg);
SA(scanindex_arg);
SA(nextelem_arg);
SA(closescan_arg);
SA(begintrans_arg);
SA(abort1_arg);
SA(commit_arg);
SA(nfs_begintrans_arg);
SA(nfs_abort1_arg);
SA(nfs_commit_arg);
SA(enter2pc_arg);
SA(prepare_arg);
SA(continue2pc_arg);
SA(recover2pc_arg);
SA(lockobj_arg);
SA(upgrade_arg);
SA(unlockobj_arg);
SA(notifylock_arg);
SA(notifyandlock_arg);
SA(selectnotifications_arg);
SA(appendobj_arg);
SA(updateobj1_arg);
SA(updateobj2_arg);
SA(writeobj_arg);
SA(truncobj_arg);
S(batch_req);
SA(batched_req_arg);
SA(mkvolref_arg);
SA(snapref_arg);
SA(validateref_arg);
SA(physicaloid_arg);
SA(diskusage_arg);
SA(offvolref_arg);
SA(transferref_arg);
SA(gather_stats_arg);
SA(nullcmd_arg);
SA(path2loid_arg);
SA(openfile_arg);
SA(vzero_arg);
SA(shutdown1_arg);

S(w_statistics_t);

	cout << " largest=" << largest << endl;
	cout << " smallest=" << smallest << endl;

	cout << " largest_arg=" << largest_arg << endl;
	cout << " smallest_arg=" << smallest_arg << endl;

	cout << " largest_reply=" << largest_reply << endl;
	cout << " smallest_reply=" << smallest_reply << endl;
}
