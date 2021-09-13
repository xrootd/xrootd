#ifndef __XRDCMSROLE_HH__
#define __XRDCMSROLE_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d C m s R o l e . h h                          */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <cstring>
  
// This class centralizes the naming of various roles servers can have.

class XrdCmsRole
{
public:

enum RoleID {MetaManager = 0,
             Manager,      Supervisor, Server,
             ProxyManager, ProxySuper, ProxyServer,
             PeerManager,  Peer,       noRole
            };

static RoleID      Convert(const char *Tok1, const char *Tok2)
                   {if (!Tok2)
                       {if (!strcmp(   Tok1, "server"))     return Server;
                        if (!strcmp(   Tok1, "supervisor")) return Supervisor;
                        return (strcmp(Tok1, "manager") ?   noRole:Manager);
                       }
                    if (!strcmp(       Tok1, "proxy"))
                       {if (!strcmp(   Tok2, "server"))     return ProxyServer;
                        if (!strcmp(   Tok2, "supervisor")) return ProxySuper;
                        return (strcmp(Tok2, "manager") ?   noRole:ProxyManager);
                       }
                    if (!strcmp(       Tok1, "meta"))
                        return (strcmp(Tok2, "manager") ?   noRole:MetaManager);
                    return noRole;
                   }

static const char *Name(RoleID rid)
                   {static const char *rName[] = {"meta manager",  // MetaMan
                                                  "manager",       // Manager
                                                  "supervisor",    // Super
                                                  "server",        // Server
                                                  "proxy manager", // ProxyMan
                                                  "proxy supervisor",
                                                  "proxy server",  // ProxyServ
                                                  "peer manager",  // PeerMan
                                                  "peer"           // Peer
                                                 };
                    if (rid >= MetaManager && rid < noRole) return rName[rid];
                    return "??";
                   }

static const char *Type(RoleID rid) // Maximum of 3 characters plus null byte!
                   {static const char *tName[] = {"MM",            // MetaMan
                                                  "M",             // Manager
                                                  "R",             // Super
                                                  "S",             // Server
                                                  "PM",            // ProxyMan
                                                  "PR",            // ProxySuper
                                                  "PS",            // ProxyServ
                                                  "EM",            // PeerMan
                                                  "E"              // Peer
                                                 };
                    if (rid >= MetaManager && rid < noRole) return tName[rid];
                    return "??";
                   }

static const char *Type(const char *rtype)
                   {if (*rtype == 'M') return "manager";
                    if (*rtype == 'R') return "supervisor";
                    if (*rtype == 'S') return "server";
                    if (*rtype == 'P') return "proxy";
                    if (*rtype == 'E') return "peer";
                    return "";
                   }

                   XrdCmsRole() {}
                  ~XrdCmsRole() {}
};
#endif
