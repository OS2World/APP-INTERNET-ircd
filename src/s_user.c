/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_user.c (formerly ircd/s_msg.c)
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
static  char sccsid[] = "@(#)s_user.c	2.74 2/8/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include <sys/stat.h>
#if !defined _WIN32 && !defined __EMX__
#include <utmp.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

void	send_umode_out PROTO((aClient*, aClient *, int));
void	send_svsmode_out PROTO((aClient*, aClient *, aClient *, int));
void	send_umode PROTO((aClient *, aClient *, int, int, char *));
static	is_silenced PROTO((aClient *, aClient *));
/* static  Link    *is_banned PROTO((aClient *, aChannel *)); */

static char buf[BUFSIZE], buf2[BUFSIZE];

/*
** m_functions execute protocol messages on this server:
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the m_function to be
**		executed--some m_functions may call others...).
**
**	sptr	is the source of the message, defined by the
**		prefix part of the message if present. If not
**		or prefix not found, then sptr==cptr.
**
**		(!IsServer(cptr)) => (cptr == sptr), because
**		prefixes are taken *only* from servers...
**
**		(IsServer(cptr))
**			(sptr == cptr) => the message didn't
**			have the prefix.
**
**			(sptr != cptr && IsServer(sptr) means
**			the prefix specified servername. (?)
**
**			(sptr != cptr && !IsServer(sptr) means
**			that message originated from a remote
**			user (not local).
**
**		combining
**
**		(!IsServer(sptr)) means that, sptr can safely
**		taken as defining the target structure of the
**		message in this server.
**
**	*Always* true (if 'parse' and others are working correct):
**
**	1)	sptr->from == cptr  (note: cptr->from == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->from) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[0], sender (prefix string), if not present
**				this points to an empty string.
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[0]..parv[parc-1] are all
**			non-NULL pointers.
*/

/*
** next_client
**	Local function to find the next matching client. The search
**	can be continued from the specified client entry. Normal
**	usage loop is:
**
**	for (x = client; x = next_client(x,mask); x = x->next)
**		HandleMatchingClient;
**	      
*/
aClient *next_client(next, ch)
Reg1	aClient *next;	/* First client to check */
Reg2	char	*ch;	/* search string (may include wilds) */
{
	Reg3	aClient	*tmp = next;

	next = find_client(ch, tmp);
	if (tmp && tmp->prev == next)
		return NULL;
	if (next != tmp)
		return next;
	for ( ; next; next = next->next)
	    {
		if (IsService(next))
			continue;
		if (!match(ch, next->name) || !match(next->name, ch))
			break;
	    }
	return next;
}

