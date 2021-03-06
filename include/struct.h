/************************************************************************
 *   IRC - Internet Relay Chat, include/struct.h
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

#ifndef	__struct_include__
#define __struct_include__

#include "config.h"
#ifndef __EMX__
#include "common.h"
#include "sys.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <netdb.h>
#endif
#ifdef STDDEFH
# include <stddef.h>
#endif

#ifdef USE_SYSLOG
# include <syslog.h>
# ifdef SYSSYSLOGH
#  include <sys/syslog.h>
# endif
#endif
#ifdef	pyr
#include <sys/time.h>
#endif

typedef struct  SqlineItem aSqlineItem;
typedef	struct	ConfItem aConfItem;
typedef struct	Notify	aNotify;
typedef	struct 	Client	aClient;
typedef	struct	Channel	aChannel;
typedef	struct	User	anUser;
typedef	struct	Server	aServer;
typedef	struct	SLink	Link;
typedef struct	SBan	Ban;
typedef	struct	SMode	Mode;
typedef struct	ListOptions	LOpts;

typedef struct  CloneItem aClone;

#ifdef NEED_U_INT32_T
typedef unsigned int  u_int32_t; /* XXX Hope this works! */
#endif

#ifndef VMSP
#include "class.h"
#include "dbuf.h"	/* THIS REALLY SHOULDN'T BE HERE!!! --msa */
#endif

#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define	NICKLEN		30	/* Necessary to put 9 here instead of 10
				** if s_msg.c/m_nick has been corrected.
				** This preserves compatibility with old
				** servers --msa
				*/
#define	USERLEN		10
#define	REALLEN	 	50
#define	TOPICLEN	307
/* DAL MADE ME PUT THIS IN THE FIEND:  --Russell
 * This number will be expanded to 200 in the near future
 */
#define	CHANNELLEN	32
#define	PASSWDLEN 	32	/* orig. 20, changed to 32 for nickpasswords */
#define	KEYLEN		23
#define	BUFSIZE		512		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXRECIPIENTS 	20
#define	MAXKILLS	20
#define	MAXBANS		60
#define	MAXBANLENGTH	1024
#define	MAXSILES	5
#define	MAXSILELENGTH	128
/*
 * Watch it - Don't change this unless you also change the ERR_TOOMANYWATCH
 * and PROTOCOL_SUPPORTED settings.
 */
#define MAXWATCH	128

#define	USERHOST_REPLYLEN	(NICKLEN+HOSTLEN+USERLEN+5)

#ifdef USE_SERVICES
#include "service.h"
#endif

/* NOTE: this must be down here so the stuff from struct.h IT uses works */
#include "whowas.h"


/*
** 'offsetof' is defined in ANSI-C. The following definition
** is not absolutely portable (I have been told), but so far
** it has worked on all machines I have needed it. The type
** should be size_t but...  --msa
*/
#ifndef offsetof
#define	offsetof(t,m) (int)((&((t *)0L)->m))
#endif

#define	elementsof(x) (sizeof(x)/sizeof(x[0]))

/*
** flags for bootup options (command line flags)
*/
#define	BOOT_CONSOLE	1
#define	BOOT_QUICK	2
#define	BOOT_DEBUG	4
#define	BOOT_INETD	8
#define	BOOT_TTY	16
#define	BOOT_OPER	32
#define	BOOT_AUTODIE	64

#define	STAT_LOG	-6	/* logfile for -x */
#define	STAT_MASTER	-5	/* Local ircd master before identification */
#define	STAT_CONNECTING	-4
#define	STAT_HANDSHAKE	-3
#define	STAT_ME		-2
#define	STAT_UNKNOWN	-1
#define	STAT_SERVER	0
#define	STAT_CLIENT	1
#define	STAT_SERVICE	2	/* Services not implemented yet */

/*
 * status macros.
 */
#define	IsRegisteredUser(x)	((x)->status == STAT_CLIENT)
#define	IsRegistered(x)		((x)->status >= STAT_SERVER)
#define	IsConnecting(x)		((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)		((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)			((x)->status == STAT_ME)
#define	IsUnknown(x)		((x)->status == STAT_UNKNOWN || \
				 (x)->status == STAT_MASTER)
#define	IsServer(x)		((x)->status == STAT_SERVER)
#define	IsClient(x)		((x)->status == STAT_CLIENT)
#define	IsLog(x)		((x)->status == STAT_LOG)
#define IsService(x)		0

#define	SetMaster(x)		((x)->status = STAT_MASTER)
#define	SetConnecting(x)	((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)		((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)		((x)->status = STAT_ME)
#define	SetUnknown(x)		((x)->status = STAT_UNKNOWN)
#define	SetServer(x)		((x)->status = STAT_SERVER)
#define	SetClient(x)		((x)->status = STAT_CLIENT)
#define	SetLog(x)		((x)->status = STAT_LOG)
#define	SetService(x)		((x)->status = STAT_SERVICE)

