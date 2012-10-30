#ifndef _GLOBALS_H_
#define _GLOBALS_H_
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/* Default port for NFS MOUNT requests in the Shore server */
#define SHORE_MPORT 2997

/* Default port for NFS requests in the Shore server */
#define SHORE_PORT 2999

typedef struct mnt_entry {
	char *name;	/* (remote) name of mounted filesystem */
	char *dir;	/* (local) mount point */
	char *type;
	char *options;
#if #platform(solaris)
	char *timestamp;
#elif #platform(ultrix)
	dev_t dev;
#else
	int frequency;
	int passno;
#endif
	struct mnt_entry *next;
} mnt_entry;

int verbose;
char *progname;

/* These routines all manipulate the global list pointed to by (head,tail) */
int get_mounts(void);
int put_mounts(void);
mnt_entry *remove_mount(char *dir, char *fname);
void add_mount(char *dir, char *fname, char *opts);
void free_mount(mnt_entry *ent);

char *xmalloc(size_t s);
char *xstrdup(const char *);

#endif /* _GLOBALS_H_ */
