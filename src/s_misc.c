/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_misc.c (formerly ircd/date.c)
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef lint
static  char sccsid[] = "@(#)s_misc.c	2.42 3/1/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#ifndef _WIN32
#include <sys/time.h>
#endif
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "userload.h"
#include <sys/stat.h>
#include <fcntl.h>
#if !defined(ULTRIX) && !defined(SGI) && !defined(sequent) && \
    !defined(__convex__) && !defined(_WIN32)
# include <sys/param.h>
#endif
#if defined(PCS) || defined(AIX) || defined(SVR3)
# include <time.h>
#endif
#ifdef HPUX
#include <unistd.h>
#endif
#ifdef DYNIXPTX
#include <sys/types.h>
#include <time.h>
#endif
#ifdef _WIN32
# include <io.h>
#endif
#include "h.h"

static	void	exit_one_client PROTO((aClient *,aClient *,aClient *,char *));
static	void	exit_one_client_in_split PROTO((aClient *,aClient *,aClient *,char *));

static	char	*months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
};

static	char	*weekdays[] = {
	"Sunday",	"Monday",	"Tuesday",	"Wednesday",
	"Thursday",	"Friday",	"Saturday"
};

/*
 * stats stuff
 */
struct	stats	ircst, *ircstp = &ircst;

char	*date(clock) 
time_t	clock;
{
	static	char	buf[80], plus;
	Reg1	struct	tm *lt, *gm;
	struct	tm	gmbuf;
	int	minswest;

	if (!clock) 
		time(&clock);
	gm = gmtime(&clock);
	bcopy((char *)gm, (char *)&gmbuf, sizeof(gmbuf));
	gm = &gmbuf;
	lt = localtime(&clock);

	if (lt->tm_yday == gm->tm_yday)
		minswest = (gm->tm_hour - lt->tm_hour) * 60 +
			   (gm->tm_min - lt->tm_min);
	else if (lt->tm_yday > gm->tm_yday)
		minswest = (gm->tm_hour - (lt->tm_hour + 24)) * 60;
	else
		minswest = ((gm->tm_hour + 24) - lt->tm_hour) * 60;

	plus = (minswest > 0) ? '-' : '+';
	if (minswest < 0)
		minswest = -minswest;

	(void)sprintf(buf, "%s %s %d 19%02d -- %02d:%02d %c%02d:%02d",
		weekdays[lt->tm_wday], months[lt->tm_mon],lt->tm_mday,
		lt->tm_year, lt->tm_hour, lt->tm_min,
		plus, minswest/60, minswest%60);

	return buf;
}

/*
 *  Fixes a string so that the first white space found becomes an end of
 * string marker (`\-`).  returns the 'fixed' string or "*" if the string
 * was NULL length or a NULL pointer.
 */
char	*check_string(s)
Reg1	char *s;
{
	static	char	star[2] = "*";
	char	*str = s;

	if (BadPtr(s))
		return star;

	for ( ;*s; s++)
		if (isspace(*s))
		    {
			*s = '\0';
			break;
		    }

	return (BadPtr(str)) ? star : str;
}

/*
 * create a string of form "foo!bar@fubar" given foo, bar and fubar
 * as the parameters.  If NULL, they become "*".
 */
char *make_nick_user_host(nick, name, host)
Reg1	char	*nick, *name, *host;
{
	static	char	namebuf[NICKLEN+USERLEN+HOSTLEN+6];
	Reg2	char	*s = namebuf;

	bzero(namebuf, sizeof(namebuf));
	nick = check_string(nick);
	strncpyzt(namebuf, nick, NICKLEN + 1);
	s += strlen(s);
	*s++ = '!';
	name = check_string(name);
	strncpyzt(s, name, USERLEN + 1);
	s += strlen(s);
	*s++ = '@';
	host = check_string(host);
	strncpyzt(s, host, HOSTLEN + 1);
	s += strlen(s);
	*s = '\0';
	return (namebuf);
}

/**
 ** myctime()
 **   This is like standard ctime()-function, but it zaps away
 **   the newline from the end of that string. Also, it takes
 **   the time value as parameter, instead of pointer to it.
 **   Note that it is necessary to copy the string to alternate
 **   buffer (who knows how ctime() implements it, maybe it statically
 **   has newline there and never 'refreshes' it -- zapping that
 **   might break things in other places...)
 **
 **/

