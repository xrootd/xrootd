#ifndef __FRCCID_H__
#define __FRCCID_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d F r c C I D . h h                           */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <string.h>

#include "XrdSys/XrdSysPthread.hh"

class XrdOucEnv;
class XrdOucStream;

class XrdFrcCID
{
public:
       int    Add(const char *iName, const char *cName, time_t addT, pid_t Pid);

       int    Get(const char *iName, char *buff, int blen);

       int    Get(const char *iName, const char *vName, XrdOucEnv *evP);

       int    Init(const char *qPath);

       void   Ref(const char *iName);

              XrdFrcCID() : Dflt(0), First(0), cidFN(0), cidFN2(0) {}
             ~XrdFrcCID() {}

private:

struct cidEnt
      {cidEnt *Next;
       char   *iName;
       char   *cName;
       time_t  addT;
       pid_t   Pid;
       int     useCnt;
       short   iNLen;
       short   cNLen;

               cidEnt(cidEnt *epnt,const char *iname,const char *cname,
                      time_t addt, pid_t idp)
                     : Next(epnt), iName(strdup(*iname ? iname : "anon")),
                       cName(strdup(cname)), addT(addt), Pid(idp), useCnt(0),
                       iNLen(strlen(iName)), cNLen(strlen(cName)) {}
              ~cidEnt() {if (iName) free(iName); if (cName) free(cName);}

      };

class  cidMon {public:
               cidMon() {cidMutex.Lock();}
              ~cidMon() {cidMutex.UnLock();}
               private:
               static XrdSysMutex cidMutex;
              };

cidEnt *Find(const char *iName);
int     Init(XrdOucStream &cidFile);
int     Update();

cidEnt *Dflt;
cidEnt *First;
char   *cidFN;
char   *cidFN2;
};

namespace XrdFrc
{
extern XrdFrcCID CID;
}
#endif
