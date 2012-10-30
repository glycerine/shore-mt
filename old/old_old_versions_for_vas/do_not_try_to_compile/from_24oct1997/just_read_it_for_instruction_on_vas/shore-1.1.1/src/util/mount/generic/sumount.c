/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef __GNUC__
#define __attribute__(x)
#endif
static char *rcsid  __attribute__((unused)) =
"$Header: /p/shore/shore_cvs/src/util/mount/generic/sumount.c,v 1.9 1997/06/13 22:33:56 solomon Exp $";

#include "platform.h"

#define NFSCLIENT
#include <unistd.h>
#include <sys/types.h>

#include "globals.h"

#if #platform(ultrix)
	#include <sys/param.h>
#endif
#include <sys/mount.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/time.h>
#if #platform(solaris)
#	define PORTMAP
#	include <rpc/bootparam.h>
#endif
#include <rpc/rpc.h>
#include <nfs/nfs.h>
#include <rpcsvc/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <assert.h>


/* Platform-dependent stuff */

/*************************** SunOS 4.1 *******************************/
#if #platform(sunos)
#define UNMOUNT unmount
/* Various prototypes that should be in standard hearder files but aren't
 * (SunOS 4.1.3).
 */
void fprintf(FILE *, const char *, ...);
void printf(const char *, ...);
void exit(int);
int getopt(int, char *const*, const char *);
void perror(const char *);
int unmount(const char *);
#endif

/*************************** Solaris 2.4 (a.k.a. SunOS 5.2) ***************/
#if #platform(solaris)
#define UNMOUNT umount
int xdr_path(XDR *xdrs, char **objp) {
	return xdr_string(xdrs, objp, MAX_PATH_LEN);
}
#endif /* solaris */

/*************************** HPUX 9 *******************************/
#if #platform(hpux)
#define UNMOUNT umount
#endif /* hpux */

/*************************** Ultrix 4.3 *******************************/
#if #platform(ultrix)
#define UNMOUNT umount
int xdr_path(XDR *xdrs, char **objp) {
	return xdr_string(xdrs, objp, NFS_MAXPATHLEN);
}
int getopt(int, char *const*, const char *);
int umount(dev_t);
#endif /* ultrix */

/*************************** Linux 2.1 *******************************/
#if #platform(linux)
#define UNMOUNT ???
#endif /* linux */
/*************************** End Platform-dependent Stuff  ****************/

extern int errno; /* in case errno.h omits it */

/* Functions defined below */
int remote_umount(char *host, char *path);
void usage(void);
static void getmport(char *opts);
int find_and_unmount(char *local, char *remote);

/* Global flags and parameters */
int opt_force;
int opt_ignore_errors;
int opt_nolocal;
int opt_noremote;
int opt_nomtab;
int mport = SHORE_MPORT;

/* Contact the remote host using the NFS MOUNT protocol and tell it that
 * PATH is no longer mounted by us.
 */
int remote_umount(char *host, char *path) {
	struct hostent *h;
	static struct sockaddr_in sock_addr;
	struct timeval tv;
	int sock = RPC_ANYSOCK;
	CLIENT *rpc_handle;
	enum clnt_stat status;
	static struct fhstatus result;
	char *warn = opt_ignore_errors ? ": warning" : "";

	/* Create an RPC handle to talk to the server */
	h = gethostbyname(host);
	if (!h) {
		fprintf(stderr, "%s%s: host %s not found\n", progname, warn, host);
		return 0;
	}
	memcpy((char *)&sock_addr.sin_addr, h->h_addr, sizeof sock_addr.sin_addr);
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(mport);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	rpc_handle = clntudp_create(
		&sock_addr, (u_long)MOUNTPROG, (u_long)MOUNTVERS, tv, &sock);
	if (!rpc_handle) {
		fprintf(stderr, "%s%s", progname, warn);
		clnt_pcreateerror("umount");
		return 0;
	}
	rpc_handle->cl_auth = authunix_create_default();

	status = clnt_call(rpc_handle, MOUNTPROC_UMNT, xdr_path, (caddr_t)&path,
		xdr_void, (caddr_t)&result, tv);
	if (status != RPC_SUCCESS) {
		fprintf(stderr, "%s%s: nfs server %s/%d not responding",
			progname, warn, host, mport);
		clnt_perror(rpc_handle, "");
		return 0;
	}
	return 1;
} /* remote_umount */

void usage(void) {
	fprintf(stderr, "usage: %s [-vflrm] [-p port] [host:]path\n",
		progname);
	fprintf(stderr, "	or %s [-vflrm] [-p port] host:path path\n",
		progname);
	exit(1);
}

static void getmport(char *opts) {
	if (!opts)
		return;
	for (;;) {
		if (strncmp(opts,"mport=",6)==0) {
			opts += strlen("mport=");
			mport = atoi(opts);
			return;
		}
		opts = strchr(opts,  ',');
		if (!opts)
			return;
		opts++;
	}
}