/*
** hunt_server
**
**	Do the basic thing in delivering the message (command)
**	across the relays to the specific server (server) for
**	actions.
**
**	Note:	The command is a format string and *MUST* be
**		of prefixed style (e.g. ":%s COMMAND %s ...").
**		Command can have only max 8 parameters.
**
**	server	parv[server] is the parameter identifying the
**		target server.
**
**	*WARNING*
**		parv[server] is replaced with the pointer to the
**		real servername from the matched client (I'm lazy
**		now --msa).
**
**	returns: (see #defines)
*/
int	hunt_server(cptr, sptr, command, server, parc, parv)
aClient	*cptr, *sptr;
char	*command, *parv[];
int	server, parc;
    {
	aClient *acptr;

	/*
	** Assume it's me, if no server
	*/
	if (parc <= server || BadPtr(parv[server]) ||
	    match(me.name, parv[server]) == 0 ||
	    match(parv[server], me.name) == 0)
		return (HUNTED_ISME);
	/*
	** These are to pickup matches that would cause the following
	** message to go in the wrong direction while doing quick fast
	** non-matching lookups.
	*/
	if ((acptr = find_client(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	if (!acptr && (acptr = find_server(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	if (!acptr)
		for (acptr = client, (void)collapse(parv[server]);
		     (acptr = next_client(acptr, parv[server]));
		     acptr = acptr->next)
		    {
			if (acptr->from == sptr->from && !MyConnect(acptr))
				continue;
			/*
			 * Fix to prevent looping in case the parameter for
			 * some reason happens to match someone from the from
			 * link --jto
			 */
			if (IsRegistered(acptr) && (acptr != cptr))
				break;
		    }
	 if (acptr)
	    {
		if (IsMe(acptr) || MyClient(acptr))
			return HUNTED_ISME;
		if (match(acptr->name, parv[server]))
			parv[server] = acptr->name;
		sendto_one(acptr, command, parv[0],
			   parv[1], parv[2], parv[3], parv[4],
			   parv[5], parv[6], parv[7], parv[8]);
		return(HUNTED_PASS);
	    } 
	sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name,
		   parv[0], parv[server]);
	return(HUNTED_NOSUCH);
    }

/*
** check_for_target_limit
**
** Return Values:
** True(1) == too many targets are addressed
** False(0) == ok to send message
**
*/
int check_for_target_limit(aClient *sptr, void *target, const char *name)
{
	register u_char *p;
	register u_int tmp = ((u_int)target & 0xffff00) >> 8;

	u_char hash = (tmp * tmp) >> 12;

	if(IsAnOper(sptr))
		return 0;
	if (sptr->targets[0] == hash)
		return 0;

	for (p = sptr->targets; p < &sptr->targets[MAXTARGETS - 1];)
		if (*++p == hash) {
			memmove(&sptr->targets[1], &sptr->targets[0], p - sptr->targets);
			sptr->targets[0] = hash;
			return 0;
		}

	if (now < sptr->nexttarget) {
		if (sptr->nexttarget - now < TARGET_DELAY + 8) {
			sptr->nexttarget += 2;
			sendto_one(sptr, err_str(ERR_TARGETTOOFAST),
				me.name, sptr->name, name, sptr->nexttarget - now);
		}
	return 1;
	} else {
		sptr->nexttarget += TARGET_DELAY;
		if (sptr->nexttarget < now - (TARGET_DELAY * (MAXTARGETS - 1)))
			sptr->nexttarget = now - (TARGET_DELAY * (MAXTARGETS - 1));
	}
	memmove(&sptr->targets[1], &sptr->targets[0], MAXTARGETS - 1);
	sptr->targets[0] = hash;   
	return 0;
}


    

/*
** 'do_nick_name' ensures that the given parameter (nick) is
** really a proper string for a nickname (note, the 'nick'
** may be modified in the process...)
**
**	RETURNS the length of the final NICKNAME (0, if
**	nickname is illegal)
**
**  Nickname characters are in range
**	'A'..'}', '_', '-', '0'..'9'
**  anything outside the above set will terminate nickname.
**  In addition, the first character cannot be '-'
**  or a Digit.
**
**  Note:
**	'~'-character should be allowed, but
**	a change should be global, some confusion would
**	result if only few servers allowed it...
*/

int	do_nick_name(nick)
char	*nick;
{
	Reg1 char *ch;

	if (*nick == '-' || isdigit(*nick)) /* first character in [0..9-] */
		return 0;

	for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
		if (!isvalid(*ch) || isspace(*ch))
			break;

	*ch = '\0';

	return (ch - nick);
}


/*
** canonize
**
** reduce a string of duplicate list entries to contain only the unique
** items.  Unavoidably O(n^2).
*/
char	*canonize(buffer)
char	*buffer;
{
	static	char	cbuf[BUFSIZ];
	register char	*s, *t, *cp = cbuf;
	register int	l = 0;
	char	*p = NULL, *p2;

	*cp = '\0';

	for (s = strtoken(&p, buffer, ","); s; s = strtoken(&p, NULL, ","))
	    {
		if (l)
		    {
			for (p2 = NULL, t = strtoken(&p2, cbuf, ","); t;
			     t = strtoken(&p2, NULL, ","))
				if (!mycmp(s, t))
					break;
				else if (p2)
					p2[-1] = ',';
		    }
		else
			t = NULL;
		if (!t)
		    {
			if (l)
				*(cp-1) = ',';
			else
				l = 1;
			(void)strcpy(cp, s);
			if (p)
				cp += (p - s);
		    }
		else if (p2)
			p2[-1] = ',';
	    }
	return cbuf;
}


/*
** register_user
**	This function is called when both NICK and USER messages
**	have been accepted for the client, in whatever order. Only
**	after this the USER message is propagated.
**
**	NICK's must be propagated at once when received, although
**	it would be better to delay them too until full info is
**	available. Doing it is not so simple though, would have
**	to implement the following:
**
**	1) user telnets in and gives only "NICK foobar" and waits
**	2) another user far away logs in normally with the nick
**	   "foobar" (quite legal, as this server didn't propagate
**	   it).
**	3) now this server gets nick "foobar" from outside, but
**	   has already the same defined locally. Current server
**	   would just issue "KILL foobar" to clean out dups. But,
**	   this is not fair. It should actually request another
**	   nick from local user or kill him/her...
*/

static	int	register_user(cptr, sptr, nick, username)
aClient	*cptr;
aClient	*sptr;
char	*nick, *username;
{
	Reg1	aConfItem *aconf;
        char        *parv[3], *tmpstr, c;
#ifdef HOSTILENAME
	char	stripuser[USERLEN+1], *u1=stripuser, *u2, olduser[USERLEN+1],
		userbad[USERLEN*2+1], *ubad=userbad, noident=0;
#endif
        short   oldstatus = sptr->status, upper = 0, lower = 0, special = 0;
	anUser	*user = sptr->user;
	aClient *nsptr;
	int	i;

	user->last = time(NULL);
	parv[0] = sptr->name;
	parv[1] = parv[2] = NULL;

	if (MyConnect(sptr))
	    {
		if ((i = check_client(sptr)))
		    {
			sendto_umode(UMODE_OPER|UMODE_CLIENT,
				   "*** Notice -- %s from %s.",
				   i == -3 ? "Too many connections" :
					     "Unauthorized connection",
				   get_client_host(sptr));
			ircstp->is_ref++;
			return exit_client(cptr, sptr, &me, i == -3 ?
			   "This server is full.  Please try irc.dal.net" :
			   "You are not authorized to connect to this server");
		      } 
		if (IsUnixSocket(sptr))
			strncpyzt(user->host, me.sockhost, sizeof(user->host));
		else if (sptr->hostp) {
			/* No control-chars or ip-like dns replies... I cheat :)
			   -- OnyxDragon */
			for (tmpstr = sptr->sockhost; *tmpstr > ' ' &&
				*tmpstr < 127; tmpstr++);
			if (*tmpstr || !*user->host || isdigit(*(tmpstr-1)))
				strncpyzt(sptr->sockhost, (char *)inetntoa((char *)&sptr->ip), sizeof(sptr->sockhost)); /* Fix the sockhost for debug jic */
			strncpyzt(user->host, sptr->sockhost, sizeof(sptr->sockhost));
		}
		else /* Failsafe point, don't let the user define their
		        own hostname via the USER command --Cabal95 */
			strncpyzt(user->host, sptr->sockhost, HOSTLEN+1);
		aconf = sptr->confs->value.aconf;
		/*
		 * I do not consider *, ~ or ! 'hostile' in usernames,
		 * as it is easy to differentiate them (Use \*, \? and \\)
		 * with the possible?
		 * exception of !. With mIRC etc. ident is easy to fake
		 * to contain @ though, so if that is found use non-ident
		 * username. -Donwulff
		 *
		 * I do, We only allow a-z A-Z 0-9 _ - and . now so the
		 * !strchr(sptr->username, '@') check is out of date. -Cabal95
		 *
		 * Moved the noident stuff here. -OnyxDragon
		 */
		if (!(sptr->flags & FLAGS_DOID))
			strncpyzt(user->username, username, USERLEN+1);
		else if (sptr->flags & FLAGS_GOTID)
			strncpyzt(user->username, sptr->username, USERLEN+1);
		else
		    {
			/* because username may point to user->username */
			char	temp[USERLEN+1];

			strncpyzt(temp, username, USERLEN+1);
			*user->username = '~';
			(void)strncpy(&user->username[1], temp, USERLEN);
			user->username[USERLEN] = '\0';
#ifdef HOSTILENAME
			noident = 1;
#endif
		    }
#ifdef HOSTILENAME
		/*
		 * Limit usernames to just 0-9 a-z A-Z _ - and .
		 * It strips the "bad" chars out, and if nothing is left
		 * changes the username to the first 8 characters of their
		 * nickname. After the MOTD is displayed it sends numeric
		 * 455 to the user telling them what(if anything) happened.
		 * -Cabal95
		 *
		 * Moved the noident thing to the right place - see above
		 * -OnyxDragon
		 */
		for (u2 = user->username+noident; *u2; u2++)
		    {
			if (isallowed(*u2))
				*u1++ = *u2;
			else if (*u2 < 32)
			    {
				/*
				 * Make sure they can read what control
				 * characters were in their username.
				 */
				*ubad++ = '^';
				*ubad++ = *u2+'@';
			    }
			else
				*ubad++ = *u2;
		    }
		*u1 = '\0';
		*ubad = '\0';
		if (strlen(stripuser) != strlen(user->username+noident))
		    {
			if (stripuser[0] == '\0')
			    {
				strncpy(stripuser, cptr->name, 8);
				stripuser[8] = '\0';
			    }

			strcpy(olduser, user->username+noident);
			strncpy(user->username+1, stripuser, USERLEN-1);
			user->username[0] = '~';
			user->username[USERLEN] = '\0';
		    }
		else
			u1 = NULL;
#endif

		if (!BadPtr(aconf->passwd)) {
			if (!StrEq(sptr->passwd, aconf->passwd)) {
				ircstp->is_ref++;
				sendto_one(sptr, err_str(ERR_PASSWDMISMATCH),
					   me.name, parv[0]);
				return exit_client(cptr, sptr, &me, "Bad Password");
			}
			/* .. Else password check was successful, clear the pass
			 * so it doesn't get sent to NickServ.
			 * - Wizzu
			 */
			else sptr->passwd[0] = '\0';
		}

		/*
		 * following block for the benefit of time-dependent K:-lines
		 */
		if (find_kill(sptr))
		    {
			ircstp->is_ref++;
			return exit_client(cptr, sptr, &me, "K-lined");
		    }
#ifdef R_LINES
		if (find_restrict(sptr))
		    {
			ircstp->is_ref++;
			return exit_client(cptr, sptr, &me , "R-lined");
		    }
#endif

#ifdef                DISALLOW_MIXED_CASE
/* check for mixed case usernames, meaning probably hacked   Jon2 3-94
*/
#ifdef        IGNORE_CASE_FIRST_CHAR
              tmpstr = (username[0] == '~' ? &username[2] : &username[1]);
#else
              tmpstr = (username[0] == '~' ? &username[1] : username);
#endif        /* IGNORE_CASE_FIRST_CHAR */
             while (*tmpstr && !(lower && upper || special)) {
               c = *tmpstr;
               tmpstr++;
               if (islower(c)) {
                 lower++;
                 continue; /* bypass rest of tests */
               }
               if (isupper(c)) {
                 upper++;
                 continue;
               }
               if (c == '-' || c == '_' || c == '.' || isdigit(c))
                 continue;
               special++;
             }
             if (lower && upper || special) {
                   sendto_ops("Invalid username: %s",
                              get_client_name(sptr,FALSE));
                   ircstp->is_ref++;
                   return exit_client(cptr, sptr, sptr , "Invalid username");
             }

#endif                /* DISALLOW_MIXED_CASE */

		if (oldstatus == STAT_MASTER && MyConnect(sptr))
			(void)m_oper(&me, sptr, 1, parv);
	    }
	else
		strncpyzt(user->username, username, USERLEN+1);
	SetClient(sptr);
	if (MyConnect(sptr))
	    {
		sendto_one(sptr, rpl_str(RPL_WELCOME), me.name, nick, nick,
			   user->username, user->host);
		/* This is a duplicate of the NOTICE but see below...*/
		sendto_one(sptr, rpl_str(RPL_YOURHOST), me.name, nick,
			   get_client_name(&me, FALSE), version);
#ifdef	IRCII_KLUDGE
		/*
		** Don't mess with this one - IRCII needs it! -Avalon
		*/
		sendto_one(sptr,
			"NOTICE %s :*** Your host is %s, running version %s",
			nick, get_client_name(&me, FALSE), version);
#endif
		sendto_one(sptr, rpl_str(RPL_CREATED),me.name,nick,creation);
		sendto_one(sptr, rpl_str(RPL_MYINFO), me.name, parv[0],
			   me.name, version);
		sendto_one(sptr, rpl_str(RPL_PROTOCTL), me.name, parv[0],
			   PROTOCTL_SUPPORTED);
		(void)m_lusers(sptr, sptr, 1, parv);
		update_load();
		(void)m_motd(sptr, sptr, 1, parv);
#ifdef HOSTILENAME
		/*
		 * Now send a numeric to the user telling them what, if
		 * anything, happened.
		 */
		if (u1)
			sendto_one(sptr, err_str(ERR_HOSTILENAME), me.name,
				   sptr->name, olduser, userbad, stripuser);
#endif
		nextping = time(NULL);
		sendto_umode(UMODE_OPER|UMODE_CLIENT,"*** Notice -- Client connecting on port %d: %s (%s@%s)", 
			       sptr->acpt->port, nick, user->username,
			       user->host);
	    }
	else if (IsServer(cptr))
	    {
		aClient	*acptr;

		if (!(acptr = find_server(user->server, NULL)))
		{
		  sendto_ops("Bad USER [%s] :%s USER %s %s : No such server",
		      cptr->name, nick, user->username, user->server);
		  sendto_one(cptr, ":%s KILL %s :%s (No such server: %s)",
		      me.name, sptr->name, me.name, user->server);
		  sptr->flags |= FLAGS_KILLED;
		  return exit_client(sptr, sptr, &me,
		      "USER without prefix(2.8) or wrong prefix");
		 }
		 else if (acptr->from != sptr->from)
		   {
			sendto_ops("Bad User [%s] :%s USER %s %s, != %s[%s]",
				cptr->name, nick, user->username, user->server,
				acptr->name, acptr->from->name);
			sendto_one(cptr, ":%s KILL %s :%s (%s != %s[%s])",
				   me.name, sptr->name, me.name, user->server,
				   acptr->from->name, acptr->from->sockhost);
			sptr->flags |= FLAGS_KILLED;
			return exit_client(sptr, sptr, &me,
					   "USER server wrong direction");
		   }
		else sptr->flags|=(acptr->flags & FLAGS_TS8);
		/* *FINALL* this gets in ircd... -- Barubary */
		if (find_conf_host(cptr->confs, sptr->name, CONF_UWORLD)
		    || (sptr->user && find_conf_host(cptr->confs,
		    sptr->user->server, CONF_UWORLD)))
			sptr->flags |= FLAGS_ULINE;
	    }

	hash_check_notify(sptr, RPL_LOGON);	/* Uglier hack */
	sendto_serv_butone(cptr, "NICK %s %d %d %s %s %s %lu :%s", nick,
	  sptr->hopcount+1, sptr->lastnick, user->username, user->host,
	  user->server, sptr->user->servicestamp, sptr->info);

	/* Send password from sptr->passwd to NickServ for identification,
	 * if passwd given and if NickServ is online.
	 * - by taz, modified by Wizzu
	 */
	if (MyConnect(sptr)) {
	    send_umode_out(cptr, sptr, 0);
	    if (sptr->passwd[0] && (nsptr = find_person(NickServ, NULL)))
		sendto_one(nsptr,":%s PRIVMSG %s@%s :SIDENTIFY %s",
		  sptr->name, NickServ, SERVICES_NAME, sptr->passwd);
	}

#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_NICK, sptr,
	  "NICK %s %d %d %s %s %s %lu :%s", nick, sptr->hopcount,
	  sptr->lastnick, user->username, user->host, user->server,
	  sptr->user->servicestamp, sptr->info);
	check_services_butone(SERVICE_WANT_USER, sptr, ":%s USER %s %s %s :%s",
	  nick, user->username, user->host, user->server, sptr->info);
#endif
	if(MyConnect(sptr) && !BadPtr(sptr->passwd))
		bzero(sptr->passwd, sizeof(sptr->passwd));

	return 0;
    }

/*
** m_svsnick
**	parv[0] = sender
**	parv[1] = old nickname
**	parv[2] = new nickname
**	parv[3] = timestamp
*/
int	m_svsnick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	nick[NICKLEN+2], *s;
	time_t	lastnick = (time_t)0;

	if (!IsULine(cptr, sptr) || parc < 4 || (strlen(parv[2]) > NICKLEN))
		return;

	if (!hunt_server(cptr, sptr, ":%s SVSNICK %s %s :%s", 1, parc, parv) != HUNTED_ISME) {
		if ((acptr = find_person(parv[1], NULL))) {
			acptr->umodes &= ~UMODE_REGNICK;
			acptr->lastnick = atoi(parv[3]);
			sendto_common_channels(acptr, ":%s NICK :%s", parv[1], parv[2]);
			if (IsPerson(acptr))
				add_history(acptr);
			sendto_serv_butone(NULL, ":%s NICK %s :%i", parv[1], parv[2], atoi(parv[3]));
			if(acptr->name[0])
				(void)del_from_client_hash_table(acptr->name, acptr);
			(void)strcpy(acptr->name, parv[2]);
			(void)add_to_client_hash_table(parv[2], acptr);
		}
	}
}

/*
** m_nick
**	parv[0] = sender prefix
**	parv[1] = nickname
**  if from new client  -taz
**	parv[2] = nick password
**  if from server:
**      parv[2] = hopcount
**      parv[3] = timestamp
**      parv[4] = username
**      parv[5] = hostname
**      parv[6] = servername
**      parv[7] = servicestamp
**	parv[8] = info
*/
int	m_nick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aConfItem *aconf;
	aSqlineItem *asqline;
	aClient *acptr, *serv;
	char	nick[NICKLEN+2], *s;
	Link	*lp;
	time_t	lastnick = (time_t)0;
	int	differ = 1;

        static int firstnsrun=0;
	u_int32_t     md5data[16];
	static u_int32_t     md5hash[4];

	/*
	 * If the user didn't specify a nickname, complain
	 */
	if (parc < 2) {
	  sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
		     me.name, parv[0]);
	  return 0;
	}

	if (MyConnect(sptr) && (s = (char *)index(parv[1], '~')))
	  *s = '\0';

	strncpyzt(nick, parv[1], NICKLEN+1);
	/*
	 * if do_nick_name() returns a null name OR if the server sent a nick
	 * name and do_nick_name() changed it in some way (due to rules of nick
	 * creation) then reject it. If from a server and we reject it,
	 * and KILL it. -avalon 4/4/92
	 */
	if (do_nick_name(nick) == 0 ||
	    (IsServer(cptr) && strcmp(nick, parv[1]))) {
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME),
			   me.name, parv[0], parv[1], "Illegal characters");

		if (IsServer(cptr)) {
			ircstp->is_kill++;
			sendto_ops("Bad Nick: %s From: %s %s",
				   parv[1], parv[0],
				   get_client_name(cptr, FALSE));
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
				   me.name, parv[1], me.name, parv[1],
				   nick, cptr->name);
			if (sptr != cptr) { /* bad nick change */
				sendto_serv_butone(cptr,
					":%s KILL %s :%s (%s <- %s!%s@%s)",
					me.name, parv[0], me.name,
					get_client_name(cptr, FALSE),
					parv[0],
					sptr->user ? sptr->username : "",
					sptr->user ? sptr->user->server :
						     cptr->name);
				sptr->flags |= FLAGS_KILLED;
				return exit_client(cptr,sptr,&me,"BadNick");
			    }
		    }
		return 0;
	    }

	/*
	** Protocol 4 doesn't send the server as prefix, so it is possible
	** the server doesn't exist (a lagged net.burst), in which case
	** we simply need to ignore the NICK. Also when we got that server
	** name (again) but from another direction. --Run
	*/
	/* 
	** We should really only deal with this for msgs from servers.
	** -- Aeto
	*/
	if (IsServer(cptr) && 
	    (parc > 7 && (!(serv = find_server(parv[6], NULL)) ||
			  serv->from != cptr->from)))
	  return 0;

	/*
	** Check against nick name collisions.
	**
	** Put this 'if' here so that the nesting goes nicely on the screen :)
	** We check against server name list before determining if the nickname
	** is present in the nicklist (due to the way the below for loop is
	** constructed). -avalon
	*/
	if ((acptr = find_server(nick, NULL))) {
		if (MyConnect(sptr))
		    {
			sendto_one(sptr, err_str(ERR_NICKNAMEINUSE), me.name,
				   BadPtr(parv[0]) ? "*" : parv[0], nick);
			return 0; /* NICK message ignored */
		    }
	}

	/*
	** Check for a Q-lined nickname. If we find it, and it's our
	** client, just reject it. -Lefler
	** Allow opers to use Q-lined nicknames. -Russell
	*/

	if ((aconf = find_conf_name(nick, CONF_QUARANTINED_NICK)) || 
		(asqline = find_sqline_match(nick))) {
	  sendto_realops ("Q-lined nick %s from %s on %s.", nick,
		      (*sptr->name!=0 && !IsServer(sptr)) ? sptr->name :
		       "<unregistered>",
		      (sptr->user==NULL) ? ((IsServer(sptr)) ? parv[6] : me.name) :
					   sptr->user->server);
	  if ((!IsServer(cptr)) && (!IsOper(cptr))) {

	    if(aconf)
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME), me.name,
			   BadPtr(parv[0]) ? "*" : parv[0], nick,
			   BadPtr(aconf->passwd) ? "reason unspecified" :
			   aconf->passwd);
	    else if(asqline)
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME), me.name,
			   BadPtr(parv[0]) ? "*" : parv[0], nick,
			   BadPtr(asqline->reason) ? "reason unspecified" :
			   asqline->reason);

		sendto_realops("Forbidding Q-lined nick %s from %s.",
			nick, get_client_name(cptr, FALSE));
		return 0; /* NICK message ignored */
	  }
	}

	/*
	** acptr already has result from previous find_server()
	*/
	if (acptr)
	    {
		/*
		** We have a nickname trying to use the same name as
		** a server. Send out a nick collision KILL to remove
		** the nickname. As long as only a KILL is sent out,
		** there is no danger of the server being disconnected.
		** Ultimate way to jupiter a nick ? >;-). -avalon
		*/
		sendto_ops("Nick collision on %s(%s <- %s)",
			   sptr->name, acptr->from->name,
			   get_client_name(cptr, FALSE));
		ircstp->is_kill++;
		sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
			   me.name, sptr->name, me.name, acptr->from->name,
			   /* NOTE: Cannot use get_client_name
			   ** twice here, it returns static
			   ** string pointer--the other info
			   ** would be lost
			   */
			   get_client_name(cptr, FALSE));
		sptr->flags |= FLAGS_KILLED;
		return exit_client(cptr, sptr, &me, "Nick/Server collision");
	    }
	if (MyClient(cptr) && !IsOper(cptr))
		cptr->since += 3; /* Nick-flood prot. -Donwulff */

	if (!(acptr = find_client(nick, NULL)))
		goto nickkilldone;  /* No collisions, all clear... */
	/*
	** If the older one is "non-person", the new entry is just
	** allowed to overwrite it. Just silently drop non-person,
	** and proceed with the nick. This should take care of the
	** "dormant nick" way of generating collisions...
	*/
	/* Moved before Lost User Field to fix some bugs... -- Barubary */
	if (IsUnknown(acptr) && MyConnect(acptr))
	    {
		/* This may help - copying code below */
		if (acptr == cptr)
			return 0;
		acptr->flags |= FLAGS_KILLED;
		exit_client(NULL, acptr, &me, "Overridden");
		goto nickkilldone;
	    }
	/* A sanity check in the user field... */
	if (acptr->user == NULL) {
	  /* This is a Bad Thing */
	  sendto_ops("Lost user field for %s in change from %s",
		     acptr->name, get_client_name(cptr,FALSE));
	  ircstp->is_kill++;
	  sendto_one(acptr, ":%s KILL %s :%s (Lost user field!)",
		     me.name, acptr->name, me.name);
	  acptr->flags |= FLAGS_KILLED;
	  /* Here's the previous versions' desynch.  If the old one is
	     messed up, trash the old one and accept the new one.
	     Remember - at this point there is a new nick coming in!
	     Handle appropriately. -- Barubary */
	  exit_client (NULL, acptr, &me, "Lost user field");
	  goto nickkilldone;
	}
	/*
	** If acptr == sptr, then we have a client doing a nick
	** change between *equivalent* nicknames as far as server
	** is concerned (user is changing the case of his/her
	** nickname or somesuch)
	*/
	if (acptr == sptr)
	  if (strcmp(acptr->name, nick) != 0)
	    /*
	    ** Allows change of case in his/her nick
	    */
	    goto nickkilldone; /* -- go and process change */
	  else
	    /*
	    ** This is just ':old NICK old' type thing.
	    ** Just forget the whole thing here. There is
	    ** no point forwarding it to anywhere,
	    ** especially since servers prior to this
	    ** version would treat it as nick collision.
	    */
	    return 0; /* NICK Message ignored */
	/*
	** Note: From this point forward it can be assumed that
	** acptr != sptr (point to different client structures).
	*/
	/*
	** Decide, we really have a nick collision and deal with it
	*/
	if (!IsServer(cptr))
	    {
		/*
		** NICK is coming from local client connection. Just
		** send error reply and ignore the command.
		*/
		sendto_one(sptr, err_str(ERR_NICKNAMEINUSE),
			   /* parv[0] is empty when connecting */
			   me.name, BadPtr(parv[0]) ? "*" : parv[0], nick);
		return 0; /* NICK message ignored */
	    }
	/*
	** NICK was coming from a server connection.
	** This means we have a race condition (two users signing on
	** at the same time), or two net fragments reconnecting with
	** the same nick.
	** The latter can happen because two different users connected
	** or because one and the same user switched server during a
	** net break.
	** If we have the old protocol (no TimeStamp and no user@host)
	** or if the TimeStamps are equal, we kill both (or only 'new'
	** if it was a "NICK new"). Otherwise we kill the youngest
	** when user@host differ, or the oldest when they are the same.
	** --Run
	** 
	*/
	if (IsServer(sptr))
	{
#ifdef USE_CASETABLES
	  /* OK.  Here's what we know now...  (1) acptr points to
	  ** someone or something using the nick we want to add,
	  ** (2) we are getting the nick request from a server, not
	  ** a local client.  What we need to figure out is whether
	  ** this is a user changing the case on its nick or a server
	  ** with a different set of case tables trying to introduce
	  ** a nick it thinks is unique, but we don't.  What we are going
	  ** to do is see if the acptr == find_client(parv[0]).
	  **    -Aeto 
	  */
	  /* Have to make sure it's not a server else we go back to the old
	     "kill both" situation.  Removed for now. -- Barubary */
	  if ((find_client(parv[0],NULL) != acptr) && !IsServer(find_client(
		parv[0],NULL)))
	  {
		sendto_one(sptr, ":%s KILL %s :%s (%s Self collision)",
			   me.name, nick, me.name,
			   /* NOTE: Cannot use get_client_name
			   ** twice here, it returns static
			   ** string pointer--the other info
			   ** would be lost
			   */
			   get_client_name(cptr, FALSE));
		return 0;
	  }
#endif
	  /*
	    ** A new NICK being introduced by a neighbouring
	    ** server (e.g. message type "NICK new" received)
	    */
	    if (parc > 3)
	    { lastnick = atoi(parv[3]);
	      if (parc > 5)
		differ = (mycmp(acptr->user->username, parv[4]) ||
		    mycmp(acptr->user->host, parv[5])); }
	    sendto_failops("Nick collision on %s (%s %d <- %s %d)",
		    acptr->name, acptr->from->name, acptr->lastnick, 
		    get_client_name(cptr, FALSE), lastnick);
	}
	else
	{
	    /*
	    ** A NICK change has collided (e.g. message type ":old NICK new").
	    */
	    if (parc > 2)
		    lastnick = atoi(parv[2]);
	    differ = (mycmp(acptr->user->username, sptr->user->username) ||
		mycmp(acptr->user->host, sptr->user->host));
	    sendto_ops("Nick change collision from %s to %s (%s %d <- %s %d)",
		    sptr->name, acptr->name, acptr->from->name,
		    acptr->lastnick, get_client_name(cptr, FALSE), lastnick);
	}
	/*
	** Now remove (kill) the nick on our side if it is the youngest.
	** If no timestamp was received, we ignore the incoming nick
	** (and expect a KILL for our legit nick soon ):
	** When the timestamps are equal we kill both nicks. --Run
	** acptr->from != cptr should *always* be true (?).
	*/
	if (acptr->from != cptr) {
	  if (!lastnick || differ && lastnick >= acptr->lastnick ||
	      !differ && lastnick <= acptr->lastnick) {
	    if (!IsServer(sptr)) {
	      ircstp->is_kill++;
	      sendto_serv_butone(cptr, /* Kill old from outgoing servers */
				 ":%s KILL %s :%s (%s <- %s)",
				 me.name, sptr->name, me.name,
				 acptr->from->name,
				 get_client_name(cptr, FALSE));
	      sptr->flags |= FLAGS_KILLED;
	      (void)exit_client(NULL, sptr, &me,
				"Nick collision (you're a ghost)");
	    }
	    if (lastnick && lastnick != acptr->lastnick)
	      return 0; /* Ignore the NICK */
	  }
	  sendto_one(acptr, err_str(ERR_NICKCOLLISION),
		     me.name, acptr->name, nick); }
	ircstp->is_kill++;
	sendto_serv_butone(cptr, /* Kill our old from outgoing servers */
			   ":%s KILL %s :%s (%s <- %s)",
			   me.name, acptr->name, me.name,
			   acptr->from->name, get_client_name(cptr, FALSE));
	acptr->flags |= FLAGS_KILLED;
	(void)exit_client(NULL, acptr, &me,
			  "Nick collision (older nick overruled)");

	if (lastnick == acptr->lastnick)
	  return 0;
	