char	*myctime(value)
time_t	value;
{
	static	char	buf[28];
	Reg1	char	*p;

	(void)strcpy(buf, ctime(&value));
	if ((p = (char *)index(buf, '\n')) != NULL)
		*p = '\0';

	return buf;
}

/*
** check_registered_user is used to cancel message, if the
** originator is a server or not registered yet. In other
** words, passing this test, *MUST* guarantee that the
** sptr->user exists (not checked after this--let there
** be coredumps to catch bugs... this is intentional --msa ;)
**
** There is this nagging feeling... should this NOT_REGISTERED
** error really be sent to remote users? This happening means
** that remote servers have this user registered, althout this
** one has it not... Not really users fault... Perhaps this
** error message should be restricted to local clients and some
** other thing generated for remotes...
*/
int	check_registered_user(sptr)
aClient	*sptr;
{
	if (!IsRegisteredUser(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return -1;
	    }
	return 0;
}

/*
** check_registered user cancels message, if 'x' is not
** registered (e.g. we don't know yet whether a server
** or user)
*/
int	check_registered(sptr)
aClient	*sptr;
{
	if (!IsRegistered(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return -1;
	    }
	return 0;
}

/*
** get_client_name
**      Return the name of the client for various tracking and
**      admin purposes. The main purpose of this function is to
**      return the "socket host" name of the client, if that
**	differs from the advertised name (other than case).
**	But, this can be used to any client structure.
**
**	Returns:
**	  "name[user@ip#.port]" if 'showip' is true;
**	  "name[sockethost]", if name and sockhost are different and
**	  showip is false; else
**	  "name".
**
** NOTE 1:
**	Watch out the allocation of "nbuf", if either sptr->name
**	or sptr->sockhost gets changed into pointers instead of
**	directly allocated within the structure...
**
** NOTE 2:
**	Function return either a pointer to the structure (sptr) or
**	to internal buffer (nbuf). *NEVER* use the returned pointer
**	to modify what it points!!!
*/
char	*get_client_name(sptr, showip)
aClient *sptr;
int	showip;
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (MyConnect(sptr))
	    {
		if (IsUnixSocket(sptr))
		    {
			if (showip)
				(void) sprintf(nbuf, "%s[%s]",
					sptr->name, sptr->sockhost);
			else
				(void) sprintf(nbuf, "%s[%s]",
					sptr->name, me.sockhost);
		    }
		else
		    {
			if (showip)
				(void)sprintf(nbuf, "%s[%s@%s.%u]",
					sptr->name,
					(!(sptr->flags & FLAGS_GOTID)) ? "" :
					sptr->username,
					inetntoa((char *)&sptr->ip),
					(unsigned int)sptr->port);
			else
			    {
				if (mycmp(sptr->name, sptr->sockhost))
					(void)sprintf(nbuf, "%s[%s]",
						sptr->name, sptr->sockhost);
				else
					return sptr->name;
			    }
		    }
		return nbuf;
	    }
	return sptr->name;
}

char	*get_client_host(cptr)
aClient	*cptr;
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (!MyConnect(cptr))
		return cptr->name;
	if (!cptr->hostp)
		return get_client_name(cptr, FALSE);
	if (IsUnixSocket(cptr))
		(void) sprintf(nbuf, "%s[%s]", cptr->name, me.name);
	else
		(void)sprintf(nbuf, "%s[%-.*s@%-.*s]",
			cptr->name, USERLEN,
			(!(cptr->flags & FLAGS_GOTID)) ? "" : cptr->username,
			HOSTLEN, cptr->hostp->h_name);
	return nbuf;
}

/*
 * Form sockhost such that if the host is of form user@host, only the host
 * portion is copied.
 */
void	get_sockhost(cptr, host)
Reg1	aClient	*cptr;
Reg2	char	*host;
{
	Reg3	char	*s;
	if ((s = (char *)index(host, '@')))
		s++;
	else
		s = host;
	strncpyzt(cptr->sockhost, s, sizeof(cptr->sockhost));
}

/*
 * Return wildcard name of my server name according to given config entry
 * --Jto
 */
