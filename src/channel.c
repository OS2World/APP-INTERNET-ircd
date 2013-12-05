/************************************************************************
 *   IRC - Internet Relay Chat, ircd/channel.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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

/* -- Jto -- 09 Jul 1990
 * Bug fix
 */

/* -- Jto -- 03 Jun 1990
 * Moved m_channel() and related functions from s_msg.c to here
 * Many changes to start changing into string channels...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() from list.c
 */

#ifndef	lint
static	char sccsid[] = "@(#)channel.c	2.58 2/18/94 (C) 1990 University of Oulu, Computing\
 Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include "msg.h"	/* For TOK_*** and MSG_*** strings  */
#include "hash.h"	/* For CHANNELHASHSIZE */
#include "h.h"

aChannel *channel = NullChn;

static	void	add_invite PROTO((aClient *, aChannel *));
static	int	add_banid PROTO((aClient *, aChannel *, char *));
static	int	can_join PROTO((aClient *, aChannel *, char *));
static	void	channel_modes PROTO((aClient *, char *, char *, aChannel *));
static	int	check_channelmask PROTO((aClient *, aClient *, char *));
static	int	del_banid PROTO((aChannel *, char *));
static	int	find_banid PROTO((aChannel *, char *));
/*static	Link	*is_banned PROTO((aClient *, aChannel *));*/
static  int     have_ops PROTO((aChannel *));
static	int	number_of_zombies PROTO((aChannel *));
static  int     is_deopped PROTO((aClient *, aChannel *));
static	int	set_mode PROTO((aClient *, aClient *, aChannel *, int,\
			        char **, char *,char *, int *, int));
static	void	sub1_from_channel PROTO((aChannel *));

void	clean_channelname PROTO((char *));
void	del_invite PROTO((aClient *, aChannel *));

static	char	*PartFmt = ":%s PART %s";
static	char	*PartFmt2 = ":%s PART %s :%s";
/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static	char	nickbuf[BUFSIZE], buf[BUFSIZE];
static	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];

/*
 * return the length (>=0) of a chain of links.
 */
static	int	list_length(lp)
Reg1	Link	*lp;
{
	Reg2	int	count = 0;

	for (; lp; lp = lp->next)
		count++;
	return count;
}

/*
** find_chasing
**	Find the client structure for a nick name (user) using history
**	mechanism if necessary. If the client is not found, an error
**	message (NO SUCH NICK) is generated. If the client was found
**	through the history, chasing will be 1 and otherwise 0.
*/
static	aClient *find_chasing(sptr, user, chasing)
aClient *sptr;
char	*user;
Reg1	int	*chasing;
{
	Reg2	aClient *who = find_client(user, (aClient *)NULL);

	if (chasing)
		*chasing = 0;
	if (who)
		return who;
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	    {
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			   me.name, sptr->name, user);
		return NULL;
	    }
	if (chasing)
		*chasing = 1;
	return who;
}

/*
 * Ban functions to work with mode +b
 */
/* add_banid - add an id to be banned to the channel  (belongs to cptr) */

static	int	add_banid(cptr, chptr, banid)
aClient	*cptr;
aChannel *chptr;
char	*banid;
{
	Reg1	Ban	*ban;
	Reg2	int	cnt = 0, len = 0;

	if (MyClient(cptr))
		(void)collapse(banid);
	for (ban = chptr->banlist; ban; ban = ban->next)
	    {
		len += strlen(ban->banstr);
		if (MyClient(cptr))
			if ((len > MAXBANLENGTH) || (++cnt >= MAXBANS))
			    {
				sendto_one(cptr, err_str(ERR_BANLISTFULL),
					   me.name, cptr->name,
					   chptr->chname, banid);
				return -1;
			    }
			else
			    {
				if (!match(ban->banstr, banid) ||
				    !match(banid, ban->banstr))
					return -1;
			    }
		else if (!mycmp(ban->banstr, banid))
			return -1;
		
	    }
	ban = make_ban();
	bzero((char *)ban, sizeof(Ban));
/*	ban->flags = CHFL_BAN;			They're all bans!! */
	ban->next = chptr->banlist;
	ban->banstr = (char *)MyMalloc(strlen(banid)+1);
	(void)strcpy(ban->banstr, banid);
	ban->who = (char *)MyMalloc(strlen(cptr->name)+1);
	(void)strcpy(ban->who, cptr->name);
	ban->when = time(NULL);
	chptr->banlist = ban;
	return 0;
}
/*
 * del_banid - delete an id belonging to cptr
 */
static	int	del_banid(chptr, banid)
aChannel *chptr;
char	*banid;
{
	Reg1 Ban **ban;
	Reg2 Ban *tmp;

	if (!banid)
		return -1;
 	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (mycmp(banid, (*ban)->banstr)==0)
		    {
			tmp = *ban;
			*ban = tmp->next;
			MyFree(tmp->banstr);
			MyFree(tmp->who);
			free_ban(tmp);
			return 0;
		    }
	return -1;
}

/*
 * find_banid - Find an exact match for a ban
 */
static	int	find_banid(chptr, banid)
aChannel *chptr;
char	*banid;
{
	Reg1 Ban **ban;
	Reg2 Ban *tmp;

	if (!banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (!mycmp(banid, (*ban)->banstr)) return 1;
	return 0;
}

/*
 * IsMember - returns 1 if a person is joined and not a zombie
 */
int	IsMember(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{ Link *lp;
	/* Zombies have been removed */
	return (((lp=find_user_link(chptr->members, cptr)) /* &&
			!(lp->flags & CHFL_ZOMBIE)*/)?1:0);
}

/*
 * is_banned - returns a pointer to the ban structure if banned else NULL
 */
extern	Ban	*is_banned(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Ban	*tmp;
	char	*s;

	if (!IsPerson(cptr))
		return NULL;

	s = make_nick_user_host(cptr->name, cptr->user->username,
				  cptr->user->host);

	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
		if (match(tmp->banstr, s) == 0)
			break;
	return (tmp);
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
static	void	add_user_to_channel(chptr, who, flags)
aChannel *chptr;
aClient *who;
int	flags;
{
	Reg1	Link *ptr;

	if (who->user)
	    {
		ptr = make_link();
		ptr->value.cptr = who;
		ptr->flags = flags;
		ptr->next = chptr->members;
		chptr->members = ptr;
		chptr->users++;

		ptr = make_link();
		ptr->value.chptr = chptr;
		ptr->next = who->user->channel;
		who->user->channel = ptr;
		who->user->joined++;
	    }
}

void	remove_user_from_channel(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
	Reg1	Link	**curr;
	Reg2	Link	*tmp;
	Reg3	Link	*lp = chptr->members;

	for (; lp && (lp->flags & CHFL_ZOMBIE || lp->value.cptr==sptr);
	    lp=lp->next);
	for (;;)
	{
	  for (curr = &chptr->members; (tmp = *curr); curr = &tmp->next)
		  if (tmp->value.cptr == sptr)
		      {
			  *curr = tmp->next;
			  free_link(tmp);
			  break;
		      }
	  for (curr = &sptr->user->channel; (tmp = *curr); curr = &tmp->next)
		  if (tmp->value.chptr == chptr)
		      {
			  *curr = tmp->next;
			  free_link(tmp);
			  break;
		      }
	  sptr->user->joined--;
	  if (lp) break;
	  if (chptr->members) sptr = chptr->members->value.cptr;
	  else break;
	  sub1_from_channel(chptr);
	}
	sub1_from_channel(chptr);
}


static	int	have_ops(chptr)
aChannel *chptr;
{
	Reg1	Link	*lp;

	if (chptr)
        {
	  lp=chptr->members;
	  while (lp)
	  {
	    if (lp->flags & CHFL_CHANOP) return(1);
	    lp = lp->next;
	  }
        }
	return 0;
}

int	is_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;

	if (chptr)
		if (lp = find_user_link(chptr->members, cptr))
			return (lp->flags & CHFL_CHANOP);

	return 0;
}

/* This was the original function that allowed mode hacking - not
   anymore. -- Barubary */
static	int	is_deopped(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	if (!IsPerson(cptr)) return 0;
	return !is_chan_op(cptr, chptr);
}

int	is_zombie(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;

/* We have no Zombies...
	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_ZOMBIE);
*/
	return 0;
}

int	has_voice(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)) &&
		    !(lp->flags & CHFL_ZOMBIE))
			return (lp->flags & CHFL_VOICE);

	return 0;
}

int	can_send(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;
	Reg2	int	member;

	/* Moved check here, kinda faster.
	 * Note IsULine only uses the other parameter. -Donwulff */
	if (IsULine(cptr,cptr)||IsServer(cptr)) return 0;

	member = IsMember(cptr, chptr);
	lp = find_user_link(chptr->members, cptr);

	if (chptr->mode.mode & MODE_MODERATED &&
	    (!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE)) ||
	    (lp->flags & CHFL_ZOMBIE)))
			return (MODE_MODERATED);

	if (chptr->mode.mode & MODE_NOPRIVMSGS && !member)
		return (MODE_NOPRIVMSGS);

if ((!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE)) ||
    (lp->flags & CHFL_ZOMBIE)) && MyClient(cptr) &&
    is_banned(cptr, chptr))
	return (MODE_BAN);

	return 0;
}