#define	FLAGS_PINGSENT   0x0001	/* Unreplied ping sent */
#define	FLAGS_DEADSOCKET 0x0002	/* Local socket is dead--Exiting soon */
#define	FLAGS_KILLED     0x0004	/* Prevents "QUIT" from being sent for this */
#define	FLAGS_BLOCKED    0x0008	/* socket is in a blocked condition */
#define	FLAGS_UNIX	 0x0010	/* socket is in the unix domain, not inet */
#define	FLAGS_CLOSING    0x0020	/* set when closing to suppress errors */
#define	FLAGS_LISTEN     0x0040 /* used to mark clients which we listen() on */
#define	FLAGS_CHKACCESS  0x0080 /* ok to check clients access if set */
#define	FLAGS_DOINGDNS	 0x0100 /* client is waiting for a DNS response */
#define	FLAGS_AUTH	 0x0200 /* client is waiting on rfc931 response */
#define	FLAGS_WRAUTH	 0x0400	/* set if we havent writen to ident server */
#define	FLAGS_LOCAL	 0x0800 /* set for local clients */
#define	FLAGS_GOTID	 0x1000	/* successful ident lookup achieved */
#define	FLAGS_DOID	 0x2000	/* I-lines say must use ident return */
#define	FLAGS_NONL	 0x4000 /* No \n in buffer */
#define FLAGS_TS8	 0x8000 /* Why do you want to know? */
#define FLAGS_ULINE	0x10000 /* User/server is considered U-lined */
#define FLAGS_SQUIT	0x20000 /* Server has been /squit by an oper */
#define FLAGS_PROTOCTL	0x40000 /* Received a PROTOCTL message */

/* Dec 26th, 1997 - added flags2 when I ran out of room in flags -DuffJ 

/* Dec 26th, 1997 - having a go at
 * splitting flags into flags and umodes
 * -DuffJ
 */

#define	UMODE_INVISIBLE  0x0001 /* makes user invisible */
#define	UMODE_OPER       0x0002	/* Operator */
#define	UMODE_WALLOP     0x0004 /* send wallops to them */
#define UMODE_FAILOP	 0x0008 /* Shows some global messages */
#define UMODE_HELPOP	 0x0010 /* Help system operator */
#define UMODE_REGNICK	 0x0020 /* Nick set by services as registered */
#define UMODE_SADMIN	 0x0040 /* Services Admin */
#define UMODE_ADMIN	 0x0080 /* Admin */

#define	UMODE_SERVNOTICE 0x0100 /* server notices such as kill */
#define	UMODE_LOCOP      0x0200 /* Local operator -- SRB */
#define UMODE_KILLS	 0x0400 /* Show server-kills... */
#define UMODE_CLIENT	 0x0800 /* Show client information */
#define UMODE_FLOOD	 0x1000 /* Receive flood warnings */
#define UMODE_CHATOP	 0x2000 /* can receive chatops */

#define	SEND_UMODES	(UMODE_INVISIBLE|UMODE_OPER|UMODE_WALLOP|UMODE_FAILOP|UMODE_HELPOP|UMODE_REGNICK|UMODE_SADMIN|UMODE_ADMIN)
#define	ALL_UMODES	(SEND_UMODES|UMODE_SERVNOTICE|UMODE_LOCOP|UMODE_KILLS|UMODE_CLIENT|UMODE_FLOOD|UMODE_CHATOP)
#define	FLAGS_ID	(FLAGS_DOID|FLAGS_GOTID)

#define PROTO_NOQUIT	0x1	/* Negotiated NOQUIT protocol */
#define PROTO_TOKEN	0x2	/* Negotiated TOKEN protocol */

/*
 * flags macros.
 */
#define IsKillsF(x)		((x)->umodes & UMODE_KILLS)
#define IsClientF(x)		((x)->umodes & UMODE_CLIENT)
#define IsFloodF(x)		((x)->umodes & UMODE_FLOOD)
#define IsHelpOp(x)		((x)->umodes & UMODE_HELPOP)
#define IsAdmin(x)		((x)->umodes & UMODE_ADMIN)
#define IsSAdmin(x)		((x)->umodes & UMODE_SADMIN)
#define SendFailops(x)		((x)->umodes & UMODE_FAILOP)
#define SendChatops(x)		((x)->umodes & UMODE_CHATOP)
#define	IsOper(x)		((x)->umodes & UMODE_OPER)
#define	IsLocOp(x)		((x)->umodes & UMODE_LOCOP)
#define	IsInvisible(x)		((x)->umodes & UMODE_INVISIBLE)
#define	IsAnOper(x)		((x)->umodes & (UMODE_OPER|UMODE_LOCOP))
#define IsARegNick(x)		((x)->umodes & (UMODE_REGNICK))
#define IsRegNick(x)		((x)->umodes & UMODE_REGNICK)
#define	IsPerson(x)		((x)->user && IsClient(x))
#define	IsPrivileged(x)		(IsAnOper(x) || IsServer(x))
#define	SendWallops(x)		((x)->umodes & UMODE_WALLOP)
#define	SendServNotice(x)	((x)->umodes & UMODE_SERVNOTICE)
#define	IsUnixSocket(x)		((x)->flags & FLAGS_UNIX)
#define	IsListening(x)		((x)->flags & FLAGS_LISTEN)
#define	DoAccess(x)		((x)->flags & FLAGS_CHKACCESS)
#define	IsLocal(x)		((x)->flags & FLAGS_LOCAL)
#define	IsDead(x)		((x)->flags & FLAGS_DEADSOCKET)
#define GotProtoctl(x)		((x)->flags & FLAGS_PROTOCTL)
#define IsBlocked(x)		((x)->flags & FLAGS_BLOCKED)

