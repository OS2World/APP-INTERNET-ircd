/************************************************************************
 *   IRC - Internet Relay Chat, include/common.h
 *   Copyright (C) 1990 Armin Gruner
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

#ifndef	__common_include__
#define __common_include__

#include <time.h>
#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#include <winsock.h>
#include <process.h>
#include <io.h>
#include "struct.h"
#endif

#ifdef	PARAMH
#include <sys/param.h>
#endif

#ifndef PROTO
#if __STDC__
#	define PROTO(x)	x
#else
#	define PROTO(x)	()
#endif
#endif

#ifndef NULL
#define NULL 0
#endif

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define FALSE (0)
#define TRUE  (!FALSE)

#ifndef UNSURE
#define UNSURE (2)
#endif

#if 0
#ifndef	MALLOCH
char	*malloc(), *calloc();
void	free();
#else
#include MALLOCH
#endif
#endif

extern	int	match PROTO((char *, char *));
#define mycmp(a,b) \
 ( (toupper((a)[0])!=toupper((b)[0])) || smycmp((a)+1,(b)+1) )
extern  int     smycmp PROTO((char *, char *));
extern	int	myncmp PROTO((char *, char *, int));
#ifdef NEED_STRTOK
extern	char	*strtok PROTO((char *, char *));
#endif
#ifdef NEED_STRTOKEN
extern	char	*strtoken PROTO((char **, char *, char *));
#endif
#ifdef NEED_INET_ADDR
extern unsigned long inet_addr PROTO((char *));
#endif

#if defined(NEED_INET_NTOA) || defined(NEED_INET_NETOF) && !defined(_WIN32) \
	&& !defined(SCOUNIX)
#include <netinet/in.h>
#endif

#ifndef SCOUNIX /* SCO Openserver has these, but the header files are broken */
#ifdef NEED_INET_NTOA
extern char *inet_ntoa PROTO((struct in_addr));
#endif

#ifdef NEED_INET_NETOF
extern int inet_netof PROTO((struct in_addr));
#endif
#endif /* SCOUNIX */

int global_count, max_global_count;
extern char *myctime PROTO((time_t));
extern char *strtoken PROTO((char **, char *, char *));

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define DupString(x,y) do{x=MyMalloc(strlen(y)+1);(void)strcpy(x,y);}while(0)

#ifdef USE_CASETABLES
extern int casetable;
extern u_char *tolowertab, tolowertab1[], tolowertab2[];
extern u_char *touppertab, touppertab1[], touppertab2[];
#else
#ifndef SCOUNIX
extern u_char tolowertab[], touppertab[];
#else
extern unsigned char tolowertab[], touppertab[]; 
	/* Typedef for u_char exists and is wrong. */
#endif
#endif

#undef tolower
#define tolower(c) (tolowertab[(u_char)(c)])

#undef toupper
#define toupper(c) (touppertab[(u_char)(c)])

#undef isalpha
#undef isdigit
#undef isxdigit
#undef isalnum
#undef isprint
#undef isascii
#undef isgraph
#undef ispunct
#undef islower
#undef isupper
#undef isspace
#undef iscntrl

extern unsigned char char_atribs[];

#define PRINT 1
#define CNTRL 2
#define ALPHA 4
#define PUNCT 8
#define DIGIT 16
#define SPACE 32
#define ALLOW 64

#ifndef KLINE_TEMP
#define KLINE_PERM 0
#define KLINE_TEMP 1
#define KLINE_AKILL 2
#endif

#define	iscntrl(c) (char_atribs[(u_char)(c)]&CNTRL)
#define isallowed(c) (char_atribs[(u_char)(c)]&ALLOW)
#define isalpha(c) (char_atribs[(u_char)(c)]&ALPHA)
#define isspace(c) (char_atribs[(u_char)(c)]&SPACE)
#define islower(c) ((char_atribs[(u_char)(c)]&ALPHA) && ((u_char)(c) > 0x5f))
#define isupper(c) ((char_atribs[(u_char)(c)]&ALPHA) && ((u_char)(c) < 0x60))
#define isdigit(c) (char_atribs[(u_char)(c)]&DIGIT)
#define	isxdigit(c) (isdigit(c) || 'a' <= (c) && (c) <= 'f' || \
		     'A' <= (c) && (c) <= 'F')
#define isalnum(c) (char_atribs[(u_char)(c)]&(DIGIT|ALPHA))
#define isprint(c) (char_atribs[(u_char)(c)]&PRINT)
#define isascii(c) ((u_char)(c) >= 0 && (u_char)(c) <= 0x7f)
#define isgraph(c) ((char_atribs[(u_char)(c)]&PRINT) && ((u_char)(c) != 0x32))
#define ispunct(c) (!(char_atribs[(u_char)(c)]&(CNTRL|ALPHA|DIGIT)))

extern char *MyMalloc();
extern void flush_connections();
extern struct SLink *find_user_link(/* struct SLink *, struct Client * */);

/*
 * Protocol support text.  DO NO CHANGE THIS unless you know what
 * you are doing.
 */
#define PROTOCTL_SUPPORTED "NOQUIT TOKEN WATCH=128 SAFELIST"

#ifdef _WIN32
/*
 * Used to display a string to the GUI interface.
 * Windows' internal strerror() function doesn't work with socket errors.
 */
extern	int	DisplayString(HWND hWnd, char *InBuf, ...);
#undef	strerror
#endif

#ifdef __FreeBSD__
extern char *malloc_options;
#endif

extern int lu_noninv, lu_inv, lu_serv, lu_oper, 
           lu_unknown, lu_channel, lu_lu, lu_lulocal, lu_lserv, 
           lu_clu, lu_mlu, lu_cglobalu, lu_mglobalu;
           
time_t	now;

#endif /* __common_include__ */