char	*my_name_for_link(name, aconf)
char	*name;
aConfItem *aconf;
{
	static	char	namebuf[HOSTLEN];
	register int	count = aconf->port;
	register char	*start = name;

	if (count <= 0 || count > 5)
		return start;

	while (count-- && name)
	    {
		name++;
		name = (char *)index(name, '.');
	    }
	if (!name)
		return start;

	namebuf[0] = '*';
	(void)strncpy(&namebuf[1], name, HOSTLEN - 1);
	namebuf[HOSTLEN - 1] = '\0';

	return namebuf;
}

/*
** exit_client
**	This is old "m_bye". Name  changed, because this is not a
**	protocol function, but a general server utility function.
**
**	This function exits a client of *any* type (user, server, etc)
**	from this server. Also, this generates all necessary prototol
**	messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**	exits all other clients depending on this connection (e.g.
**	remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_funtion return value:
**
**	FLUSH_BUFFER	if (cptr == sptr)
**	0		if (cptr != sptr)
*/
int	exit_client(cptr, sptr, from, comment)
aClient *cptr;	/*
		** The local client originating the exit or NULL, if this
		** exit is generated by this server for internal reasons.
		** This will not get any of the generated messages.
		*/
aClient *sptr;	/* Client exiting */
aClient *from;	/* Client firing off this Exit, never NULL! */
char	*comment;	/* Reason for the exit */
    {
	Reg1	aClient	*acptr;
	Reg2	aClient	*next;
#ifdef	FNAME_USERLOG
	time_t	on_for;
#endif
	static	char	comment1[HOSTLEN + HOSTLEN + 2];
	static	int	recurse=0;


	if (MyConnect(sptr))
	    {
		sptr->flags |= FLAGS_CLOSING;
                if (IsPerson(sptr))
                 sendto_umode(UMODE_OPER|UMODE_CLIENT,"*** Notice -- Client exiting: %s (%s@%s) [%s]", 
                  sptr->name, sptr->user->username, sptr->user->host, comment);
		current_load_data.conn_count--;
		if (IsPerson(sptr)) {
		  char mydom_mask[HOSTLEN + 1];
		  mydom_mask[0] = '*';
		  strncpy(&mydom_mask[1], DOMAINNAME, HOSTLEN - 1);
		  current_load_data.client_count--;
		  if (match(mydom_mask, sptr->sockhost) == 0)
		    current_load_data.local_count--;
		  /* Clean out list and watch structures -Donwulff */
		  hash_del_notify_list(sptr);
		  if (sptr->lopt) {
		    free_str_list(sptr->lopt->yeslist);
		    free_str_list(sptr->lopt->nolist);
		    MyFree(sptr->lopt);
		  }
		}
		update_load();
#ifdef FNAME_USERLOG
		on_for = time(NULL) - sptr->firsttime;
# if defined(USE_SYSLOG) && defined(SYSLOG_USERS)
		if (IsPerson(sptr))
			syslog(LOG_NOTICE, "%s (%3d:%02d:%02d): %s@%s (%s)\n",
			       myctime(sptr->firsttime),
			       on_for / 3600, (on_for % 3600)/60,
			       on_for % 60, sptr->user->username,
			       sptr->sockhost, sptr->name);
# else
	    {
		char	linebuf[160];
		int	logfile;

		/*
 		 * This conditional makes the logfile active only after
		 * it's been created - thus logging can be turned off by
		 * removing the file.
		 *
		 * stop NFS hangs...most systems should be able to open a
		 * file in 3 seconds. -avalon (curtesy of wumpus)
		 */
		if (IsPerson(sptr) &&
		    (logfile = open(FNAME_USERLOG, O_WRONLY|O_APPEND)) != -1)
		    {
			(void)sprintf(linebuf,
				"%s (%3d:%02d:%02d): %s@%s [%s]\n",
				myctime(sptr->firsttime),
				on_for / 3600, (on_for % 3600)/60,
				on_for % 60,
				sptr->user->username, sptr->user->host,
				sptr->username);
			(void)write(logfile, linebuf, strlen(linebuf));
			(void)close(logfile);
		    }
		/* Modification by stealth@caen.engin.umich.edu */
	    }
# endif
#endif
		if (sptr->fd >= 0 && !IsConnecting(sptr))
		    {
		      if (cptr != NULL && sptr != cptr)
			sendto_one(sptr, "ERROR :Closing Link: %s %s (%s)",
				   get_client_name(sptr,FALSE),
				   cptr->name, comment);
		      else
			sendto_one(sptr, "ERROR :Closing Link: %s (%s)",
				   get_client_name(sptr,FALSE), comment);
		    }
		/*
		** Currently only server connections can have
		** depending remote clients here, but it does no
		** harm to check for all local clients. In
		** future some other clients than servers might
		** have remotes too...
		**
		** Close the Client connection first and mark it
		** so that no messages are attempted to send to it.
		** (The following *must* make MyConnect(sptr) == FALSE!).
		** It also makes sptr->from == NULL, thus it's unnecessary
		** to test whether "sptr != acptr" in the following loops.
		*/
		close_connection(sptr);
	    }

	/*
	 * Recurse down the client list and get rid of clients who are no
	 * longer connected to the network (from my point of view)
	 * Only do this expensive stuff if exited==server -Donwulff
	 */

	if (IsServer(sptr)) {
		/*
		 * Is this right? Not recreateing the split message if
		 * we have been called recursivly? I hope so, cuz thats
		 * the only way I could make this give the right servers
		 * in the quit msg. -Cabal95
		 */
		if (cptr && !recurse) {
			(void)strcpy(comment1, sptr->srvptr->name);
			(void)strcat(comment1, " ");
			(void)strcat(comment1, sptr->name);
		}
		/*
		 * First, remove the clients on the server itself.
		 */
		for (acptr = client; acptr; acptr=next) {
			next = acptr->next;
			if (IsClient(acptr) && (acptr->srvptr == sptr))
				exit_one_client_in_split(NULL, acptr,
					&me, comment1);
#ifdef DEBUGMODE
			else if (IsClient(acptr) &&
			    (find_server(acptr->user->server,NULL)==sptr)) {
				sendto_ops("WARNING, srvptr!=sptr but "
					"find_server did!  User %s on %s "
					"thought it was on %s while "
					"loosing %s.  Tell coding team.",
					acptr->name, acptr->user->server,
					acptr->srvptr?acptr->srvptr->name:"<noserver>",
					sptr->name);
				exit_one_client_in_split(NULL, acptr,
					&me, comment1);
			}
#endif
		}

		/*
		 * Now, go SQUIT off the servers which are down-stream of
		 * the one we just lost.
		 */
		recurse++;
		for (acptr = client; acptr; acptr=next) {
			next = acptr->next;
			if (IsServer(acptr) && acptr->srvptr == sptr)
				exit_client(sptr, acptr, /* RECURSION */
					    sptr, comment1);
			/*
			 * I am not masking SQUITS like I do QUITs.  This
			 * is probobly something we could easily do, but
			 * how much savings is there really in something
			 * like that?
			 */
#ifdef DEBUGMODE
			else if (IsServer(acptr) &&
			    (find_server(acptr->serv->up, NULL)==sptr)) {
				sendto_ops("WARNING, srvptr!=sptr but "
					"find_server did!  Server %s on "
					"%s thought it was on %s while "
					"loosing %s.  Tell coding team.",
					acptr->name, acptr->serv->up,
					acptr->srvptr?acptr->srvptr->name:"<noserver>",
					sptr->name);
				exit_client(sptr, acptr, sptr, comment1);
			}
#endif
		}
		recurse--;
	}

	/*
	 * Finally, clear out the server we lost itself
	 */
	exit_one_client(cptr, sptr, from, comment);
	return cptr == sptr ? FLUSH_BUFFER : 0;
    }

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
/* DANGER: Ugly hack follows. */
/* Yeah :/ */
static	void	exit_one_client_backend(cptr, sptr, from, comment, split)
aClient *sptr;
aClient *cptr;
aClient *from;
char	*comment;
int	split;
{
	Reg1	aClient *acptr;
	Reg2	int	i;
	Reg3	Link	*lp;

	/*
	**  For a server or user quitting, propagage the information to
	**  other servers (except to the one where is came from (cptr))
	*/
	if (IsMe(sptr))
	    {
		sendto_ops("ERROR: tried to exit me! : %s", comment);
		return;	/* ...must *never* exit self!! */
	    }
	else if (IsServer(sptr)) {
	 /*
	 ** Old sendto_serv_but_one() call removed because we now
	 ** need to send different names to different servers
	 ** (domain name matching)
	 */
	 	for (i = 0; i <= highest_fd; i++)
		    {
			Reg4	aConfItem *aconf;

			if (!(acptr = local[i]) || !IsServer(acptr) ||
			    acptr == cptr || IsMe(acptr))
				continue;
			if ((aconf = acptr->serv->nline) &&
			    (match(my_name_for_link(me.name, aconf),
				     sptr->name) == 0))
				continue;
			/*
			** SQUIT going "upstream". This is the remote
			** squit still hunting for the target. Use prefixed
			** form. "from" will be either the oper that issued
			** the squit or some server along the path that
			** didn't have this fix installed. --msa
			*/
			if (sptr->from == acptr)
			    {
				sendto_one(acptr, ":%s SQUIT %s :%s",
					   from->name, sptr->name, comment);
#ifdef	USE_SERVICES
				check_services_butone(SERVICE_WANT_SQUIT, sptr,
							":%s SQUIT %s :%s",
							from->name,
							sptr->name, comment);
#endif
			    }
			else
			    {
				sendto_one(acptr, "SQUIT %s :%s",
					   sptr->name, comment);
#ifdef	USE_SERVICES
				check_services_butone(SERVICE_WANT_SQUIT, sptr,
							"SQUIT %s :%s",
							sptr->name, comment);
#endif
			    }
	    }
	} else if (!(IsPerson(sptr) || IsService(sptr)))
				    /* ...this test is *dubious*, would need
				    ** some thougth.. but for now it plugs a
				    ** nasty hole in the server... --msa
				    */
		; /* Nothing */
	else if (sptr->name[0]) /* ...just clean all others with QUIT... */
	    {
		/*
		** If this exit is generated from "m_kill", then there
		** is no sense in sending the QUIT--KILL's have been
		** sent instead.
		*/
		if ((sptr->flags & FLAGS_KILLED) == 0)
		    {
			if (split == 0)
			sendto_serv_butone(cptr,":%s QUIT :%s",
					   sptr->name, comment);
			else
				/*
				 * Then this is a split, only old (stupid)
				 * clients need to get quit messages
				 */
				sendto_serv_butone_quit(cptr, ":%s QUIT :%s",
					sptr->name, comment);
#ifdef	USE_SERVICES
			check_services_butone(SERVICE_WANT_QUIT,
						":%s QUIT :%s", sptr->name,
						comment);
#endif
		    }
		/*
		** If a person is on a channel, send a QUIT notice
		** to every client (person) on the same channel (so
		** that the client can show the "**signoff" message).
		** (Note: The notice is to the local clients *only*)
		*/
		if (sptr->user)
		    {
			sendto_common_channels(sptr, ":%s QUIT :%s",
						sptr->name, comment);

			while ((lp = sptr->user->channel))
				remove_user_from_channel(sptr,lp->value.chptr);

			/* Clean up invitefield */
			while ((lp = sptr->user->invited))
				del_invite(sptr, lp->value.chptr);
				/* again, this is all that is needed */

			/* Clean up silencefield */
			while ((lp = sptr->user->silence))
				(void)del_silence(sptr, lp->value.cp);
		    }
	    }

	/* Remove sptr from the client list */
	if (del_from_client_hash_table(sptr->name, sptr) != 1)
		Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
			sptr, sptr->name,
			sptr->from ? sptr->from->sockhost : "??host",
			sptr->from, sptr->next, sptr->prev, sptr->fd,
			sptr->status, sptr->user));
	if (IsRegisteredUser(sptr))
		hash_check_notify(sptr, RPL_LOGOFF);
	remove_client_from_list(sptr);
	return;
}

