/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/do_shutdown.c,v 1.12 1997/09/19 11:54:23 solomon Exp $
 */

#include <copyright.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/clnt.h>
#include <msg.h>
#include <uname.h>
#include <unix_error.h>
#ifdef DEBUG
#include <stdio.h>
#endif
#ifdef SOLARIS2
#include <netdb.h>
#endif

#ifdef I860
BEGIN_EXTERNCLIST
#	include <nx.h>
END_EXTERNCLIST
#endif

#if defined(SOLARIS2)
typedef char bool;
#endif
/* otherwise bool is defined in <rpc/types.h> */

static int
CLIENT2socket(
    CLIENT *cl
)
{
    typedef struct ct_data {
        int ct_sock;
    }ct_data;

    ct_data *ct =   (ct_data *) cl->cl_private;
    int             sock = ct->ct_sock;

    return sock;
}

void
do_shutdown(const char *host, u_short port, bool use_pmap,
	bool	crash, bool wait
)
{
    CLIENT      *clnt;
	void_reply 	*r;
	shutdown1_arg a;

	int program = VAS_PROGRAM;
	int	version = VAS_VERSION;
		version = port;

	a.wait = wait;
	a.crash = crash;


#ifdef DEBUG
	fprintf(stderr, "Trying to contact server at program= %d, version= %d ...\n", program, version );
#endif

	if(use_pmap) {
		clnt = clnt_create(host, program, version, "tcp");
	} else {
#ifndef SOLARIS2
		clnt = clnt_create_port(host, program, version, "tcp", port);
#else
		struct sockaddr_in addr;
		int					sock;
		struct hostent 		*h;

		h = (struct hostent *)gethostbyname(host);

		if (!h) {
			fprintf(stderr, "%s: ", host);
			catastrophic("host unknown");
		}
		memcpy(&addr.sin_addr, h->h_addr, sizeof addr.sin_addr);
		addr.sin_family = AF_INET;

		addr.sin_port = port;
		sock = RPC_ANYSOCK;
		clnt = (CLIENT *)clnttcp_create(&addr, program, version, &sock, 0, 0);
#endif
	}
    if (clnt == NULL) {
		clnt_pcreateerror(host);
        exit(1);
    }
	{
		char *name;
		int sock;

		if((name = uid2uname(getuid(), 0))==NULL) {
			perror("uid2uname");
			exit(1);
		}
		sock = CLIENT2socket(clnt);
#ifdef USE_KRB
#error need to add kerberos
#else
	
		{
			/* just send login name */
			int cc;
			cc = strlen(name);
			if( write(sock, name, cc) != cc ) {
				perror("write login name");
				exit(1);
			}
		}
#endif
	  {
			/* server will send back uid of this guy */
			c_uid_t srvuid;

			if(read(sock, (char *)&srvuid, sizeof(c_uid_t)) != sizeof(c_uid_t)) {
				perror("read after sendauth: did not get uid back from VAS");
				exit(1);
			}
			if(srvuid !=  (c_uid_t)getuid()) {
				if(ntohl(srvuid) ==  getuid()) {
					perror("byte-order mismatch");
					exit(1);
				} else {
					perror("uid mismatch");
					exit(1);
				}
			}
		}
	}

    r = shutdown1_1(&a, clnt);
    if (r == NULL) {
        clnt_perror(clnt, "shutdown");
        exit(1);
    }
	if((errno = r->status.vasreason) != 0) {
		perror("reply: ");
	}
    exit(0);
}
