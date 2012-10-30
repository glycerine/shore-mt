#define TIMEOUT 10
%#include <sys/types.h>

#include "../include/vas_types.h"

enum common_objmsg_rq { cor_scan=1, cor_read=2, cor_sysp=4, cor_page=8 };

struct regid {
	c_gid_t	rgid;
	c_gid_t	egid;
};
struct reuid {
	c_uid_t	ruid;
	c_uid_t	euid;
};

struct void_arg {
	char	dummy;
};


