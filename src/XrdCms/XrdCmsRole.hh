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
/******************************************************************************/

#include <string.h>
  
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
                        if (!strcmp(   Tok1, "peer"))       return Peer;
                        return (strcmp(Tok1, "manager") ?   noRole:Manager);
                       }
                    if (!strcmp(       Tok1, "proxy"))
                       {if (!strcmp(   Tok2, "server"))     return ProxyServer;
                        if (!strcmp(   Tok2, "supervisor")) return ProxySuper;
                        return (strcmp(Tok2, "manager") ?   noRole:ProxyManager);
                       }
                    if (!strcmp(       Tok1, "meta"))
                        return (strcmp(Tok2, "manager") ?   noRole:MetaManager);
                    if (!strcmp(       Tok1, "peer"))
                        return (strcmp(Tok2, "manager") ?   noRole:Peer);
                    return noRole;
                   }

static const char *Name(RoleID rid)
                   {static const char *rName[] = {"metamanager",   // MetaMan
                                                  "manager",       // Manager
                                                  "supervisor",    // Super
                                                  "server",        // Server
                                                  "proxy-manager", // ProxyMan
                                                  "proxy-super",   // ProxySuper
                                                  "proxy-server",  // ProxyServ
                                                  "peer-manager",  // PeerMan
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

                   XrdCmsRole() {}
                  ~XrdCmsRole() {}
};
#endif