aChannel *find_channel(chname, chptr)
Reg1	char	*chname;
Reg2	aChannel *chptr;
{
	return hash_find_channel(chname, chptr);
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
static	void	channel_modes(cptr, mbuf, pbuf, chptr)
aClient	*cptr;
Reg1	char	*mbuf, *pbuf;
aChannel *chptr;
{
	*mbuf++ = '+';
	if (chptr->mode.mode & MODE_SECRET)
		*mbuf++ = 's';
	else if (chptr->mode.mode & MODE_PRIVATE)
		*mbuf++ = 'p';
	if (chptr->mode.mode & MODE_MODERATED)
		*mbuf++ = 'm';
	if (chptr->mode.mode & MODE_TOPICLIMIT)
		*mbuf++ = 't';
	if (chptr->mode.mode & MODE_INVITEONLY)
		*mbuf++ = 'i';
	if (chptr->mode.mode & MODE_NOPRIVMSGS)
		*mbuf++ = 'n';
	if (chptr->mode.mode & MODE_RGSTR)
		*mbuf++ = 'r';
	if (chptr->mode.mode & MODE_RGSTRONLY)
		*mbuf++ = 'R';
	if (chptr->mode.limit)
	    {
		*mbuf++ = 'l';
		if (IsMember(cptr, chptr) || IsServer(cptr) || IsULine(cptr,cptr))
			(void)sprintf(pbuf, "%d ", chptr->mode.limit);
	    }
	if (*chptr->mode.key)
	    {
		*mbuf++ = 'k';
		if (IsMember(cptr, chptr) || IsServer(cptr) || IsULine(cptr,cptr))
			(void)strcat(pbuf, chptr->mode.key);
	    }
	*mbuf++ = '\0';
	return;
}

static	int send_mode_list(cptr, chname, creationtime, top, mask, flag)
aClient	*cptr;
Link	*top;
int	mask;
char	flag, *chname;
time_t	creationtime;
{
	Reg1	Link	*lp;
	Reg2	char	*cp, *name;
	int	count = 0, send = 0, sent = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)	/* mode +l or +k xx */
		count = 1;
	for (lp = top; lp; lp = lp->next)
	    {
		/* 
		 * Okay, since ban's are stored in their own linked
		 * list, we won't even bother to check if CHFL_BAN
		 * is set in the flags. This should work as long
		 * as only ban-lists are feed in with CHFL_BAN mask.
		 * However, we still need to typecast... -Donwulff 
		 */
		if (mask == CHFL_BAN) {
/*			if (!(((Ban *)lp)->flags & mask)) continue; */
			name = ((Ban *)lp)->banstr;
		} else {
			if (!(lp->flags & mask)) continue;
			name = lp->value.cptr->name;
		}
		if (strlen(parabuf) + strlen(name) + 11 < (size_t) MODEBUFLEN)
		    {
			if(*parabuf) (void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = flag;
			*cp = '\0';
		    }
		else if (*parabuf)
			send = 1;
		if (count == RESYNCMODES)
			send = 1;
		if (send)
		    {
		       /* cptr is always a server! So we send creationtimes */
			sendmodeto_one(cptr, me.name, chname, modebuf,
				   parabuf, creationtime);
                        sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != RESYNCMODES)
			    {
				(void)strcpy(parabuf, name);
				*cp++ = flag;
			    }
			count = 0;
			*cp = '\0';
		    }
	    }
     return sent;
}

/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void	send_channel_modes(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{       int sent;
	if (*chptr->chname != '#')
		return;

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	sent=send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_CHANOP, 'o');
	if (!sent && chptr->creationtime)
	  sendto_one(cptr, ":%s %s %s %s %s %lu", me.name,
	      (IsToken(cptr)?TOK_MODE:MSG_MODE), chptr->chname, modebuf,
	      parabuf, chptr->creationtime);
	else if (modebuf[1] || *parabuf)
	  sendmodeto_one(cptr, me.name,
	      chptr->chname, modebuf, parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname,chptr->creationtime,
	    chptr->banlist, CHFL_BAN, 'b');
	if (modebuf[1] || *parabuf)
	  sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
	      parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname,chptr->creationtime,
	    chptr->members, CHFL_VOICE, 'v');
	if (modebuf[1] || *parabuf)
	  sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
	      parabuf, chptr->creationtime);
}

/*
 * m_samode
 * parv[0] = sender
 * parv[1] = channel
 * parv[2] = modes
 * -taz
 */
int	m_samode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	static char tmp[MODEBUFLEN];
	int badop, sendts;
	aChannel *chptr;

	if (check_registered(cptr))
		return 0;

	if (!IsPrivileged(cptr)) {
	  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
	  return 0;
	}

	if(!IsSAdmin(cptr) || parc < 2)
		return 0;

	chptr = find_channel(parv[1], NullChn);
	if (chptr == NullChn)
		return 0;

	sptr->flags&=~FLAGS_TS8;

	clean_channelname(parv[1]);

	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	sendts = set_mode(cptr, sptr, chptr, parc - 2, parv + 2,
		modebuf, parabuf, &badop, 1);

	if (strlen(modebuf) > (size_t)1 || sendts > 0)
	{   if (badop!=2 && strlen(modebuf) > (size_t)1)
	      sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
	          me.name, chptr->chname, modebuf, parabuf);

	      sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s 1",
	          parv[0], chptr->chname, modebuf, parabuf);

	if(MyClient(sptr)) {
	    sendto_serv_butone(&me, ":%s GLOBOPS :%s used SAMODE (%s %s %s)", 
		me.name, sptr->name, chptr->chname, modebuf, parabuf);
	     sendto_failops_whoare_opers("from %s: %s used SAMODE (%s %s %s)", 
		me.name, sptr->name, chptr->chname, modebuf, parabuf);
	}
    }
	return 0;
}

/*
 * m_mode
 * parv[0] - sender
 * parv[1] - channel
 */
int	m_mode(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
	static char tmp[MODEBUFLEN];
	int badop, sendts;
	aChannel *chptr;

	if (check_registered(sptr))
		return 0;

	/* Now, try to find the channel in question */
	if (parc > 1)
	    {
		chptr = find_channel(parv[1], NullChn);
		if (chptr == NullChn)
			return m_umode(cptr, sptr, parc, parv);
	    }
	else
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "MODE");
 	 	return 0;
	    }

	sptr->flags&=~FLAGS_TS8;

	if (MyConnect(sptr))
		clean_channelname(parv[1]);
	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	if (parc < 3)
	    {
		*modebuf = *parabuf = '\0';
		modebuf[1] = '\0';
		channel_modes(sptr, modebuf, parabuf, chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, parv[0],
			   chptr->chname, modebuf, parabuf);
		sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name, parv[0],
				 chptr->chname, chptr->creationtime);
		return 0;
	    }

	if (!(sendts = set_mode(cptr, sptr, chptr, parc - 2, parv + 2,
	    modebuf, parabuf, &badop, 0)) && !IsULine(cptr,sptr))
	    {
		sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			   me.name, parv[0], chptr->chname);
		return 0;
	    }

	if ((badop>=2) && !IsULine(cptr,sptr))
	{
	  int i=3;
	  *tmp='\0';
	  while (i < parc)
	  { strcat(tmp, " ");
	    strcat(tmp, parv[i++]); }
/* I hope I've got the fix *right* this time.  --Russell
	  sendto_ops("%sHACK(%d): %s MODE %s %s%s [%lu]",
	      (badop==3)?"BOUNCE or ":"", badop,
	      parv[0],parv[1],parv[2],tmp,chptr->creationtime);*/
	}

	if (strlen(modebuf) > (size_t)1 || sendts > 0)
	{   if (badop!=2 && strlen(modebuf) > (size_t)1)
	      sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
	          parv[0], chptr->chname, modebuf, parabuf);
            /* We send a creationtime of 0, to mark it as a hack --Run */
	    if ((IsServer(sptr) && (badop==2 || sendts > 0)) ||
		 IsULine(cptr,sptr))
	    { if (*modebuf == '\0') strcpy(modebuf,"+");
	      if (badop==2)
		badop = 2;
/*	        sendto_serv_butone(cptr, ":%s WALLOPS :HACK: %s MODE %s %s%s",
	            me.name,parv[0],parv[1],parv[2],tmp);*/
	      else
	        sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s %lu",
		    parv[0], chptr->chname, modebuf, parabuf,
		    (badop==4)?(time_t)0:chptr->creationtime); }
            else
	      sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s",
	          parv[0], chptr->chname, modebuf, parabuf);
	}
	return 0;
}

int DoesOp(modebuf)
char *modebuf;
{
  modebuf--; /* Is it possible that a mode starts with o and not +o ? */
  while (*++modebuf) if (*modebuf=='o' || *modebuf=='v') return(1);
  return 0;
}

int sendmodeto_one(cptr, from, name, mode, param, creationtime)
Reg2 aClient *cptr;
char *from,*name,*mode,*param;
time_t creationtime;
{
	if ((IsServer(cptr) && DoesOp(mode) && creationtime) ||
 		IsULine(cptr,cptr))
	  sendto_one(cptr,":%s %s %s %s %s %lu", from,
	      (IsToken(cptr)?TOK_MODE:MSG_MODE), name, mode, param,
	      creationtime);
	else
	  sendto_one(cptr,":%s %s %s %s %s", from,
	      (IsToken(cptr)?TOK_MODE:MSG_MODE), name, mode, param);
}

char *pretty_mask(mask)
char *mask;
{ Reg1 char *cp;
  Reg2 char *user;
  Reg3 char *host;

  if ((user = index((cp = mask), '!'))) *user++ = '\0';
  if ((host = rindex(user ? user : cp, '@')))
  { *host++ = '\0';
    if (!user) return make_nick_user_host(NULL, cp, host); }
  else if (!user && index(cp, '.')) return make_nick_user_host(NULL, NULL, cp);
  return make_nick_user_host(cp, user, host);
}

/*
 * Check and try to apply the channel modes passed in the parv array for
 * the client ccptr to channel chptr.  The resultant changes are printed
 * into mbuf and pbuf (if any) and applied to the channel.
 */