#ifdef NOSPOOF
#define	IsNotSpoof(x)		((x)->nospoof == 0)
#else
#define IsNotSpoof(x)           (1)
#endif

#define SetKillsF(x)		((x)->umodes |= UMODE_KILLS)
#define SetClientF(x)		((x)->umodes |= UMODE_CLIENT)
#define SetFloodF(x)		((x)->umodes |= UMODE_FLOOD)
#define SetHelpOp(x)		((x)->umodes |= UMODE_HELPOP)
#define	SetOper(x)		((x)->umodes |= UMODE_OPER)
#define	SetLocOp(x)    		((x)->umodes |= UMODE_LOCOP)
#define	SetInvisible(x)		((x)->umodes |= UMODE_INVISIBLE)
#define	SetWallops(x)  		((x)->umodes |= UMODE_WALLOP)
#define	SetUnixSock(x)		((x)->flags |= FLAGS_UNIX)
#define	SetDNS(x)		((x)->flags |= FLAGS_DOINGDNS)
#define	DoingDNS(x)		((x)->flags & FLAGS_DOINGDNS)
#define	SetAccess(x)		((x)->flags |= FLAGS_CHKACCESS)
#define SetBlocked(x)		((x)->flags |= FLAGS_BLOCKED)
#define	DoingAuth(x)		((x)->flags & FLAGS_AUTH)
#define	NoNewLine(x)		((x)->flags & FLAGS_NONL)
#define SetRegNick(x)		((x)->umodes & UMODE_REGNICK)

#define ClearAdmin(x)		((x)->umodes &= ~UMODE_ADMIN)
#define ClearSAdmin(x)		((x)->umodes &= ~UMODE_SADMIN)
#define ClearKillsF(x)		((x)->umodes &= ~UMODE_KILLS)
#define ClearClientF(x)		((x)->umodes &= ~UMODE_CLIENT)
#define ClearFloodF(x)		((x)->umodes &= ~UMODE_FLOOD)
#define ClearHelpOp(x)		((x)->umodes &= ~UMODE_HELPOP)
#define ClearFailops(x)		((x)->umodes &= ~UMODE_FAILOP)
#define ClearChatops(x)		((x)->umodes &= ~UMODE_CHATOP)
#define	ClearOper(x)		((x)->umodes &= ~UMODE_OPER)
#define	ClearInvisible(x)	((x)->umodes &= ~UMODE_INVISIBLE)
#define	ClearWallops(x)		((x)->umodes &= ~UMODE_WALLOP)
#define	ClearDNS(x)		((x)->flags &= ~FLAGS_DOINGDNS)
#define	ClearAuth(x)		((x)->flags &= ~FLAGS_AUTH)
#define	ClearAccess(x)		((x)->flags &= ~FLAGS_CHKACCESS)
#define ClearBlocked(x)		((x)->flags &= ~FLAGS_BLOCKED)

/*
 * ProtoCtl options
 */
#define DontSendQuit(x)		((x)->proto & PROTO_NOQUIT)
#define IsToken(x)		((x)->proto & PROTO_TOKEN)

#define SetNoQuit(x)		((x)->proto |= PROTO_NOQUIT)
#define SetToken(x)		((x)->proto |= PROTO_TOKEN)

#define ClearNoQuit(x)		((x)->proto &= ~PROTO_NOQUIT)
#define ClearToken(x)		((x)->proto &= ~PROTO_TOKEN)


/*
 * defined operator access levels
 */