nickkilldone:
	if (IsServer(sptr)) {
	  /* A server introducing a new client, change source */

	  sptr = make_client(cptr, serv);
	  add_client_to_list(sptr);
	  if (parc > 2)
	    sptr->hopcount = atoi(parv[2]);
	  if (parc > 3)
	    sptr->lastnick = atoi(parv[3]);
	  else /* Little bit better, as long as not all upgraded */
	    sptr->lastnick = time(NULL);
	} else if (sptr->name[0]&&IsPerson(sptr)) {
	  /*
	  ** If the client belongs to me, then check to see
	  ** if client is currently on any channels where it
	  ** is currently banned.  If so, do not allow the nick
	  ** change to occur.
	  ** Also set 'lastnick' to current time, if changed.
	  */
	  if (MyClient(sptr))
	    for (lp = cptr->user->channel; lp; lp = lp->next)
	      if (is_banned(cptr, lp->value.chptr)) {
		sendto_one(cptr,
			   err_str(ERR_BANNICKCHANGE),
			   me.name, parv[0],
			   lp->value.chptr->chname);
		return 0;
	      }
	  
	  /*
	   * Client just changing his/her nick. If he/she is
	   * on a channel, send note of change to all clients
	   * on that channel. Propagate notice to other servers.
	   */
	  if (mycmp(parv[0], nick) ||
	      /* Next line can be removed when all upgraded  --Run */
	      !MyClient(sptr) && parc>2 && atoi(parv[2])<sptr->lastnick)
	    sptr->lastnick = (MyClient(sptr) || parc < 3) ?
	      time(NULL):atoi(parv[2]);
	  SetRegNick(sptr);
	  add_history(sptr);
	  sendto_common_channels(sptr, ":%s NICK :%s", parv[0], nick);
	  sendto_serv_butone(cptr, ":%s NICK %s :%d",
			     parv[0], nick, sptr->lastnick);
	} else if(!sptr->name[0]) {
#ifdef NOSPOOF
	  /*
	   * Client setting NICK the first time.
	   *
	   * Generate a random string for them to pong with.
	   *
	   * The first two are server specific.  The intent is to randomize
	   * things well.
	   *
	   * We use lots of junk here, but only "low cost" things.
	   */
	  md5data[0] = NOSPOOF_SEED01;
	  md5data[1] = NOSPOOF_SEED02;
	  md5data[2] = time(NULL);
	  md5data[3] = me.sendM;
	  md5data[4] = me.receiveM;
	  md5data[5] = 0;
	  md5data[6] = getpid();
	  md5data[7] = sptr->ip.s_addr;
	  md5data[8] = sptr->fd;
	  md5data[9] = 0;
	  md5data[10] = 0;
	  md5data[11] = 0;
	  md5data[12] = md5hash[0];  /* previous runs... */
	  md5data[13] = md5hash[1];
	  md5data[14] = md5hash[2];
	  md5data[15] = md5hash[3];
	  
	  /*
	   * initialize the md5 buffer to known values
	   */
	  MD5Init(md5hash);

	  /*
	   * transform the above information into gibberish
	   */
	  MD5Transform(md5hash, md5data);

	  /*
	   * Never release any internal state of our generator.  Instead,
	   * use two parts of the returned hash and xor them to hide
	   * both values.
	   */
	  sptr->nospoof = (md5hash[0] ^ md5hash[1]);

	  /*
	   * If on the odd chance it comes out zero, make it something
	   * non-zero.
	   */
	  if (sptr->nospoof == 0)
	    sptr->nospoof = 0xdeadbeef;
	  sendto_one(sptr, "NOTICE %s :*** If you are having problems"
		     " connecting due to ping timeouts, please"
		     " type /notice %X nospoof now.",
		     nick, sptr->nospoof, sptr->nospoof);
	  sendto_one(sptr, "PING :%X", sptr->nospoof);
#endif /* NOSPOOF */
	  
#ifdef CONTACT_EMAIL
	  sendto_one(sptr, ":%s NOTICE %s :*** If you need assistance with a"
		     " connection problem, please email " CONTACT_EMAIL
		     " with the name and version of the client you are"
		     " using, and the server you tried to connect to: %s",
		     me.name, nick, me.name);
#endif /* CONTACT_EMAIL */
#ifdef CONTACT_URL
	  sendto_one(sptr, ":%s NOTICE %s :*** If you need assistance with"
		     " connecting to this server, %s, please refer to: "
		     CONTACT_URL, me.name, nick, me.name);
#endif /* CONTACT_URL */
	  
	  /* Copy password to the passwd field if it's given after NICK
	   * - originally by taz, modified by Wizzu
	   */
	  if((parc > 2) && (strlen(parv[2]) < sizeof(sptr->passwd)))
	   (void)strcpy(sptr->passwd,parv[2]);

	  /* This had to be copied here to avoid problems.. */
	  (void)strcpy(sptr->name, nick);
	  if (sptr->user && IsNotSpoof(sptr)) {
	    /*
	    ** USER already received, now we have NICK.
	    ** *NOTE* For servers "NICK" *must* precede the
	    ** user message (giving USER before NICK is possible
	    ** only for local client connection!). register_user
	    ** may reject the client and call exit_client for it
	    ** --must test this and exit m_nick too!!!
	    */
	    sptr->lastnick = time(NULL); /* Always local client */
	    if (register_user(cptr, sptr, nick,
			      sptr->user->username)
		== FLUSH_BUFFER)
	      return FLUSH_BUFFER; }
	}
	/*
	 *  Finally set new nick name.
	 */
	if (sptr->name[0]) {
	  (void)del_from_client_hash_table(sptr->name, sptr);
	  if (IsPerson(sptr))
	    hash_check_notify(sptr, RPL_LOGOFF);
	}
	(void)strcpy(sptr->name, nick);
	(void)add_to_client_hash_table(nick, sptr);
	if (IsServer(cptr) && parc>7) {
	  parv[3]=nick;
	  return m_user(cptr, sptr, parc-3, &parv[3]); }
	else if (IsPerson(sptr))
	  hash_check_notify(sptr, RPL_LOGON);

	return 0;
}