static	int	set_mode(cptr, sptr, chptr, parc, parv, mbuf, pbuf, badop, sadmin)
Reg2	aClient *cptr, *sptr;
aChannel *chptr;
int	parc, *badop, sadmin;
char	*parv[], *mbuf, *pbuf;
{
	static	Link	chops[MAXMODEPARAMS];
	static	int	flags[] = {
				MODE_PRIVATE,    'p', MODE_SECRET,     's',
				MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
				MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
				MODE_VOICE,      'v', MODE_KEY,        'k',
				MODE_RGSTR,	 'r', MODE_RGSTRONLY,  'R',
				0x0, 0x0 };

	Link	*lp;
	Ban	*ban;
	char	*curr = parv[0], *cp;
	int	*ip;
	Link    *member, *tmp = NULL;
	u_int	whatt = MODE_ADD, bwhatt = 0;
	int	limitset = 0, chasing = 0, bounce;
	int	nusers, new, len, blen, keychange = 0, opcnt = 0, banlsent = 0;
	int     doesdeop = 0, doesop = 0, hacknotice = 0, change, gotts = 0;
	char	fm = '\0';
	aClient *who;
	Mode	*mode, oldm;
	char	chase_mode[3];
	static  char bmodebuf[MODEBUFLEN], bparambuf[MODEBUFLEN], numeric[16];
        char    *bmbuf = bmodebuf, *bpbuf = bparambuf, *mbufp = mbuf;
	time_t	newtime = (time_t)0;
	aConfItem *aconf;

	*mbuf=*pbuf=*bmbuf=*bpbuf='\0';
	*badop=0;
	if (parc < 1)
		return 0;
	mode = &(chptr->mode);
	bcopy((char *)mode, (char *)&oldm, sizeof(Mode));
        /* Mode is accepted when sptr is a channel operator
	 * but also when the mode is received from a server. --Run
	 */

/*
 * This was modified to allow non-chanops to see bans..perhaps not as clean
 * as other versions, but it works. Basically, if a person who's not a chanop
 * calls set_mode() with a /mode <some parameter here> command, this function
 * checks to see if that parameter is +b, -b, or just b. If it is one of
 * these, and there are NO other parameters (i.e. no multiple mode changes),
 * then it'll display the banlist. Else, it just returns "You're not chanop."
 * Enjoy. --dalvenjah, dalvenja@rahul.net
 */

	if (!(IsServer(cptr) || IsULine(cptr,sptr) || is_chan_op(sptr, chptr) || sadmin == 1))
		{
			if ( ((*curr=='b' && (strlen(parv[0])==1 && parc==1))
			      || ((*curr=='+') && (*(curr+1)=='b') && (strlen(parv[0])==2 && parc==1))
			      || ((*curr=='-') && (*(curr+1)=='b') && (strlen(parv[0])==2 && parc==1)))
			    && (!is_chan_op(sptr, chptr)))
			    {
				if (--parv <= 0)
					{
					for (ban=chptr->banlist;ban;ban=ban->next)
						sendto_one(cptr,
							   rpl_str(RPL_BANLIST),
							   me.name, cptr->name,
							   chptr->chname,
							   ban->banstr,
							   ban->who,
							   ban->when);
						sendto_one(cptr,
							   rpl_str(RPL_ENDOFBANLIST),
							   me.name, cptr->name,
							   chptr->chname);
					}
			    }
			else
			{
				return 0;
			}
		}

	new = mode->mode;

	while (curr && *curr)
	    {
		switch (*curr)
		{
		case '+':
			whatt = MODE_ADD;
			break;
		case '-':
			whatt = MODE_DEL;
			break;
		case 'o' :
		case 'v' :
			if (--parc <= 0)
				break;
			parv++;
			*parv = check_string(*parv);
			if (MyClient(sptr) && opcnt >= MODEPARAMS)
				break;
			if (opcnt >= MAXMODEPARAMS)
				break;
			/*
			 * Check for nickname changes and try to follow these
			 * to make sure the right client is affected by the
			 * mode change.
			 */
			if (!(who = find_chasing(sptr, parv[0], &chasing)))
				break;
	  		if (!(member = find_user_link(chptr->members,who)))
			    {
	    			sendto_one(cptr, err_str(ERR_USERNOTINCHANNEL),
					   me.name, cptr->name,
					   parv[0], chptr->chname);
				break;
			    }
			if (whatt == MODE_ADD)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				if ((IsServer(sptr) &&
				    (!(who->flags & FLAGS_TS8) ||
				    ((*curr == 'o') && !(member->flags &
				    (CHFL_SERVOPOK|CHFL_CHANOP))) ||
				    who->from != sptr->from)) ||
				    IsULine(cptr,sptr) || sadmin == 1 ||
					(!MyClient(sptr) && IsSAdmin(sptr)))
				  *badop=((member->flags & CHFL_DEOPPED) &&
				      (*curr == 'o'))?2:3;
				lp->flags = (*curr == 'o') ? MODE_CHANOP:
					                     MODE_VOICE;
				lp->flags |= MODE_ADD;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				doesdeop = 1; /* Also when -v */
				lp->flags = (*curr == 'o') ? MODE_CHANOP:
							     MODE_VOICE;
				lp->flags |= MODE_DEL;
			    }
			if (*curr == 'o')
			  doesop=1;
			break;
		case 'k':
			if (--parc <= 0)
				break;
			parv++;
			/* check now so we eat the parameter if present */
			if (keychange)
				break;
			*parv = check_string(*parv);
			{
				u_char	*s1,*s2;

				for (s1 = s2 = (u_char *)*parv; *s2; s2++)
				  if ((*s1 = *s2 & 0x7f) > (u_char)32 &&
				      *s1 != ':') s1++;
				*s1 = '\0';
			}
			if (MyClient(sptr) && opcnt >= MODEPARAMS)
				break;
			if (opcnt >= MAXMODEPARAMS)
				break;

			if (whatt == MODE_ADD)
			    {
				if (*mode->key && !IsServer(cptr) &&
					!IsULine(cptr,sptr) && sadmin == 0 
						&& !IsSAdmin(sptr))
					sendto_one(cptr, err_str(ERR_KEYSET),
						   me.name, cptr->name,
						   chptr->chname);
				else if (!*mode->key || IsServer(cptr) ||
					IsULine(cptr,sptr) || sadmin == 1 
					   || (!MyClient(sptr) && IsSAdmin(sptr)))
				    {
					lp = &chops[opcnt++];
					lp->value.cp = *parv;
					if (strlen(lp->value.cp) >
					    (size_t) KEYLEN)
						lp->value.cp[KEYLEN] = '\0';
					lp->flags = MODE_KEY|MODE_ADD;
					keychange = 1;
				    }
			    }
			else if (whatt == MODE_DEL)
			    {
				if (mycmp(mode->key, *parv) == 0 ||
				    IsServer(cptr) || IsULine(cptr,sptr) || 
					sadmin == 1 || (!MyClient(sptr) && IsSAdmin(sptr)))
				    {
					lp = &chops[opcnt++];
					lp->value.cp = mode->key;
					lp->flags = MODE_KEY|MODE_DEL;
					keychange = 1;
				    }
			    }
			break;
		case 'b':
			if (--parc <= 0)
			    {
                                if (banlsent) /* Only send it once */
                                  break;
				for (ban = chptr->banlist; ban; ban = ban->next)
					sendto_one(cptr, rpl_str(RPL_BANLIST),
					     me.name, cptr->name,
						   chptr->chname,
						   ban->banstr,
						   ban->who,
						   ban->when);
				sendto_one(cptr, rpl_str(RPL_ENDOFBANLIST),
					   me.name, cptr->name, chptr->chname);
                                banlsent = 1;
				break;
			    }
			parv++;
			if (BadPtr(*parv))
				break;
			if (MyClient(sptr) && opcnt >= MODEPARAMS)
				break;
			if (opcnt >= MAXMODEPARAMS)
				break;
			if (whatt == MODE_ADD)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_ADD|MODE_BAN;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_DEL|MODE_BAN;
			    }
			break;
		case 'l':
			/*
			 * limit 'l' to only *1* change per mode command but
			 * eat up others.
			 */
			if (limitset)
			    {
				if (whatt == MODE_ADD && --parc > 0)
					parv++;
				break;
			    }
			if (whatt == MODE_DEL)
			    {
				limitset = 1;
				nusers = 0;
				break;
			    }
			if (--parc > 0)
			    {
				if (BadPtr(*parv))
					break;
				if (MyClient(sptr) && opcnt >= MODEPARAMS)
					break;
				if (opcnt >= MAXMODEPARAMS)
					break;
				if (!(nusers = atoi(*++parv)))
					continue;
				lp = &chops[opcnt++];
				lp->flags = MODE_ADD|MODE_LIMIT;
				limitset = 1;
				break;
			    }
			sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
					   me.name, cptr->name, "MODE +l");
			break;
		case 'r' :
			if(IsServer(cptr)) {
				if(whatt == MODE_DEL)
					new &= ~MODE_RGSTR;
				else
					new |= MODE_RGSTR;
			} else
	    			sendto_one(cptr, err_str(ERR_ONLYSERVERSCANCHANGE),
				    	me.name, cptr->name, chptr->chname);
			break;
		case 'i' : /* falls through for default case */
			if (whatt == MODE_DEL)
				while (lp = chptr->invites)
					del_invite(lp->value.cptr, chptr);
		default:
			for (ip = flags; *ip; ip += 2)
				if (*(ip+1) == *curr)
					break;

			if (*ip)
			    {
				if (whatt == MODE_ADD)
				    {
					if (*ip == MODE_PRIVATE)
						new &= ~MODE_SECRET;
					else if (*ip == MODE_SECRET)
						new &= ~MODE_PRIVATE;
					new |= *ip;
				    }
				else
					new &= ~*ip;
			    }
			else if (!IsServer(cptr) && !IsULine(cptr,sptr))
				sendto_one(cptr, err_str(ERR_UNKNOWNMODE),
					    me.name, cptr->name, *curr);
			break;
		}
		curr++;
		/*
		 * Make sure mode strings such as "+m +t +p +i" are parsed
		 * fully.
		 */
		if (!*curr && parc > 0)
		    {
			curr = *++parv;
			parc--;
			/* If this was from a server, and it is the last
			 * parameter and it starts with a digit, it must
			 * be the creationtime.  --Run
			 */
			if (IsServer(sptr) || IsULine(cptr,sptr))
			{ if (parc==1 && isdigit(*curr))
			  { 
			    newtime=atoi(curr);
			    gotts=1;
			    if (newtime == 0)
			    { *badop=2;
			      hacknotice = 1; }
			    else if (newtime > chptr->creationtime)
			    { /* It is a net-break ride if we have ops.
			       * bounce modes if we have ops.
			       * --Run
			       */
			      if (doesdeop) *badop=2;
			      else if (chptr->creationtime==0 ||
			      	  !have_ops(chptr))
			      { if (chptr->creationtime && doesop)
				  ;
/*			          sendto_ops("NET.RIDE on opless %s from %s",
				      chptr->chname,sptr->name);*/
				if (chptr->creationtime == 0 || doesop)
				  chptr->creationtime=newtime;
				*badop=0; }
			      /* Bounce: */
			      else *badop=1;
			    }
			    else if (doesdeop && newtime < chptr->creationtime)
			      *badop=2;
			    /* A legal *badop can occur when two
			     * people join simultaneously a channel,
			     * Allow for 10 min of lag (and thus hacking
			     * on channels younger then 10 min) --Run
			     */
			    else if (*badop==0 ||
				chptr->creationtime > (time(NULL)-(time_t)600))
			    { if (doesop || !have_ops(chptr))
			        chptr->creationtime=newtime;
		              *badop=0; }
			    break;
			  } }
                        else *badop=0;
		    }
	    } /* end of while loop for MODE processing */

	if (doesop && newtime==0 && (IsServer(sptr) || IsULine(cptr,sptr)
		|| sadmin == 1 || (!MyClient(sptr) && IsSAdmin(sptr))))
	  *badop=2;

	if (*badop>=2 &&
	    (aconf = find_conf_host(cptr->confs, sptr->name, CONF_UWORLD)))
	  *badop=4;
	if (*badop>=2 && (IsULine(cptr,sptr) || sadmin == 1 ||
		(!MyClient(sptr) && IsSAdmin(sptr))))
	  *badop=4;

	/* Fixes channel hacking.  Problem before was that it didn't bounce
	   unless user was deopped by server.  Now modes from *all*
	   non-chanops are bounced.  -- Barubary */
	if (!IsServer(sptr) && !IsULine(cptr, sptr) &&
		!is_chan_op(sptr, chptr)) bounce = 1;
	else bounce = (*badop==1 || *badop==2)?1:0;
	if (IsULine(cptr,sptr) || sadmin == 1 || (!MyClient(sptr) && IsSAdmin(sptr)))
		 bounce = 0;

        whatt = 0;
	for (ip = flags; *ip; ip += 2)
		if ((*ip & new) && !(*ip & oldm.mode))
		    {
			if (bounce)
			{ if (bwhatt != MODE_DEL)
			  { *bmbuf++ = '-';
			    bwhatt = MODE_DEL; }
			  *bmbuf++ = *(ip+1); }
			else
			{ if (whatt != MODE_ADD)
			  { *mbuf++ = '+';
			    whatt = MODE_ADD; }
			  mode->mode |= *ip;
			  *mbuf++ = *(ip+1); }
		    }

	for (ip = flags; *ip; ip += 2)
		if ((*ip & oldm.mode) && !(*ip & new))
		    {
			if (bounce)
			{ if (bwhatt != MODE_ADD)
			  { *bmbuf++ = '+';
			    bwhatt = MODE_ADD; }
			  *bmbuf++ = *(ip+1); }
                        else
			{ if (whatt != MODE_DEL)
			  { *mbuf++ = '-';
			    whatt = MODE_DEL; }
			  mode->mode &= ~*ip;
			  *mbuf++ = *(ip+1); }
		    }

	blen = 0;
	if (limitset && !nusers && mode->limit)
	    {
		if (bounce)
		{ if (bwhatt != MODE_ADD)
		  { *bmbuf++ = '+';
		    bwhatt = MODE_ADD; }
                  *bmbuf++ = 'l';
		  (void)sprintf(numeric, "%-15d", mode->limit);
		  if ((cp = index(numeric, ' '))) *cp = '\0';
		  (void)strcat(bpbuf, numeric);
		  blen += strlen(numeric);
		  (void)strcat(bpbuf, " ");
		}
		else
		{ if (whatt != MODE_DEL)
		  { *mbuf++ = '-';
		    whatt = MODE_DEL; }
		  mode->mode &= ~MODE_LIMIT;
		  mode->limit = 0;
		  *mbuf++ = 'l'; }
	    }
	/*
	 * Reconstruct "+bkov" chain.
	 */
	if (opcnt)
	    {
		Reg1	int	i = 0;
		Reg2	char	c;
		char	*user, *host;
		u_int prev_whatt;

		for (; i < opcnt; i++)
		    {
			lp = &chops[i];
			/*
			 * make sure we have correct mode change sign
			 */
			if (whatt != (lp->flags & (MODE_ADD|MODE_DEL)))
				if (lp->flags & MODE_ADD)
				    {
					*mbuf++ = '+';
					prev_whatt = whatt;
					whatt = MODE_ADD;
				    }
				else
				    {
					*mbuf++ = '-';
					prev_whatt = whatt;
					whatt = MODE_DEL;
				    }
			len = strlen(pbuf);
			/*
			 * get c as the mode char and tmp as a pointer to
			 * the parameter for this mode change.
			 */
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_CHANOP :
				c = 'o';
				cp = lp->value.cptr->name;
				break;
			case MODE_VOICE :
				c = 'v';
				cp = lp->value.cptr->name;
				break;
			case MODE_BAN :
                         /* I made this a bit more user-friendly (tm):
			  * nick = nick!*@*
			  * nick!user = nick!user@*
			  * user@host = *!user@host
			  * host.name = *!*@host.name    --Run
			  */
				c = 'b';
				cp = pretty_mask(lp->value.cp);
				break;
			case MODE_KEY :
				c = 'k';
				cp = lp->value.cp;
				break;
			case MODE_LIMIT :
				c = 'l';
				(void)sprintf(numeric, "%-15d", nusers);
				if ((cp = index(numeric, ' ')))
					*cp = '\0';
				cp = numeric;
				break;
			}

			if (len + strlen(cp) + 12 > (size_t) MODEBUFLEN)
				break;
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_KEY :
				if (strlen(cp) > (size_t) KEYLEN)
					*(cp+KEYLEN) = '\0';
				if ((whatt == MODE_ADD && (*mode->key=='\0' ||
				    mycmp(mode->key,cp)!=0)) ||
				    (whatt == MODE_DEL && (*mode->key!='\0')))
				{ if (bounce)
				  { if (*mode->key=='\0')
				    { if (bwhatt != MODE_DEL)
				      { *bmbuf++ = '-';
				        bwhatt = MODE_DEL; }
				      (void)strcat(bpbuf, cp);
				      blen += strlen(cp);
				      (void)strcat(bpbuf, " ");
				      blen++; }
                                    else
				    { if (bwhatt != MODE_ADD)
				      { *bmbuf++ = '+';
					bwhatt = MODE_ADD; }
				      (void)strcat(bpbuf, mode->key);
				      blen += strlen(mode->key);
				      (void)strcat(bpbuf, " ");
				      blen++; }
				    *bmbuf++ = c;
				    mbuf--;
				    if (*mbuf!='+' && *mbuf!='-') mbuf++;
				    else whatt = prev_whatt; }
				  else
				  { *mbuf++ = c;
				    (void)strcat(pbuf, cp);
				    len += strlen(cp);
				    (void)strcat(pbuf, " ");
				    len++;
				    if (whatt == MODE_ADD)
				      strncpyzt(mode->key, cp,
					  sizeof(mode->key));
				    else *mode->key = '\0'; } }
				break;
			case MODE_LIMIT :
				if (nusers && nusers != mode->limit)
				{ if (bounce)
				  { if (mode->limit == 0)
				    { if (bwhatt != MODE_DEL)
				      { *bmbuf++ = '-';
				        bwhatt = MODE_DEL; } }
                                    else
				    { if (bwhatt != MODE_ADD)
				      { *bmbuf++ = '+';
					bwhatt = MODE_ADD; }
				      (void)sprintf(numeric, "%-15d",
					  mode->limit);
				      if ((cp = index(numeric, ' ')))
					*cp = '\0';
				      (void)strcat(bpbuf, numeric);
				      blen += strlen(numeric);
				      (void)strcat(bpbuf, " ");
				      blen++;
				    }
				    *bmbuf++ = c;
				    mbuf--;
				    if (*mbuf!='+' && *mbuf!='-') mbuf++;
				    else whatt = prev_whatt; }
				  else
				  { *mbuf++ = c;
				    (void)strcat(pbuf, cp);
				    len += strlen(cp);
				    (void)strcat(pbuf, " ");
				    len++;
				    mode->limit = nusers; } }
				break;
			case MODE_CHANOP :
			case MODE_VOICE :
				tmp = find_user_link(chptr->members,
				    lp->value.cptr);
				if (lp->flags & MODE_ADD)
				{ change=(~tmp->flags) & CHFL_OVERLAP &
				      lp->flags;
				  if (change && bounce)
				  { if (lp->flags & MODE_CHANOP)
				      tmp->flags |= CHFL_DEOPPED;
				    if (bwhatt != MODE_DEL)
				    { *bmbuf++ = '-';
				      bwhatt = MODE_DEL; }
				    *bmbuf++ = c;
				    (void)strcat(bpbuf, lp->value.cptr->name);
				    blen += strlen(lp->value.cptr->name);
				    (void)strcat(bpbuf, " ");
				    blen++;
				    change=0; }
				  else if (change)
				  { tmp->flags |= lp->flags & CHFL_OVERLAP;
				    if (lp->flags & MODE_CHANOP)
				    { tmp->flags &= ~CHFL_DEOPPED;
				      if (IsServer(sptr) || IsULine(cptr,sptr))
					tmp->flags &= ~CHFL_SERVOPOK; } } }
				else
				{ change=tmp->flags & CHFL_OVERLAP & lp->flags;
				  if (change && bounce)
				  { if (lp->flags & MODE_CHANOP)
				      tmp->flags &= ~CHFL_DEOPPED;
				    if (bwhatt != MODE_ADD)
				    { *bmbuf++ = '+';
				      bwhatt = MODE_ADD; }
				    *bmbuf++ = c;
				    (void)strcat(bpbuf, lp->value.cptr->name);
				    blen += strlen(lp->value.cptr->name);
				    (void)strcat(bpbuf, " ");
				    blen++;
				    change=0; }
				  else
				  { tmp->flags &= ~change;
				    if ((change & MODE_CHANOP) &&
					(IsServer(sptr) || IsULine(cptr,sptr) 
					|| sadmin == 1 || (!MyClient(sptr) && IsSAdmin(sptr))))
				      tmp->flags |= CHFL_DEOPPED; } }
				if (change || *badop==2 || *badop==4)
				{ *mbuf++ = c;
				  (void)strcat(pbuf, cp);
				  len += strlen(cp);
				  (void)strcat(pbuf, " ");
				  len++; }
				else
				{ mbuf--;
				  if (*mbuf!='+' && *mbuf!='-') mbuf++;
				  else whatt = prev_whatt; }
				break;
			case MODE_BAN :
/* Only bans aren't bounced, it makes no sense to bounce last second
 * bans while propagating bans done before the net.rejoin. The reason
 * why I don't bounce net.rejoin bans is because it is too much
 * work to take care of too long strings adding the necessary TS to
 * net.burst bans -- RunLazy
 * We do have to check for *badop==2 now, we don't want HACKs to take
 * effect.
 */
/* Not anymore.  They're not bounced from servers/ulines, but they *are*
   bounced if from a user without chanops. -- Barubary */
				if (IsServer(sptr) || IsULine(cptr, sptr)
					|| !bounce)
				{
				if (*badop!=2 &&
				    ((whatt & MODE_ADD) &&
				     !add_banid(sptr, chptr, cp) ||
				     (whatt & MODE_DEL) &&
				     !del_banid(chptr, cp)))
				{ 
				  *mbuf++ = c;
				  (void)strcat(pbuf, cp);
				  len += strlen(cp);
				  (void)strcat(pbuf, " ");
				  len++;
				}
				} else {
				if (whatt & MODE_ADD)
				{
				  if (!find_banid(chptr, cp)) {
				  if (bwhatt != MODE_DEL)
					*bmbuf++ = '-';
				  *bmbuf++ = c;
				  strcat(bpbuf, cp);
				  blen += strlen(cp);
				  strcat(bpbuf, " ");
				  blen++; }
				}
				else
				{
				  if (find_banid(chptr, cp)) {
				  if (bwhatt != MODE_ADD)
					*bmbuf++ = '+';
				  *bmbuf++ = c;
				  strcat(bpbuf, cp);
				  blen += strlen(cp);
				  strcat(bpbuf, " ");
				  blen++; }
				}
				}
				break;
			}
		    } /* for (; i < opcnt; i++) */
	    } /* if (opcnt) */

	*mbuf++ = '\0';
	*bmbuf++ = '\0';