#define OFLAG_REHASH	0x00000001  /* Oper can /rehash server */
#define OFLAG_DIE	0x00000002  /* Oper can /die the server */
#define OFLAG_RESTART	0x00000004  /* Oper can /restart the server */
#define OFLAG_HELPOP	0x00000010  /* Oper can send /HelpOps */
#define OFLAG_GLOBOP	0x00000020  /* Oper can send /GlobOps */
#define OFLAG_WALLOP	0x00000040  /* Oper can send /WallOps */
#define OFLAG_LOCOP	0x00000080  /* Oper can send /LocOps */
#define OFLAG_LROUTE	0x00000100  /* Oper can do local routing */
#define OFLAG_GROUTE	0x00000200  /* Oper can do global routing */
#define OFLAG_LKILL	0x00000400  /* Oper can do local kills */
#define OFLAG_GKILL	0x00000800  /* Oper can do global kills */
#define OFLAG_KLINE	0x00001000  /* Oper can /kline users */
#define OFLAG_UNKLINE	0x00002000  /* Oper can /unkline users */
#define OFLAG_LNOTICE	0x00004000  /* Oper can send local serv notices */
#define OFLAG_GNOTICE	0x00008000  /* Oper can send global notices */
#define OFLAG_ADMIN	0x00010000  /* Admin */
#define OFLAG_UMODEC	0x00020000  /* Oper can set umode +c */
#define OFLAG_UMODEF	0x00040000  /* Oper can set umode +f */
#define OFLAG_SADMIN    0x00080000  /* Oper can be a services admin */
#define OFLAG_ZLINE	0x00100000  /* Oper can use /zline and /unzline */

#define OFLAG_LOCAL	(OFLAG_REHASH|OFLAG_HELPOP|OFLAG_GLOBOP|OFLAG_WALLOP|OFLAG_LOCOP|OFLAG_LROUTE|OFLAG_LKILL|OFLAG_KLINE|OFLAG_UNKLINE|OFLAG_LNOTICE|OFLAG_UMODEC|OFLAG_UMODEF)
#define OFLAG_GLOBAL	(OFLAG_LOCAL|OFLAG_GROUTE|OFLAG_GKILL|OFLAG_GNOTICE)
#define OFLAG_ISGLOBAL	(OFLAG_GROUTE|OFLAG_GKILL|OFLAG_GNOTICE)

#define OPCanZline(x)	((x)->oflag & OFLAG_SADMIN)
#define OPCanRehash(x)	((x)->oflag & OFLAG_REHASH)
#define OPCanDie(x)	((x)->oflag & OFLAG_DIE)
#define OPCanRestart(x)	((x)->oflag & OFLAG_RESTART)
#define OPCanHelpOp(x)	((x)->oflag & OFLAG_HELPOP)
#define OPCanGlobOps(x)	((x)->oflag & OFLAG_GLOBOP)
#define OPCanWallOps(x)	((x)->oflag & OFLAG_WALLOP)
#define OPCanLocOps(x)	((x)->oflag & OFLAG_LOCOP)
#define OPCanLRoute(x)	((x)->oflag & OFLAG_LROUTE)
#define OPCanGRoute(x)	((x)->oflag & OFLAG_GROUTE)
#define OPCanLKill(x)	((x)->oflag & OFLAG_LKILL)
#define OPCanGKill(x)	((x)->oflag & OFLAG_GKILL)
#define OPCanKline(x)	((x)->oflag & OFLAG_KLINE)
#define OPCanUnKline(x)	((x)->oflag & OFLAG_UNKLINE)
#define OPCanLNotice(x)	((x)->oflag & OFLAG_LNOTICE)
#define OPCanGNotice(x)	((x)->oflag & OFLAG_GNOTICE)
#define OPIsAdmin(x)	((x)->oflag & OFLAG_ADMIN)
#define OPIsSAdmin(x)	((x)->oflag & OFLAG_SADMIN)
#define OPCanUModeC(x)	((x)->oflag & OFLAG_UMODEC)
#define OPCanUModeF(x)	((x)->oflag & OFLAG_UMODEF)

#define OPSetRehash(x)	((x)->oflag |= OFLAG_REHASH)
#define OPSetDie(x)	((x)->oflag |= OFLAG_DIE)
#define OPSetRestart(x)	((x)->oflag |= OFLAG_RESTART)
#define OPSetHelpOp(x)	((x)->oflag |= OFLAG_HELPOP)
#define OPSetGlobOps(x)	((x)->oflag |= OFLAG_GLOBOP)
#define OPSetWallOps(x)	((x)->oflag |= OFLAG_WALLOP)
#define OPSetLocOps(x)	((x)->oflag |= OFLAG_LOCOP)
#define OPSetLRoute(x)	((x)->oflag |= OFLAG_LROUTE)
#define OPSetGRoute(x)	((x)->oflag |= OFLAG_GROUTE)
#define OPSetLKill(x)	((x)->oflag |= OFLAG_LKILL)
#define OPSetGKill(x)	((x)->oflag |= OFLAG_GKILL)
#define OPSetKline(x)	((x)->oflag |= OFLAG_KLINE)
#define OPSetUnKline(x)	((x)->oflag |= OFLAG_UNKLINE)
#define OPSetLNotice(x)	((x)->oflag |= OFLAG_LNOTICE)
#define OPSetGNotice(x)	((x)->oflag |= OFLAG_GNOTICE)
#define OPSSetAdmin(x)	((x)->oflag |= OFLAG_ADMIN)
#define OPSSetSAdmin(x) ((x)->oflag |= OFLAG_SADMIN)
#define OPSetUModeC(x)	((x)->oflag |= OFLAG_UMODEC)
#define OPSetUModeF(x)	((x)->oflag |= OFLAG_UMODEF)
#define OPSetZLine(x)	((x)->oflag |= OFLAG_ZLINE)