/*
** m_message (used in m_private() and m_notice())
** the general function to deliver MSG's between users/channels
**
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
**
** massive cleanup
** rev argv 6/91
**
*/

static	int	m_message(cptr, sptr, parc, parv, notice)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
int	notice;
{
	Reg1	aClient	*acptr;
	Reg2	char	*s;
	aChannel *chptr;
	char	*nick, *server, *p, *cmd, *host;

	if (notice)
	    {
		if (check_registered(sptr))
			return 0;
	    }
	else if (check_registered_user(sptr))
		return 0;

	sptr->flags&=~FLAGS_TS8;

	cmd = notice ? MSG_NOTICE : MSG_PRIVATE;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NORECIPIENT),
			   me.name, parv[0], cmd);
		return -1;
	    }

	if (parc < 3 || *parv[2] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	    }

	if (MyConnect(sptr))
		parv[1] = canonize(parv[1]);
	for (p = NULL, nick = strtoken(&p, parv[1], ","); nick;
	     nick = strtoken(&p, NULL, ","))
	    {
		/*
		** nickname addressed?
		*/
		if ((acptr = find_person(nick, NULL)))
		{
		    if (MyClient(sptr) && check_for_target_limit(sptr, acptr, acptr->name))
			continue;

		if (!is_silenced(sptr, acptr))
		    {
			if (!notice && MyConnect(sptr) &&
			    acptr->user && acptr->user->away)
				sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
					   parv[0], acptr->name,
					   acptr->user->away);
			sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
					  parv[0], cmd, nick, parv[2]);
		    }
		    continue;
                }

                if (nick[0] == '@')
                    {

		/*   
		 * If its a message for all Channel OPs call the function
		 * sendto_channelops_butone() to handle it.  -Cabal95
		 */
			if ( nick[1] == '#' )
			    {
                                if(chptr = find_channel(nick+1, NullChn))
                                    {
                          		if (can_send(sptr, chptr) == 0 || IsULine(cptr,sptr))
	        				sendto_channelops_butone(cptr, sptr, chptr, ":%s %s %s :%s", parv[0], cmd, nick, parv[2]);
       		        		else if (!notice)
			        		sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN), me.name, parv[0], nick+1);
                                    }
                                else sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], nick+1);
        		    }

			/*   
			 * If its a message for all Channel ops and voices call the function
		 	 * sendto_channelvoice_butone() to handle it.  -DuffJ
			 */
			else if ( nick[1] == '+' && nick[2] == '#' )
	      		    {
                                if (chptr = find_channel(nick+2, NullChn))
                                    {
        				if (can_send(sptr, chptr) == 0 || IsULine(cptr,sptr))
	        				sendto_channelvoice_butone(cptr, sptr, chptr, ":%s %s %s :%s", parv[0], cmd, nick, parv[2]);
		        		else if (!notice)
			        		sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN), me.name, parv[0], nick+2);
                                    }
			        else sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], nick+2);
			    }
			continue;
                    }
		/*
		** channel msg?
		** Now allows U-lined users to send to channel no problemo
		** -- Barubary
		*/
		if ((chptr = find_channel(nick, NullChn)))
		    {
			if (can_send(sptr, chptr) == 0) {
				if(MyClient(sptr) && check_for_target_limit(sptr, chptr, chptr->chname))
					continue;
				sendto_channel_butone(cptr, sptr, chptr,
						      ":%s %s %s :%s",
						      parv[0], cmd, nick,
						      parv[2]);
			}
			else if (!notice)
				sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					   me.name, parv[0], nick);
			continue;
		    }
	
		/*
		** the following two cases allow masks in NOTICEs
		** (for OPERs only)
		**
		** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
		*/
		if ((*nick == '$' || *nick == '#') && IsAnOper(sptr))
		    {
			if (!(s = (char *)rindex(nick, '.')))
			    {
				sendto_one(sptr, err_str(ERR_NOTOPLEVEL),
					   me.name, parv[0], nick);
				continue;
			    }
			while (*++s)
				if (*s == '.' || *s == '*' || *s == '?')
					break;
			if (*s == '*' || *s == '?')
			    {
				sendto_one(sptr, err_str(ERR_WILDTOPLEVEL),
					   me.name, parv[0], nick);
				continue;
			    }
			sendto_match_butone(IsServer(cptr) ? cptr : NULL, 
					    sptr, nick + 1,
					    (*nick == '#') ? MATCH_HOST :
							     MATCH_SERVER,
					    ":%s %s %s :%s", parv[0],
					    cmd, nick, parv[2]);
			continue;
		    }
	
		/*
		** user[%host]@server addressed?
		*/
		if ((server = (char *)index(nick, '@')) &&
		    (acptr = find_server(server + 1, NULL)))
		    {
			/*
			** Not destined for a user on me :-(
			*/
			if (!IsMe(acptr))
			    {
				sendto_one(acptr,":%s %s %s :%s", parv[0],
					   cmd, nick, parv[2]);
				continue;
			    }

			/*
			** Find the nick@server using hash.
			*/
			acptr = find_nickserv(nick, (aClient *)NULL);
			if (acptr) {
				sendto_prefix_one(acptr, sptr,
						  ":%s %s %s :%s",
				 		  parv[0], cmd,
						  acptr->name,
						  parv[2]);
				continue;
			}
		    }
		if (server && strncasecmp(server+1, SERVICES_NAME, 
		    strlen(SERVICES_NAME))==0)
			sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name, 
			    parv[0], nick);
		else 
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
			    parv[0], nick);
            }
    return 0;
}

/*
** m_private
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
*/

int	m_private(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	return m_message(cptr, sptr, parc, parv, 0);
}

/*
 * Built in services aliasing for ChanServ, Memoserv, NickServ and
 * OperServ. This not only is an alias, but is also a security measure,
 * because PRIVMSG's arent sent to 'ChanServ' they are now sent to
 * 'ChanServ@services.dal.net' so nobody can snoop /cs commands :) -taz
 */

int	m_chanserv(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0') {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if ((acptr = find_person(ChanServ, NULL)))
		sendto_one(acptr,":%s PRIVMSG %s@%s :%s", parv[0], 
			ChanServ, SERVICES_NAME, parv[1]);
	else
		sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
			parv[0], ChanServ);
}

int	m_memoserv(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0') {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if ((acptr = find_person(MemoServ, NULL)))
		sendto_one(acptr,":%s PRIVMSG %s@%s :%s", parv[0], 
			MemoServ, SERVICES_NAME, parv[1]);
	else
		sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
			parv[0], MemoServ);
}

int	m_nickserv(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0') {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if ((acptr = find_person(NickServ, NULL)))
		sendto_one(acptr,":%s PRIVMSG %s@%s :%s", parv[0], 
			NickServ, SERVICES_NAME, parv[1]);
	else
		sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
			parv[0], NickServ);
}

int	m_operserv(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0') {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if ((acptr = find_person(OperServ, NULL)))
		sendto_one(acptr,":%s PRIVMSG %s@%s :%s", parv[0], 
			OperServ, SERVICES_NAME, parv[1]);
	else
		sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
			parv[0], OperServ);
}

/* 
 * Automatic NickServ/ChanServ direction for the identify command
 * -taz
 */
int	m_identify(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0') {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if (*parv[1]) {
		if((*parv[1] == '#') && ((char *)index(parv[1], ' '))) {
			if ((acptr = find_person(ChanServ, NULL)))
				sendto_one(acptr,":%s PRIVMSG %s@%s :IDENTIFY %s", parv[0], 
					ChanServ, SERVICES_NAME, parv[1]);
			else
				sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
					parv[0], ChanServ);
		} else {
			if ((acptr = find_person(NickServ, NULL)))
				sendto_one(acptr,":%s PRIVMSG %s@%s :IDENTIFY %s", parv[0],
					NickServ, SERVICES_NAME, parv[1]);
			else
				sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
					parv[0], NickServ);
		}
	}
}

/*
 * Automatic NickServ/ChanServ parsing. If the second word of parv[1]
 * starts with a '#' this message goes to ChanServ. If it starts with 
 * anything else, it goes to NickServ. If there is no second word in 
 * parv[1], the message defaultly goes to NickServ. If parv[1] == 'help'
 * the user in instructed to /cs, /ns or /ms HELP for the help they need.
 * -taz
 */

int	m_services(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char *tmps;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0') {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if ((strlen(parv[1])>=4) && (!strncmp(parv[1], "help", 4))) {
		sendto_one(sptr, ":services!service@%s NOTICE %s :For ChanServ help use: /chanserv help", SERVICES_NAME, sptr->name);
		sendto_one(sptr, ":services!service@%s NOTICE %s :For NickServ help use: /nickserv help", SERVICES_NAME, sptr->name);
		sendto_one(sptr, ":services!service@%s NOTICE %s :For MemoServ help use: /memoserv help", SERVICES_NAME, sptr->name);
		return;
	}

	if ((tmps = (char *)index(parv[1], ' '))) {
		tmps++;
		if(*tmps == '#')
			return m_chanserv(cptr, sptr, parc, parv);
		else
			return m_nickserv(cptr, sptr, parc, parv);
	}

	return m_nickserv(cptr, sptr, parc, parv);

}

/*
** m_notice
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = notice text
*/

int	m_notice(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	if (!IsRegistered(cptr) && (cptr->name[0]) && !IsNotSpoof(cptr))
	{
		if (BadPtr(parv[1])) return 0;
#ifdef NOSPOOF
		if (strtoul(parv[1], NULL, 16) != cptr->nospoof)
			goto temp;
		sptr->nospoof = 0;
#endif
		if (sptr->user && sptr->name[0])
			return register_user(cptr, sptr, sptr->name,
				sptr->user->username);
		return 0;
	}
	temp:
	return m_message(cptr, sptr, parc, parv, 1);
}

static	void	do_who(sptr, acptr, repchan)
aClient *sptr, *acptr;
aChannel *repchan;
{
	char	status[5];
	int	i = 0;

	if (acptr->user->away)
		status[i++] = 'G';
	else
		status[i++] = 'H';
	if (IsAnOper(acptr))
		status[i++] = '*';
	else if (IsInvisible(acptr) && sptr != acptr && IsAnOper(sptr))
		status[i++] = '%';
	if (repchan && is_chan_op(acptr, repchan))
		status[i++] = '@';
	else if (repchan && has_voice(acptr, repchan))
		status[i++] = '+';
	else if (repchan && is_zombie(acptr, repchan))
		status[i++] = '!';
	status[i] = '\0';
	sendto_one(sptr, rpl_str(RPL_WHOREPLY), me.name, sptr->name,
		   (repchan) ? (repchan->chname) : "*", acptr->user->username,
		   acptr->user->host, acptr->user->server, acptr->name,
		   status, acptr->hopcount, acptr->info);
}