/* Bounce here */
	if (!hacknotice && *bmodebuf && chptr->creationtime)
	  sendto_one(cptr,":%s %s %s %s %s %lu",
	      me.name, (IsToken(cptr)?TOK_MODE:MSG_MODE), chptr->chname,
	      bmodebuf, bparambuf, *badop==2?(time_t)0:chptr->creationtime);

	return gotts?1:-1;
}

/* Now let _invited_ people join thru bans, +i and +l.
 * Checking if an invite exist could be done only if a block exists,
 * but I'm not too fancy of the complicated structure that'd cause,
 * when optimization will hopefully take care of it. Most of the time
 * a user won't have invites on him anyway. -Donwulff
 */

static	int	can_join(sptr, chptr, key)
aClient	*sptr;
Reg2	aChannel *chptr;
char	*key;
{
	Reg1	Link	*lp;

	if ((chptr->mode.mode & MODE_RGSTRONLY) && !IsARegNick(sptr))
		return (ERR_NEEDREGGEDNICK);
		

	if (*chptr->mode.key && (BadPtr(key) || mycmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	for (lp = sptr->user->invited; lp; lp = lp->next)
		if (lp->value.chptr == chptr)
			break;

	if ((chptr->mode.mode & MODE_INVITEONLY) && !lp)
		return (ERR_INVITEONLYCHAN);

	if ((chptr->mode.limit && chptr->users >= chptr->mode.limit) && !lp)
		return (ERR_CHANNELISFULL);

	if (is_banned(sptr, chptr) && !lp)
		return (ERR_BANNEDFROMCHAN);
		
	return 0;
}

/*
** Remove bells and commas from channel name
*/

void	clean_channelname(cn)
char	*cn;
{
	Reg1	u_char	*ch = (u_char *)cn;


	for (; *ch; ch++)
		/* Don't allow any control chars, the space, the comma,
		 * or the "non-breaking space" in channel names.
		 * Might later be changed to a system where the list of
		 * allowed/non-allowed chars for channels was a define
		 * or some such.
		 *   --Wizzu
		 */
		if (*ch < 33 || *ch == ',' || *ch == 160)
		    {
			*ch = '\0';
			return;
		    }
}

/*
** Return -1 if mask is present and doesnt match our server name.
*/
static	int	check_channelmask(sptr, cptr, chname)
aClient	*sptr, *cptr;
char	*chname;
{
	Reg1	char	*s;

	if (*chname == '&' && IsServer(cptr))
		return -1;
	s = rindex(chname, ':');
	if (!s)
		return 0;

	s++;
	if (match(s, me.name) || (IsServer(cptr) && match(s, cptr->name)))
	    {
		if (MyClient(sptr))
			sendto_one(sptr, err_str(ERR_BADCHANMASK),
				   me.name, sptr->name, chname);
		return -1;
	    }
	return 0;
}

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
static	aChannel *get_channel(cptr, chname, flag)
aClient *cptr;
char	*chname;
int	flag;
    {
	Reg1	aChannel *chptr;
	int	len;

	if (BadPtr(chname))
		return NULL;

	len = strlen(chname);
	if (MyClient(cptr) && len > CHANNELLEN)
	    {
		len = CHANNELLEN;
		*(chname+CHANNELLEN) = '\0';
	    }
	if ((chptr = find_channel(chname, (aChannel *)NULL)))
		return (chptr);
	if (flag == CREATE)
	    {
		chptr = (aChannel *)MyMalloc(sizeof(aChannel) + len);
		bzero((char *)chptr, sizeof(aChannel));
		strncpyzt(chptr->chname, chname, len+1);
		if (channel)
			channel->prevch = chptr;
		chptr->prevch = NULL;
		chptr->nextch = channel;
		chptr->creationtime = MyClient(cptr)?time(NULL):(time_t)0;
		channel = chptr;
		(void)add_to_channel_hash_table(chname, chptr);
	    }
	return chptr;
    }

/*
 * Slight changes in routine, now working somewhat symmetrical:
 *   First try to remove the client & channel pair to avoid duplicates
 *   Second check client & channel invite-list lengths and remove tail
 *   Finally add new invite-links to both client and channel
 * Should U-lined clients have higher limits?   -Donwulff
 */

static	void	add_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*inv, *tmp;

	del_invite(cptr, chptr);
	/*
	 * delete last link in chain if the list is max length
	 */
	if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER)
	    {
/*		This forgets the channel side of invitation     -Vesa
		inv = cptr->user->invited;
		cptr->user->invited = inv->next;
		free_link(inv);
*/
		for (tmp = cptr->user->invited; tmp->next; tmp = tmp->next)
			;
		del_invite(cptr, tmp->value.chptr);
 
	    }
	/* We get pissy over too many invites per channel as well now,
	 * since otherwise mass-inviters could take up some major
	 * resources -Donwulff
	 */
	if (list_length(chptr->invites) >= MAXCHANNELSPERUSER) {
		for (tmp = chptr->invites; tmp->next; tmp = tmp->next)
			;
		del_invite(tmp->value.cptr, chptr);
	}
	/*
	 * add client to the beginning of the channel invite list
	 */
	inv = make_link();
	inv->value.cptr = cptr;
	inv->next = chptr->invites;
	chptr->invites = inv;
	/*
	 * add channel to the beginning of the client invite list
	 */
	inv = make_link();
	inv->value.chptr = chptr;
	inv->next = cptr->user->invited;
	cptr->user->invited = inv;
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void	del_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	**inv, *tmp;

	for (inv = &(chptr->invites); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.cptr == cptr)
		    {
			*inv = tmp->next;
			free_link(tmp);
			break;
		    }

	for (inv = &(cptr->user->invited); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.chptr == chptr)
		    {
			*inv = tmp->next;
			free_link(tmp);
			break;
		    }
}

