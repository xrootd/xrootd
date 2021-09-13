/******************************************************************************/
/*                                                                            */
/*                       X r d A d m i n F i n d . c c                        */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>
#include <cstring>
#include <strings.h>
#include <ctime>
#include <sys/param.h>

#include "XrdCks/XrdCksManager.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrm/XrdFrmAdmin.hh"
#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdFrm/XrdFrmFiles.hh"
#include "XrdOuc/XrdOucArgs.hh"
#include "XrdOuc/XrdOucNSWalk.hh"

using namespace XrdFrc;
using namespace XrdFrm;

/******************************************************************************/
/*                              F i n d F a i l                               */
/******************************************************************************/
  
int XrdFrmAdmin::FindFail(XrdOucArgs &Spec)
{
   XrdOucNSWalk::NSEnt *nP, *fP;
   XrdOucNSWalk *nsP;
   char pDir[MAXPATHLEN], *dotP, *dirFN, *lDir = Opt.Args[1];
   char *dirP = pDir + Config.xfrFdln;
   int   dirL = sizeof(pDir) - Config.xfrFdln;
   int opts = XrdOucNSWalk::retFile | (Opt.Recurse ? XrdOucNSWalk::Recurse : 0);
   int ec, rc = 0, num = 0;

// Check if fail files are being relocated
//
   if (Config.xfrFdir) strcpy(pDir, Config.xfrFdir);

// Process each directory
//
   do {if (!Config.LocalPath(lDir, dirP, dirL)) continue;
       nsP = new XrdOucNSWalk(&Say, pDir, 0, opts);
       while((nP = nsP->Index(ec)) || ec)
            {while((fP = nP))
                  {if ((dotP = rindex(fP->File,'.')) && !strcmp(dotP,".fail"))
                      {Msg(fP->Path); num++;}
                   nP = fP->Next; delete fP;
                  }
            if (ec) rc = 4;
            }
        delete nsP;
       } while((dirFN = Spec.getarg()));

// All done
//
   sprintf(pDir, "%d fail file%s found.", num, (num == 1 ? "" : "s"));
   Msg(pDir);
   return rc;
}
  
/******************************************************************************/
/*                              F i n d M m a p                               */
/******************************************************************************/
  
int XrdFrmAdmin::FindMmap(XrdOucArgs &Spec)
{
   XrdOucXAttr<XrdFrcXAttrMem> memInfo;
   XrdFrmFileset *sP;
   XrdFrmFiles   *fP;
   char buff[128], pDir[MAXPATHLEN], *lDir = Opt.Args[1];
   int opts = (Opt.Recurse ? XrdFrmFiles::Recursive : 0);
   int mFlag, ec = 0, rc = 0, num = 0;

// Process each directory
//
   do {if (!Config.LocalPath(lDir, pDir, sizeof(pDir))) continue;
       fP = new XrdFrmFiles(pDir, opts | XrdFrmFiles::NoAutoDel);
       while((sP = fP->Get(ec)))
            {if (memInfo.Get(sP->basePath()) >= 0
             &&  (mFlag = memInfo.Attr.Flags))
                {const char *Kp = (mFlag&XrdFrcXAttrMem::memKeep ? "-keep ":0);
                 const char *Lk = (mFlag&XrdFrcXAttrMem::memLock ? "-lock ":0);
                 Msg("mmap ", Kp, Lk, sP->basePath()); num++;
                }
             delete sP;
            }
       if (ec) rc = 4;
       delete fP;
      } while((lDir = Spec.getarg()));

// All done
//
   sprintf(buff,"%d mmapped file%s found.",num,(num == 1 ? "" : "s"));
   Msg(buff);
   return rc;
}

/******************************************************************************/
/*                              F i n d N o c s                               */
/******************************************************************************/
  
int XrdFrmAdmin::FindNocs(XrdOucArgs &Spec)
{
   XrdFrmFileset *sP;
   XrdFrmFiles   *fP;
   char buff[128], pDir[MAXPATHLEN], *lDir = Opt.Args[1];
   int opts = (Opt.Recurse ? XrdFrmFiles::Recursive : 0);
   int ec = 0, rc = 0, num = 0;

// Check if this is even supported
//
   if (!Config.CksMan)
      {Emsg("Checksum support has not been configured!"); return 8;}


// First get the checksum type
//
   if (!(CksData.Length = Config.CksMan->Size(Opt.Args[1]))
   ||  !CksData.Set(Opt.Args[1]))
      {Emsg(Opt.Args[1], " checksum is not supported."); return 4;}

// Now get the actual target
//
   if (!(lDir = Spec.getarg()))
      {Emsg("Find target not specified."); return 4;}

// Process each directory
//
   do {if (!Config.LocalPath(lDir, pDir, sizeof(pDir))) continue;
       fP = new XrdFrmFiles(pDir, opts | XrdFrmFiles::NoAutoDel);
       while((sP = fP->Get(ec)))
            {if ((rc = Config.CksMan->Get(sP->basePath(), CksData)) <= 0)
                {num++;
                 Msg((rc == -ESTALE ? "Invalid " : "Missing "), CksData.Name,
                      " ",sP->basePath());
                }
             delete sP;
            }
       if (ec) rc = 4;
       delete fP;
      } while((lDir = Spec.getarg()));

// All done
//
   sprintf(buff,"%d file%s found with no chksum.",num,(num == 1 ? "" : "s"));
   Msg(buff);
   return rc;
}
  
