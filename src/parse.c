/************************************************************************
 *   IRC - Internet Relay Chat, common/parse.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

/* -- Jto -- 03 Jun 1990
 * Changed the order of defines...
 */

#ifndef lint
static  char sccsid[] = "@(#)parse.c	2.33 1/30/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif
#include "struct.h"
#include "common.h"
#define MSGTAB
#include "msg.h"
#undef MSGTAB
#include "sys.h"
#include "numeric.h"
#include "h.h"

/*
 * NOTE: parse() should not be called recursively by other functions!
 */
static	char	*para[MAXPARA+1];

#ifdef	CLIENT_COMPILE
static	char	sender[NICKLEN+USERLEN+HOSTLEN+3];
char	userhost[USERLEN+HOSTLEN+2];
#else
static	char	sender[HOSTLEN+1];
static	int	cancel_clients PROTO((aClient *, aClient *, char *));
static	void	remove_unknown PROTO((aClient *, char *));
#endif

/*
**  Find a client (server or user) by name.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string and the search is the for server and user.
*/
#ifndef CLIENT_COMPILE
aClient *find_client(name, cptr)
char	*name;
Reg1	aClient *cptr;
    {
	if (name)
		cptr = hash_find_client(name, cptr);

	return cptr;
    }

aClient	*find_nickserv(name, cptr)
char	*name;
Reg1	aClient *cptr;
    {
	if (name)
		cptr = hash_find_nickserver(name, cptr);

	return cptr;
    }

#else
aClient *find_client(name, cptr)
char *name;
aClient *cptr;
    {
	Reg1 aClient *c2ptr = cptr;

	if (!name)
		return c2ptr;

	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next) 
		if (mycmp(name, c2ptr->name) == 0)
			return c2ptr;
	return cptr;
    }
#endif

/*
**  Find server by name.
**
**	This implementation assumes that server and user names
**	are unique, no user can have a server name and vice versa.
**	One should maintain separate lists for users and servers,
**	if this restriction is removed.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string.
*/
#ifndef CLIENT_COMPILE
aClient *find_server(name, cptr)
char	*name;
Reg1	aClient *cptr;
{
	if (name)
		cptr = hash_find_server(name, cptr);
	return cptr;
}

aClient *find_name(name, cptr)
char	*name;
aClient *cptr;
{
	Reg1 aClient *c2ptr = cptr;

	if (!collapse(name))
		return c2ptr;

	if ((c2ptr = hash_find_server(name, cptr)))
		return (c2ptr);
	if (!index(name, '*'))
		return c2ptr;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
		if (!IsServer(c2ptr) && !IsMe(c2ptr))
			continue;
		if (match(name, c2ptr->name) == 0)
			break;
		if (index(c2ptr->name, '*'))
			if (match(c2ptr->name, name) == 0)
					break;
	    }
	return (c2ptr ? c2ptr : cptr);
}
#else
aClient	*find_server(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg1	aClient *c2ptr = cptr;

	if (!collapse(name))
		return c2ptr;

	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
		if (!IsServer(c2ptr) && !IsMe(c2ptr))
			continue;
		if (match(c2ptr->name, name) == 0 ||
		    match(name, c2ptr->name) == 0)
			break;
	    }
	return (c2ptr ? c2ptr : cptr);
}
#endif

/*
**  Find person by (nick)name.
*/
aClient *find_person(name, cptr)
char	*name;
aClient *cptr;
    {
	Reg1	aClient	*c2ptr = cptr;

	c2ptr = find_client(name, c2ptr);

	if (c2ptr && IsClient(c2ptr) && c2ptr->user)
		return c2ptr;
	else
		return cptr;
    }

/*
 * parse a buffer.
 *
 * NOTE: parse() should not be called recusively by any other fucntions!
 */