/*
**  Subtract one user from channel i (and free channel
**  block, if channel became empty).
*/
static	void	sub1_from_channel(chptr)
Reg1	aChannel *chptr;
{
	Ban	*ban;
	Link	*lp;

	if (--chptr->users <= 0)
	    {
		/*
		 * Now, find all invite links from channel structure
		 */
		while ((lp = chptr->invites))
			del_invite(lp->value.cptr, chptr);

		while (chptr->banlist)
		{
			ban = chptr->banlist;
			chptr->banlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
		if (chptr->prevch)
			chptr->prevch->nextch = chptr->nextch;
		else
			channel = chptr->nextch;
		if (chptr->nextch)
			chptr->nextch->prevch = chptr->prevch;
		(void)del_from_channel_hash_table(chptr->chname, chptr);
		MyFree((char *)chptr);
	    }
}

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (key)
*/
int	m_join(cptr, sptr, parc, parv)
Reg2	aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	static	char	jbuf[BUFSIZE];
	Reg1	Link	*lp;
	Reg3	aChannel *chptr;
	Reg4	char	*name, *key = NULL;
	int	i, flags = 0, zombie = 0;
	char	*p = NULL, *p2 = NULL;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "JOIN");
		return 0;
	    }

	*jbuf = '\0';
	/*
	** Rebuild list of channels joined to be the actual result of the
	** JOIN.  Note that "JOIN 0" is the destructive problem.
	*/
	for (i = 0, name = strtoken(&p, parv[1], ","); name;
	     name = strtoken(&p, NULL, ","))
	    {
		/* pathological case only on longest channel name.
		** If not dealt with here, causes desynced channel ops
		** since ChannelExists() doesn't see the same channel
		** as one being joined. cute bug. Oct 11 1997, Dianora/comstud
		** Copied from Dianora's "hybrid 5" ircd.
		*/

		if(strlen(name) >  CHANNELLEN)  /* same thing is done in get_channel() */
			name[CHANNELLEN] = '\0';

		if (MyConnect(sptr))
			clean_channelname(name);
		if (check_channelmask(sptr, cptr, name)==-1)
			continue;
		if (*name == '&' && !MyConnect(sptr))
			continue;
		if (*name == '0' && !atoi(name))
		    {
		    	(void)strcpy(jbuf, "0");
		        i = 1;
		        continue;
		    }
		else if (!IsChannelName(name))
		    {
			if (MyClient(sptr))
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
					   me.name, parv[0], name);
			continue;
		    }
		if (*jbuf)
			(void)strcat(jbuf, ",");
		(void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
		i += strlen(name)+1;
	    }
	(void)strcpy(parv[1], jbuf);

	p = NULL;
	if (parv[2])
		key = strtoken(&p2, parv[2], ",");
	parv[2] = NULL;	/* for m_names call later, parv[parc] must == NULL */
	for (name = strtoken(&p, jbuf, ","); name;
	     key = (key) ? strtoken(&p2, NULL, ",") : NULL,
	     name = strtoken(&p, NULL, ","))
	    {
		/*
		** JOIN 0 sends out a part for all channels a user
		** has joined.
		*/
		if (*name == '0' && !atoi(name))
		    {
			while ((lp = sptr->user->channel))
			    {
				chptr = lp->value.chptr;
				if (!is_zombie(sptr, chptr))
				  sendto_channel_butserv(chptr, sptr, PartFmt,
				      parv[0], chptr->chname);
				remove_user_from_channel(sptr, chptr);
			    }
			sendto_serv_butone(cptr, ":%s JOIN 0", parv[0]);
			continue;
		    }

		if (MyConnect(sptr))
		    {
			/*
			** local client is first to enter previously nonexistant
			** channel so make them (rightfully) the Channel
			** Operator.
			*/
			if (!IsModelessChannel(name))
			    flags = (ChannelExists(name)) ? CHFL_DEOPPED :
							    CHFL_CHANOP;
			else
			    flags = CHFL_DEOPPED;

			if (sptr->user->joined >= MAXCHANNELSPERUSER)
			    {
				sendto_one(sptr, err_str(ERR_TOOMANYCHANNELS),
					   me.name, parv[0], name);
				return 0;
			    }
		    }
		chptr = get_channel(sptr, name, CREATE);
                if (chptr && (lp=find_user_link(chptr->members, sptr)))
		{ if (lp->flags & CHFL_ZOMBIE)
		  { zombie = 1;
		    flags = lp->flags & (CHFL_DEOPPED|CHFL_SERVOPOK);
		    remove_user_from_channel(sptr, chptr);
		    chptr = get_channel(sptr, name, CREATE); }
		  else continue; }
		if (!zombie)
		{ if (!MyConnect(sptr)) flags = CHFL_DEOPPED;
		  if (sptr->flags & FLAGS_TS8) flags|=CHFL_SERVOPOK; }
		if (!chptr ||
		    (MyConnect(sptr) && (i = can_join(sptr, chptr, key))))
		    { 
			sendto_one(sptr, err_str(i),
				   me.name, parv[0], name);
			continue;
		    }

		/*
		**  Complete user entry to the new channel (if any)
		*/
		add_user_to_channel(chptr, sptr, flags);
		/*
		** notify all other users on the new channel
		*/
		sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
					parv[0], name);
		sendto_match_servs(chptr, cptr, ":%s JOIN :%s", parv[0], name);

		if (MyClient(sptr))
		    {
		        /*
			** Make a (temporal) creationtime, if someone joins
			** during a net.reconnect : between remote join and
			** the mode with TS. --Run
			*/
		        if (chptr->creationtime == 0)
		        { chptr->creationtime = time(NULL);
			  sendto_match_servs(chptr, cptr, ":%s MODE %s + %lu",
			      me.name, name, chptr->creationtime);
			}
			del_invite(sptr, chptr);
			if (flags & CHFL_CHANOP)
				sendto_match_servs(chptr, cptr,
				  ":%s MODE %s +o %s %lu",
				  me.name, name, parv[0], chptr->creationtime);
			if (chptr->topic[0] != '\0') {
				sendto_one(sptr, rpl_str(RPL_TOPIC), me.name,
					   parv[0], name, chptr->topic);
				sendto_one(sptr, rpl_str(RPL_TOPICWHOTIME),
					    me.name, parv[0], name,
					    chptr->topic_nick,
					    chptr->topic_time);
			      }
			parv[1] = name;
			(void)m_names(cptr, sptr, 2, parv);
		    }
	    }
	return 0;
}

