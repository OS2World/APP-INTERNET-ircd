/************************************************************************
 *   IRC - Internet Relay Chat, include/sock.h
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

/*
 * $Id: sock.h,v 1.2 1997/12/29 07:17:35 wd Exp $
 *
 * $Log: sock.h,v $
 * Revision 1.2  1997/12/29 07:17:35  wd
 * df4.6.2
 * ee CHANGES for updates
 * -wd
 *
 * Revision 1.1.1.1  1997/08/22 17:23:01  donwulff
 * Original import from the "deadlined" version.
 *
 * Revision 1.1.1.1  1996/11/18 07:53:41  explorer
 * ircd 4.3.3 -- about time
 *
 * Revision 1.1.1.1.4.1  1996/09/16 02:45:38  donwulff
 * *** empty log message ***
 *
 * Revision 6.1  1991/07/04  21:04:35  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:04  gruner
 * frozen beta revision 2.6.1
 *
 */

#ifndef FD_ZERO
#define FD_ZERO(set)      (((set)->fds_bits[0]) = 0)
#define FD_SET(s1, set)   (((set)->fds_bits[0]) |= 1 << (s1))
#define FD_ISSET(s1, set) (((set)->fds_bits[0]) & (1 << (s1)))
#define FD_SETSIZE        30
#endif
