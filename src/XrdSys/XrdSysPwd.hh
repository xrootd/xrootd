#ifndef __XRDSYSPWD_HH__
#define __XRDSYSPWD_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d S y s P w d . h h                           */
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
  
#include <sys/types.h>
#include <pwd.h>

class XrdSysPwd
{
public:

int   rc;

struct passwd *Get(const char *Usr)
                  {rc = getpwnam_r(Usr,&pwStruct,pwBuff,sizeof(pwBuff),&Ppw);
                   return Ppw;
                  }

struct passwd *Get(uid_t       Uid)
                  {rc = getpwuid_r(Uid,&pwStruct,pwBuff,sizeof(pwBuff),&Ppw);
                   return Ppw;
                  }

               XrdSysPwd() : rc(2) {}

               XrdSysPwd(const char *Usr, struct passwd **pwP)
                  {rc = getpwnam_r(Usr,&pwStruct,pwBuff,sizeof(pwBuff),pwP);}

               XrdSysPwd(uid_t       Uid, struct passwd **pwP)
                  {rc = getpwuid_r(Uid,&pwStruct,pwBuff,sizeof(pwBuff),pwP);}

              ~XrdSysPwd() {}

private:

struct passwd pwStruct, *Ppw;
char          pwBuff[4096];
};
#endif
