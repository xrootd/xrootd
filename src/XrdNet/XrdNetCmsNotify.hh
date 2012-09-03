#ifndef __NETCMSNOTIFY_HH
#define __NETCMSNOTIFY_HH
/******************************************************************************/
/*                                                                            */
/*                    X r d N e t C m s N o t i f y . h h                     */
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
  
class XrdNetMsg;
class XrdSysError;

class XrdNetCmsNotify
{
public:

int  Gone(const char *Path, int isPfn=1);

int  Have(const char *Path, int isPfn=1);

static const int isServ = 0x0001;
static const int noPace = 0x0002;

             XrdNetCmsNotify(XrdSysError *erp, const char *aPath,
                             const char *iName, int Opts=0);
            ~XrdNetCmsNotify();

private:
int  Send(const char *Buff, int Blen);

XrdSysError *eDest;
XrdNetMsg   *xMsg;
char        *destPath;
int          Pace;
};
#endif