/*
** m_who
**	parv[0] = sender prefix
**	parv[1] = nickname mask list
**	parv[2] = additional selection flag, only 'o' for now.
*/
int	m_who(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	aClient *acptr;
	Reg2	char	*mask = parc > 1 ? parv[1] : NULL;
	Reg3	Link	*lp;
	aChannel *chptr;
	aChannel *mychannel;
	char	*channame = NULL, *s;
	int	oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
	int	member;

	if (check_registered_user(sptr))
		return 0;

	if (!BadPtr(mask))
	    {
		if ((s = (char *)index(mask, ',')))
		    {
			parv[1] = ++s;
			(void)m_who(cptr, sptr, parc, parv);
		    }
		clean_channelname(mask);
	    }

	mychannel = NullChn;
	if (sptr->user)
		if ((lp = sptr->user->channel))
			mychannel = lp->value.chptr;

	/* Allow use of m_who without registering */
	
	/*
	**  Following code is some ugly hacking to preserve the
	**  functions of the old implementation. (Also, people
	**  will complain when they try to use masks like "12tes*"
	**  and get people on channel 12 ;) --msa
	*/
	if (!mask || *mask == '\0')
		mask = NULL;
	else if (mask[1] == '\0' && mask[0] == '*')
	    {
		mask = NULL;
		if (mychannel)
			channame = mychannel->chname;
	    }
	else if (mask[1] == '\0' && mask[0] == '0') /* "WHO 0" for irc.el */
		mask = NULL;
	else
		channame = mask;
	(void)collapse(mask);

	if (IsChannelName(channame))
	    {
		/*
		 * List all users on a given channel
		 */
		chptr = find_channel(channame, NULL);
		if (chptr)
		  {
		    member = IsMember(sptr, chptr);
		    if (member || !SecretChannel(chptr))
			for (lp = chptr->members; lp; lp = lp->next)
			    {
				if (oper && !IsAnOper(lp->value.cptr))
					continue;
				if (lp->value.cptr!=sptr &&
				    (lp->flags & CHFL_ZOMBIE))
					continue;
				if (lp->value.cptr!=sptr && IsInvisible(lp->value.cptr) && !member)
					continue;
				do_who(sptr, lp->value.cptr, chptr);
			    }
		  }
	    }
	else for (acptr = client; acptr; acptr = acptr->next)
	    {
		aChannel *ch2ptr = NULL;
		int	showperson, isinvis;

		if (!IsPerson(acptr))
			continue;
		if (oper && !IsAnOper(acptr))
			continue;
		showperson = 0;
		/*
		 * Show user if they are on the same channel, or not
		 * invisible and on a non secret channel (if any).
		 * Do this before brute force match on all relevant fields
		 * since these are less cpu intensive (I hope :-) and should
		 * provide better/more shortcuts - avalon
		 */
		isinvis = acptr!=sptr && IsInvisible(acptr) && !IsAnOper(sptr);
		for (lp = acptr->user->channel; lp; lp = lp->next)
		    {
			chptr = lp->value.chptr;
			member = IsMember(sptr, chptr);
			if (isinvis && !member)
				continue;
			if (is_zombie(acptr, chptr))
				continue;
			if (IsAnOper(sptr)) showperson = 1;
			if (member || (!isinvis && 
				ShowChannel(sptr, chptr)))
			    {
				ch2ptr = chptr;
				showperson = 1;
				break;
			    }
			if (HiddenChannel(chptr) && !SecretChannel(chptr) &&
			    !isinvis)
				showperson = 1;
		    }
		if (!acptr->user->channel && !isinvis)
			showperson = 1;
		/*
		** This is brute force solution, not efficient...? ;( 
		** Show entry, if no mask or any of the fields match
		** the mask. --msa
		*/
		if (showperson &&
		    (!mask ||
		     match(mask, acptr->name) == 0 ||
		     match(mask, acptr->user->username) == 0 ||
		     match(mask, acptr->user->host) == 0 ||
		     match(mask, acptr->user->server) == 0 ||
		     match(mask, acptr->info) == 0))
			do_who(sptr, acptr, ch2ptr);
	    }
	sendto_one(sptr, rpl_str(RPL_ENDOFWHO), me.name, parv[0],
		   BadPtr(mask) ?  "*" : mask);
	return 0;
}

/*
** m_whois
**	parv[0] = sender prefix
**	parv[1] = nickname masklist
*/
int	m_whois(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	static anUser UnknownUser =
	    {
		NULL,	/* nextu */
		NULL,	/* channel */
		NULL,   /* invited */
		NULL,	/* silence */
		NULL,	/* away */
		0,	/* last */
		0,	/* servicestamp */
		1,      /* refcount */
		0,	/* joined */
		"<Unknown>",	/* username */
		"<Unknown>",	/* host */
		"<Unknown>"	/* server */
	    };
	Reg2	Link	*lp;
	Reg3	anUser	*user;
	aClient *acptr, *a2cptr;
	aChannel *chptr;
	char	*nick, *tmp, *name, *temp;
	char	*p = NULL;
	int	found, len, mlen, t;


	if (check_registered_user(sptr))
		return 0;

    	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
			   me.name, parv[0]);
		return 0;
	    }

	if (parc > 2)
	    {
		if (hunt_server(cptr,sptr,":%s WHOIS %s :%s", 1,parc,parv) !=
		    HUNTED_ISME)
			return 0;
		parv[1] = parv[2];
	    }

	for (tmp = parv[1]; (nick = strtoken(&p, tmp, ",")); tmp = NULL)
	    {
		int	invis, showperson, member, wilds;

		found = 0;
		(void)collapse(nick);
		wilds = (index(nick, '?') || index(nick, '*'));
		if (wilds && IsServer(cptr)) continue;
		for (acptr = client; (acptr = next_client(acptr, nick));
		     acptr = acptr->next)
		    {
			if (IsServer(acptr))
				continue;
			/*
			 * I'm always last :-) and acptr->next == NULL!!
			 */
			if (IsMe(acptr))
				break;
			/*
			 * 'Rules' established for sending a WHOIS reply:
			 *
			 * - only allow a remote client to get replies for
			 *   local clients if wildcards are being used;
			 *
			 * - if wildcards are being used dont send a reply if
			 *   the querier isnt any common channels and the
			 *   client in question is invisible and wildcards are
			 *   in use (allow exact matches only);
			 *
			 * - only send replies about common or public channels
			 *   the target user(s) are on;
			 */
			if (!MyConnect(sptr) && !MyConnect(acptr) && wilds)
				continue;
			user = acptr->user ? acptr->user : &UnknownUser;
			name = (!*acptr->name) ? "?" : acptr->name;

			invis = acptr!=sptr && IsInvisible(acptr);
			member = (user->channel) ? 1 : 0;
			showperson = (wilds && !invis && !member) || !wilds;
			for (lp = user->channel; lp; lp = lp->next)
			    {
				chptr = lp->value.chptr;
				member = IsMember(sptr, chptr);
				if (invis && !member)
					continue;
				if (is_zombie(acptr, chptr))
					continue;
				if (member || (!invis && PubChannel(chptr)))
				    {
					showperson = 1;
					break;
				    }
				if (!invis && HiddenChannel(chptr) &&
				    !SecretChannel(chptr))
					showperson = 1;
			    }
			if (!showperson)
				continue;

			a2cptr = find_server(user->server, NULL);

			sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
				   parv[0], name,
				   user->username, user->host, acptr->info);
			if (IsARegNick(acptr))
				sendto_one(sptr, rpl_str(RPL_WHOISREGNICK),
					   me.name, parv[0], name);
			found = 1;
			mlen = strlen(me.name) + strlen(parv[0]) + 6 +
				strlen(name);
			for (len = 0, *buf = '\0', lp = user->channel; lp;
			     lp = lp->next)
			    {
				chptr = lp->value.chptr;
				if (ShowChannel(sptr, chptr) &&
				    (acptr==sptr || !is_zombie(acptr, chptr)))
				    {
					if (len + strlen(chptr->chname)
                                            > (size_t) BUFSIZE - 4 - mlen)
					    {
						sendto_one(sptr,
							   ":%s %d %s %s :%s",
							   me.name,
							   RPL_WHOISCHANNELS,
							   parv[0], name, buf);
						*buf = '\0';
						len = 0;
					    }
					if (is_chan_op(acptr, chptr))
						*(buf + len++) = '@';
					else if (has_voice(acptr, chptr))
						*(buf + len++) = '+';
 					else if (is_zombie(acptr, chptr))
						*(buf + len++) = '!';
					if (len)
						*(buf + len) = '\0';
					(void)strcpy(buf + len, chptr->chname);
					len += strlen(chptr->chname);
					(void)strcat(buf + len, " ");
					len++;
				    }
			    }
			if (buf[0] != '\0')
				sendto_one(sptr, rpl_str(RPL_WHOISCHANNELS),
					   me.name, parv[0], name, buf);

			sendto_one(sptr, rpl_str(RPL_WHOISSERVER),
				   me.name, parv[0], name, user->server,
				   a2cptr?a2cptr->info:"*Not On This Net*");

			if (user->away)
				sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
					   parv[0], name, user->away);

			/* The following includes admin/Sadmin
			 * status in the WHOISOPERATOR reply.
                         * -DuffJ
			 */
			buf[0]='\0';
			      if (IsAnOper(acptr))
				strcat(buf, "an IRC Operator");
			      if (IsAdmin(acptr))
				strcat(buf, " - Server Administrator");
			      else if (IsSAdmin(acptr))
				strcat(buf, " - Services Administrator");

			if(buf[0])
			 sendto_one(sptr, rpl_str(RPL_WHOISOPERATOR),
				me.name, parv[0], name, buf);

                        if (IsHelpOp(acptr))
                         sendto_one(sptr, rpl_str(RPL_WHOISHELPOP),
				me.name, parv[0], name);

			if (acptr->user && MyConnect(acptr))
				sendto_one(sptr, rpl_str(RPL_WHOISIDLE),
					   me.name, parv[0], name,
					   time(NULL) - user->last,
					   acptr->firsttime);
		    }
		if (!found)
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				   me.name, parv[0], nick);
		if (p)
			p[-1] = ',';
	    }
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOIS), me.name, parv[0], parv[1]);

	return 0;
}

/*
** m_user
**	parv[0] = sender prefix
**	parv[1] = username (login name, account)
**	parv[2] = client host name (used only from other servers)
**	parv[3] = server host name (used only from other servers)
**	parv[4] = users real name info
*/
int	m_user(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
#define	UFLAGS	(UMODE_INVISIBLE|UMODE_WALLOP|UMODE_SERVNOTICE)
	char	*username, *host, *server, *realname;
	u_int32_t sstamp = 0;
	anUser	*user;
 
	if (IsServer(cptr) && !IsUnknown(sptr))
		return 0;

	if (parc > 2 && (username = (char *)index(parv[1],'@')))
		*username = '\0'; 
	if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
	    *parv[3] == '\0' || *parv[4] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
	    		   me.name, parv[0], "USER");
		if (IsServer(cptr))
			sendto_ops("bad USER param count for %s from %s",
				   parv[0], get_client_name(cptr, FALSE));
		else
			return 0;
	    }

	/* Copy parameters into better documenting variables */

	username = (parc < 2 || BadPtr(parv[1])) ? "<bad-boy>" : parv[1];
	host     = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
	server   = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];

	/* This we can remove as soon as all servers have upgraded. */

	if (parc == 6 && IsServer(cptr)) {
		if (isdigit(*parv[4]))
	 		sstamp = atol(parv[4]);
		realname = (BadPtr(parv[5])) ? "<bad-realname>" : parv[5];
	} else
		realname = (BadPtr(parv[4])) ? "<bad-realname>" : parv[4];

 	user = make_user(sptr);

	if (!MyConnect(sptr))
	    {
		if (sptr->srvptr == NULL)
			sendto_ops("WARNING, User %s introduced as being "
				"on non-existant server %s.", sptr->name,
				server);
		strncpyzt(user->server, server, sizeof(user->server));
		strncpyzt(user->host, host, sizeof(user->host));
		goto user_finish;
	    }

	if (!IsUnknown(sptr))
	    {
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED),
			   me.name, parv[0]);
		return 0;
	    }
#ifndef	NO_DEFAULT_INVISIBLE
	sptr->umodes |= UMODE_INVISIBLE;
#endif
	sptr->umodes |= (UFLAGS & atoi(host));
	strncpyzt(user->host, host, sizeof(user->host));
	strncpyzt(user->server, me.name, sizeof(user->server));
user_finish:
	user->servicestamp = sstamp;
	strncpyzt(sptr->info, realname, sizeof(sptr->info));
	if (sptr->name[0] && (IsServer(cptr) ? 1 : IsNotSpoof(sptr)))
	/* NICK and no-spoof already received, now we have USER... */
		return register_user(cptr, sptr, sptr->name, username);
	else
		strncpyzt(sptr->user->username, username, USERLEN+1);
	return 0;
}

/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
int	m_quit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char *ocomment = (parc > 1 && parv[1]) ? parv[1] : parv[0];
	static char comment[TOPICLEN];

	if (!IsServer(cptr)) {
		strcat(comment, "Quit: ");
		strncpy(comment+6,ocomment,TOPICLEN-7);
		comment[TOPICLEN] = '\0';
		return exit_client(cptr, sptr, sptr, comment);
	} else
		return exit_client(cptr, sptr, sptr, ocomment);
    }