static	void	exit_one_client(cptr, sptr, from, comment)
aClient	*sptr, *cptr, *from;
char	*comment;
{
	exit_one_client_backend(cptr, sptr, from, comment, 0);
}

static	void	exit_one_client_in_split(cptr, sptr, from, comment)
aClient	*sptr, *cptr, *from;
char	*comment;
{
	exit_one_client_backend(cptr, sptr, from, comment, 1);
}


void	checklist()
{
	Reg1	aClient	*acptr;
	Reg2	int	i,j;

	if (!(bootopt & BOOT_AUTODIE))
		return;
	for (j = i = 0; i <= highest_fd; i++)
		if (!(acptr = local[i]))
			continue;
		else if (IsClient(acptr))
			j++;
	if (!j)
	    {
#ifdef	USE_SYSLOG
		syslog(LOG_WARNING,"ircd exiting: autodie");
#endif
		exit(0);
	    }
	return;
}

void	initstats()
{
	bzero((char *)&ircst, sizeof(ircst));
}

void	tstats(cptr, name)
aClient	*cptr;
char	*name;
{
	Reg1	aClient	*acptr;
	Reg2	int	i;
	Reg3	struct stats	*sp;
	struct	stats	tmp;
	time_t	now = time(NULL);

	sp = &tmp;
	bcopy((char *)ircstp, (char *)sp, sizeof(*sp));
#ifndef _WIN32
	for (i = 0; i < MAXCONNECTIONS; i++)
#else
	for (i = 0; i < highest_fd; i++)
#endif
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsServer(acptr))
		    {
			sp->is_sbs += acptr->sendB;
			sp->is_sbr += acptr->receiveB;
			sp->is_sks += acptr->sendK;
			sp->is_skr += acptr->receiveK;
			sp->is_sti += now - acptr->firsttime;
			sp->is_sv++;
			if (sp->is_sbs > 1023)
			    {
				sp->is_sks += (sp->is_sbs >> 10);
				sp->is_sbs &= 0x3ff;
			    }
			if (sp->is_sbr > 1023)
			    {
				sp->is_skr += (sp->is_sbr >> 10);
				sp->is_sbr &= 0x3ff;
			    }
		    }
		else if (IsClient(acptr))
		    {
			sp->is_cbs += acptr->sendB;
			sp->is_cbr += acptr->receiveB;
			sp->is_cks += acptr->sendK;
			sp->is_ckr += acptr->receiveK;
			sp->is_cti += now - acptr->firsttime;
			sp->is_cl++;
			if (sp->is_cbs > 1023)
			    {
				sp->is_cks += (sp->is_cbs >> 10);
				sp->is_cbs &= 0x3ff;
			    }
			if (sp->is_cbr > 1023)
			    {
				sp->is_ckr += (sp->is_cbr >> 10);
				sp->is_cbr &= 0x3ff;
			    }
		    }
		else if (IsUnknown(acptr))
			sp->is_ni++;
	    }

	sendto_one(cptr, ":%s %d %s :accepts %u refused %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_ac, sp->is_ref);
	sendto_one(cptr, ":%s %d %s :unknown commands %u prefixes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_unco, sp->is_unpf);
	sendto_one(cptr, ":%s %d %s :nick collisions %u unknown closes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_kill, sp->is_ni);
	sendto_one(cptr, ":%s %d %s :wrong direction %u empty %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_wrdi, sp->is_empt);
	sendto_one(cptr, ":%s %d %s :numerics seen %u mode fakes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_num, sp->is_fake);
	sendto_one(cptr, ":%s %d %s :auth successes %u fails %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_asuc, sp->is_abad);
	sendto_one(cptr, ":%s %d %s :local connections %u udp packets %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_loc, sp->is_udp);
	sendto_one(cptr, ":%s %d %s :Client Server",
		   me.name, RPL_STATSDEBUG, name);
	sendto_one(cptr, ":%s %d %s :connected %u %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_cl, sp->is_sv);
	sendto_one(cptr, ":%s %d %s :bytes sent %u.%uK %u.%uK",
		   me.name, RPL_STATSDEBUG, name,
		   sp->is_cks, sp->is_cbs, sp->is_sks, sp->is_sbs);
	sendto_one(cptr, ":%s %d %s :bytes recv %u.%uK %u.%uK",
		   me.name, RPL_STATSDEBUG, name,
		   sp->is_ckr, sp->is_cbr, sp->is_skr, sp->is_sbr);
	sendto_one(cptr, ":%s %d %s :time connected %u %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_cti, sp->is_sti);
}