int	parse(cptr, buffer, bufend, mptr)
aClient *cptr;
char	*buffer, *bufend;
struct	Message *mptr;
    {
	Reg1	aClient *from = cptr;
	Reg2	char *ch, *s;
	Reg3	int	len, i, numeric, paramcount, noprefix = 0;
#ifdef DEBUGMODE
	time_t	then, ticks;
	int	retval;
#endif

	Debug((DEBUG_DEBUG,"Parsing: %s", buffer));
#ifndef	CLIENT_COMPILE
	if (IsDead(cptr))
		return 0;
#endif

	s = sender;
	*s = '\0';
	for (ch = buffer; *ch == ' '; ch++)
		;
	para[0] = from->name;
	if (*ch == ':')
	    {
		/*
		** Copy the prefix to 'sender' assuming it terminates
		** with SPACE (or NULL, which is an error, though).
		*/
		for (++ch, i = 0; *ch && *ch != ' '; ++ch )
			if (s < (sender + sizeof(sender)-1))
				*s++ = *ch; /* leave room for NULL */
		*s = '\0';
#ifdef CLIENT_COMPILE
		if ((s = index(sender, '!')))
		    {
			*s++ = '\0';
			strncpyzt(userhost, s, sizeof(userhost));
		    }
		else if ((s = index(sender, '@')))
		    {
			*s++ = '\0';
			strncpyzt(userhost, s, sizeof(userhost));
		    }
#endif
		/*
		** Actually, only messages coming from servers can have
		** the prefix--prefix silently ignored, if coming from
		** a user client...
		**
		** ...sigh, the current release "v2.2PL1" generates also
		** null prefixes, at least to NOTIFY messages (e.g. it
		** puts "sptr->nickname" as prefix from server structures
		** where it's null--the following will handle this case
		** as "no prefix" at all --msa  (": NOTICE nick ...")
		*/
		if (*sender && IsServer(cptr))
		    {
 			from = find_client(sender, (aClient *) NULL);
			if (!from || match(from->name, sender))
				from = find_server(sender, (aClient *)NULL);
#ifndef	CLIENT_COMPILE
			else if (!from && index(sender, '@'))
				from = find_nickserv(sender, (aClient *)NULL);
#endif

			para[0] = sender;

			/* Hmm! If the client corresponding to the
			 * prefix is not found--what is the correct
			 * action??? Now, I will ignore the message
			 * (old IRC just let it through as if the
			 * prefix just wasn't there...) --msa
			 */
			if (!from)
			    {
				Debug((DEBUG_ERROR,
					"Unknown prefix (%s)(%s) from (%s)",
					sender, buffer, cptr->name));
				ircstp->is_unpf++;
#ifndef	CLIENT_COMPILE
				remove_unknown(cptr, sender);
#endif
				return -1;
			    }
			if (from->from != cptr)
			    {
				ircstp->is_wrdi++;
				Debug((DEBUG_ERROR,
					"Message (%s) coming from (%s)",
					buffer, cptr->name));
#ifndef	CLIENT_COMPILE
				return cancel_clients(cptr, from, ch);
#else
				return -1;
#endif
			    }
		    }
		while (*ch == ' ')
			ch++;
	    }
	else
	  noprefix = 1;
	if (*ch == '\0')
	    {
		ircstp->is_empt++;
		Debug((DEBUG_NOTICE, "Empty message from host %s:%s",
		      cptr->name, from->name));
		return(-1);
	    }
	/*
	** Extract the command code from the packet.  Point s to the end
	** of the command code and calculate the length using pointer
	** arithmetic.  Note: only need length for numerics and *all*
	** numerics must have paramters and thus a space after the command
	** code. -avalon
	*/
	s = (char *)index(ch, ' '); /* s -> End of the command code */
	len = (s) ? (s - ch) : 0;
	if (len == 3 &&
	    isdigit(*ch) && isdigit(*(ch + 1)) && isdigit(*(ch + 2)))
	    {
		mptr = NULL;
		numeric = (*ch - '0') * 100 + (*(ch + 1) - '0') * 10
			+ (*(ch + 2) - '0');
		paramcount = MAXPARA;
		ircstp->is_num++;
	    }
	else
	    {
		if (s)
			*s++ = '\0';
		if (ch[1] == '\0' && IsToken(cptr))
			mptr = msgmap[(u_char)*ch];
		else
			for (; mptr->cmd; mptr++) 
				if (mycmp(mptr->cmd, ch) == 0)
					break;

		if (!mptr->cmd)
		    {
			/*
			** Note: Give error message *only* to recognized
			** persons. It's a nightmare situation to have
			** two programs sending "Unknown command"'s or
			** equivalent to each other at full blast....
			** If it has got to person state, it at least
			** seems to be well behaving. Perhaps this message
			** should never be generated, though...  --msa
			** Hm, when is the buffer empty -- if a command
			** code has been found ?? -Armin
			*/
			if (buffer[0] != '\0')
			    {
				if (IsPerson(from))
					sendto_one(from,
					    ":%s %d %s %s :Unknown command",
					    me.name, ERR_UNKNOWNCOMMAND,
					    from->name, ch);
#ifdef	CLIENT_COMPILE
				Debug((DEBUG_ERROR,"Unknown (%s) from %s[%s]",
					ch, cptr->name, cptr->sockhost));
#else
				Debug((DEBUG_ERROR,"Unknown (%s) from %s",
					ch, get_client_name(cptr, TRUE)));
#endif
			    }
			ircstp->is_unco++;
			return(-1);
		    }
		paramcount = mptr->parameters;
		i = bufend - ch; /* Is this right? -Donwulff */
		mptr->bytes += i;
		if ((mptr->flags & 1) && !(IsServer(cptr) || IsService(cptr)))
			cptr->since += (2 + i / 120);
					/* Allow only 1 msg per 2 seconds
					 * (on average) to prevent dumping.
					 * to keep the response rate up,
					 * bursts of up to 5 msgs are allowed
					 * -SRB
					 */
	    }
	/*
	** Must the following loop really be so devious? On
	** surface it splits the message to parameters from
	** blank spaces. But, if paramcount has been reached,
	** the rest of the message goes into this last parameter
	** (about same effect as ":" has...) --msa
	*/

	/* Note initially true: s==NULL || *(s-1) == '\0' !! */

#ifdef	CLIENT_COMPILE
	if (me.user)
		para[0] = sender;
#endif
	i = 0;
	if (s)
	    {
		if (paramcount > MAXPARA)
			paramcount = MAXPARA;
		for (;;)
		    {
			/*
			** Never "FRANCE " again!! ;-) Clean
			** out *all* blanks.. --msa
			*/
			while (*s == ' ')
				*s++ = '\0';

			if (*s == '\0')
				break;
			if (*s == ':')
			    {
				/*
				** The rest is single parameter--can
				** include blanks also.
				*/
				para[++i] = s + 1;
				break;
			    }
			para[++i] = s;
			if (i >= paramcount)
				break;
			for (; *s != ' ' && *s; s++)
				;
		    }
	    }
	para[++i] = NULL;
	if (mptr == NULL)
		return (do_numeric(numeric, cptr, from, i, para));
	mptr->count++;
	if (IsRegisteredUser(cptr) &&
#ifdef	IDLE_FROM_MSG
	    mptr->func == m_private)
#else
	    mptr->func != m_ping && mptr->func != m_pong)
