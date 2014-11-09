/******************************************************************************/
/*                                                                            */
/*                      X r d M a p C l u s t e r . c c                       */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* This utility maps the connections in a cluster starting at some node. It
   can also, optionally, check for file existence at each point. Syntax:

   xrdmapc <host>:<port> [<path>]

*/

/******************************************************************************/
/*                         i n c l u d e   f i l e s                          */
/******************************************************************************/
  
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>

#include "XrdCl/XrdClEnv.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*                     L o c a l   D e f i n i t i o n s                      */
/******************************************************************************/

#define EMSG(x) cerr <<"xrdmapc: " <<x <<endl

// Bypass stupid issue with stupid solaris for missdefining 'struct opt'.
//
#ifdef __solaris__
#define OPT_TYPE (char *)
#else
#define OPT_TYPE
#endif
  
namespace
{
struct clMap
{      clMap *nextMan;
       clMap *nextSrv;
       clMap *nextLvl;
 const char  *state;
       char  *key;
       char   name[285];
       char   hasfile;
       char   verfile;
       char   valid;
       char   isMan;

       clMap(const char *addr) : nextMan(0), nextSrv(0),   nextLvl(0),
                                 state(""),  hasfile(' '), verfile(' '),
                                 valid(1),   isMan(0)
                               {if (addr)
                                   {XrdNetAddr epAddr;
                                    epAddr.Set(addr); epAddr.Name();
                                    epAddr.Format(name, sizeof(name));
                                    key = strdup(addr);
                                   } else {
                                    *name = 0;
                                    key   = strdup("");
                                   }
                               }
      ~clMap() {}
};
};

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

extern int optind, optopt;

namespace
{
bool listMan = true, listSrv = true, doVerify = false;

clMap    *clLost = 0;

char     *Path   = 0;

uint16_t  theTO  = 30;

XrdOucHash<clMap> clHash;
};
  
/******************************************************************************/
/*                               M a k e U R L                                */
/******************************************************************************/
  
namespace
{
const char *MakeURL(const char *name, char *buff, int blen)
{
   snprintf(buff, blen, "xroot://%s//", name);
   return buff;
}
};

/******************************************************************************/
/*                            M a p C l u s t e r                             */
/******************************************************************************/

namespace
{
void MapCluster(clMap *node)
{
   static XrdCl::OpenFlags::Flags flags = XrdCl::OpenFlags::None;
   char buff[2048];
   XrdCl::URL theURL((const std::string)MakeURL(node->name,buff,sizeof(buff)));
   XrdCl::FileSystem xrdFS(theURL);
   XrdCl::XRootDStatus Status;
   XrdCl::LocationInfo              *info = 0;
   XrdCl::LocationInfo::Iterator     it;
   XrdCl::LocationInfo::LocationType locType;
   clMap *clmP, *branch;

// Issue a locate
//
   Status = xrdFS.Locate((const std::string)"*", flags, info, theTO);

// Make sure all went well
//
   if (!Status.IsOK())
      {EMSG("Unable to connect to " <<node->name <<"; "
            <<Status.ToStr().c_str());
       node->state = "unreachable";
//cerr <<"MapCluster set state: " <<node->state <<endl;
       node->valid = 0;
       return;
      }

// Grab all of the information
//
   for( it = info->Begin(); it != info->End(); ++it )
      {clmP = new clMap(it->GetAddress().c_str());
       locType = it->GetType();
       if (locType == XrdCl::LocationInfo::ServerOnline
       ||  locType == XrdCl::LocationInfo::ServerPending)
          {clmP->nextSrv = node->nextSrv;
           node->nextSrv = clmP;
          } else {
           clmP->nextMan = node->nextMan;
           node->nextMan = clmP;
           clmP->isMan = 1;
          }
       clHash.Add(clmP->key, clmP, 0, Hash_keep);
      }

// Now map all managers
//
   clmP = node->nextMan;
   while(clmP)
        {branch = new clMap(clmP->name);
         MapCluster(branch);
         if (branch->nextSrv || branch->nextMan) clmP->nextLvl = branch;
            else delete branch;
         clmP = clmP->nextMan;
        }

// All done
//
   delete info;
}
};

/******************************************************************************/
/*                               M a p C o d e                                */
/******************************************************************************/