/*
** m_kill
**	parv[0] = sender prefix
**	parv[1] = kill victim(s) - comma separated list
**	parv[2] = kill path
*/
int	m_kill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	static anUser UnknownUser =
	    {
		NULL,	/* nextu */
		NULL,	/* channel */
		NULL,   /* invited */
		NULL,	/* silence */
		NULL,	/* away */
		0,	/* last */
		0,	/* servicestamp */
		1,      /* refcount */
		0,	/* joined */
		"<Unknown>",	/* username */
		"<Unknown>",	/* host */
		"<Unknown>"	/* server */
	    };
	aClient *acptr;
	anUser 	*auser;
	char    inpath[HOSTLEN * 2 + USERLEN + 5];
	char	*oinpath = get_client_name(cptr,FALSE);
	char	*user, *path, *killer, *nick, *p, *s;
	int	chasing = 0, kcount = 0;


        if (check_registered(sptr))
                return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "KILL");
		return 0;
	    }

	user = parv[1];
	path = parv[2]; /* Either defined or NULL (parc >= 2!!) */
	strcpy(inpath, oinpath);
	if (IsServer(cptr) && (s = (char *)index(inpath, '.')) != NULL)
		*s = '\0';	/* Truncate at first "." */
	if (!IsPrivileged(cptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
	if (IsAnOper(cptr))
	    {
		if (BadPtr(path))
		    {
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
				   me.name, parv[0], "KILL");
			return 0;
		    }
		if (strlen(path) > (size_t) TOPICLEN)
			path[TOPICLEN] = '\0';
	    }
	if (MyClient(sptr))
		user = canonize(user);
	for (p = NULL, nick = strtoken(&p, user, ","); nick;
		nick = strtoken(&p, NULL, ","))
	{
	chasing = 0;
	if (!(acptr = find_client(nick, NULL)))
	    {
		/*
		** If the user has recently changed nick, we automaticly
		** rewrite the KILL for this new nickname--this keeps
		** servers in synch when nick change and kill collide
		*/
		if (!(acptr = get_history(nick, (long)KILLCHASETIMELIMIT)))
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				   me.name, parv[0], nick);
			continue;
		    }
		sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
			   me.name, parv[0], nick, acptr->name);
		chasing = 1;
	    }
	if ((!MyConnect(acptr) && MyClient(cptr) && !OPCanGKill(cptr)) ||
	    (MyConnect(acptr) && MyClient(cptr) && !OPCanLKill(cptr)))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		continue;
	    }
	if (IsServer(acptr) || IsMe(acptr))
	    {
		sendto_one(sptr, err_str(ERR_CANTKILLSERVER),
			   me.name, parv[0]);
		continue;
	    }
	/* From here on, the kill is probably going to be successful. */
	kcount++;
	if (!IsServer(sptr) && (kcount > MAXKILLS))
	{
	  sendto_one(sptr,":%s NOTICE %s :Too many targets, kill list was truncated. Maximum is %d.",me.name, parv[0], MAXKILLS);
	  break;
	}
	if (!IsServer(cptr))
	    {
		/*
		** The kill originates from this server, initialize path.
		** (In which case the 'path' may contain user suplied
		** explanation ...or some nasty comment, sigh... >;-)
		**
		**	...!operhost!oper
		**	...!operhost!oper (comment)
		*/
		if (IsUnixSocket(cptr)) /* Don't use get_client_name syntax */
			strcpy(inpath, me.sockhost);
		else
			strcpy(inpath, cptr->sockhost);
		if (kcount < 2) /* Only check the path the first time 
				   around, or it gets appended to itself. */
		 if (!BadPtr(path))
		    {
			(void)sprintf(buf, "%s%s (%s)",
				cptr->name, IsOper(sptr) ? "" : "(L)", path);
			path = buf;
		    }
		 else
			path = cptr->name;
	    }
	else if (BadPtr(path))
		 path = "*no-path*"; /* Bogus server sending??? */
	/*
	** Notify all *local* opers about the KILL (this includes the one
	** originating the kill, if from this server--the special numeric
	** reply message is not generated anymore).
	**
	** Note: "acptr->name" is used instead of "user" because we may
	**	 have changed the target because of the nickname change.
	*/
	auser = acptr->user ? acptr->user : &UnknownUser;
	if (index(parv[0], '.'))
		sendto_umode(UMODE_KILLS, "*** Notice -- Received KILL message for %s!%s@%s from %s Path: %s!%s",
			acptr->name, auser->username, auser->host,
			parv[0], inpath, path);
	else
		sendto_ops("Received KILL message for %s!%s@%s from %s Path: %s!%s",
			acptr->name, auser->username, auser->host,
			parv[0], inpath, path);
#if defined(USE_SYSLOG) && defined(SYSLOG_KILL)
	if (IsOper(sptr))
		syslog(LOG_DEBUG,"KILL From %s For %s Path %s!%s",
			parv[0], acptr->name, inpath, path);
#endif
	/*
	** And pass on the message to other servers. Note, that if KILL
	** was changed, the message has to be sent to all links, also
	** back.
	** Suicide kills are NOT passed on --SRB
	*/
	if (!MyConnect(acptr) || !MyConnect(sptr) || !IsAnOper(sptr))
	    {
		sendto_serv_butone(cptr, ":%s KILL %s :%s!%s",
				   parv[0], acptr->name, inpath, path);
		if (chasing && IsServer(cptr))
			sendto_one(cptr, ":%s KILL %s :%s!%s",
				   me.name, acptr->name, inpath, path);
		acptr->flags |= FLAGS_KILLED;
	    }
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_KILL, sptr, ":%s KILL %s :%s!%s",
				parv[0], acptr->name, inpath, path);
#endif
	/*
	** Tell the victim she/he has been zapped, but *only* if
	** the victim is on current server--no sense in sending the
	** notification chasing the above kill, it won't get far
	** anyway (as this user don't exist there any more either)
	*/
	if (MyConnect(acptr))
		sendto_prefix_one(acptr, sptr,":%s KILL %s :%s!%s",
				  parv[0], acptr->name, inpath, path);
	/*
	** Set FLAGS_KILLED. This prevents exit_one_client from sending
	** the unnecessary QUIT for this. (This flag should never be
	** set in any other place)
	*/
	if (MyConnect(acptr) && MyConnect(sptr) && IsAnOper(sptr))
		(void)sprintf(buf2, "Local kill by %s (%s)", sptr->name,
			BadPtr(parv[2]) ? sptr->name : parv[2]);
	else
	    {
		if ((killer = index(path, ' ')))
		    {
			while (*killer && *killer != '!')
				killer--;
			if (!*killer)
				killer = path;
			else
				killer++;
		    }
		else
			killer = path;
		(void)sprintf(buf2, "Killed (%s)", killer);
	    }
	    if(exit_client(cptr, acptr, sptr, buf2) == FLUSH_BUFFER)
		return FLUSH_BUFFER;
	}
	return 0;
}
/***********************************************************************
 * m_away() - Added 14 Dec 1988 by jto. 
 *            Not currently really working, I don't like this
 *            call at all...
 *
 *            ...trying to make it work. I don't like it either,
 *	      but perhaps it's worth the load it causes to net.
 *	      This requires flooding of the whole net like NICK,
 *	      USER, MODE, etc messages...  --msa
 ***********************************************************************/
/*
** m_away
**	parv[0] = sender prefix
**	parv[1] = away message
*/
int	m_away(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	char	*away, *awy2 = parv[1];
	if (check_registered_user(sptr))
		return 0;
	away = sptr->user->away;
	if (parc < 2 || !*awy2)
	    {
		/* Marking as not away */
		if (away)
		    {
			MyFree(away);
			sptr->user->away = NULL;
		    }
		sendto_serv_butone(cptr, ":%s AWAY", parv[0]);
		if (MyConnect(sptr))
			sendto_one(sptr, rpl_str(RPL_UNAWAY),
				   me.name, parv[0]);
#ifdef	USE_SERVICES
		check_services_butonee(SERVICE_WANT_AWAY, ":%s AWAY", parv[0]);
#endif
		return 0;
	    }
	/* Marking as away */
	if (strlen(awy2) > (size_t) TOPICLEN)
		awy2[TOPICLEN] = '\0';
	sendto_serv_butone(cptr, ":%s AWAY :%s", parv[0], awy2);
#ifdef	USE_SERVICES
	check_services_butonee(SERVICE_WANT_AWAY, ":%s AWAY :%s",
				parv[0], parv[1]);
#endif
	if (away)
		away = (char *)MyRealloc(away, strlen(awy2)+1);
	else
		away = (char *)MyMalloc(strlen(awy2)+1);
	sptr->user->away = away;
	(void)strcpy(away, awy2);
	if (MyConnect(sptr))
		sendto_one(sptr, rpl_str(RPL_NOWAWAY), me.name, parv[0]);
	return 0;
}
/*
** m_ping
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_ping(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	*origin, *destination;
        if (check_registered(sptr))
                return 0;
 
 	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	    }
	origin = parv[1];
	destination = parv[2]; /* Will get NULL or pointer (parc >= 2!!) */
	acptr = find_client(origin, NULL);
	if (!acptr)
		acptr = find_server(origin, NULL);
	if (acptr && acptr != sptr)
		origin = cptr->name;
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	    {
		if ((acptr = find_server(destination, NULL)))
			sendto_one(acptr,":%s PING %s :%s", parv[0],
				   origin, destination);
	    	else
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
				   me.name, parv[0], destination);
			return 0;
		    }
	    }
	else
		sendto_one(sptr,":%s PONG %s :%s", me.name,
			   (destination) ? destination : me.name, origin);
	return 0;
    }
#ifdef NOSPOOF
/*
** m_nospoof - allows clients to respond to no spoofing patch
**	parv[0] = prefix
**	parv[1] = code
*/
int	m_nospoof(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
unsigned long result;
	if (IsNotSpoof(cptr)) return 0;
	if (IsRegistered(cptr)) return 0;
	if (!*sptr->name) return 0;
	if (BadPtr(parv[1])) goto temp;
	result = strtoul(parv[1], NULL, 16);
	/* Accept code in second parameter (ircserv) */
	if (result != sptr->nospoof)
	{
		if (BadPtr(parv[2])) goto temp;
		result = strtoul(parv[2], NULL, 16);
		if (result != sptr->nospoof) goto temp;
	}
	sptr->nospoof = 0;
	if (sptr->user && sptr->name[0])
		return register_user(cptr, sptr, sptr->name,
			sptr->user->username);
	return 0;
	temp:
	/* Homer compatibility */
	sendto_one(cptr, ":%X!nospoof@%s PRIVMSG %s :%cVERSION%c",
		cptr->nospoof, me.name, cptr->name, (char) 1, (char) 1);
	return 0;
}
#endif /* NOSPOOF */
/*
** m_pong
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_pong(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	*origin, *destination;
#ifdef NOSPOOF
	if (!IsRegistered(cptr))
		return m_nospoof(cptr, sptr, parc, parv);
#endif
	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	    }
	origin = parv[1];
	destination = parv[2];
	cptr->flags &= ~FLAGS_PINGSENT;
	sptr->flags &= ~FLAGS_PINGSENT;
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	    {
		if ((acptr = find_client(destination, NULL)) ||
		    (acptr = find_server(destination, NULL)))
		{
			if (!IsServer(cptr) && !IsServer(acptr))
			{
				sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
					me.name, parv[0], destination);
				return 0;
			}
			else
			sendto_one(acptr,":%s PONG %s %s",
				   parv[0], origin, destination);
		}
		else
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
				   me.name, parv[0], destination);
			return 0;
		    }
	    }
#ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE, "PONG: %s %s", origin,
		      destination ? destination : "*"));
#endif
	return 0;
    }
/*
** m_oper
**	parv[0] = sender prefix
**	parv[1] = oper name
**	parv[2] = oper password
*/
int	m_oper(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aConfItem *aconf;
	char	*name, *password, *encr;
#ifdef CRYPT_OPER_PASSWORD
	char	salt[3];
	extern	char *crypt();
#endif /* CRYPT_OPER_PASSWORD */
	if (check_registered_user(sptr))
		return 0;
	name = parc > 1 ? parv[1] : NULL;
	password = parc > 2 ? parv[2] : NULL;
	if (!IsServer(cptr) && (BadPtr(name) || BadPtr(password)))
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "OPER");
		return 0;
	    }
	
	/* if message arrived from server, trust it, and set to oper */
	    
	if ((IsServer(cptr) || IsMe(cptr)) && !IsOper(sptr))
	    {
		sptr->umodes |= UMODE_OPER;
		sendto_serv_butone(cptr, ":%s MODE %s :+o", parv[0], parv[0]);
		if (IsMe(cptr))
			sendto_one(sptr, rpl_str(RPL_YOUREOPER),
				   me.name, parv[0]);
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr,
				      ":%s MODE %s :+o", parv[0], parv[0]);
