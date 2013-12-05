/************************************************************************
 *   IRC - Internet Relay Chat, ircd/help.c
 *   Copyright (C) 1996 DALnet
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
static char sccsid[] = "@(#)help.c	6.00 9/22/96 (C) 1996 DALnet";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "msg.h"

#define HDR(str) sendto_one(sptr, ":%s 290 %s :" str "", me.name, name)
#define SND(str) sendto_one(sptr, ":%s 291 %s :" str "", me.name, name)
#define FTR(str) sendto_one(sptr, ":%s 292 %s :" str "", me.name, name)
#define HLP(str) sendto_one(sptr, ":%s 293 %s :" str "", me.name, name)

/*
 * This is _not_ the final help.c, we're just testing the functionality...
 * Return 0 to forward request to helpops... -Donwulff
 */
int parse_help (sptr, name, help)
aClient	*sptr;
char	*name;
char	*help;
{
  int i;
  if(BadPtr(help) || !mycmp(help, "HELP")) {
HDR("0 ***** DALnet Help System *****");
SND("  You need to specify your question after the help-command. For");
SND("  an index of topics, use /helpop index");
SND("  If the DALnet IRC Server is unable to satisfy your help-request");
SND("  it will be forwarded to appropriate people for handling.");
SND("  Precede your question with ! to automatically forward it to");
SND("  DALnet qualified helpers, or ? to never forward it.");

  } else if(!myncmp(help, "NICKSERV", 8)) {
HDR("0 ***** NickServ Help *****");
    if(!*(help+8) || !mycmp(help+9, "HELP")) {

SND("NickServ permits users to 'register' a nickname, and stop");
SND("  others from using that nick. NickServ allows the owner of a");
SND("  nick to disconnect a user using the owners registered nick.");
SND("  If a registered nick is not used by the owner for 20 days,");
SND("  NickServ will drop it, leaving it up for grabs by another user.");
SND("  Please do NOT register more nicks than you will actively use! =)");
SND("For more information on a command /msg NickServ help <command>");
SND("");
SND("Core Commands:");
SND("       REGISTER  - Register a nickname");
SND("       SET       - Change settings, including the KILL option");
SND("       ACCESS    - Change the list of addresses allowed to use");
SND("                   a nick");
SND("       IDENTIFY  - Authorize yourself using a password");
SND("       RECOVER   - Stop someone from using your registered nick");
SND("       GHOST     - Terminate a ghosted nickname");
SND("       DROP      - Drop a registered nickname");
SND("  Other Commands:");
SND("       RELEASE       INFO          ACC");

} else
    if(!mycmp(help+9, "REGISTER")) {

SND("Command - REGISTER");
SND("Usage   - REGISTER <password>");
SND("  This will register your current nickname with NickServ. This");
SND("  means that only you can have access to this nickname, unless");
SND("  you give access to others. The password is a case-sensative");
SND("  password that you make up. Please write down or memorize");
SND("  your password! You will need it later to change settings.");
SND("  When you register a nickname, an entry is added to your");
SND("  access list based on your current address.");
SND("Example:");
SND("  /msg NickServ REGISTER AnyGoodPassword");
SND("   (If dalvenjah's user@host mask was dalvenja@123.321.231.132,");
SND("    then dalvenja@123.321.231.* would be added to his access list.)");

} else
    if(!mycmp(help+9, "SET")) {

SND("Command - SET <command> [<value>]");
SND("  Lets you change your nickname settings. These are several options");
SND("  here, but to use any of them, your nickname MUST be registered.");
SND("For more information on a command /helpop NickServ SET <command>");
SND("");
SND("Available SET Commands:");
SND("        PASSWD    - Lets you change your nickname's password");
SND("        KILL      - Forcibly prevent people from using your nick");
SND("        URL       - Attaches a Uniform Resource Locator to your nick");
SND("        NOMEMO    - Disables ALL memos to your nick");
SND("        NOOP      - Stops users from adding you to AOp/SOp lists");

} else
  if(!myncmp(help+9, "SET ", 4)) {
    if(!mycmp(help+13, "PASSWD")) {

SND("Command - SET PASSWD");
SND("Usage   - SET PASSWD <password>");
SND("  This command changes the current password for your nickname.");
SND("  You must use the NickServ IDENTIFY command before doing this.");
SND("Example:");
SND("  /msg NickServ SET PASSWD AnyGoodPassword");

} else
    if(!mycmp(help+13, "KILL")) {

SND("Command - SET KILL");
SND("Usage   - SET KILL [ON|OFF]");
SND("  This option will make it much harder for anybody to use your");
SND("  nick. If someone changes their nick to a nick registered with");
SND("  kill on, they will be given 60 seconds in which they must");
SND("  either change nicks, or IDENTIFY themselves.");
SND("Example:");
SND("  /msg NickServ SET KILL ON");

} else
    if(!mycmp(help+13, "URL")) {

SND("Command - SET URL");
SND("Usage   - SET URL [<URL>]");
SND("  Will show anyone who asks NickServ for info on your nickname an");
SND("  URL (Uniform Resource Locator) along with the rest of your info.");
SND("  This could be your home page location, or an email address.");
SND("Examples:");
SND("  /msg NickServ SET URL http://www.dal.net/lefler/");
SND("  /msg NickServ SET URL mailto:lefler@dal.net");
SND("  /msg NickServ SET URL");
SND("           - The first shows a Web Page URL, the second");
SND("             one is an email address. The final example");
SND("             clears any previous URL setting.");

} else
    if(!mycmp(help+13, "NOMEMO")) {

SND("Command - SET NOMEMO");
SND("Usage   - SET NOMEMO ON|OFF");
SND("  When this option is on, ALL memos to your nickname will be");
SND("  ignored. MemoServ will simply not store them. This command could");
SND("  be usful if you were going on vacation for a while, and did not");
SND("  want your memo list to build up.");
SND("Example:");
SND("  /msg NickServ SET NOMEMO ON");

} else
    if(!mycmp(help+13, "NOOP")) {

SND("Command - SET NOOP");
SND("Usage   - SET NOOP ON|OFF");
SND("  When this is on, ChanServ will not allow your nick to be added to");
SND("  any AOp/SOp lists in any channels. Whoever tries to add you will");
SND("  get a notice from ChanServ back saying that you have NOOP set on.");
SND("Example:");
SND("  /msg NickServ SET NOOP ON");

  } else HLP("Try NICKSERV SET for list of available settings.");

} else
    if(!mycmp(help+9, "ACCESS")) {

SND("Command - ACCESS");
SND("Usage   - ACCESS LIST");
SND("          ACCESS ADD <user@host mask>");
SND("          ACCESS DEL <user@host mask>");
SND("  Allows you to list, add, or delete user@host masks from your");
SND("  access list. If your address matches a mask in this list, you");
SND("  will not need to IDENTIFY to NickServ to use your nick.");
SND("Examples:");
SND("  /msg NickServ ACCESS ADD calvin@hobbes.catt.ncsu.edu");
SND("  /msg NickServ ACCESS DEL birdman@alcatraz.com");

} else
    if(!mycmp(help+9, "IDENTIFY")) {

SND("Command - IDENTIFY");
SND("Usage   - IDENTIFY <password>");
SND("  This tells NickServ that you are the owner of a certain nickname.");
SND("  After you have identified, you have full access to the nickname.");
SND("Example:");
SND("  /msg NickServ IDENTIFY AnyGoodPassword");

} else
    if(!mycmp(help+9, "RECOVER")) {

SND("Command - RECOVER");
SND("Usage   - RECOVER <nick> [<password>]");
SND("  If you haven't set your nick kill switch to on, then this is the");
SND("  manual version of preventing someone from using your nickname.");
SND("  You only need to send the password with this command if your");
SND("  current address doesn't match the nick's access list. This");
SND("  will forcibly stop them from using your nick. A check in");
SND("  this command prevents you from killing yourself. :)");
SND("Example:");
SND("  /msg NickServ RECOVER dalvenjah AnyGoodPassword");

} else
    if(!mycmp(help+9, "GHOST")) {

SND("Command - GHOST");
SND("Usage   - GHOST <nick> [<password>]");
SND("  If, for some reason, your Internet connection fails, then your");
SND("  client may not log off IRC properly. This is called a ghost.");
SND("  With this command, you can remove your ghost.");
SND("  If you're not in the access list for the nick, you must");
SND("  use the password.");
SND("Example:");
SND("  /msg NickServ GHOST dalvenjah AnyGoodPassword");

} else
    if(!mycmp(help+9, "DROP")) {

SND("Command - DROP");
SND("Usage   - DROP <nick>");
SND("  Using this command makes NickServ stop watching your nick, and");
SND("  will remove it from services' database. If a nick is dropped,");
SND("  anyone else can register it. You must use the NickServ");
SND("  IDENTIFY command before doing this.");
SND("Example:");
SND("  /msg NickServ DROP Cit");

} else
    if(!mycmp(help+9, "RELEASE")) {

SND("Command - RELEASE");
SND("Usage   - RELEASE <nick> [password]");
SND("  If you used the recover command to stop another user from using");
SND("  your nick, then NickServ won't let go of the nick for 2 minutes.");
SND("  This command overrides that, telling NickServ to release the");
SND("  nick now. If you're not in the access list for the nick you");
SND("  want to release, then you will need to include the password.");
SND("Example:");
SND("  /msg NickServ RELEASE dalvenjah AnyGoodPassword");

} else
    if(!mycmp(help+9, "INFO")) {

SND("Command - INFO");
SND("Usage   - INFO <nick>");
SND("  If a nick is registered, this command shows certain info about");
SND("  it, such as the owner, when it was registered, when it was last");
SND("  recognised by NickServ, and what options the owner has set.");
SND("Example:");
SND("  /msg NickServ INFO Cit");

} else
    if(!mycmp(help+9, "ACC")) {

SND("Command - ACC");
SND("Usage   - ACC <nick>");
SND("  This can be used by bots to determine if the person using the");
SND("  nick is the real owner. The reply will be given in a notice:");
SND("");
SND("        ACC <nick> <access level>");
SND("");
SND("The returned access level is a number from 0 to 3:");
SND("   0 = No such registered nick.");
SND("   1 = User is not online or is not recognized by");
SND("       NickServ as the true owner.");
SND("   2 = User has access to a nick by matching a mask in");
SND("       the nick's access list.");
SND("   3 = User is identified by password to NickServ.");
SND("");
SND("Example:");
SND("  /msg NickServ ACC dalvenjah");

} else
    HLP("I do not know what you mean. Try NICKSERV HELP instead.");

  } else if(!myncmp(help, "CHANSERV", 8)) {
HDR("0 ***** ChanServ Help *****");
    if(!*(help+8) || !mycmp(help+9, "HELP")) {

SND("ChanServ gives normal users the ability to keep hold of a");
SND("  channel, without the need for a bot. Unlike other IRC networks,");
SND("  channel takeovers are virtually impossible, when they are registered.");
SND("  Registration is a quick and painless process. Once registered,");
SND("  the founder can maintain complete and total control of the");
SND("  channel. ChanServ will stop monitoring a channel if no Op enters");
SND("  the channel for 20 days or the founder's nick expires.");
SND("For more information on a command /msg ChanServ help <command>");
SND("");
SND("Core Commands:");
SND("      REGISTER  - Register a channel");
SND("      SET       - Change various channel configuration settings");
SND("      SOP       - Maintain SuperOp channel operator list");
SND("      AOP       - Maintain AutoOp channel operator list");
SND("      AKICK     - Maintain the channel AutoKick banned user list");
SND("      DROP      - Drop a registered channel");
SND("Other Commands:");
SND("      IDENTIFY      ACCESS        OP            DEOP");
SND("      INFO          INVITE        MKICK         MDEOP");
SND("      UNBAN         COUNT         WHY");

} else
    if(!mycmp(help+9, "REGISTER")) {

SND("Command - REGISTER");
SND("Usage   - REGISTER <channel> <password> <description>");
SND("  When you register a channel with ChanServ, you don't need to");
SND("  worry about takeovers, or bots to keep a list of Ops. ChanServ");
SND("  does all of this and more. The founder is the person who does");
SND("  the registering. Make up a password to register with. The");
SND("  password is used so that only the founder can completely");
SND("  control the channel. The description is only used when a");
SND("  user asks ChanServ for information on a channel, and you make");
SND("  this up also.");
SND("*** PLEASE NOTE: Also, PLEASE memorize your password, write");
SND("  it down somewhere, but PLEASE remember it! You will need it");
SND("  later on. Also, all passwords are case-sensative!");
SND("Examples:");
SND("  /msg ChanServ register #DragonRealm adminchannel Admin channel");
SND("  /msg ChanServ register #Macintosh NeXTStep Macintosh discussion");

} else
    if(!mycmp(help+9, "SET")) {

SND("Command - SET <channel> <command> [<value>]");
SND("  Allows you to change your channel settigns. These affect a");
SND("  registered channel's status and operation. Only the");
SND("  channel founder can use the SET commands.");
SND("For more information on a command /helpop ChanServ SET <command>");
SND("");
SND("Core SET Commands:");
SND("       FOUNDER   - Allows you to change the channel founder");
SND("       PASSWD    - Lets you change the channel founder password");
SND("       DESC      - Modify the channel's description");
SND("       MLOCK     - Locks the channel to certain modes");
SND("       OPGUARD   - More militant channel operator protection");
SND("       KEEPTOPIC - Maintain the topic when no one is on the channel");
SND("       URL       - Attaches a URL to the channel");
SND("       IDENT     - Ops have to identify to NickServ before");
SND("                   ChanServ ops them");
SND("       RESTRICT  - Allow ops only into a channel");
SND("Additional Comands:");
SND("       TOPICLOCK      LEAVEOPS     UNSECURE      PRIVATE");
SND("       MEMO");

} else
  if(!myncmp(help+9, "SET ", 4)) {
    if(!mycmp(help+13, "FOUNDER")) {

SND("Command - SET FOUNDER");
SND("Usage   - SET <channel> FOUNDER");
SND("  Allows a channel's founder to be changed to suit different");
SND("  situations should they arise. This command sets the founder");
SND("  of a channel to the current user. You must use the ChanServ");
SND("  IDENTIFY command before using this command.");
SND("Example:");
SND("  /msg ChanServ SET #DragonRealm FOUNDER");
SND("           - This would make the person using this command");
SND("             the new founder of #DragonRealm. This person");
SND("             would need to use ChanServ's IDENTIFY command");
SND("             before using this command.");

} else
    if(!mycmp(help+13, "PASSWD")) {

SND("Command - SET PASSWD");
SND("Usage   - SET <channel> PASSWD <password>");
SND("  Will change the password needed for founder access on a");
SND("  channel. You must use the IDENTIFY command to gain access.");
SND("Example:");
SND("  /msg ChanServ SET #DragonRealm PASSWD ANewPassword");

} else
    if(!mycmp(help+13, "DESC")) {

SND("Command - SET DESC");
SND("Usage   - SET <channel> DESC <description>");
SND("  Allows the founder to change the description of a channel.");
SND("Example:");
SND("  /msg ChanServ SET #DragonRealm DESC");

} else
    if(!mycmp(help+13, "MLOCK")) {

SND("Command - SET MLOCK");
SND("Usage   - SET <channel> MLOCK <mode lock mask>");
SND("  Changes mode lock pattern for specified channel. Setting");
SND("  a lock mask of * will turn off all mode locking.");
SND("Examples:");
SND("  /msg ChanServ SET #dragonrealm MLOCK +nt-ispklm");
SND("  /msg ChanServ SET #dragonrealm MLOCK +nt-ipklm");
SND("  /msg ChanServ SET #dragonrealm MLOCK *");
SND("           - The first example keeps modes nt ON and ispklm");
SND("             OFF. The second is the same, except mode s can be");
SND("             ON or OFF in the channel. The last example clears");
SND("             ALL previous mode locks on a channel.");

} else
    if(!mycmp(help+13, "OPGUARD")) {

SND("Command - SET OPGUARD");
SND("Usage   - SET <channel> OPGUARD [ON|OFF]");
SND("  Turns on/off ops guarding on a channel.  When ON, only");
SND("  AutoOps, SuperOps, and the channel founder will be allowed");
SND("  ops on the channel.");
SND("Example:");
SND("  /msg ChanServ SET #DragonRealm OPGUARD ON");

} else
    if(!mycmp(help+13, "KEEPTOPIC")) {

SND("Command - SET KEEPTOPIC");
SND("Usage   - SET <channel> KEEPTOPIC [ON|OFF]");
SND("  Turns \"Sticky\" topics for a channel on or off. If everybody in a");
SND("  certain channel leaves that channel, then the topic is normally");
SND("  lost. But with this option on, ChanServ will save the last topic");
SND("  and when the channel is recreated, it will set the topic to what it");
SND("  previously was, followed by the nick who previously set that topic.");
SND("Example:");
SND("  /msg ChanServ SET #DragonRealm KEEPTOPIC ON");

} else
    if(!mycmp(help+13, "URL")) {

SND("Command - SET URL");
SND("Usage   - SET <channel> URL [<url>]");
SND("  Allows a URL (Uniform Resource Locator) to be shown");
SND("  in the channel's INFO listing. Usually indicates");
SND("  where more information on the channel may be found.");
SND("Examples:");
SND("  /msg ChanServ SET #dragonrealm URL http://www.dal.net/");
SND("  /msg ChanServ SET #dragonrealm URL mailto:info@dal.net");
SND("  /msg ChanServ SET #dragonrealm URL");
SND("           - The first example is setting the URL to a Web");
SND("             Site, the second to an email address. The final");
SND("             is used to clear any former URL setting.");

} else
    if(!mycmp(help+13, "IDENT")) {

SND("Command - SET IDENT");
SND("Usage   - SET <channel> IDENT [ON|OFF]");
SND("  This command makes it harder for anyone to use someone elses nick");
SND("  to gain ops in your channel. If this option is on, then all ops");
SND("  have to identify to NickServ before ChanServ ops them.");
SND("  NickServ's Access Lists will have no effect on ops. Any AOp or");
SND("  SOp entries that don't use registered nicknames will also");
SND("  become useless.");
SND("Example:");
SND("  /msg ChanServ SET #Macintosh IDENT ON");

} else
    if(!mycmp(help+13, "RESTRICT")) {

SND("Command - SET RESTRICT");
SND("Usage   - SET <channel> RESTRICT [ON|OFF]");
SND("  This option will only allow ops into a channel. If someone who");
SND("  joins is not on the AOp/SOp list, they will be kickbanned when");
SND("  they enter the channel.");
SND("Example:");
SND("  /msg ChanServ SET #Macintosh RESTRICT ON");

} else
    if(!mycmp(help+13, "TOPICLOCK")) {

SND("Command - SET TOPICLOCK");
SND("Usage   - SET <channel> TOPICLOCK [FOUNDER|SOP|OFF]");
SND("  Sets the \"topic lock\" option for a channel. When on, only the");
SND("  founder or SuperOps (depending on the option) are able to change");
SND("  the topic. This setting also performs the function of the");
SND("  KEEPTOPIC command.");
SND("Example:");
SND("  /msg ChanServ SET #DragonRealm TOPICLOCK SOP");

} else
    if(!mycmp(help+13, "LEAVEOPS")) {

SND("Command - SET LEAVEOPS");
SND("Usage   - SET <channel> LEAVEOPS [ON|OFF]");
SND("  Turns on/off leave-ops behavior on a channel.  When ON, the");
SND("  channel will behave as if ChanServ was not present, and will");
SND("  not deop users who 'create' the channel. AutoOps and");
SND("  SuperOps will still be opped.");
SND("Example:");
SND("  /msg ChanServ SET #DragonRealm LEAVEOPS ON");

} else
    if(!mycmp(help+13, "UNSECURE")) {

SND("Command - SET UNSECURE");
SND("Usage   - SET <channel> UNSECURE [ON|OFF]");
SND("  Will make a channel a little less secure than normal. When on,");
SND("  you need only be in the founders nick access list to make founder");
SND("  level changes; you won't need to identify.");
SND("*** NOTE: This setting can cause problems if you do not have");
SND("  a secure NickServ Access List. This setting is strongly");
SND("  discouraged.");
SND("Example:");
SND("  /msg ChanServ SET #afd UNSECURE ON");

} else
    if(!mycmp(help+13, "PRIVATE")) {

SND("Command - SET PRIVATE");
SND("Usage   - SET <channel> PRIVATE [ON|OFF]");
SND("  When this option is ON, only those who know about the channel");
SND("  will have access to it. An MLOCK of +p will be set.");
SND("Example:");
SND("  /msg ChanServ SET #Macintosh PRIVATE ON");

} else
    if(!mycmp(help+13, "MEMO")) {

SND("Command - SET MEMO");
SND("Usage   - SET <channel> MEMO NONE|AOP|SOP|FOUNDER");
SND("  Use this command to limit who can send channel memos. When");
SND("  NONE, no-one can send channel memos. When AOP, only AOps and");
SND("  above can send channel memos (this is the default setting). When");
SND("  SOP, only SOps and the founder can send channel memos. When");
SND("  FOUNDER, only the Channel Founder can send channel memos.");
SND("Example:");
SND("  /msg ChanServ SET #DragonRealm MEMO AOP");

  } else HLP("Try CHANSERV SET for list of available settings.");

} else
    if(!mycmp(help+9, "SOP")) {

SND("Command - SOP");
SND("Usage   - SOP <channel> ADD <nick or mask>");
SND("          SOP <channel> DEL <index number or mask>");
SND("          SOP <channel> LIST [<search pattern>]");
SND("          SOP <channel> WIPE");
SND("          SOP <channel> CLEAN");
SND("  Maintains the channel SuperOp list. All SOP commands are limited");
SND("  to the channel founder except the LIST command, which is");
SND("  available to all AutoOps or above.");
SND("  ADD adds a user to a channels SuperOp access list. DEL removes");
SND("  a user from a channel's SuperOp access list. LIST lists the");
SND("  SuperOp access list, with an index number. When LIST is used");
SND("  with a search pattern, only those entries to the SOp access list");
SND("  matching your search pattern will be shown to you. LIST will");
SND("  only display the first 100 entries.");
SND("  WIPE will totally clear the SOp list.");
SND("  CLEAN will remove any SOp entries whose nicks have expired.");
SND("Examples:");
SND("  /msg ChanServ SOP #dragonrealm ADD dalvenjah");
SND("  /msg ChanServ SOP #dragonrealm ADD *!besmith@*.uncc.edu");
SND("  /msg ChanServ SOP #dragonrealm DEL 3");
SND("  /msg ChanServ SOP #dragonrealm LIST");
SND("  /msg ChanServ SOP #dragonrealm WIPE");
SND("  /msg ChanServ SOP #dragonrealm CLEAN");

} else
    if(!mycmp(help+9, "AOP")) {

SND("Command - AOP");
SND("Usage   - AOP <channel> ADD <nick or mask>");
SND("          AOP <channel> DEL <index number or mask>");
SND("          AOP <channel> LIST [<search pattern>]");
SND("          AOP <channel> WIPE");
SND("          AOP <channel> CLEAN");
SND("  Maintains the channel AutoOp list. Only the channel founder");
SND("  may use CLEAN or WIPE, Sops or above may ADD and DEL AOps,");
SND("  and any AOp or above may use LIST.");
SND("  ADD adds a user to a channels AutoOp access list. DEL removes a");
SND("  user from a channel's AutoOp access list. LIST lists the");
SND("  AutoOp access list, with an index number. When LIST is used");
SND("  with a search pattern, only those entries to the AOp access list");
SND("  matching your search pattern will be shown to you. In a list,");
SND("  only the first 100 entries will be shown.");
SND("  WIPE will totally clear the AOp list.");
SND("  CLEAN will remove any expired nicknames from the list.");
SND("Examples:");
SND("  /msg ChanServ AOP #dragonrealm ADD dalvenjah");
SND("  /msg ChanServ AOP #dragonrealm ADD *!besmith@*.uncc.edu");
SND("  /msg ChanServ AOP #dragonrealm DEL 3");
SND("  /msg ChanServ AOP #dragonrealm LIST");
SND("  /msg ChanServ AOP #dragonrealm WIPE");
SND("  /msg ChanServ AOP #dragonrealm CLEAN");

} else
    if(!mycmp(help+9, "AKICK")) {

SND("Command - AKICK");
SND("Usage   - AKICK <channel> ADD <nick or mask>");
SND("          AKICK <channel> DEL <index number or mask>");
SND("          AKICK <channel> LIST [<search pattern>]");
SND("          AKICK <channel> WIPE");
SND("  Maintains the channel AutoKick list. If a user on the channel");
SND("  AKICK list does try to join a channel, then they will be");
SND("  kicked by ChanServ, as well as banned.");
SND("  WIPE is limited to the channel founder, ADDing and DELeting");
SND("  AKICKs is limited to SOps or above, list can be used by");
SND("  AOps or above.");
SND("  ADD adds a user to a channel's AutoKick list. DEL removes");
SND("  a user from a channel's AutoKick list. LIST lists the");
SND("  AutoKick list, with an index number. When LIST is used");
SND("  with a search pattern, only those entries to the");
SND("  AutoKick list matching your search pattern will be shown");
SND("  to you. Only the first 100 entries in a LIST will be shown.");
SND("  WIPE will remove all entries from the AutoKick list.");
SND("Examples:");
SND("  /msg ChanServ AKICK #dragonrealm ADD dalvenjah");
SND("  /msg ChanServ AKICK #dragonrealm ADD *!besmith@*.uncc.edu");
SND("  /msg ChanServ AKICK #dragonrealm DEL 3");
SND("  /msg ChanServ AKICK #dragonrealm LIST");
SND("  /msg ChanServ AKICK #dragonrealm WIPE");

} else
    if(!mycmp(help+9, "DROP")) {

SND("Command - DROP");
SND("Usage   - DROP <channel>");
SND("  This command is used when you no longer want ChanServ to manage");
SND("  a channel. Use of this command is limited to the channel founder.");
SND("  Before using the DROP command you MUST use the ChanServ");
SND("  IDENTIFY command.");
SND("Example:");
SND("  /msg ChanServ DROP #DragonRealm");

} else
    if(!mycmp(help+9, "IDENTIFY")) {

SND("Command - IDENTIFY");
SND("Usage   - IDENTIFY <channel> <password>");
SND("  This command will identify the user to ChanServ as the founder,");
SND("  and give them full access to the channel.");
SND("Example:");
SND("  /msg ChanServ IDENTIFY #DragonRealm AnyGoodPassword");

} else
    if(!mycmp(help+9, "ACCESS")) {

SND("Command - ACCESS");
SND("Usage   - ACCESS <channel> [<nick>]");
SND("  When used without the optional <nick> parameter, this command");
SND("  allows a user to query their access on any registered channel.");
SND("  The reply for this form of the command will verbosly state your");
SND("  access on the given channel (Basic, AOP, SOP, or Founder).");
SND("  When used with the optional <nick> parameter, you must be");
SND("  an AOp or higher on the specificed channel. This form of the");
SND("  command could serve as a protocol for bots to query user access");
SND("  on a channel using the ChanServ facility, thus relieving the");
SND("  bot of keeping track of access lists. For a registered channel,");
SND("  the reply is given in a NOTICE of the following format:");
SND("");
SND("       ACC <channel> <nick> <user@host> <access level>");
SND("");
SND("The returned access level is a number from -1 to 5, where:");
SND(" -1 = AutoKICKed from the channel");
SND("  0 = basic");
SND("  1 = AutoOp");
SND("  3 = Has founder access via a NickServ access list mask");
SND("  4 = Has founder access via identification to NickServ");
SND("  5 = Has founder access via identification to ChanServ");
SND("");
SND("  If the user is not online, the user@host and access level will be");
SND("  *UNKNOWN* and 0, respectively.");
SND("Examples:");
SND("  /msg ChanServ ACCESS #dalnethelp");
SND("  /msg ChanServ ACCESS #dalnethelp Cit");

} else
    if(!mycmp(help+9, "ACC")) {

SND("Command - ACC");
SND("Usage   - ACC <channel> <nick>");
SND("  This command gives exactly the same results as specifying a");
SND("  nick with the ACCESS command. ACCESS can effectivly be used");
SND("  in place of this command. Refer to the help for ACCESS for");
SND("  the format of the output and the meaning of the levels.");
SND("  Limited to channel AOps or above.");
SND("Example:");
SND("  /msg ChanServ ACC #dalnethelp Cit");

} else
    if(!mycmp(help+9, "OP")) {

SND("Command - OP");
SND("Usage   - OP <channel> [-]<nick>...");
SND("  Will op the given nick(s) in the given channel. Will not work if");
SND("  secured ops is on and the user isn't in the AOp or SOp list.");
SND("  When used with the - flag, it deops the named user(s). You cannot");
SND("  deop someone who outranks you, ie an AOp can't deop a SOp using");
SND("  this command. Limited to AutoOp or above.");
SND("Example:");
SND("  /msg ChanServ OP #DragonRealm Cit");

} else
    if(!mycmp(help+9, "DEOP")) {

SND("Command - DEOP");
SND("Usage   - DEOP <channel> <nick>");
SND("  This command is used to remove operator status from someone in a");
SND("  channel. It's the same as /msg ChanServ op #name -nick; You cannot");
SND("  deop someone who outranks you, ie an AOp can't deop a SOp using");
SND("  this command. Limited to AutoOp or above.");
SND("Example:");
SND("  /msg ChanServ DEOP #DragonRealm Cit");

} else
    if(!mycmp(help+9, "INFO")) {

SND("Command - INFO");
SND("Usage   - INFO <channel>");
SND("  Shows information for a channel, such as the channel founder,");
SND("  any mode locks, the current topic, the channel description,");
SND("  any settings such as topic lock, the time the channel was");
SND("  registered, and the time of the last opping.");
SND("Example:");
SND("  /msg ChanServ info #DragonRealm");

} else
    if(!mycmp(help+9, "INVITE")) {

SND("Command - INVITE");
SND("Usage   - INVITE <channel>");
SND("  Invites the sender to a channel that is set to invite only (mode");
SND("  +i). Used mainly for channels where the MLOCK is set +i. You");
SND("  cannot invite other users to a channel via ChanServ.");
SND("  Limited to channel AutoOps or above.");
SND("Example:");
SND("  /msg ChanServ INVITE #DragonRealm");

} else
    if(!mycmp(help+9, "MKICK")) {

SND("Command - MKICK");
SND("Usage   - MKICK <channel>");
SND("  Evacuates a channel completely by kicking everyone out, banning");
SND("  *!*@* (everyone), setting mode +i (invite only) and +l 1. When");
SND("  MKICKing a channel, you cannot UNAN or INVITE yourself back");
SND("  into the channel. This should only be used in a takeover");
SND("  situation. Limited to channel AutoOps and above while they are");
SND("  not outranked on the channel (if there is a SOp present, an AOp");
SND("  cannot MKICK).");
SND("Example:");
SND("  /msg ChanServ MKICK #Macintosh");

} else
    if(!mycmp(help+9, "MDEOP")) {

SND("Command - MDEOP");
SND("Usage   - MDEOP <channel>");
SND("  Removes operator status from all users on the named channel who");
SND("  don't outrank the user, ie an AOp can't deop a SOp. Limited to");
SND("  channel AutoOps and above.");
SND("Example:");
SND("  /msg ChanServ MDEOP #dalnethelp");

} else
    if(!mycmp(help+9, "UNBAN")) {

SND("Command - UNBAN");
SND("Usage   - UNBAN <channel> [ME|ALL]");
SND("  Will remove bans in the specified channels. The ME option,");
SND("  available to channel AutoOps or higher, will remove all");
SND("  bans that match your current address. The ALL option,");
SND("  which is limited to channel SuperOps or higher, will");
SND("  remove all bans from the channel ban list.");
SND("Examples:");
SND("  /msg ChanServ UNBAN #dragonrealm ME");
SND("  /msg ChanServ UNBAN #afd ALL");

} else
  if(!mycmp(help+9, "COUNT")) {

SND("Command - COUNT");
SND("Usage   - COUNT <channel>");
SND("  This command will display the number of AOps, SOps, and");
SND("  AKICKs in a channel. Limited to AutoOp or higher.");
SND("Example:");
SND("  /msg ChanServ COUNT #Macintosh");

} else
    if(!mycmp(help+9, "WHY")) {

SND("Command - WHY");
SND("Usage   - WHY <channel> <nick>");
SND("  This command is used to tell why the given nick is gaining");
SND("  operator status (ops) on the given registered channel. The reply");
SND("  will tell which entry, either registered nick or mask, in the AOp");
SND("  or SOp list is allowing the nick to gain ops. This command has no");
SND("  useful effect if the given nick is not gaining ops or some type");
SND("  of access in the given channel.");
SND("Example:");
SND("  /msg ChanServ why #dalnethelp Ramuh");

} else
    HLP("I do not know what you mean. Try CHANSERV HELP instead.");

  } else if(!myncmp(help, "MEMOSERV", 8)) {
HDR("0 ***** MemoServ Help *****");
    if(!*(help+8) || !mycmp(help+9, "HELP")) {

SND("MemoServ allows users registered with NickServ to send");
SND("  each other messages, which can be read by the recipient at");
SND("  his/her leisure. Memos can be sent to people even when they");
SND("  are not on IRC.");
SND("For more information on a command /helpop MemoServ <command>");
SND("");
SND("Commands:");
SND("       SEND      - Send another user or a channel a memo");
SND("       SENDSOP   - Sends a memo to all SOPs");
SND("       LIST      - List your current memos");
SND("       READ      - Read a memo");
SND("       DEL       - Mark a memo as deleted");
SND("       UNDEL     - Mark a memo as not deleted");
SND("       PURGE     - Erase memos marked as deleted");
SND("       FORWARD   - Arrange or modify memo forwarding");

} else
    if(!mycmp(help+9, "SEND")) {

SND("Command - SEND");
SND("Usage   - SEND <nick> <memo>");
SND("          SEND <channel> <memo>");
SND("  This lets you send a short memo to another nick. Both your nick,");
SND("  and the nick of the person you're sending the memo to must be");
SND("  registered, otherwise you will be unable to send a memo.");
SND("");
SND("  This command can also be used to send a memo to all the ops of");
SND("  a channel registered with ChanServ. When a memo is sent to a");
SND("  channel, all AOps and SOps entered by registered nick, and the");
SND("  channel founder will receive a copy. Access for sending channel");
SND("  memos depends on the channel founder's ChanServ MEMO Setting,");
SND("  however you must always be an AOp or better on the channel.");
SND("Examples:");
SND("  /msg MemoServ SEND JoeUser Meet me in #Macintosh at 8pm.");
SND("  /msg MemoServ SEND #dalnethelp Channel meeting, 8pm!");

} else
    if(!mycmp(help+9, "SENDSOP")) {

SND("Command - SENDSOP");
SND("Usage   - SENDSOP <channel> <memo>");
SND("  This command is used to send memos to all SOps and the founder");
SND("  in a channel. Only gets sent to those nicks in the SOp list");
SND("  that are registered. Limited to AutoOp and above.");
SND("Example:");
SND("  /msg MemoServ SENDSOP #DragonRealm This is a SOps only memo.");

} else
    if(!mycmp(help+9, "LIST")) {

SND("Command - LIST");
SND("Usage   - LIST");
SND("  Displays a list of memos sent to you. You cannot see memos that");
SND("  you have sent. Only 20 memos will be displayed. If you have more");
SND("  than 20 memos, you will have to delete some of the earlier memos,");
SND("  to get at the ones near the top.");
SND("Example:");
SND("  /msg MemoServ LIST");

} else
    if(!mycmp(help+9, "READ")) {

SND("Command - READ");
SND("Usage   - READ <memo number>");
SND("  Shows you the contents of <memo number>. To find out what <memo");
SND("  number> is, use /msg MemoServ List.");
SND("Example:");
SND("  /msg MemoServ READ 1");
SND("  /msg MemoServ READ 15");

} else
    if(!mycmp(help+9, "DEL")) {

SND("Command - DEL");
SND("Usage   - DEL <memo number>");
SND("          DEL ALL");
SND("  Deletes memo <memo number> (as given by the LIST command)");
SND("  from your memo list. DEL ALL deletes and purges all of your");
SND("  memos without restriction, so use it with care. There is no way");
SND("  to get a memo back after it has been deleted and PURGED.");
SND("Note: DEL does not actually remove the memo, but simply");
SND("  marks it as deleted (indicated with D in the LIST display).");
SND("  The memo is not removed until you sign off or the PURGE");
SND("  command is used.");
SND("Examples:");
SND("  /msg MemoServ DEL ALL");
SND("  /msg MemoServ DEL 13");

} else
    if(!mycmp(help+9, "UNDEL")) {

SND("Command - UNDEL");
SND("Usage   - UNDEL <memo number>");
SND("  Marks memo <memo numbe> (as given by the LIST command) as");
SND("  undeleted, if has been marked deleted with the DEL command");
SND("Example:");
SND("  /msg MemoServ UNDEL 1");

} else
    if(!mycmp(help+9, "PURGE")) {

SND("Command - PURGE");
SND("Usage   - PURGE");
SND("  This command removes all memos which have been marked as");
SND("  deleted from your memo list.");
SND("  This command cannot be reversed, so use it carefully.");
SND("Example:");
SND("  /msg MemoServ PURGE");

} else
    if(!mycmp(help+9, "FORWARD")) {

SND("Command - FORWARD");
SND("Usage   - FORWARD");
SND("          FORWARD -");
SND("          FORWARD <nickname> [password]");
SND("  This command will 'forward' memos from one registered nickname");
SND("  to another. The first command will tell you if forwarding on your");
SND("  current nickname is on. The second command will turn forwarding");
SND("  for your current nickname off.");
SND("  The third will forward memos from your current nick to <nick>.");
SND("  You must know the password of the nick you wish to forward");
SND("  memos to.");
SND("Examples:");
SND("  /msg MemoServ FORWARD");
SND("  /msg MemoServ FORWARD -");
SND("  /msg MemoServ FORWARD dalvenjah AnyGoodPassword");

} else
    HLP("I do not know what you mean. Try MEMOSERV HELP instead.");

  } else if(!myncmp(help, "IRCD", 8)) {
HDR("0 ***** ircd Help *****");
SND("Currently help is available on the following topics:");
SND("  WATCH    - new DALnet notify system");
/* SND("  HELPOP   - DALnet Help System"); */
SND("  LIST     - new /list features");
/* SND("  WHO      - new /who features"); */
/* SND("  PROTOCTL - protocol control (for client/script authors)"); */
SND("  PRIVMSG  - special /msg and /notice formats");
SND("  COMMANDS - full list of server commands");
  } else if(!myncmp(help, "WATCH", 8)) {
HDR("0 ***** ircd Help *****");
SND("Watch is a new notify-type system on DALnet which is both faster");
SND("and uses less network resources than any old-style notify");
SND("system. You may add entries to your Watch list with the command");
SND("/watch +nick1 [+nick2 +nick3 ...], and the server will send");
SND("you a message when any nickname in your watch list logs on or off.");
SND("Use /watch -nick to remove a nickname from the watch list, and");
SND("just /watch to view your watch list.");
SND("The watch list DOES NOT REMAIN BETWEEN SESSIONS - you (or your");
SND("script or client) must add the nicknames to your watch list every");
SND("time you connect to an IRC server.");
  } else if(!myncmp(help, "HELPOP", 8)) {
HDR("0 ***** ircd Help *****");
SND("Sorry, we're working on this.");
  } else if(!myncmp(help, "LIST", 8)) {
HDR("0 ***** ircd Help *****");
SND("New extended /list command options are supported.  To use these");
SND("features, you will likely need to prefix the LIST command with");
SND("/quote to avoid your client interpreting the command.");
SND("");
SND("Usage: /quote LIST options");
SND("");
SND("If you don't include any options, the default is to send you the");
SND("entire unfiltered list of channels. Below are the options you can");
SND("use, and what channels LIST will return when you use them.");
SND(">number  List channels with more than <number> people.");
SND("<number  List channels with less than <number> people.");
SND("C>number List channels created between now and <number> minutes ago.");
SND("C<number List channels created earlier than <number> minutes ago.");
SND("T>number List channels whose topics are older than <number> minutes");
SND("         (Ie., they have not changed in the last <number> minutes.");
SND("T<number List channels whose topics are newer than <number> minutes.");
SND("*mask*   List channels that match *mask*");
SND("!*mask*  List channels that do not match *mask*");
SND("LIST defaults to sending a list of channels with 2 or more members,");
SND("so use the >0 option to get the full channel listing.");
  } else if(!myncmp(help, "WHO", 8)) {
HDR("0 ***** ircd Help *****");
SND("Sorry, we're working on this.");
  } else if(!myncmp(help, "PROTOCTL", 8)) {
HDR("0 ***** ircd Help *****");
SND("Sorry, we're working on this.");
  } else if(!myncmp(help, "PRIVMSG", 8)) {
HDR("0 ***** ircd Help *****");
SND("PRIVMSG and NOTICE, which are used internally by the client for");
SND("/msg and /notice, on DALnet support two additional formats:");
SND("/msg @#channel <text> will send the text to channel-ops on the");
SND("given channel only. /msg @+#channel <text> will send the text");
SND("to both ops and voiced users on the channel. While some clients");
SND("may support these as-is, on others (such as ircII), it's necessary");
SND("to use /quote privmsg @#channel <text> instead. It's perhaps a");
SND("good idea to add the /alias omsg /quote privmsg @$0 $1- into");
SND("your script (.ircrc) file in that case.");
  } else if(!myncmp(help, "COMMANDS", 8)) {
HDR("0 ***** ircd Help *****");
SND("Full command list, in the format: command token");
	/* Send the command list (with tokens)
	 * -Wizzu
	 */
	for (i = 0; msgtab[i].cmd; i++)
		/* The following command is (almost) the same as the SND
		 * macro, but includes variables.  -Wizzu
		 */
		sendto_one(sptr,":%s 291 %s :%s %s",
			me.name, name, msgtab[i].cmd, msgtab[i].token);
  } else { /* Flood back the user ;) */
    HLP("If you need help on DALnet services, try...");
    HLP("  /helpop nickserv - for help on registering nicknames");
    HLP("  /helpop chanserv - for help on registering channels");
    HLP("  /helpop memoserv - for help on sending short messages");
    HLP("For help on DALnet IRC server extra features, use...");
    HLP("  /helpop ircd     - special DALnet ircd features");
    HLP("If /helpop gives \"unknown command\" on your client, try using");
    HLP("/quote helpop instead.");
    FTR(" ***** Go to #dalnethelp if you have any further questions *****");
    if(!myncmp(help, "INDEX", 8))
      return 1;
    else
      return 0;
  }
  FTR(" ***** Go to #dalnethelp if you have any further questions *****");
  return 1;
}