#define OPClearRehash(x)	((x)->oflag &= ~OFLAG_REHASH)
#define OPClearDie(x)		((x)->oflag &= ~OFLAG_DIE)  
#define OPClearRestart(x)	((x)->oflag &= ~OFLAG_RESTART)
#define OPClearHelpOp(x)	((x)->oflag &= ~OFLAG_HELPOP)
#define OPClearGlobOps(x)	((x)->oflag &= ~OFLAG_GLOBOP)
#define OPClearWallOps(x)	((x)->oflag &= ~OFLAG_WALLOP)
#define OPClearLocOps(x)	((x)->oflag &= ~OFLAG_LOCOP)
#define OPClearLRoute(x)	((x)->oflag &= ~OFLAG_LROUTE)
#define OPClearGRoute(x)	((x)->oflag &= ~OFLAG_GROUTE)
#define OPClearLKill(x)		((x)->oflag &= ~OFLAG_LKILL)
#define OPClearGKill(x)		((x)->oflag &= ~OFLAG_GKILL)
#define OPClearKline(x)		((x)->oflag &= ~OFLAG_KLINE)
#define OPClearUnKline(x)	((x)->oflag &= ~OFLAG_UNKLINE)
#define OPClearLNotice(x)	((x)->oflag &= ~OFLAG_LNOTICE)
#define OPClearGNotice(x)	((x)->oflag &= ~OFLAG_GNOTICE)
#define OPClearAdmin(x)		((x)->oflag &= ~OFLAG_ADMIN)
#define OPClearSAdmin(x)	((x)->oflag &= ~OFLAG_SADMIN)
#define OPClearUModeC(x)	((x)->oflag &= ~OFLAG_UMODEC)
#define OPClearUModeF(x)	((x)->oflag &= ~OFLAG_UMODEF)
#define OPClearZLine(x)		((x)->oflag &= ~OFLAG_ZLINE)

/*
 * defined debugging levels
 */
#define	DEBUG_FATAL  0
#define	DEBUG_ERROR  1	/* report_error() and other errors that are found */
#define	DEBUG_NOTICE 3
#define	DEBUG_DNS    4	/* used by all DNS related routines - a *lot* */
#define	DEBUG_INFO   5	/* general usful info */
#define	DEBUG_NUM    6	/* numerics */
#define	DEBUG_SEND   7	/* everything that is sent out */
#define	DEBUG_DEBUG  8	/* anything to do with debugging, ie unimportant :) */
#define	DEBUG_MALLOC 9	/* malloc/free calls */
#define	DEBUG_LIST  10	/* debug list use */

/*
 * defines for curses in client
 */
#define	DUMMY_TERM	0
#define	CURSES_TERM	1
#define	TERMCAP_TERM	2

struct  SqlineItem	{
	unsigned int	status;
	char *sqline;
	char *reason;
	struct	SqlineItem *next;
};

struct	ConfItem	{
	unsigned int	status;	/* If CONF_ILLEGAL, delete when no clients */
	int	clients;	/* Number of *LOCAL* clients using this */
	struct	in_addr ipnum;	/* ip number of host field */
	char	*host;
	char	*passwd;
	char	*name;
	int	port;
	time_t	hold;	/* Hold action until this time (calendar time) */
	int	tmpconf;
#ifndef VMSP
	aClass	*class;  /* Class of connection */
#endif
	struct	ConfItem *next;
};

#define	CONF_ILLEGAL		0x80000000
#define	CONF_MATCH		0x40000000
#define	CONF_QUARANTINED_SERVER	0x0001
#define	CONF_CLIENT		0x0002
#define	CONF_CONNECT_SERVER	0x0004
#define	CONF_NOCONNECT_SERVER	0x0008
#define	CONF_LOCOP		0x0010
#define	CONF_OPERATOR		0x0020
#define	CONF_ME			0x0040
#define	CONF_KILL		0x0080
#define	CONF_ADMIN		0x0100
#ifdef 	R_LINES
#define	CONF_RESTRICT		0x0200
#endif
#define	CONF_CLASS		0x0400
#define	CONF_SERVICE		0x0800
#define	CONF_LEAF		0x1000
#define	CONF_LISTEN_PORT	0x2000
#define	CONF_HUB		0x4000
#define	CONF_UWORLD		0x8000
#define CONF_QUARANTINED_NICK	0x10000
#define CONF_ZAP		0x20000
#define CONF_CONFIG             0x100000
#define CONF_CRULEALL           0x200000
#define CONF_CRULEAUTO          0x400000
#define CONF_MISSING		0x800000
#define CONF_SADMIN		0x1000000
#define CONF_DRPASS		0x2000000   /* DIE/RESTART pass - NikB */

