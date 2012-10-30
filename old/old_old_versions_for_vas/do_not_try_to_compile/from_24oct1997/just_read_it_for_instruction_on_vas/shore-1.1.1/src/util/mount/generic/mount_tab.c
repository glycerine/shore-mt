/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
/* Routines to handle /etc/mtab (or whatever it's called on a particular
 * platform) in a platform-independent manner.
 */
#ifndef __GNUC__
#define __attribute__(x)
#endif
static char *rcsid  __attribute__((unused)) =
"$Header: /p/shore/shore_cvs/src/util/mount/generic/mount_tab.c,v 1.8 1997/06/13 22:33:55 solomon Exp $";

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#if #platform(solaris)
#	include <sys/mnttab.h>
#	include <time.h>
#	include <fcntl.h>
#elif #platform(ultrix)
#	include <sys/types.h>
#	include <sys/param.h>
#	include <sys/mount.h>
#	include <rpc/rpc.h>
#	include <nfs/nfs_clnt.h>
#	include <nfs/vfs.h>
#	include <sys/fs_types.h>
#else
#	include <mntent.h>
#endif

#include "globals.h"

static mnt_entry *head, *tail;
static void append_mount(mnt_entry *ent);

/* Platform-dependent stuff */

/*************************** SunOS 4.1 *******************************/
#if #platform(sunos)
#define MNTENT mntent
/* Various prototypes that should be in standard hearder files but aren't
 * (SunOS 4.1.3).
 */
void fprintf(FILE *, const char *, ...);
void printf(const char *, ...);
void exit(int);
void perror(const char *);
int addmntent(const FILE *, const struct mntent *);
void *malloc(size_t);
void free(void *);
int ftruncate(int, off_t);
void rewind(FILE *);

/*************************** Solaris 2.4 (a.k.a. SunOS 5.2) ***************/
#elif #platform(solaris)
#define MNTENT mnttab
#define MOUNTED MNTTAB
#define LOCKFILE "/etc/.mnttab.lock"
#define addmntent(FI, MP)  (putmntent(FI, MP) < 0)
#define endmntent(FI) solaris_unlock()
#define mnt_fsname	mnt_special
#define mnt_dir		mnt_mountp
#define	mnt_type	mnt_fstype
#define	mnt_opts	mnt_mntopts
static FILE *solaris_lock(void);
static FILE *solaris_getlock(void);
static void solaris_unlock(void);

/*************************** HPUX 9 *******************************/
#elif #platform(hpux)
#define MNTENT mntent

/*************************** Ultrix 4.3 *******************************/
#elif #platform(ultrix)
/* apparently, Ultrix doesn't have anything like /etc/mtab.
 * Everything is just kept in the kernel (see getmountent(3) and getmnt(2)).
 */
int getmountent(int *start, struct fs_data *buffer, int nentries);

#else
#	error: platform not supported
#endif

/* Read in all the mount entries from a file (normally /etc/mtab) and
 * build a list.
 * Return 1 on success, 0 on failure.
 */
#if #platform(solaris)
int get_mounts() {
	FILE *fi;
	mnt_entry *ent;
	struct MNTENT m;
	int res;

	fi = solaris_lock();
	head = tail = 0;
	while ((res = getmntent(fi, &m))==0) {
		ent = (mnt_entry *)xmalloc(sizeof *ent);
		ent->name = xstrdup(m.mnt_special);
		ent->dir = xstrdup(m.mnt_mountp);
		ent->type = xstrdup(m.mnt_fstype);
		ent->options = xstrdup(m.mnt_mntopts);
		ent->timestamp = xstrdup(m.mnt_time);
		append_mount(ent);
	}
	return 1;
}
#elif #platform(ultrix)
int get_mounts() {
	int cookie = 0;
	struct fs_data buf;
	struct v_fs_data *vbuf = (struct v_fs_data *)&buf;
	mnt_entry *ent;
	int res;

	while ((res = getmountent(&cookie, &buf, 1)) > 0) {
		char *opts = vbuf->fd_un.gvfs.mi.mi_optstr;

		ent = (mnt_entry *)xmalloc(sizeof *ent);
		ent->name = xstrdup(buf.fd_req.devname);
		ent->dir = xstrdup(buf.fd_req.path);
		ent->type = xstrdup(gt_names[buf.fd_req.fstype]);
		ent->dev = buf.fd_dev;
		ent->options = xstrdup(opts);
		append_mount(ent);
	}
	return 1;
}
#else /* not solaris or ultrix  */
int get_mounts() {
	FILE *fi;
	mnt_entry *ent;
	struct MNTENT *m;

	fi = setmntent(MOUNTED, "r");
	if (!fi) {
		perror(MOUNTED);
		return 0;
	}

	head = tail = 0;
	while ((m = getmntent(fi))) {
		ent = (mnt_entry *)xmalloc(sizeof *ent);
		ent->name = xstrdup(m->mnt_fsname);
		ent->dir = xstrdup(m->mnt_dir);
		ent->type = xstrdup(m->mnt_type);
		ent->options = xstrdup(m->mnt_opts);
		ent->frequency = m->mnt_freq;
		ent->passno = m->mnt_passno;
		append_mount(ent);
	}
	endmntent(fi);
	return 1;
}
#endif /* not not solaris or ultrix  */