/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = comment (added by Lefler)
*/
int	m_part(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	Reg1	aChannel *chptr;
	Reg2  Link	*lp;
	char	*p = NULL, *name;
	register char *comment = (parc > 2 && parv[2]) ? parv[2] : NULL;

	if (check_registered_user(sptr))
		return 0;

        sptr->flags&=~FLAGS_TS8;

	if (parc < 2 || parv[1][0] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "PART");
		return 0;
	    }

#ifdef	V28PlusOnly
	*buf = '\0';
#endif

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		chptr = get_channel(sptr, name, 0);
		if (!chptr)
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		/* Do not use IsMember here: zombies must be able to part too */
		if (!(lp=find_user_link(chptr->members, sptr)))
		    {
			/* Normal to get get when our client did a kick
			** for a remote client (who sends back a PART),
			** so check for remote client or not --Run
			*/
			if (MyClient(sptr))
			  sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
		    	      me.name, parv[0], name);
			continue;
		    }
		/*
		**  Remove user from the old channel (if any)
		*/
#ifdef	V28PlusOnly
		if (*buf)
			(void)strcat(buf, ",");
		(void)strcat(buf, name);
#else
                if (parc < 3)
		  sendto_match_servs(chptr, cptr, PartFmt, parv[0], name);
		else
		  sendto_match_servs(chptr, cptr, PartFmt2, parv[0], name, comment);
#endif
		if (!(lp->flags & CHFL_ZOMBIE)) {
                if (parc < 3)
		  sendto_channel_butserv(chptr, sptr, PartFmt, parv[0], name);
		else
		  sendto_channel_butserv(chptr, sptr, PartFmt2, parv[0], name, comment);
                } else if (MyClient(sptr)) {
		  if (parc < 3)
		    sendto_one(sptr, PartFmt, parv[0], name);
		  else
		    sendto_one(sptr, PartFmt2, parv[0], name, comment);
                }
		remove_user_from_channel(sptr, chptr);
	    }
#ifdef	V28PlusOnly
	if (*buf)
		sendto_serv_butone(cptr, PartFmt, parv[0], buf);
#endif
	return 0;
    }

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = client to kick
**	parv[3] = kick comment
*/
int	m_kick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *who;
	aChannel *chptr;
	int	chasing = 0;
	char	*comment, *name, *p = NULL, *user, *p2 = NULL;
	Link *lp,*lp2;

	if (check_registered(sptr))
		return 0;

	sptr->flags&=~FLAGS_TS8;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "KICK");
		return 0;
	    }

        if (IsServer(sptr) && !IsULine(cptr, sptr))
	  sendto_ops("HACK: KICK from %s for %s %s", parv[0], parv[1], parv[2]);

	comment = (BadPtr(parv[3])) ? parv[0] : parv[3];
	if (strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN] = '\0';

	*nickbuf = *buf = '\0';

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		chptr = get_channel(sptr, name, !CREATE);
		if (!chptr)
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		if (!IsServer(cptr) && !IsULine(cptr,sptr) && !is_chan_op(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0], chptr->chname);
			continue;
		    }

	        lp2=find_user_link(chptr->members, sptr);
		for (; (user = strtoken(&p2, parv[2], ",")); parv[2] = NULL)
		    {   if (!(who = find_chasing(sptr, user, &chasing)))
				continue; /* No such user left! */
			if ((lp=find_user_link(chptr->members, who)) &&
			    !(lp->flags & CHFL_ZOMBIE) || IsServer(sptr))
	                {
			  /* Bounce all KICKs from a non-chanop unless it
			     would cause a fake direction -- Barubary */
			  if ((who->from != cptr) && !IsServer(sptr) &&
			     !IsULine(cptr, sptr) && !is_chan_op(sptr, chptr))
/*			  if (who->from!=cptr &&
			     ((lp2 && (lp2->flags & CHFL_DEOPPED)) ||
			     (!lp2 && IsPerson(sptr))) && !IsULine(cptr,sptr))*/
			  {
			  /* Bounce here:
			   * cptr must be a server (or cptr==sptr and
			   * sptr->flags can't have DEOPPED set
			   * when CHANOP is set).
			   */
			    sendto_one(cptr,":%s JOIN %s",who->name,name);
			    /* Slight change here - the MODE is sent only
			       once even if overlapping -- Barubary */
			    if (lp->flags & CHFL_OVERLAP)
			    {
				char *temp = buf;
				*temp++ = '+';
				if (lp->flags & CHFL_CHANOP) *temp++ = 'o';
				if (lp->flags & CHFL_VOICE) *temp++ = 'v';
				*temp = 0;
				sendto_one(cptr, ":%s MODE %s %s %s %s %lu",
				me.name, chptr->chname, buf, who->name,
				((lp->flags & CHFL_OVERLAP) == CHFL_OVERLAP)
				? who->name : "", chptr->creationtime);
			    }
			/*  if (lp->flags & CHFL_CHANOP)
			      sendmodeto_one(cptr, me.name, name, "+o",
			          who->name, chptr->creationtime);
			    if (lp->flags & CHFL_VOICE)
			      sendmodeto_one(cptr, me.name, name, "+v",
			      who->name, chptr->creationtime); */
			  }
			  else
			  {
			    if (lp)
			      sendto_channel_butserv(chptr, sptr,
			          ":%s KICK %s %s :%s", parv[0],
			          name, who->name, comment);
			    sendto_match_servs(chptr, cptr,
					       ":%s KICK %s %s :%s",
					       parv[0], name,
					       who->name, comment);
			    /* Finally, zombies totally removed -- Barubary */
			    if (lp)
			    {
			      if (MyClient(who))
			      { sendto_match_servs(chptr, NULL,
				      PartFmt, who->name, name);
			        remove_user_from_channel(who, chptr); }
			      else
			      {
			          remove_user_from_channel(who, chptr); }
			    }
			  }
			}
			else if (MyClient(sptr))
			  sendto_one(sptr, err_str(ERR_USERNOTINCHANNEL),
					   me.name, parv[0], user, name);
			if (!IsServer(cptr) || !IsULine(cptr,sptr))
			  break;
		    } /* loop on parv[2] */
		if (!IsServer(cptr) || !IsULine(cptr,sptr))
		  break;
	    } /* loop on parv[1] */

	return (0);
}

int	count_channels(sptr)
aClient	*sptr;
{
Reg1	aChannel	*chptr;
	Reg2	int	count = 0;

	for (chptr = channel; chptr; chptr = chptr->nextch)
#ifdef	SHOW_INVISIBLE_LUSERS
		if (SecretChannel(chptr))
		    {
			if (IsAnOper(sptr))
				count++;
		    }
		else
#endif
			count++;
	return (count);
}

/*
** m_topic
**	parv[0] = sender prefix
**	parv[1] = topic text
**
**	For servers using TS: (Lefler)
**	parv[0] = sender prefix
**	parv[1] = channel list
**	parv[2] = topic nickname
**	parv[3] = topic time
**	parv[4] = topic text
*/
int	m_topic(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aChannel *chptr = NullChn;
	char	*topic = NULL, *name, *p = NULL, *tnick = NULL;
	time_t	ttime = 0;
	
	if (check_registered(sptr))
		return 0;

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "TOPIC");
		return 0;
	    }

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		if (parc > 1 && IsChannelName(name))
		    {
			chptr = find_channel(name, NullChn);
			if (!chptr || (!IsMember(sptr, chptr) &&
			    !IsServer(sptr) && !IsULine(cptr,sptr)))
			    {
				sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
					   me.name, parv[0], name);
				continue;
			    }				
			if (parc > 2)
				topic = parv[2];
			if (parc > 4) {
				tnick = parv[2];
				ttime = atol(parv[3]);
				topic = parv[4];
			}
		    }

		if (!chptr)
		    {
			sendto_one(sptr, rpl_str(RPL_NOTOPIC),
				   me.name, parv[0], name);
			return 0;
		    }

		if (check_channelmask(sptr, cptr, name))
			continue;
	
		if (!topic)  /* only asking  for topic  */
		    {
			if (chptr->topic[0] == '\0')
			sendto_one(sptr, rpl_str(RPL_NOTOPIC),
				   me.name, parv[0], chptr->chname);
			else {
				sendto_one(sptr, rpl_str(RPL_TOPIC),
					   me.name, parv[0],
					   chptr->chname, chptr->topic);
				sendto_one(sptr, rpl_str(RPL_TOPICWHOTIME),
					   me.name, parv[0], chptr->chname,
					   chptr->topic_nick,
					   chptr->topic_time);
		    } 
		    } 
		else if (ttime && topic && (IsServer(sptr) || IsULine(cptr,sptr)))
		    {
			if (!chptr->topic_time || ttime < chptr->topic_time)
			   {
				/* setting a topic */
				strncpyzt(chptr->topic, topic, sizeof(chptr->topic));
				strcpy(chptr->topic_nick, tnick);
				chptr->topic_time = ttime;
				sendto_match_servs(chptr, cptr,":%s TOPIC %s %s %lu :%s",
					   parv[0], chptr->chname,
					   chptr->topic_nick, chptr->topic_time,
					   chptr->topic);
				sendto_channel_butserv(chptr, sptr, ":%s TOPIC %s :%s (%s)",
					       parv[0], chptr->chname,
					       chptr->topic, chptr->topic_nick);
			   }
		    }
		else if (((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
			  is_chan_op(sptr, chptr)) || IsULine(cptr,sptr)
			  && topic)
		    {
			/* setting a topic */
			strncpyzt(chptr->topic, topic, sizeof(chptr->topic));
			strcpy(chptr->topic_nick, sptr->name);
                        if (ttime && IsServer(cptr))
			  chptr->topic_time = ttime;
			else
			  chptr->topic_time = time(NULL);
			sendto_match_servs(chptr, cptr,":%s TOPIC %s %s %lu :%s",
					   parv[0], chptr->chname, parv[0],
					   chptr->topic_time, chptr->topic);
			sendto_channel_butserv(chptr, sptr, ":%s TOPIC %s :%s",
					       parv[0],
					       chptr->chname, chptr->topic);
		    }
		else
		      sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				 me.name, parv[0], chptr->chname);
	    }
	return 0;
    }