#define	CONF_OPS		(CONF_OPERATOR | CONF_LOCOP)
#define	CONF_SERVER_MASK	(CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER)
#define	CONF_CLIENT_MASK	(CONF_CLIENT | CONF_SERVICE | CONF_OPS | \
				 CONF_SERVER_MASK)
#define CONF_CRULE              (CONF_CRULEALL | CONF_CRULEAUTO)
#define CONF_QUARANTINE		(CONF_QUARANTINED_SERVER|CONF_QUARANTINED_NICK)

#define	IsIllegal(x)	((x)->status & CONF_ILLEGAL)
#define IsTemp(x)	((x)->tmpconf)

/*
 * Client structures
 */
struct	User	{
	struct	User	*nextu;
	Link	*channel;	/* chain of channel pointer blocks */
	Link	*invited;	/* chain of invite pointer blocks */
	Link	*silence;	/* chain of silence pointer blocks */
	char	*away;		/* pointer to away message */
	time_t	last;
	u_int32_t servicestamp;  /* Services' time stamp variable */
	int	refcnt;		/* Number of times this block is referenced */
	int	joined;		/* number of channels joined */
	char	username[USERLEN+1];
	char	host[HOSTLEN+1];
        char	server[HOSTLEN+1];
				/*
				** In a perfect world the 'server' name
				** should not be needed, a pointer to the
				** client describing the server is enough.
				** Unfortunately, in reality, server may
				** not yet be in links while USER is
				** introduced... --msa
				** No.  We are going to assume the server
				** is there.  There is no good reason at
				** all that we should get a NICK line
				** before a SERVER line.   -- Aeto
				** BUT, it is hiddenin aClient! -- Aeto
				*/
#ifdef	LIST_DEBUG
	aClient	*bcptr;
#endif
};

struct	Server	{
	struct	Server	*nexts;
	anUser	*user;		/* who activated this connection */
	char	up[HOSTLEN+1];	/* uplink for this server */
	char	by[NICKLEN+1];
	aConfItem *nline;	/* N-line pointer for this server */
#ifdef	LIST_DEBUG
	aClient	*bcptr;
#endif
};

struct Client	{
	struct	Client *next, *prev, *hnext;
	anUser	*user;		/* ...defined, if this is a User */
	aServer	*serv;		/* ...defined, if this is a server */
#ifdef USE_SERVICES
	aService *service;
#endif
	time_t	lasttime;	/* ...should be only LOCAL clients? --msa */
	time_t	firsttime;	/* time client was created */
	time_t	since;		/* last time we parsed something */
	time_t	lastnick;	/* TimeStamp on nick */
	time_t  nextnick;	/* Time the next nick change will be allowed */
	time_t  nexttarget;	/* Time until a change in targets is allowed */
	u_char targets[MAXTARGETS]; /* Hash values of current targets */	
	long	flags;		/* client flags */
	long	umodes;		/* client usermodes */
	aClient	*from;		/* == self, if Local Client, *NEVER* NULL! */
	int	fd;		/* >= 0, for local clients */
	int	hopcount;	/* number of servers to this 0 = local */
	short	status;		/* Client type */
	char	name[HOSTLEN+1]; /* Unique name of the client, nick or host */
	char	username[USERLEN+1]; /* username here now for auth stuff */
	char	info[REALLEN+1]; /* Free form additional client information */
	aClient	*srvptr;	/* Server introducing this.  May be &me */
	Link	*history;	/* Whowas linked list */
	/*
	** The following fields are allocated only for local clients
	** (directly connected to *this* server with a socket.
	** The first of them *MUST* be the "count"--it is the field
	** to which the allocation is tied to! *Never* refer to
	** these fields, if (from != self).
	*/
	int	count;		/* Amount of data in buffer */
	char	buffer[BUFSIZE]; /* Incoming message buffer */
	short	lastsq;		/* # of 2k blocks when sendqueued called last*/
	dbuf	sendQ;		/* Outgoing message queue--if socket full */
	dbuf	recvQ;		/* Hold for data incoming yet to be parsed */
#ifdef NOSPOOF
	u_int32_t	nospoof;	/* Anti-spoofing random number */
#endif
	long	oflag;		/* Operator access flags -Cabal95 */
	long	proto;		/* ProtoCtl options */
	long	sendM;		/* Statistics: protocol messages send */
	long	sendK;		/* Statistics: total k-bytes send */
	long	receiveM;	/* Statistics: protocol messages received */
	long	receiveK;	/* Statistics: total k-bytes received */
	u_short	sendB;		/* counters to count upto 1-k lots of bytes */
	u_short	receiveB;	/* sent and received. */
	aClient	*acpt;		/* listening client which we accepted from */
	Link	*confs;		/* Configuration record associated */
	int	authfd;		/* fd for rfc931 authentication */
	struct	in_addr	ip;	/* keep real ip# too */
	u_short	port;		/* and the remote port# too :-) */
	struct	hostent	*hostp;
	u_short	notifies;	/* Keep track of count of notifies */
	Link	*notify;	/* Links to clients notify-structures */
	LOpts	*lopt;		/* Saved /list options */
#ifdef	pyr
	struct	timeval	lw;
#endif
	char	sockhost[HOSTLEN+1]; /* This is the host name from the socket
				     ** and after which the connection was
				     ** accepted.
				     */
	char	passwd[PASSWDLEN+1];
#ifdef DEBUGMODE
	time_t	cputime;
#endif
};