namespace
{
void MapCode(XrdCl::XRootDStatus &Status, clMap *node)
{
   char buff[128];

   node->verfile = '?';

   if (Status.code == XrdCl::errErrorResponse)
      {switch(Status.errNo)
             {case kXR_FSError:        node->state = " fs error";       break;
              case kXR_IOError:        node->state = " io error";       break;
              case kXR_NoMemory:       node->state = " no memory";      break;
              case kXR_NotAuthorized:  node->state = " not authorized"; break;
              case kXR_NotFound:       node->state = "";
                                       node->verfile = '-';             break;
              case kXR_NotFile:        node->state = " not a file";     break;
              default: sprintf(buff, " xrootd error %d", Status.errNo);
                                       node->state = strdup(buff);
                                       break;
             }
//cerr <<"MapCode set state: " <<node->state <<endl;
       return;
      }

  switch(Status.code)
        {case XrdCl::errInvalidAddr:        node->state = " invalid addr";
                                            break;
         case XrdCl::errSocketError:        node->state = " socket error";
                                            break;
         case XrdCl::errSocketTimeout:      node->state = " timeout";
                                            break;
         case XrdCl::errSocketDisconnected: node->state = " disconnect";
                                            break;
         case XrdCl::errStreamDisconnect:   node->state = " disconnect";
                                            break;
         case XrdCl::errConnectionError:    node->state = " connect error";
                                            break;
         case XrdCl::errHandShakeFailed:    node->state = " handshake failed";
                                            break;
         case XrdCl::errLoginFailed:        node->state = " login failed";
                                            break;
         case XrdCl::errAuthFailed:         node->state = " auth failed";
                                            break;
         case XrdCl::errOperationExpired:   node->state = " op expired";
                                            break;
         case XrdCl::errRedirectLimit:      node->state = " redirect loop";
                                            break;
         default: node->state = strdup(Status.ToStr().c_str());
                                            break;
        }
//cerr <<"MapCode set state: " <<node->state <<endl;
}
};
  
/******************************************************************************/
/*                               M a p P a t h                                */
/******************************************************************************/

namespace
{
void MapPath(clMap *node, const char *Path, bool doRefresh=false)
{
   XrdCl::OpenFlags::Flags flags = XrdCl::OpenFlags::None;
   char buff[2048];
   XrdCl::URL theURL((const std::string)MakeURL(node->name,buff,sizeof(buff)));
   XrdCl::FileSystem xrdFS(theURL);
   XrdCl::XRootDStatus Status;
   XrdCl::LocationInfo              *info = 0;
   XrdCl::LocationInfo::Iterator     it;
   clMap *clmP;

// Insert refresh is so wanted
//
   if (doRefresh) flags = XrdCl::OpenFlags::Refresh;

// Issue a locate
//
   Status = xrdFS.Locate((const std::string)Path, flags, info, theTO);

// Make sure all went well
//
   if (!Status.IsOK())
      {EMSG("Unable to connect to " <<node->name <<"; "
            <<Status.ToStr().c_str());
       node->state = "unreachable";
//cerr <<"MapPath set state: " <<node->state <<endl;
       return;
      }

// Recursively mark each node as having the file
//
   for( it = info->Begin(); it != info->End(); ++it )
      {const char *clAddr = it->GetAddress().c_str();
       if ((clmP = clHash.Find(clAddr)))
          {clmP->hasfile = '>';
           if (clmP->isMan) MapPath(clmP, Path);
          } else {
           clmP = new clMap(clAddr);
           clmP->nextSrv = clLost;
           clLost = clmP;
          }
      }

// All done here
//
   delete info;
}
};
  
/******************************************************************************/
/*                                O p N a m e                                 */
/******************************************************************************/
  
namespace
{
const char *OpName(char *Argv[])
{
   static char oName[4] = {'-', 0, 0, 0};

   if (!optopt || optopt == '-' || *(Argv[optind-1]+1) == '-')
      return Argv[optind-1];
   oName[1] = optopt;
   return oName;
}
};

/******************************************************************************/
/*                               P a t h C h k                                */
/******************************************************************************/

namespace
{
void PathChk(clMap *node)
{
   char buff[2048];
   XrdCl::URL theURL((const std::string)MakeURL(node->name,buff,sizeof(buff)));
   XrdCl::FileSystem xrdFS(theURL);
   XrdCl::XRootDStatus Status;
   XrdCl::StatInfo    *info = 0;

// Issue a stat for the file
//
   Status = xrdFS.Stat((const std::string)Path, info);

// Make sure all went well
//
   if (!Status.IsOK()) MapCode(Status, node);
      else node->verfile = '+';

// All done here
//
   delete info;
}
};

/******************************************************************************/
/*                              P r i n t M a p                               */
/******************************************************************************/
  
namespace
{
void PrintMap(clMap *clmP, int lvl)
{
   clMap *clnow;
   const char *pfx = "";
   char *pfxbuff = 0;
   int n;

// Compute index spacing
//
   if ((n = lvl*5))
      {pfxbuff = (char *)malloc(n+1);
       memset(pfxbuff, ' ', n); pfxbuff[n] = 0;
       pfx = pfxbuff;
      }

// Print all of the servers first
//
   if (listSrv)
      {clnow = clmP->nextSrv;
       while(clnow)
            {if (doVerify) PathChk(clnow);
             if (lvl)
                {pfxbuff[1] = clnow->hasfile;
                 pfxbuff[2] = clnow->verfile;
                }
             cout <<' ' <<pfx <<"Srv " <<clnow->name <<' ' <<clnow->state <<endl;
             clnow = clnow->nextSrv;
            }
      }

// Now recursively print the managers
//
   if (listMan)
      {clnow = clmP->nextMan;
       if (lvl) pfxbuff[2] = ' ';
       while(clnow)
            {if (lvl) pfxbuff[1] = clnow->hasfile;
             cout <<lvl <<pfx <<"Man " <<clnow->name <<' ' <<clnow->state <<endl;
             if (clnow->valid && clnow->nextLvl) PrintMap(clnow->nextLvl,lvl+1);
             clnow = clnow->nextMan;
            }
      }

// All done
//
   if (lvl) free(pfxbuff);
}
};