#endif
		return 0;
	    }
	else if (IsOper(sptr))
	    {
		if (MyConnect(sptr))
			sendto_one(sptr, rpl_str(RPL_YOUREOPER),
				   me.name, parv[0]);
		return 0;
	    }
	if (!(aconf = find_conf_exact(name, sptr->username, sptr->sockhost,
				      CONF_OPS)) &&
	    !(aconf = find_conf_exact(name, sptr->username,
				      inetntoa((char *)&cptr->ip), CONF_OPS)))
	    {
		sendto_one(sptr, err_str(ERR_NOOPERHOST), me.name, parv[0]);
                sendto_realops("Failed OPER attempt by %s (%s@%s)",
                  parv[0], sptr->user->username, sptr->sockhost);
		sptr->since += 7;
		return 0;
	    }
#ifdef CRYPT_OPER_PASSWORD
        /* use first two chars of the password they send in as salt */
        /* passwd may be NULL. Head it off at the pass... */
        salt[0] = '\0';
        if (password && aconf->passwd && aconf->passwd[0] && aconf->passwd[1])
	    {
        	salt[0] = aconf->passwd[0];
		salt[1] = aconf->passwd[1];
		salt[2] = '\0';
		encr = crypt(password, salt);
	    }
	else
		encr = "";
#else
	encr = password;
#endif  /* CRYPT_OPER_PASSWORD */
	if ((aconf->status & CONF_OPS) &&
	    StrEq(encr, aconf->passwd) && !attach_conf(sptr, aconf))
	    {
		int old = (sptr->umodes & ALL_UMODES);
		char *s;
		s = index(aconf->host, '@');
		*s++ = '\0';
		if (!(aconf->port & OFLAG_ISGLOBAL))
			SetLocOp(sptr);
		else
			SetOper(sptr);
		sptr->oflag = aconf->port;
		*--s =  '@';
		sendto_ops("%s (%s!%s@%s) is now operator (%c)", parv[1],
			   parv[0], sptr->user->username,
			   sptr->user->host, IsOper(sptr) ? 'O' : 'o');
/*                sendto_serv_butone("%s (%s@%s) is now 
			   operator (%c)", parv[0],
                           sptr->user->username, sptr->user->host,
                           IsOper(sptr) ? 'O' : 'o');*/
		sptr->umodes |=
		(UMODE_SERVNOTICE|UMODE_WALLOP|UMODE_FAILOP|UMODE_FLOOD);
		send_umode_out(cptr, sptr, old);
 		sendto_one(sptr, rpl_str(RPL_YOUREOPER), me.name, parv[0]);
#if !defined(CRYPT_OPER_PASSWORD) && (defined(FNAME_OPERLOG) ||\
    (defined(USE_SYSLOG) && defined(SYSLOG_OPER)))
		encr = "";
#endif
#if defined(USE_SYSLOG) && defined(SYSLOG_OPER)
		syslog(LOG_INFO, "OPER (%s) (%s) by (%s!%s@%s)",
			name, encr,
			parv[0], sptr->user->username, sptr->sockhost);
#endif
#ifdef FNAME_OPERLOG
	      {
                int     logfile;
                /*
                 * This conditional makes the logfile active only after
                 * it's been created - thus logging can be turned off by
                 * removing the file.
                 *
                 * stop NFS hangs...most systems should be able to open a
                 * file in 3 seconds. -avalon (curtesy of wumpus)
                 */
                if (IsPerson(sptr) &&
                    (logfile = open(FNAME_OPERLOG, O_WRONLY|O_APPEND)) != -1)
		{
                        (void)sprintf(buf, "%s OPER (%s) (%s) by (%s!%s@%s)\n",
				      myctime(time(NULL)), name, encr,
				      parv[0], sptr->user->username,
				      sptr->sockhost);
		  (void)write(logfile, buf, strlen(buf));
		  (void)close(logfile);
		}
                /* Modification by pjg */
	      }
#endif
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr,
				      ":%s MODE %s :+o", parv[0], parv[0]);
#endif
	    }
	else
	    {
		(void)detach_conf(sptr, aconf);
		sendto_one(sptr,err_str(ERR_PASSWDMISMATCH),me.name, parv[0]);
#ifndef	SHOW_PASSWORD
#ifdef  FAILOPER_WARN
		sendto_one(sptr,":%s NOTICE :Your attempt has been logged.",me.name);
#endif
                  sendto_realops("Failed OPER attempt by %s (%s@%s) using UID %s [NOPASSWORD]",
                   parv[0], sptr->user->username, sptr->sockhost, name);
#else
		  sendto_realops("Failed OPER attempt by %s (%s@%s) [%s]",
		   parv[0], sptr->user->username, sptr->sockhost,password);
#endif
		  sendto_serv_butone(&me, ":%s GLOBOPS :Failed OPER attempt by %s (%s@%s) using UID %s [---]",
		    me.name, parv[0], sptr->user->username, sptr->sockhost, name);
		sptr->since += 7;
#ifdef FNAME_OPERLOG
              {
                int     logfile;
                /*
                 * This conditional makes the logfile active only after
                 * it's been created - thus logging can be turned off by
                 * removing the file.
                 *
                 * stop NFS hangs...most systems should be able to open a
                 * file in 3 seconds. -avalon (curtesy of wumpus)
                 */
                if (IsPerson(sptr) &&
                    (logfile = open(FNAME_OPERLOG, O_WRONLY|O_APPEND)) != -1)
                {
                        (void)sprintf(buf, "%s FAILED OPER (%s) (%s) by (%s!%s@%s)\n PASSWORD %s",
                                      myctime(time(NULL)), name, encr,
                                      parv[0], sptr->user->username,
                                      sptr->sockhost, password);
                  (void)write(logfile, buf, strlen(buf));
                  (void)close(logfile);
                }
                /* Modification by pjg */
              }
#endif
	    }
	return 0;
    }
/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/
/*
** m_pass
**	parv[0] = sender prefix
**	parv[1] = password
*/
int	m_pass(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char *password = parc > 1 ? parv[1] : NULL;
	if (BadPtr(password))
	    {
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "PASS");
		return 0;
	    }
	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	    {
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
			   me.name, parv[0]);
		return 0;
	    }
	strncpyzt(cptr->passwd, password, sizeof(cptr->passwd));
	return 0;
    }
/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
int	m_userhost(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
        int catsize;
	char	*p = NULL;
	aClient	*acptr;
	char	*s;
	char    *curpos;
	int	resid;
	if (check_registered(sptr))
		return 0;
	if (parc > 2)
		(void)m_userhost(cptr, sptr, parc-1, parv+1);
	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "USERHOST");
		return 0;
	    }
	/*
	 * use curpos to keep track of where we are in the output buffer,
	 * and use resid to keep track of the remaining space in the
	 * buffer
	 */
	curpos = buf;
	curpos += sprintf(curpos, rpl_str(RPL_USERHOST), me.name, parv[0]);
	resid = sizeof(buf) - (curpos - buf) - 1;  /* remaining space */
	/*
	 * for each user found, print an entry if it fits.
	 */
	for (s = strtoken(&p, parv[1], " "); s;
	     s = strtoken(&p, (char *)NULL, " "))
	  if ((acptr = find_person(s, NULL))) {
	    catsize = strlen(acptr->name)
	      + (IsAnOper(acptr) ? 1 : 0)
	      + 3 + strlen(acptr->user->username)
	      + strlen(acptr->user->host) + 1;
	    if (catsize <= resid) {
	      curpos += sprintf(curpos, "%s%s=%c%s@%s ",
				acptr->name,
				IsAnOper(acptr) ? "*" : "",
				(acptr->user->away) ? '-' : '+',
				acptr->user->username,
				acptr->user->host);
	      resid -= catsize;
	    }
	  }
	
	/*
	 * because of some trickery here, we might have the string end in
	 * "...:" or "foo " (note the trailing space)
	 * If we have a trailing space, nuke it here.
	 */
	curpos--;
	if (*curpos != ':')
	  *curpos = '\0';
	sendto_one(sptr, "%s", buf);
	return 0;
}
/*
 * m_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 */
int     m_ison(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
        char    namebuf[USERLEN+HOSTLEN+4];
        Reg1    aClient *acptr;
        Reg2    char    *s, **pav = parv, *user;
        Reg3    int     len;
        char    *p = NULL;
 
        if (check_registered(sptr))
                return 0;
 
        if (parc < 2)
	  {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "ISON");
                return 0;
	  }
 
        (void)sprintf(buf, rpl_str(RPL_ISON), me.name, *parv);
        len = strlen(buf);
 
        for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
	  {
                if(user=index(s, '!')) *user++='\0';
                if ((acptr = find_person(s, NULL)))
		  {
		    if (user) {
                                strcpy(namebuf, acptr->user->username);
                                strcat(namebuf, "@");
                                strcat(namebuf, acptr->user->host);
                                if(match(user, namebuf))
                                        continue;
                                *--user='!';
		    }
 
                        (void)strncat(buf, s, sizeof(buf) - len);
                        len += strlen(s);
                        (void)strncat(buf, " ", sizeof(buf) - len);
                        len++;
		  }
	  }
        sendto_one(sptr, "%s", buf);
        return 0;
}


static int user_modes[]	     = { UMODE_OPER, 'o',
				 UMODE_LOCOP, 'O',
				 UMODE_INVISIBLE, 'i',
				 UMODE_WALLOP, 'w',
				 UMODE_FAILOP, 'g',
				 UMODE_HELPOP, 'h',
				 UMODE_SERVNOTICE, 's',
				 UMODE_KILLS, 'k',
				 UMODE_SADMIN, 'a',
				 UMODE_ADMIN, 'A',
				 UMODE_CLIENT, 'c',
				 UMODE_FLOOD, 'f',
				 UMODE_REGNICK, 'r',
#ifdef CHATOPS
				 UMODE_CHATOP, 'b',
#endif
				 0, 0 };
/*
 * m_umode() added 15/10/91 By Darren Reed.
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
int	m_umode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	int	flag;
	Reg2	int	*s;
	Reg3	char	**p, *m;
	aClient	*acptr;
	int	what, setflags;
	if (check_registered_user(sptr))
		return 0;
	what = MODE_ADD;
	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "MODE");
		return 0;
	    }
	if (!(acptr = find_person(parv[1], NULL)))
	    {
		if (MyConnect(sptr))
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], parv[1]);
		return 0;
	    }
	if (IsServer(sptr) || sptr != acptr)
	    {
		if (IsServer(cptr))
			sendto_ops_butone(NULL, &me,
				  ":%s WALLOPS :MODE for User %s From %s!%s",
				  me.name, parv[1],
				  get_client_name(cptr, FALSE), sptr->name);
		else
			sendto_one(sptr, err_str(ERR_USERSDONTMATCH),
				   me.name, parv[0]);
			return 0;
	    }
 
	if (parc < 3)
	    {
		m = buf;
		*m++ = '+';
		for (s = user_modes; (flag = *s) && (m - buf < BUFSIZE - 4);
		     s += 2)
			if ((sptr->umodes & flag))
				*m++ = (char)(*(s+1));
		*m = '\0';
		sendto_one(sptr, rpl_str(RPL_UMODEIS),
			   me.name, parv[0], buf);
		return 0;
	    }
	/* find flags already set for user */
	setflags = 0;
	for (s = user_modes; (flag = *s); s += 2)
		if ((sptr->umodes & flag))
			setflags |= flag;
	/*
	 * parse mode change string(s)
	 */
	for (p = &parv[2]; p && *p; p++ )
		for (m = *p; *m; m++)
			switch(*m)
			{
			case '+' :
				what = MODE_ADD;
				break;
			case '-' :
				what = MODE_DEL;
				break;	
			/* we may not get these,
			 * but they shouldnt be in default
			 */
			case ' ' :
			case '\n' :
			case '\r' :
			case '\t' :
			case 'r'  :
				break;
                        case 'A'  :
				/* set auto +a if user is setting +A */
                                if (MyClient(sptr) && (what == MODE_ADD))
                                        sptr->umodes |= UMODE_SADMIN;
			default :
				for (s = user_modes; (flag = *s); s += 2)
					if (*m == (char)(*(s+1)))
				    {
					if (what == MODE_ADD)
						sptr->umodes |= flag;
					else
						sptr->umodes &= ~flag;	
					break;
				    }
				if (flag == 0 && MyConnect(sptr))
					sendto_one(sptr,
						err_str(ERR_UMODEUNKNOWNFLAG),
						me.name, parv[0]);
				break;
			}
	/*
	 * stop users making themselves operators too easily
	 */
	if (!(setflags & UMODE_OPER) && IsOper(sptr) && !IsServer(cptr))
		ClearOper(sptr);
	if (!(setflags & UMODE_LOCOP) && IsLocOp(sptr) && !IsServer(cptr))
		sptr->umodes &= ~UMODE_LOCOP;
#ifdef	USE_SERVICES
	if (IsOper(sptr) && !(setflags & UMODE_OPER))
		check_services_butone(SERVICE_WANT_OPER, sptr,
				      ":%s MODE %s :+o", parv[0], parv[0]);
	else if (!IsOper(sptr) && (setflags & UMODE_OPER))
		check_services_butone(SERVICE_WANT_OPER, sptr,
				      ":%s MODE %s :-o", parv[0], parv[0]);