/*
** m_invite
**	parv[0] - sender prefix
**	parv[1] - user to invite
**	parv[2] - channel number
*/
int	m_invite(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aClient *acptr;
	aChannel *chptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "INVITE");
		return -1;
	    }

	if (!(acptr = find_person(parv[1], (aClient *)NULL)))
	    {
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			   me.name, parv[0], parv[1]);
		return 0;
	    }
	if (MyConnect(sptr))
		clean_channelname(parv[2]);
	if (check_channelmask(sptr, cptr, parv[2]))
		return 0;
	if (!(chptr = find_channel(parv[2], NullChn)))
	    {

		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",
				  parv[0], parv[1], parv[2]);
		return 0;
	    }

	if (chptr && !IsMember(sptr, chptr) && !IsULine(cptr,sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
			   me.name, parv[0], parv[2]);
		return -1;
	    }

	if (IsMember(acptr, chptr))
	    {
		sendto_one(sptr, err_str(ERR_USERONCHANNEL),
			   me.name, parv[0], parv[1], parv[2]);
		return 0;
	    }
	if (chptr && (chptr->mode.mode & MODE_INVITEONLY))
	    {
		if (!is_chan_op(sptr, chptr) && !IsULine(cptr,sptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0], chptr->chname);
			return -1;
		    }
		else if (!IsMember(sptr, chptr) && !IsULine(cptr,sptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0],
				   ((chptr) ? (chptr->chname) : parv[2]));
			return -1;
		    }
	    }

	if (MyConnect(sptr))
	    {
		if(check_for_target_limit(sptr, acptr, acptr->name))
			return 0;

		sendto_one(sptr, rpl_str(RPL_INVITING), me.name, parv[0],
			   acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
		if (acptr->user->away)
			sendto_one(sptr, rpl_str(RPL_AWAY), me.name, parv[0],
				   acptr->name, acptr->user->away);
	    }
	/* Note: is_banned() here will cause some extra CPU load, 
	 *       and we're really only relying on the existence
	 *       of the limit because we could momentarily have
	 *       less people on channel.
	 */
	if (MyConnect(acptr))
		if (chptr && sptr->user && (is_banned(acptr, chptr) ||
		     (chptr->mode.mode & MODE_INVITEONLY) ||
		     chptr->mode.limit) &&
		    (is_chan_op(sptr, chptr) || IsULine(cptr,sptr)))
		{
			add_invite(acptr, chptr);
			sendto_channelops_butone(NULL, &me, chptr,
			  ":%s NOTICE @%s :%s invited %s into the channel.",
			  me.name, chptr->chname, sptr->name, acptr->name);
		}
	sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",parv[0],
			  acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	return 0;
    }

static int number_of_zombies(chptr)
aChannel *chptr;
{
/* No Zombies!
  Reg1 Link *lp;
  Reg2 int count = 0;
  for (lp=chptr->members; lp; lp=lp->next)
    if (lp->flags & CHFL_ZOMBIE) count++;
  return count;
*/
  return 0;
}

/*
 * send_list
 *
 * The function which sends
 * The function which sends the actual /list output back to the user.
 * Operates by stepping through the hashtable, sending the entries back if
 * they match the criteria.
 * cptr = Local client to send the output back to.
 * numsend = Number (roughly) of lines to send back. Once this number has
 * been exceeded, send_list will finish with the current hash bucket,
 * and record that number as the number to start next time send_list
 * is called for this user. So, this function will almost always send
 * back more lines than specified by numsend (though not by much,
 * assuming CHANNELHASHSIZE is was well picked). So be conservative
 * if altering numsend };> -Rak
 */
void	send_list(cptr, numsend)
aClient	*cptr;
int	numsend;
{
    int	hashptr, done = 0;
    aChannel	*chptr;
    Link	*tmpl;

#define l cptr->lopt /* lazy shortcut */

    for (hashptr = l->starthash; hashptr < CHANNELHASHSIZE; hashptr++) {
	for (chptr = hash_get_chan_bucket(hashptr);
	     chptr; chptr = chptr->hnextch) {
	    if (SecretChannel(chptr) && !IsMember(cptr, chptr))
		continue;
	    if (!l->showall && ((chptr->users <= l->usermin) ||
		((l->usermax == -1)?0:(chptr->users >= l->usermax)) ||
		((chptr->creationtime||1) <= l->chantimemin) ||
		(chptr->topic_time < l->topictimemin) ||
		(chptr->creationtime >= l->chantimemax) ||
		(chptr->topic_time > l->topictimemax)))
		continue;
	    /* For now, just extend to topics as well. Use patterns starting
	     * with # to stick to searching channel names only. -Donwulff
	     */
	    if (l->nolist && 
	        (find_str_match_link(&(l->nolist), chptr->chname) ||
	         find_str_match_link(&(l->nolist), chptr->topic)))
		continue;
	    if (l->yeslist &&
		(!find_str_match_link(&(l->yeslist), chptr->chname) &&
		 !find_str_match_link(&(l->yeslist), chptr->topic)))
		continue;

	    sendto_one(cptr, rpl_str(RPL_LIST), me.name, cptr->name,
		       ShowChannel(cptr, chptr) ? chptr->chname : "*",
		       chptr->users - number_of_zombies(chptr),
		       ShowChannel(cptr, chptr) ? chptr->topic : "");
	    if (--numsend == 0) /* Send to the end of the list and return */
	    done = 1;
	}

	if (done && (++hashptr < CHANNELHASHSIZE)) {
	    l->starthash = hashptr;
	    return;
	}
    }

    sendto_one(cptr, rpl_str(RPL_LISTEND), me.name, cptr->name);
    free_str_list(l->yeslist);
    free_str_list(l->nolist);
    MyFree(l);
    l = NULL;

    /* List finished, penalize by 10 seconds -Donwulff */
    if (!IsPrivileged(cptr))
	cptr->since+=10;

    return;
}



/*
 * m_list
 *	parv[0] = sender prefix
 *	parv[1,2,3...] = Channels or list options.
 */