/******************************************************************************/
/*                              F i n d P i n s                               */
/******************************************************************************/
  
int XrdFrmAdmin::FindPins(XrdOucArgs &Spec)
{
   XrdFrmFileset *sP;
   XrdFrmFiles   *fP;
   char buff[128], pDir[MAXPATHLEN], *lDir = Opt.Args[1];
   int opts = (Opt.Recurse ? XrdFrmFiles::Recursive : 0);
   int ec = 0, rc = 0, num = 0;

// Process each directory
//
   do {if (!Config.LocalPath(lDir, pDir, sizeof(pDir))) continue;
       fP = new XrdFrmFiles(pDir, opts | XrdFrmFiles::NoAutoDel);
       while((sP = fP->Get(ec)))
            {if (sP->pinInfo.Get(sP->basePath()) >= 0 && FindPins(sP)) num++;
             delete sP;
            }
       if (ec) rc = 4;
       delete fP;
      } while((lDir = Spec.getarg()));

// All done
//
   sprintf(buff,"%d pinned file%s found.",num,(num == 1 ? "" : "s"));
   Msg(buff);
   return rc;
}
  
/******************************************************************************/

int XrdFrmAdmin::FindPins(XrdFrmFileset *sP)
{
   static const int Week = 7*24*60*60;
   time_t pinVal;
   const char *Pfx = "+";
   char How[256], Scale;
   int pinFlag;

// If no pif flags are set then no pin value exists
//
   if (!(pinFlag = sP->pinInfo.Attr.Flags)) return 0;

// If it's pinned forever, we can blither the message right away
//
   if (pinFlag & XrdFrcXAttrPin::pinPerm)
      {Msg("pin -k forever ", sP->basePath()); return 1;}

// Be optimistic and get the pin time value
//
   pinVal = static_cast<time_t>(sP->pinInfo.Attr.pinTime);
   *How = 0;

// If it's a keep then decide how to format it. If the keep has been exceeed
// then just delete the attribute, since it isn't pinned anymore.
//
   if (pinFlag & XrdFrcXAttrPin::pinKeep)
      {time_t nowT = time(0);
       if (pinVal <= nowT) {sP->pinInfo.Del(sP->basePath()); return 0;}
       if ((pinVal - nowT) <= Week)
          {pinFlag = XrdFrcXAttrPin::pinIdle;
           pinVal = pinVal - nowT;
           Pfx = "";
          } else {
           struct tm *lclTime = localtime(&pinVal);
           sprintf(How, "-k %02d/%02d/%04d", lclTime->tm_mon+1,
                        lclTime->tm_mday, lclTime->tm_year+1900);
          }
      }

// Check for idle pin or convert keep pin to suedo-idle formatting
//
   if (pinFlag & XrdFrcXAttrPin::pinIdle)
      {     if ( pinVal        <= 180) Scale = 's';
       else if ((pinVal /= 60) <=  90) Scale = 'm';
       else if ((pinVal /= 60) <=  45) Scale = 'h';
       else {    pinVal /= 24;         Scale = 'd';}
       sprintf(How, "-k %s%d%c", Pfx, static_cast<int>(pinVal), Scale);
      } else if (!*How) return 0;

// Print the result
//
    Msg("pin ", How, " ", sP->basePath());
    return 1;
}

/******************************************************************************/
/*                              F i n d U n m i                               */
/******************************************************************************/
  
int XrdFrmAdmin::FindUnmi(XrdOucArgs &Spec)
{
   static const char *noCPT = "Unmigrated; no copy time: ";
   XrdFrmFileset *sP;
   XrdFrmFiles   *fP;
   const char *Why;
   char buff[128], pDir[MAXPATHLEN], *lDir = Opt.Args[1];
   int opts = (Opt.Recurse ? XrdFrmFiles::Recursive : 0)|XrdFrmFiles::GetCpyTim;
   int ec = 0, rc = 0, num = 0;

// Process each directory
//
   do {if (!Config.LocalPath(lDir, pDir, sizeof(pDir))) continue;
       fP = new XrdFrmFiles(pDir, opts | XrdFrmFiles::NoAutoDel);
       while((sP = fP->Get(ec)))
            {     if (!(sP->cpyInfo.Attr.cpyTime))
                     Why = noCPT;
             else if (static_cast<long long>(sP->baseFile()->Stat.st_mtime) >
                      sP->cpyInfo.Attr.cpyTime)
                     Why="Unmigrated; modified: ";
             else Why = 0;
             if (Why) {Msg(Why, sP->basePath()); num++;}
             delete sP;
            }
       if (ec) rc = 4;
       delete fP;
      } while((lDir = Spec.getarg()));

// All done
//
   sprintf(buff,"%d unmigrated file%s found.",num,(num == 1 ? "" : "s"));
   Msg(buff);
   return rc;
}