#define	CLIENT_LOCAL_SIZE sizeof(aClient)
#define	CLIENT_REMOTE_SIZE offsetof(aClient,count)

/*
 * statistics structures
 */
struct	stats {
	unsigned int	is_cl;	/* number of client connections */
	unsigned int	is_sv;	/* number of server connections */
	unsigned int	is_ni;	/* connection but no idea who it was */
	unsigned short	is_cbs;	/* bytes sent to clients */
	unsigned short	is_cbr;	/* bytes received to clients */
	unsigned short	is_sbs;	/* bytes sent to servers */
	unsigned short	is_sbr;	/* bytes received to servers */
	unsigned long	is_cks;	/* k-bytes sent to clients */
	unsigned long	is_ckr;	/* k-bytes received to clients */
	unsigned long	is_sks;	/* k-bytes sent to servers */
	unsigned long	is_skr;	/* k-bytes received to servers */
	time_t 		is_cti;	/* time spent connected by clients */
	time_t		is_sti;	/* time spent connected by servers */
	unsigned int	is_ac;	/* connections accepted */
	unsigned int	is_ref;	/* accepts refused */
	unsigned int	is_unco; /* unknown commands */
	unsigned int	is_wrdi; /* command going in wrong direction */
	unsigned int	is_unpf; /* unknown prefix */
	unsigned int	is_empt; /* empty message */
	unsigned int	is_num;	/* numeric message */
	unsigned int	is_kill; /* number of kills generated on collisions */
	unsigned int	is_fake; /* MODE 'fakes' */
	unsigned int	is_asuc; /* successful auth requests */
	unsigned int	is_abad; /* bad auth requests */
	unsigned int	is_udp;	/* packets recv'd on udp port */
	unsigned int	is_loc;	/* local connections made */
};

struct ListOptions {
	LOpts	*next;
	Link	*yeslist, *nolist;
	int	starthash;
	short int	showall;
	unsigned short	usermin;
	int	usermax;
	time_t	currenttime;
	time_t	chantimemin;
	time_t	chantimemax;
	time_t	topictimemin;
	time_t	topictimemax;
};

/* mode structure for channels */

struct	SMode	{
	unsigned int	mode;
	int	limit;
	char	key[KEYLEN+1];
};

/* Message table structure */

struct	Message	{
	char	*cmd;
	int	(* func)();
	unsigned int	count;
	int	parameters;
	char	flags;
		/* bit 0 set means that this command is allowed to be used
		 * only on the average of once per 2 seconds -SRB */
	u_char	token[2]; /* Cheat for tokenized value */
	unsigned long	bytes;
#ifdef DEBUGMODE
	unsigned long	lticks;
	unsigned long	rticks;
#endif
};

/* Used for notify-hash buckets... -Donwulff */

struct Notify {
	aNotify	*hnext;
	time_t	lasttime;
	Link	*notify;
	char	nick[1];
};

/* general link structure used for chains */

struct	SLink	{
	struct	SLink	*next;
	int	flags;
	union {
		aClient	*cptr;
		aChannel *chptr;
		aConfItem *aconf;
		aNotify	*nptr;
		aName *whowas;
		char	*cp;
	} value;
};

struct	SBan	{
	struct	SBan	*next;
	char *banstr;
	char *who;
	time_t when;
};

/* channel structure */

struct Channel	{
	struct	Channel *nextch, *prevch, *hnextch;
	Mode	mode;
	time_t	creationtime;
	char	topic[TOPICLEN+1];
	char	topic_nick[NICKLEN+1];
	time_t	topic_time;
	int	users;
	Link	*members;
	Link	*invites;
	Ban	*banlist;
	char	chname[1];
};

