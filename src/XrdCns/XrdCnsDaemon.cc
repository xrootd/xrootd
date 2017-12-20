/******************************************************************************/
/*                                                                            */
/*                       X r d C n s D a e m o n . c c                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "Xrd/XrdTrace.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"

#include "XrdOuc/XrdOucStream.hh"
#include "XrdCns/XrdCnsDaemon.hh"
#include "XrdCns/XrdCnsLogRec.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

namespace XrdCns
{
extern XrdSysError       MLog;

extern XrdOucTrace       XrdTrace;

       XrdCnsDaemon      XrdCnsd;
}

using namespace XrdCns;

/******************************************************************************/
/*                             g e t E v e n t s                              */
/******************************************************************************/
  
void XrdCnsDaemon::getEvents(XrdOucStream &Events, const char *Who)
{
   const char *Miss = 0, *TraceID = "getEvents";
   long long Size;
   mode_t    Mode;
   const char *eP;
   char *tp, *etp;
   XrdCnsLogRec *evP = 0;

// Each ofs request comes in as:
// <traceid> {closew <lfn> <size> | create <mode> <lfn> | mkdir <mode> <lfn> |
//            mv <lfn1> <lfn2>    | rm            <lfn> | rmdir        <lfn>}
//
   while((tp = Events.GetLine()) && *tp)
        {TRACE(DEBUG, "Event: '" <<tp <<"'");
         eP = "?";
               if (!(tp = Events.GetToken()))          Miss = "traceid";
         else {evP = XrdCnsLogRec::Alloc();
               if (!(eP = Events.GetToken())
               ||  !evP->setType(eP))                  Miss = "eventid";
         else {switch(evP->Type())
                     {case XrdCnsLogRec::lrClosew:
                           if (!(tp=getLFN(Events)))    {Miss = "lfn";   break;}
                           evP->setLfn1(tp);
                           if (!(tp=Events.GetToken())) {Miss = "size";  break;}
                           Size = strtoll(tp, &etp, 10);
                           if (*etp)                    {Miss = "size";  break;}
                           evP->setSize(Size);
                           break;
                      case XrdCnsLogRec::lrMkdir:
                      case XrdCnsLogRec::lrCreate:
                           if (evP->Type() == XrdCnsLogRec::lrMkdir)
                              evP->setSize(-1);
                           if (!(tp=Events.GetToken())) {Miss = "mode";  break;}
                           Mode = strtol(tp, &etp, 8);
                           if (*etp)                    {Miss = "mode";  break;}
                           evP->setMode(Mode);
                           if (!(tp=getLFN(Events)))    {Miss = "lfn";   break;}
                           evP->setLfn1(tp);
                           break;
                      case XrdCnsLogRec::lrMv:
                           if (!(tp=getLFN(Events)))    {Miss = "lfn1";  break;}
                           evP->setLfn1(tp);
                           if (!(tp=getLFN(Events)))    {Miss = "lfn2";  break;}
                           evP->setLfn2(tp);
                           break;
                      default:     // rm | rmdir
                           if (!(tp=getLFN(Events)))    {Miss = "lfn";   break;}
                           evP->setLfn1(tp);
                           break;
                     }
              } }

         if (Miss) {MLog.Emsg("doEvents", Miss, "missing in event", eP);
                    if (evP) evP->Recycle();
                    Miss = 0;
                    continue;
                   }

         evP->Queue();
        }

// If we exit then we lost the connection
//
   MLog.Emsg("doEvents", "Lost event connection to", Who, "!");
}

/******************************************************************************/
/*                                g e t L F N                                 */
/******************************************************************************/

char *XrdCnsDaemon::getLFN(XrdOucStream &Events)
{
   char *tP, *cgiP;

// Obtain the lfn but discard any CGI information that has been mistakenly
// passed. Some people recall the old documentation, sigh.
//
   if ((tP=Events.GetToken()) && (cgiP = index(tP, '?'))) *cgiP = '\0';
   return tP;
}