#endif
		from->user->last = time(NULL);

	/* Lame protocol 4 stuff... this if can be removed when all are 2.9 */
	if (noprefix && IsServer(cptr) && i >= 2 && mptr->func == m_squit &&
	    (!(from = find_server(para[1], (aClient *)NULL)) ||
	    from->from != cptr))
	{
	  Debug((DEBUG_DEBUG,"Ignoring protocol 4 \"%s %s %s ...\"",
	      para[0], para[1], para[2]));
	  return 0;
        }

#ifndef DEBUGMODE
	return (*mptr->func)(cptr, from, i, para);
#else
	then = clock();
	retval = (*mptr->func)(cptr, from, i, para);
	if (retval != FLUSH_BUFFER) {
		ticks = (clock()-then);
		if (IsServer(cptr))
			mptr->rticks += ticks;
		else
			mptr->lticks += ticks;
		cptr->cputime += ticks;
	}

	return retval;
#endif
    }

/*
 * field breakup for ircd.conf file.
 */
char	*getfield(newline)
char	*newline;
{
	static	char *line = NULL;
	char	*end, *field;
	
	if (newline)
		line = newline;
	if (line == NULL)
		return(NULL);

	field = line;
	if ((end = (char *)index(line,':')) == NULL)
	    {
		line = NULL;
		if ((end = (char *)index(field,'\n')) == NULL)
			end = field + strlen(field);
	    }
	else
		line = end + 1;
	*end = '\0';
	return(field);
}