/*
** Channel Related macros follow
*/

/* Channel related flags */

#define	CHFL_CHANOP     0x0001 /* Channel operator */
#define	CHFL_VOICE      0x0002 /* the power to speak */
#define	CHFL_DEOPPED	0x0004 /* Is de-opped by a server */
#define	CHFL_SERVOPOK   0x0008 /* Server op allowed */
#define	CHFL_ZOMBIE     0x0010 /* Kicked from channel */
/* Bans are stored in separate linked list, so phase this out? */
#define	CHFL_BAN	0x0020 /* ban channel flag */
#define	CHFL_OVERLAP    (CHFL_CHANOP|CHFL_VOICE)

/* Channel Visibility macros */

#define	MODE_CHANOP	CHFL_CHANOP
#define	MODE_VOICE	CHFL_VOICE
#define	MODE_PRIVATE	0x0004
#define	MODE_SECRET	0x0008
#define	MODE_MODERATED  0x0010
#define	MODE_TOPICLIMIT 0x0020
#define	MODE_INVITEONLY 0x0040
#define	MODE_NOPRIVMSGS 0x0080
#define	MODE_KEY	0x0100
#define	MODE_BAN	0x0200
#define	MODE_LIMIT	0x0400
#define	MODE_LIMIT	0x0400
#define MODE_RGSTR	0x0800
#define MODE_RGSTRONLY  0x1000
/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define	MODE_WPARAS	(MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY|MODE_LIMIT)
/*
 * Undefined here, these are used in conjunction with the above modes in
 * the source.
#define	MODE_DEL       0x40000000
#define	MODE_ADD       0x80000000
 */

#define	HoldChannel(x)		(!(x))
/* name invisible */
#define	SecretChannel(x)	((x) && ((x)->mode.mode & MODE_SECRET))
/* channel not shown but names are */
#define	HiddenChannel(x)	((x) && ((x)->mode.mode & MODE_PRIVATE))
/* channel visible */
#define	ShowChannel(v,c)	(PubChannel(c) || IsMember((v),(c)))
#define	PubChannel(x)		((!x) || ((x)->mode.mode &\
				 (MODE_PRIVATE | MODE_SECRET)) == 0)

#define	IsChannelName(name) ((name) && (*(name) == '#' || *(name) == '&' || *(name) == '+'))
#define IsModelessChannel(name) ((name) && (*(name) == '+'))

/* Misc macros */

#define	BadPtr(x) (!(x) || (*(x) == '\0'))

#define	isvalid(c) (((c) >= 'A' && (c) <= '~') || isdigit(c) || (c) == '-')

#define	MyConnect(x)			((x)->fd >= 0)
#define	MyClient(x)			(MyConnect(x) && IsClient(x))
#define	MyOper(x)			(MyConnect(x) && IsOper(x))

/* Lifted somewhat from Undernet code --Rak */

#define IsSendable(x)		(DBufLength(&x->sendQ) < 2048)
#define DoList(x)		((x)->lopt)

/* String manipulation macros */

/* strncopynt --> strncpyzt to avoid confusion, sematics changed
   N must be now the number of bytes in the array --msa */
#define	strncpyzt(x, y, N) do{(void)strncpy(x,y,N);x[N-1]='\0';}while(0)
#define	StrEq(x,y)	(!strcmp((x),(y)))

/* used in SetMode() in channel.c and m_umode() in s_msg.c */

#define	MODE_NULL      0
#define	MODE_ADD       0x40000000
#define	MODE_DEL       0x20000000

/* return values for hunt_server() */

#define	HUNTED_NOSUCH	(-1)	/* if the hunted server is not found */
#define	HUNTED_ISME	0	/* if this server should execute the command */
#define	HUNTED_PASS	1	/* if message passed onwards successfully */

/* used when sending to #mask or $mask */

#define	MATCH_SERVER  1
#define	MATCH_HOST    2

/* used for async dns values */

#define	ASYNC_NONE	(-1)
#define	ASYNC_CLIENT	0
#define	ASYNC_CONNECT	1
#define	ASYNC_CONF	2
#define	ASYNC_SERVER	3

/* misc variable externs */

extern	char	*version, *infotext[];
extern	char	*generation, *creation;

/* misc defines */

#define	FLUSH_BUFFER	-2
#define	UTMP		"/etc/utmp"
#define	COMMA		","

/* IRC client structures */

#ifdef	CLIENT_COMPILE
typedef	struct	Ignore {
	char	user[NICKLEN+1];
	char	from[USERLEN+HOSTLEN+2];
	int	flags;
	struct	Ignore	*next;
} anIgnore;

#define	IGNORE_PRIVATE	1
#define	IGNORE_PUBLIC	2
#define	IGNORE_TOTAL	3

#define	HEADERLEN	200

#endif

#endif /* __struct_include__ */