int find_and_unmount(char *local, char *remote) {
	mnt_entry *ent;
	char *host = 0, *rpath;
	char *local_tmp = 0, *remote_tmp = 0;
	int result = 0;

	/* get a list of all known mounts */
	if (!get_mounts() && !opt_ignore_errors)
		return 0;
	ent = remove_mount(local, remote);

	/* get missing info from the mount_table entry (if any) */
	if (ent) {
		getmport(ent->options);
		if (!local) {
			local = local_tmp = ent->dir;
			ent->dir = 0;
		}
		if (!remote) {
			remote = remote_tmp = ent->name;
			ent->name = 0;
		}
	}
	else {
		fprintf(stderr, "%s: %sno mounted filesystem matching",
			progname,
			opt_force ? "warning: " : "");
		if (remote) fprintf(stderr, " %s", remote);
		if (local) fprintf(stderr, " %s", local);
		fprintf(stderr, "\n");
		if (!opt_force)
			return 0;
	}

#if #platform(ultrix)
	/* local unmount */
	if (ent && !opt_nolocal) {
		/* make local unmount system call */
		if (verbose)
			fprintf(stderr, "%s: unmounting 0x%x locally\n",
				progname, ent->dev);
		if (UNMOUNT(ent->dev)) {
			fprintf(stderr,"%s: %s", progname,
				opt_ignore_errors ? " warning:" : "");
			perror("umount");
			if (!opt_ignore_errors)
				goto done;
		}
	}
#else /* not ultrix */
	/* local unmount */
	if (local && !opt_nolocal) {
		/* make local unmount system call */
		if (verbose)
			fprintf(stderr, "%s: unmounting %s locally\n",
				progname, local);
		if (UNMOUNT(local)) {
			fprintf(stderr,"%s: %s", progname,
				opt_ignore_errors ? " warning:" : "");
			perror(local);
			if (!opt_ignore_errors)
				goto done;
		}
	}
#endif /* not ultrix */

	/* remote unmount (RPC) */
	if (remote && !opt_noremote) {
		/* tell the server this mount point is no longer needed */
		/* make a copy of the remote name and split it into host and path */
		host = xstrdup(remote);
		rpath = strchr(host, ':');
		if (!rpath) {
			fprintf(stderr,"%s: no colon in remote name %s\n", progname, host);
			goto done;
		}
		*rpath++ = 0;
		if (verbose)
			fprintf(stderr, "%s: unmounting %s at host %s, port %d\n",
				progname, rpath, host, mport);
		result = remote_umount(host, rpath);
		if (!result && !opt_ignore_errors)
			goto done;
	}

	/* update mtab */
	if (ent && !opt_nomtab) {
		if (!put_mounts())
			goto done;
	}
	result = 1;
done:
	if (ent) free_mount(ent);
	if (host) free(host);
	if (local_tmp) free(local_tmp);
	if (remote_tmp) free(remote_tmp);
	return result;
} /* find_and_unmount */

int main(int argc, char **argv) {
	int c;
	extern char *optarg;
	extern int optind;
	char *local = 0, *remote = 0;

	progname = argv[0];
	if (geteuid() != 0) {
		fprintf(stderr, "%s: you must be root to unmount a file system\n",
			progname);
	}
	while ((c = getopt(argc,argv,"vfilrmp:"))!=EOF) switch(c) {
		case 'v':
			verbose++;
			break;
		case 'f':
			opt_force++;
			break;
		case 'i':
			opt_ignore_errors++;
			break;
		case 'l':
			opt_nolocal++;
			break;
		case 'r':
			opt_noremote++;
			break;
		case 'm':
			opt_nomtab++;
			break;
		case 'p':
			mport = atoi(optarg);
			break;
		default:
			usage();
	}

	switch (argc - optind) {
		default:
			usage();
		case 2:
			/* args should be HOST:REMOTE_PATH LOCAL_PATH */
			remote = argv[optind];
			local = argv[optind+1];
			if (!strchr(remote,':')) {
				fprintf(stderr,
					"%s: file system `%s' must have the form host:path\n",
					progname, remote);
				return 1;
			}
			if (*local != '/') {
				fprintf(stderr, "%s: mount point `%s' must start with '/'\n",
					progname, local);
				return 1;
			}
			break;
		case 1:
			/* arg is HOST:REMOTE_PATH or LOCAL_PATH */
			if (opt_force) {
				fprintf(stderr,"%s: %s\n",
					progname,
					"-f requires both mountpoint and host:path");
				usage();
			}
			local = argv[optind];
			if (strchr(local, ':')) {
				remote = local;
				local = 0;
			}
			break;
	}
	if (!find_and_unmount(local, remote))
		return 1;

	return 0;
} /* main */