#ifndef	CLIENT_COMPILE
static	int	cancel_clients(cptr, sptr, cmd)
aClient	*cptr, *sptr;
char	*cmd;
{
	char *cmdpriv;	
	/*
	 * kill all possible points that are causing confusion here,
	 * I'm not sure I've got this all right...
	 * - avalon
	 * No you didn't...
	 * - Run
	 */
	/* This little bit of code allowed paswords to nickserv to be 
	 * seen.  A definite no-no.  --Russell
	sendto_ops("Message (%s) for %s[%s!%s@%s] from %s", cmd,
		   sptr->name, sptr->from->name, sptr->from->username,
		   sptr->from->sockhost, get_client_name(cptr, TRUE));*/
	/*
	 * Incorrect prefix for a server from some connection.  If it is a
	 * client trying to be annoying, just QUIT them, if it is a server
	 * then the same deal.
	 */
	if (IsServer(sptr) || IsMe(sptr))
	    {
		/*
		 * First go at tracking down what really causes the
		 * dreaded Fake Direction error.  It should not be possible
		 * ever to happen.  Assume nothing here since this is an
		 * impossibility.
		 *
		 * Check for valid fields, then send out globops with
		 * the msg command recieved, who apperently sent it,
		 * where it came from, and where it was suppose to come
		 * from.  We send the msg command to find out if its some
		 * bug somebody found with an old command, maybe some
		 * weird thing like, /ping serverto.* serverfrom.* and on
		 * the way back, fake direction?  Don't know, maybe this
		 * will tell us.  -Cabal95
		 *
		 * Take #2 on Fake Direction.  Most of them seem to be
		 * numerics.  But sometimes its getting fake direction on
		 * SERVER msgs.. HOW??  Display the full message now to
		 * figure it out... -Cabal95
		 *
		 * Okay I give up.  Can't find it.  Seems like it will
		 * exist untill ircd is completely rewritten. :/ For now
		 * just completely ignore them.  Needs to be modified to
		 * send these messages to a special oper channel. -Cabal95
		 *
		aClient *from;
		char	*fromname=NULL, *sptrname=NULL, *cptrname=NULL, *s;

		while (*cmd == ' ')
			cmd++;
		if (s = index(cmd, ' '))
			*s++ = '\0';
		if (!strcasecmp(cmd, "PRIVMSG") ||
		    !strcasecmp(cmd, "NOTICE") ||
		    !strcasecmp(cmd, "PASS"))
			s = NULL;
		if (sptr && sptr->name)
			sptrname = sptr->name;
		if (cptr && cptr->name)
			cptrname = cptr->name;
		if (sptr && sptr->from && sptr->from->name)
			fromname = sptr->from->name;

		sendto_serv_butone(NULL, ":%s GLOBOPS :"
			"Fake Direction: Message[%s %s] from %s via %s "
			"instead of %s (Tell Cabal95)", me.name, cmd,
			(s ? s : ""),
			(sptr->name!=NULL)?sptr->name:"<unknown>",
			(cptr->name!=NULL)?cptr->name:"<unknown>",
			(fromname!=NULL)?fromname:"<unknown>");
		sendto_ops(
			"Fake Direction: Message[%s %s] from %s via %s "
			"instead of %s (Tell Cabal95)", cmd,
			(s ? s : ""),
			(sptr->name!=NULL)?sptr->name:"<unknown>",
			(cptr->name!=NULL)?cptr->name:"<unknown>",
			(fromname!=NULL)?fromname:"<unknown>");

		/*
		 * We don't drop the server anymore.  Just ignore
		 * the message and go about your business.  And hope
		 * we don't get flooded. :-)  -Cabal95
		sendto_ops("Dropping server %s", cptr->name);
		return exit_client(cptr, cptr, &me, "Fake Direction");
		 */
		return 0;
	    }
	/*
	 * Ok, someone is trying to impose as a client and things are
	 * confused.  If we got the wrong prefix from a server, send out a
	 * kill, else just exit the lame client.
	 */
	if (IsServer(cptr))
	    {
		/*
		** It is NOT necessary to send a KILL here...
		** We come here when a previous 'NICK new'
		** nick collided with an older nick, and was
		** ignored, and other messages still were on
		** the way (like the following USER).
		** We simply ignore it all, a purge will be done
		** automatically by the server 'cptr' as a reaction
		** on our 'NICK older'. --Run
		*/
		return 0; /* On our side, nothing changed */
	    }
	return exit_client(cptr, cptr, &me, "Fake prefix");
}

static	void	remove_unknown(cptr, sender)
aClient	*cptr;
char	*sender;
{
	if (!IsRegistered(cptr) || IsClient(cptr))
		return;
	/*
	 * Not from a server so don't need to worry about it.
	 */
	if (!IsServer(cptr))
		return;
	/*
	 * Do kill if it came from a server because it means there is a ghost
	 * user on the other server which needs to be removed. -avalon
	 */
	if (!index(sender, '.'))
		sendto_one(cptr, ":%s KILL %s :%s (%s(?) <- %s)",
			   me.name, sender, me.name, sender,
			   get_client_name(cptr, FALSE));
	else
		sendto_one(cptr, ":%s SQUIT %s :(Unknown from %s)",
			   me.name, sender, get_client_name(cptr, FALSE));
}
#endif