int	m_list(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
    aChannel *chptr;
    char   *name, *p = NULL;
    LOpts  *lopt;
    short  int  showall = 0;
    Link   *yeslist = NULL, *nolist = NULL, *listptr;
    short  usermin = 0, usermax = -1;
    time_t currenttime = time(NULL);
    time_t chantimemin = 0, topictimemin = 0;
    time_t chantimemax, topictimemax;

    static char *usage[] = {
	"   Usage: /raw LIST options (on mirc) or /quote LIST options (ircII)",
	"",
	"If you don't include any options, the default is to send you the",
	"entire unfiltered list of channels. Below are the options you can",
	"use, and what channels LIST will return when you use them.",
	">number  List channels with more than <number> people.",
	"<number  List channels with less than <number> people.",
	"C>number List channels created between now and <number> minutes ago.",
	"C<number List channels created earlier than <number> minutes ago.",
	"T>number List channels whose topics are older than <number> minutes",
	"         (Ie, they have not changed in the last <number> minutes.",
	"T<number List channels whose topics are not older than <number> minutes.",
	"*mask*   List channels that match *mask*",
	"!*mask*  List channels that do not match *mask*",
	NULL
    };


    /* None of that unregistered LIST stuff.  -- Barubary */
    if (check_registered(sptr)) return 0;

    /*
     * I'm making the assumption it won't take over a day to transmit
     * the list... -Rak
     */
    chantimemax = topictimemax = currenttime + 86400;


    if ((parc == 2) && (!strcasecmp(parv[1], "?"))) {
	char **ptr = usage;

	for (; *ptr; ptr++)
	    sendto_one(sptr, rpl_str(RPL_LISTSYNTAX), me.name,
		       cptr->name, *ptr);
	return 0;
    }

    /*
     * A list is already in process, for now we just interrupt the
     * current listing, perhaps later we can allow stacked ones...
     *  -Donwulff (Not that it's hard or anything, but I don't see
     *             much use for it, beyond flooding)
     */

    if(cptr->lopt) {
	free_str_list(cptr->lopt->yeslist);
	free_str_list(cptr->lopt->nolist);
	MyFree(cptr->lopt);
	cptr->lopt=NULL;
	sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, cptr->name);
	/* Interrupted list, penalize 10 seconds */
	if(!IsPrivileged(sptr))
	    sptr->since+=10;
	
	return 0;
    }

    sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, cptr->name);

    /* LIST with no arguements */
    if (parc < 2 || BadPtr(parv[1])) {
	lopt = (LOpts *)MyMalloc(sizeof(LOpts));
	    if (!lopt)
		return 0;

	/*
	 * Changed to default to ignoring channels with only
	 * 1 person on, to decrease floods... -Donwulff
	 */
	bzero(lopt, sizeof(LOpts)); /* To be sure! */
	lopt->next = (LOpts *)lopt->yeslist=lopt->nolist=(Link *)NULL;
	lopt->usermin = 1; /* Default */
	lopt->usermax = -1;
	lopt->chantimemax = lopt->topictimemax = currenttime + 86400;
	cptr->lopt = lopt;
	if (IsSendable(cptr))
	    send_list(cptr, 64);
	return 0;
    }


    /*
     * General idea: We don't need parv[0], since we can get that
     * information from cptr.name. So, let's parse each element of
     * parv[], setting pointer parv to the element being parsed.
     */
    while (--parc) {
	parv += 1;
	if (BadPtr(parv)) /* Sanity check! */
	    continue;

	name = strtoken(&p, *parv, ",");

	while (name) {
	  switch (*name) {
	    case '>':
		showall = 1;
		usermin = strtol(++name, (char **) 0, 10);
		break;

	    case '<':
		showall = 1;
		usermax = strtol(++name, (char **) 0, 10);
		break;

	    case 't':
	    case 'T':
		showall = 1;
		switch (*++name) {
		    case '>':
			topictimemax = currenttime - 60 *
				       strtol(++name, (char **) 0, 10);
			break;

		    case '<':
			topictimemin = currenttime - 60 *
				       strtol(++name, (char **) 0, 10);
			break;

		    case '\0':
			topictimemin = 1;
			break;

		    default:
			sendto_one(sptr, err_str(ERR_LISTSYNTAX),
				   me.name, cptr->name);
			free_str_list(yeslist);
			free_str_list(nolist);
			sendto_one(sptr, rpl_str(RPL_LISTEND),
				   me.name, cptr->name);

			return 0;
		}
		break;

		case 'c':
		case 'C':
		    showall = 1;
		    switch (*++name) {
			case '>':
			    chantimemin = currenttime - 60 *
					  strtol(++name, (char **) 0, 10);
			    break;

			case '<':
			    chantimemax = currenttime - 60 *
					  strtol(++name, (char **) 0, 10);
			    break;

			default:
			    sendto_one(sptr, err_str(ERR_LISTSYNTAX),
				       me.name, cptr->name);
			    free_str_list(yeslist);
			    free_str_list(nolist);
			    sendto_one(sptr, rpl_str(RPL_LISTEND),
				       me.name, cptr->name);
			    return 0;
		    }
		    break;

		default: /* A channel or channel mask */

		    /*
		     * new syntax: !channelmask will tell ircd to ignore
		     * any channels matching that mask, and then
		     * channelmask will tell ircd to send us a list of
		     * channels only masking channelmask. Note: Specifying
		     * a channel without wildcards will return that
		     * channel even if any of the !channelmask masks
		     * matches it.
		     */

		    if (*name == '!') {
			showall = 1;
			listptr = make_link();
			listptr->next = nolist;
			DupString(listptr->value.cp, name+1);
			nolist = listptr;
		    }
		    else if (strchr(name, '*') || strchr(name, '?')) {
			showall = 1;
			listptr = make_link();
			listptr->next = yeslist;
			DupString(listptr->value.cp, name);
			yeslist = listptr;
		    }
		    else {
			chptr = find_channel(name, NullChn);
			if (chptr && ShowChannel(sptr, chptr))
			    sendto_one(sptr, rpl_str(RPL_LIST),
				       me.name, cptr->name,
				       ShowChannel(sptr,chptr) ? name : "*",
				       chptr->users - number_of_zombies(chptr),
				       chptr->topic);
		    }
	  } /* switch (*name) */
	name = strtoken(&p, NULL, ",");
	} /* while(name) */
    } /* while(--parc) */

    if (!showall || (chantimemin > currenttime) ||
	 (topictimemin > currenttime)) {
	free_str_list(yeslist);
	free_str_list(nolist);
	sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, cptr->name);

	return 0;
    }

    lopt = (LOpts *)MyMalloc(sizeof(LOpts));

    lopt->showall = 0;
    lopt->next = NULL;
    lopt->yeslist = yeslist;
    lopt->nolist = nolist;
    lopt->starthash = 0;
    lopt->usermin = usermin;
    lopt->usermax = usermax;
    lopt->currenttime = currenttime;
    lopt->chantimemin = chantimemin;
    lopt->chantimemax = chantimemax;
    lopt->topictimemin = topictimemin;
    lopt->topictimemax = topictimemax;

    cptr->lopt = lopt;
    send_list(cptr, 64);



    return 0;
}


/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

/*
** m_names
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int	m_names(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{ 
	Reg1	aChannel *chptr;
	Reg2	aClient *c2ptr;
	Reg3	Link	*lp;
	aChannel *ch2ptr = NULL;
	int	idx, flag, len, mlen, x;
	char	*s, *para = parc > 1 ? parv[1] : NULL;

	if (check_registered(sptr)) return 0;

	if (parc > 1 &&
	    hunt_server(cptr, sptr, ":%s NAMES %s %s", 2, parc, parv))
		return 0;

	mlen = strlen(me.name) + 10;

	/* Only 10 requests per NAMES allowed remotely -- Barubary */
	if (!BadPtr(para) && (cptr != sptr))
	for (x = 0, s = para; *s; s++)
		if (*s == ',')
		if (++x == 10)
		{
			*s = 0;
			break;
		}

	if (BadPtr(para) && IsServer(cptr))
	{
	sendto_one(cptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
	return 0;
	}

	if (!BadPtr(para))
	    {
		s = index(para, ',');
		if (s)
		    {
			parv[1] = ++s;
			(void)m_names(cptr, sptr, parc, parv);
		    }
		if (MyConnect(sptr))
			clean_channelname(para);
		ch2ptr = find_channel(para, (aChannel *)NULL);
	    }

	*buf = '\0';

	/*
	 * First, do all visible channels (public and the one user self is)
	 */

	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
		if ((chptr != ch2ptr) && !BadPtr(para))
			continue; /* -- wanted a specific channel */
		if (!MyConnect(sptr) && BadPtr(para))
			continue;
		if (!ShowChannel(sptr, chptr))
			continue; /* -- users on this are not listed */

		/* Find users on same channel (defined by chptr) */

		(void)strcpy(buf, "* ");
		len = strlen(chptr->chname);
		(void)strcpy(buf + 2, chptr->chname);
		(void)strcpy(buf + 2 + len, " :");

		if (PubChannel(chptr))
			*buf = '=';
		else if (SecretChannel(chptr))
			*buf = '@';
		idx = len + 4;
		flag = 1;
		for (lp = chptr->members; lp; lp = lp->next)
		    {
			c2ptr = lp->value.cptr;
			if (sptr!=c2ptr && IsInvisible(c2ptr) &&
			  !IsMember(sptr,chptr))
				continue;
			if (lp->flags & CHFL_ZOMBIE)
			{ if (lp->value.cptr!=sptr)
				continue;
			  else
				(void)strcat(buf, "!"); }
		        else if (lp->flags & CHFL_CHANOP)
				(void)strcat(buf, "@");
			else if (lp->flags & CHFL_VOICE)
				(void)strcat(buf, "+");
			(void)strncat(buf, c2ptr->name, NICKLEN);
			idx += strlen(c2ptr->name) + 1;
			flag = 1;
			(void)strcat(buf," ");
			if (mlen + idx + NICKLEN > BUFSIZE - 2)
			    {
				sendto_one(sptr, rpl_str(RPL_NAMREPLY),
					   me.name, parv[0], buf);
				(void)strncpy(buf, "* ", 3);
				(void)strncpy(buf+2, chptr->chname, len + 1);
				(void)strcat(buf, " :");
				if (PubChannel(chptr))
					*buf = '=';
				else if (SecretChannel(chptr))
					*buf = '@';
				idx = len + 4;
				flag = 0;
			    }
		    }
		if (flag)
			sendto_one(sptr, rpl_str(RPL_NAMREPLY),
				   me.name, parv[0], buf);
	    }
	if (!BadPtr(para))
	    {
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0],
			   para);
		return(1);
	    }

	/* Second, do all non-public, non-secret channels in one big sweep */

	(void)strncpy(buf, "* * :", 6);
	idx = 5;
	flag = 0;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
  	        aChannel *ch3ptr;
		int	showflag = 0, secret = 0;

		if (!IsPerson(c2ptr) || sptr!=c2ptr && IsInvisible(c2ptr))
			continue;
		lp = c2ptr->user->channel;
		/*
		 * dont show a client if they are on a secret channel or
		 * they are on a channel sptr is on since they have already
		 * been show earlier. -avalon
		 */
		while (lp)
		    {
			ch3ptr = lp->value.chptr;
			if (PubChannel(ch3ptr) || IsMember(sptr, ch3ptr))
				showflag = 1;
			if (SecretChannel(ch3ptr))
				secret = 1;
			lp = lp->next;
		    }
		if (showflag) /* have we already shown them ? */
			continue;
		if (secret) /* on any secret channels ? */
			continue;
		(void)strncat(buf, c2ptr->name, NICKLEN);
		idx += strlen(c2ptr->name) + 1;
		(void)strcat(buf," ");
		flag = 1;
		if (mlen + idx + NICKLEN > BUFSIZE - 2)
		    {
			sendto_one(sptr, rpl_str(RPL_NAMREPLY),
				   me.name, parv[0], buf);
			(void)strncpy(buf, "* * :", 6);
			idx = 5;
			flag = 0;
		    }
	    }
	if (flag)
		sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name, parv[0], buf);
	sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
	return(1);
    }


void	send_user_joins(cptr, user)
aClient	*cptr, *user;
{
	Reg1	Link	*lp;
	Reg2	aChannel *chptr;
	Reg3	int	cnt = 0, len = 0, clen;
	char	 *mask;

	sprintf(buf, ":%s %s ", user->name,
		(IsToken(cptr)?TOK_JOIN:MSG_JOIN));
	len = strlen(buf);

	for (lp = user->user->channel; lp; lp = lp->next)
	    {
		chptr = lp->value.chptr;
		if ((mask = index(chptr->chname, ':')))
			if (match(++mask, cptr->name))
				continue;
		if (*chptr->chname == '&')
			continue;
		if (is_zombie(user, chptr))
			continue;
		clen = strlen(chptr->chname);
		if (clen + 1 + len > BUFSIZE - 3)
		    {
			if (cnt)
			{
				buf[len-1]='\0';
				sendto_one(cptr, "%s", buf);
			}
			sprintf(buf, ":%s %s ", user->name,
				(IsToken(cptr)?TOK_JOIN:MSG_JOIN));
			len = strlen(buf);
			cnt = 0;
		    }
		(void)strcpy(buf + len, chptr->chname);
		cnt++;
		len += clen;
		if (lp->next)
		    {
			len++;
			(void)strcat(buf, ",");
		    }
	    }
	if (*buf && cnt)
		sendto_one(cptr, "%s", buf);

	return;
}