/* Dump the list to /etc/mtab (or wherever) and free it up.
 * Return 1 on success, 0 on failure.
 */
#if #platform(ultrix)
int put_mounts() {
	/* Since Ultrix has no /etc/mtab, put_mounts does nothing but free up
	 * the data struture.
	 */
	mnt_entry *ent, *next;
	for (ent = head; ent; ent = next) {
		next = ent->next;
		free_mount(ent);
	}
	return 1;
}
#else /* not ultrix */
int put_mounts() {
	FILE *fi;
	mnt_entry *ent, *next;
	struct MNTENT m;

	if (verbose)
		fprintf(stderr, "%s: updating %s\n", progname, MOUNTED);
#if #platform(solaris)
	fi = solaris_getlock();
#else /* not solaris */
	fi = setmntent(MOUNTED, "w");
#endif /* not solaris */
	if (!fi) {
		perror(MOUNTED);
		return 0;
	}
	rewind(fi);
	if (ftruncate(fileno(fi), 0)) {
		perror("ftruncate");
		return 0;
	}

	for (ent = head; ent; ent = next) {
		m.mnt_fsname = ent->name;
		m.mnt_dir = ent->dir;
		m.mnt_type = ent->type;
		m.mnt_opts = ent->options;
#if #platform(solaris)
		m.mnt_time = ent->timestamp;
#else /* not solaris */
		m.mnt_freq = ent->frequency;
		m.mnt_passno = ent->passno;
#endif /* not solaris */
		if (addmntent(fi, &m)) {
			perror(MOUNTED);
			endmntent(fi);
			return 0;
		}

		next = ent->next;
		free_mount(ent);
	}
	endmntent(fi);
	return 1;
}
#endif /* not ultrix */

/* Create a new entry and add it to the list.
 * The args should all be malloced space that is included with the entry.
 */
void add_mount(char *dir, char *name, char *opts) {
#if #platform(ultrix)
	/* nothing needed for ultrix */
#else /* not ultrix */
	mnt_entry *ent;
	ent = (mnt_entry *)xmalloc(sizeof *ent);
	ent->name = name;
	ent->dir = dir;
	ent->type = xstrdup("nfs");
	ent->options = opts;
#if #platform(solaris)
	ent->timestamp = xmalloc(11);
	sprintf(ent->timestamp, "%ld", time(0));
#else /* not solaris */
	ent->frequency = 0;
	ent->passno = 0;
#endif /* not solaris */
	append_mount(ent);
#endif /* not ultrix */
}

/* Find an entry of type NFS matching either dir or name, return
 * a pointer to it, and remove it from the list.
 * Return nil on failure.
 */
mnt_entry *remove_mount(char *dir, char *name) {
	mnt_entry *ent, *prev;

	for (prev=0, ent=head; ent; prev=ent, ent = ent->next) {
		if (strcmp(ent->type, "nfs") != 0)
			continue;
		if (dir && strcmp(dir, ent->dir) == 0)
			break;
		if (name && strcmp(name, ent->name) == 0)
			break;
	}
	if (ent) {
		if (prev) prev->next = ent->next;
		else head = ent->next;
	}
	return ent;
}

/* Add the entry (not a copy of it) to the tail of the list. */
static void append_mount(mnt_entry *ent) {
	ent->next = 0;
	if (tail) tail->next = ent;
	else head = ent;
	tail = ent;
}

/* Delete the entry and everything in  it */
void free_mount(mnt_entry *ent) {
	if (ent->name) free(ent->name);
	if (ent->dir) free(ent->dir);
	if (ent->type) free(ent->type);
	if (ent->options) free(ent->options);
	free(ent);
}

char *xstrdup(const char *s) {
	char *res = xmalloc(strlen(s)+1);
	strcpy(res, s);
	return res;
}

char *xmalloc(size_t s) {
	char *result = malloc(s);
	if (result==0) {
		fprintf(stderr,"%s: out of memory\n", progname);
		exit(1);
	}
	return result;
}

#if #platform(solaris)
static int lockfile;
static FILE *mntfile;

/* Bizzare way of locking mnttab.
 * For Solaris, we leave the file open and locked from get_mounts
 * until put_mounts.
 * WARNING:  This code assumes that these calls strictly alternate
 * (in fact, each is called exactly once).
 */
static FILE *solaris_lock() {
	if (mntfile != 0) {
		/* cant happen! */
		fprintf(stderr, "solaris_lock: already locked\n");
		return 0;
	}
	lockfile = creat(LOCKFILE, 0644);
	if (lockfile < 0) {
		perror(LOCKFILE);
		return 0;
	}
	if (lockf(lockfile, F_LOCK, 0)) {
		perror("lockf");
		close(lockfile);
		return 0;
	}
	mntfile = fopen(MNTTAB, "r+");
	if (!mntfile) {
		perror(MNTTAB);
		close(lockfile);
		return 0;
	}
	if (lockf(fileno(mntfile), F_LOCK, 0)) {
		perror("lockf2");
		close(lockfile);
		return 0;
	}
	return mntfile;
}

static FILE *solaris_getlock() {
	return mntfile;
}

static void solaris_unlock() {
	fclose(mntfile);
	close(lockfile);
}
#endif /* solaris */