#endif
	/*
	 *  Let only operators set HelpOp
	 * Helpops get all /quote help <mess> globals -Donwulff
	 */
	if (MyClient(sptr) && IsHelpOp(sptr) && !OPCanHelpOp(sptr))
		ClearHelpOp(sptr);
#ifdef CHATOPS
	/*
	 *  Only let operators set +b
	 *  +b users can send and recieve chatops
	 *  +b is preserved after a -o    - darkrot
	 */
	if (MyClient(sptr) && SendChatops(sptr) && !IsAnOper(sptr)
		&& !(setflags & (UMODE_OPER|UMODE_LOCOP)))
		ClearChatops(sptr);
#endif

	/*
	 * Let only operators set FloodF, ClientF; also
	 * remove those flags if they've gone -o/-O.
	 *  FloodF sends notices about possible flooding -Cabal95
	 *  ClientF sends notices about clients connecting or exiting
	 *  Admin is for server admins
	 *  SAdmin is for services admins (mode changers)
	 */
	if (!IsAnOper(sptr) && !IsServer(cptr))
	    {
		if (IsClientF(sptr))
			ClearClientF(sptr);
		if (IsFloodF(sptr))
			ClearFloodF(sptr);
		if (IsAdmin(sptr))
			ClearAdmin(sptr);
		if (IsSAdmin(sptr))
			ClearSAdmin(sptr);
	    }
	/*
	 * New oper access flags - Only let them set certian usermodes on
	 * themselves IF they have access to set that specific mode in their
	 * O:Line.
	 */
	if (MyClient(sptr) && IsAnOper(sptr))
	    {
	        if (IsClientF(sptr) && !OPCanUModeC(sptr))
	                ClearClientF(sptr);
	        if (IsFloodF(sptr) && !OPCanUModeF(sptr))
	                ClearFloodF(sptr);  
		if (IsAdmin(sptr) && !OPIsAdmin(sptr))
			ClearAdmin(sptr);
		if (IsSAdmin(sptr) && !OPIsSAdmin(sptr))
			ClearSAdmin(sptr);
	    }
	/*
	 * If I understand what this code is doing correctly...
	 *   If the user WAS an operator and has now set themselves -o/-O
	 *   then remove their access, d'oh!
	 * In order to allow opers to do stuff like go +o, +h, -o and
	 * remain +h, I moved this code below those checks. It should be
	 * O.K. The above code just does normal access flag checks. This
	 * only changes the operflag access level.  -Cabal95
	 */
	if ((setflags & (UMODE_OPER|UMODE_LOCOP)) && !IsAnOper(sptr) &&
	    MyConnect(sptr))
	    {
		det_confs_butmask(sptr, CONF_CLIENT & ~CONF_OPS);
		sptr->oflag = 0;
	    }
	/*
	 * compare new flags with old flags and send string which
	 * will cause servers to update correctly.
	 */
	send_umode_out(cptr, sptr, setflags);
	return 0;
}
/*
 * m_svsmode() added by taz
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
int	m_svsmode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	int	flag;
	Reg2	int	*s;
	Reg3	char	**p, *m;
	aClient	*acptr;
	int	what, setflags;
	if (!IsULine(cptr, sptr))
		return 0;
	what = MODE_ADD;
	if (parc < 3)
		return 0;
	if (!(acptr = find_person(parv[1], NULL)))
		return 0;
	setflags = 0;
	for (s = user_modes; (flag = *s); s += 2)
		if (acptr->umodes & flag)
			setflags |= flag;
	/*
	 * parse mode change string(s)
	 */
	for (p = &parv[2]; p && *p; p++ )
		for (m = *p; *m; m++)
			switch(*m)
			{
			case '+' :
				what = MODE_ADD;
				break;
			case '-' :
				what = MODE_DEL;
				break;	
			/* we may not get these,
			 * but they shouldnt be in default
			 */
			case ' ' :
			case '\n' :
			case '\r' :
			case '\t' :
				break;
			case 'l' :
				if(parv[3] && isdigit(*parv[3]))
					max_global_count = atoi(parv[3]);
				break;
			case 'd' :
				if(parv[3] && isdigit(*parv[3]))
					acptr->user->servicestamp = atol(parv[3]);
				break;
			default :
				for (s = user_modes; (flag = *s); s += 2)
					if (*m == (char)(*(s+1)))
				    {
					if (what == MODE_ADD)
						acptr->umodes |= flag;
					else
						acptr->umodes &= ~flag;	
					break;
				    }
				break;
			}
	if(parc > 3)
		sendto_serv_butone(cptr, ":%s SVSMODE %s %s %s",
			parv[0], parv[1], parv[2], parv[3]);
	else
		sendto_serv_butone(cptr, ":%s SVSMODE %s %s", parv[0],
			parv[1], parv[2]);
	return 0;
}
	
/*
 * send the MODE string for user (user) to connection cptr
 * -avalon
 */
void	send_umode(cptr, sptr, old, sendmask, umode_buf)
aClient *cptr, *sptr;
int	old, sendmask;
char	*umode_buf;
{
	Reg1	int	*s, flag;
	Reg2	char	*m;
	int	what = MODE_NULL;
	/*
	 * build a string in umode_buf to represent the change in the user's
	 * mode between the new (sptr->flag) and 'old'.
	 */
	m = umode_buf;
	*m = '\0';
	for (s = user_modes; (flag = *s); s += 2)
	    {
		if (MyClient(sptr) && !(flag & sendmask))
			continue;
		if ((flag & old) && !(sptr->umodes & flag))
		    {
			if (what == MODE_DEL)
				*m++ = *(s+1);
			else
			    {
				what = MODE_DEL;
				*m++ = '-';
				*m++ = *(s+1);
			    }
		    }
		else if (!(flag & old) && (sptr->umodes & flag))
		    {
			if (what == MODE_ADD)
				*m++ = *(s+1);
			else
			    {
				what = MODE_ADD;
				*m++ = '+';
				*m++ = *(s+1);
			    }
		    }
	    }
	*m = '\0';
	if (*umode_buf && cptr)
		sendto_one(cptr, ":%s %s %s :%s", sptr->name,
			   (IsToken(cptr)?TOK_MODE:MSG_MODE),
			   sptr->name, umode_buf);
}
/*
 * added Sat Jul 25 07:30:42 EST 1992
 */
void	send_umode_out(cptr, sptr, old)
aClient *cptr, *sptr;
int	old;
{
	Reg1    int     i;
	Reg2    aClient *acptr;
	send_umode(NULL, sptr, old, SEND_UMODES, buf);
# ifdef NPATH
        check_command((long)4, ":%s MODE %s :%s", sptr->name, sptr->name, buf);
# endif
	for (i = highest_fd; i >= 0; i--)
		if ((acptr = local[i]) && IsServer(acptr) &&
		    (acptr != cptr) && (acptr != sptr) && *buf)
			sendto_one(acptr, ":%s MODE %s :%s",
				   sptr->name, sptr->name, buf);
	if (cptr && MyClient(cptr))
		send_umode(cptr, sptr, old, ALL_UMODES, buf);
}
/*
 * added by taz
 */
void	send_svsmode_out(cptr, sptr, bsptr, old)
aClient *cptr, *sptr, *bsptr;
int	old;
{
	Reg1    int     i;
	Reg2    aClient *acptr;
	send_umode(NULL, sptr, old, SEND_UMODES, buf);
	sendto_serv_butone(acptr, ":%s SVSMODE %s :%s",
				   bsptr->name, sptr->name, buf);
/*	if (cptr && MyClient(cptr))
		send_umode(cptr, sptr, old, ALL_UMODES, buf);
*/
}
/***********************************************************************
 * m_silence() - Added 19 May 1994 by Run. 
 *
 ***********************************************************************/
/*
 * is_silenced : Does the actual check wether sptr is allowed
 *               to send a message to acptr.
 *               Both must be registered persons.
 * If sptr is silenced by acptr, his message should not be propagated,
 * but more over, if this is detected on a server not local to sptr
 * the SILENCE mask is sent upstream.
 */
static int is_silenced(sptr, acptr)
aClient *sptr;
aClient *acptr;
{ Reg1 Link *lp;
  Reg2 anUser *user;
  static char sender[HOSTLEN+NICKLEN+USERLEN+5];
  if (!(acptr->user) || !(lp = acptr->user->silence) ||
      !(user = sptr->user)) return 0;
  sprintf(sender,"%s!%s@%s",sptr->name,user->username,user->host);
  for (; lp; lp = lp->next)
  { if (!match(lp->value.cp, sender))
    { if (!MyConnect(sptr))
      { sendto_one(sptr->from, ":%s SILENCE %s :%s",acptr->name,
            sptr->name, lp->value.cp);
        lp->flags=1; }
      return 1; } }
  return 0;
}
int del_silence(sptr, mask)
aClient *sptr;
char *mask;
{ Reg1 Link **lp;
  Reg2 Link *tmp;
  for (lp = &(sptr->user->silence); *lp; lp = &((*lp)->next))
    if (mycmp(mask, (*lp)->value.cp)==0)
    { tmp = *lp;
      *lp = tmp->next;
      MyFree(tmp->value.cp);
      free_link(tmp);
      return 0; }
  return -1;
}
static int add_silence(sptr, mask)
aClient *sptr;
char *mask;
{ Reg1 Link *lp;
  Reg2 int cnt = 0, len = 0;
  for (lp = sptr->user->silence; lp; lp = lp->next)
  { len += strlen(lp->value.cp);
    if (MyClient(sptr))
      if ((len > MAXSILELENGTH) || (++cnt >= MAXSILES))
      { sendto_one(sptr, err_str(ERR_SILELISTFULL), me.name, sptr->name, mask);
	return -1; }
      else
      { if (!match(lp->value.cp, mask))
	  return -1; }
    else if (!mycmp(lp->value.cp, mask))
      return -1;
  }
  lp = make_link();
  bzero((char *)lp, sizeof(Link));
  lp->next = sptr->user->silence;
  lp->value.cp = (char *)MyMalloc(strlen(mask)+1);
  (void)strcpy(lp->value.cp, mask);
  sptr->user->silence = lp;
  return 0;
}
/*
** m_silence
**	parv[0] = sender prefix
** From local client:
**	parv[1] = mask (NULL sends the list)
** From remote client:
**	parv[1] = nick that must be silenced
**      parv[2] = mask
*/
int m_silence(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
  Link *lp;
  aClient *acptr;
  char c, *cp, *user, *host;
  if (check_registered_user(sptr)) return 0;
  if (MyClient(sptr))
  {
    acptr = sptr;
    if (parc < 2 || *parv[1]=='\0' || (acptr = find_person(parv[1], NULL)))
    { if (!(acptr->user)) return 0;
      for (lp = acptr->user->silence; lp; lp = lp->next)
	sendto_one(sptr, rpl_str(RPL_SILELIST), me.name,
	    sptr->name, acptr->name, lp->value.cp);
      sendto_one(sptr, rpl_str(RPL_ENDOFSILELIST), me.name, acptr->name);
      return 0; }
    cp = parv[1];
    c = *cp;
    if (c=='-' || c=='+') cp++;
    else if (!(index(cp, '@') || index(cp, '.') ||
	index(cp, '!') || index(cp, '*')))
    { sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
      return -1; }
    else c = '+';
    cp = pretty_mask(cp);
    if ((c=='-' && !del_silence(sptr,cp)) ||
        (c!='-' && !add_silence(sptr,cp)))
    { sendto_prefix_one(sptr, sptr, ":%s SILENCE %c%s", parv[0], c, cp);
      if (c=='-')
	sendto_serv_butone(NULL, ":%s SILENCE * -%s", sptr->name, cp);
    }
  }
  else if (parc < 3 || *parv[2]=='\0')
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SILENCE");
    return -1;
  }
  else if ((c = *parv[2])=='-' || (acptr = find_person(parv[1], NULL)))
  {
    if (c=='-')
    { if (!del_silence(sptr,parv[2]+1))
	sendto_serv_butone(cptr, ":%s SILENCE %s :%s",
	    parv[0], parv[1], parv[2]); }
    else
    { (void)add_silence(sptr,parv[2]);
      if (!MyClient(acptr))
        sendto_one(acptr, ":%s SILENCE %s :%s",
            parv[0], parv[1], parv[2]); }
  }
  else
  {
    sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
    return -1;
  }
  return 0;
}
int m_noshortn(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{	sendto_one(sptr, "NOTICE %s :*** Please use /nickserv for that command",sptr->name);
}
int m_noshortc(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{	sendto_one(sptr, "NOTICE %s :*** Please use /chanserv for that command",sptr->name);
}
int m_noshortm(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{	sendto_one(sptr, "NOTICE %s :*** Please use /memoserv for that command",sptr->name);
}
int m_noshorto(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{	sendto_one(sptr, "NOTICE %s :*** Please use /operserv for that command",sptr->name);
}
