/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: device.h,v 1.9 1995/04/24 19:35:32 zwilling Exp $
 */

#ifndef DEVICE_H
#define DEVICE_H

#ifdef __GNUG__
#pragma interface
#endif



// must be outside of device_m due to HP CC limitation
struct device_s {
    char	name[smlevel_0::max_devname+1];
    shpid_t	quota_pages;
    devid_t	id;		// unique device id since name may
				// not be unique
    w_link_t	link;
};

struct device_hdr_s {
    device_hdr_s(uint4 version, uint4 quota_in_KB, lvid_t v) :
	format_version(version), quota_KB(quota_in_KB), lvid(v) {}
    uint4		format_version;
    uint4		quota_KB;
    lvid_t		lvid;
};

class device_m : public smlevel_0 {
public:
    enum { max = smlevel_0::max_vols };
    device_m();
    ~device_m();

    rc_t mount(const char* dev_name, const device_hdr_s& dev_hdr, u_int& vol_cnt);
    bool is_mounted(const char* dev_name);
    rc_t quota(const char* dev_name, smksize_t& quota_KB);
    rc_t dismount(const char* dev_name);
    rc_t dismount_all();
    rc_t list_devices(const char**& dev_list, devid_t*& devs, u_int& dev_cnt);
    void dump() const;
    
private:
    device_s*	_find(const char* dev_name);
    device_s*	_find(const devid_t& devid);

    // table of all devices currently mounted
    w_list_t<device_s> _tab;

    // disabled
    device_m(const device_m&);
    operator=(const device_m&);
};

#endif /* DEVICE_H */