/******************************************************************************/
/*                                S e t E n v                                 */
/******************************************************************************/

namespace
{
int cwValue = 10;
int crValue = 0;
int trValue = 5;

void SetEnv()
{
   XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();

   env->PutInt("ConnectionWindow", cwValue);
   env->PutInt("ConnectionRetry",  crValue);
   env->PutInt("TimeoutResolution",trValue);
}
};
  
/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
namespace
{
void Usage(const char *emsg)
{
   if (emsg) EMSG(emsg);
   cerr <<"Usage: xrdmapc [<opt>] <host>:<port> [<path>]\n"
        <<"<opt>: [--help] [--list {all|m|s}] [--refresh] [--verify]" <<endl;
   if (!emsg)
      {cerr <<
"--list    | -1 'all' lists managers and servers (default), 'm' lists only\n"
"               managers and 's' lists only servers.\n"
"--refresh | -r does not use cached information but will refresh the cache.\n"
"--verify  | -v verifies <path> existence status at each server.\n"
"<path>         when specified, uses <host>:<port> to determine the locations\n"
"               of path and does optional verification."
            <<endl;
      }
   exit((emsg ? 1 : 0));
}
};

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   const char   *opLetters = ":hl:rv";
   struct option opVec[] =         // For getopt_long()
     {
      {OPT_TYPE "help",      0, 0, (int)'h'},
      {OPT_TYPE "list",      1, 0, (int)'l'},
      {OPT_TYPE "refresh",   0, 0, (int)'r'},
      {OPT_TYPE "verify",    0, 0, (int)'v'},
      {0,                    0, 0, 0}
     };
   extern int   optind, opterr;
   extern char *optarg;
   XrdNetAddr sPoint;
   clMap *baseNode, *clNow;
   const char *eMsg;
   char opC;
   int i;
   bool doRefresh = false;

// Process options
//
   opterr = 0;
   optind = 1;
   while((opC = getopt_long(argc, argv, opLetters, opVec, &i)) != (char)-1)
        switch(opC)
              {case 'h': Usage(0);
                         break;
               case 'l':      if (!strcmp("all",optarg))
                                 {listMan = true;  listSrv = true;}
                         else if (!strcmp("m",  optarg))
                                 {listMan = true;  listSrv = false;}
                         else if (!strcmp("s",  optarg))
                                 {listMan = false; listSrv = true;}
                         else Usage("Invalid list argument.");
                         break;
               case 'r': doRefresh = true;
                         break;
               case 'v': doVerify  = true;
                         break;
               case ':': EMSG("'" <<OpName(argv) <<"' argument missing.");
                         exit(2); break;
               case '?': EMSG("Invalid option, '" <<OpName(argv) <<"'.");
                         exit(2); break;
               default:  EMSG("Internal error processing '" <<OpName(argv) <<"'.");
                         exit(2); break;
              }

// Make sure we have a starting point
//
   if (optind >= argc) Usage("Initial node not specified.");

// Establish starting point
//
   if ((eMsg = sPoint.Set(argv[optind])))
      {EMSG("Unable to validate initial node; " <<eMsg);
       exit(2);
      }

// Make sure it's resolvable
//
   if (!sPoint.Name(0, &eMsg))
      {EMSG("Unable to resolve " <<argv[optind] <<"; " <<eMsg);
       exit(2);
      }

// Establish the base node
//
   baseNode = new clMap(argv[optind]);

// Check if we will be checking a path
//
   if (optind+1 < argc) Path = argv[optind+1];
      else doVerify = false;

// Set default client values
//
   SetEnv();

// Map the cluster
//
   MapCluster(baseNode);

// Check if we need to do a locate on a file and possibly verify results
//
   if (Path)
      {MapPath(baseNode, Path, doRefresh);
       eMsg = (doVerify ? "0*rv* " : "0*r** ");
      } else eMsg = "0**** ";

// Print the first line
//
   cout <<eMsg <<baseNode->name <<endl;
   PrintMap(baseNode, 1);

// Check if we have any phantom nodes
//
   if (Path && clLost)
      {cerr <<"Warning! " <<baseNode->name
            <<" referred to the following unconnected node:" <<endl;
       clNow = clLost;
       while(clNow)
            {cerr <<"????? " <<clNow->name <<endl;
             clNow = clNow->nextSrv;
            }
      }

// All done
//
   exit(0);
}
