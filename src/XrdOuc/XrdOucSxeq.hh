#ifndef __OUC_SXEQ_HH__
#define __OUC_SXEQ_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d O u c S x e q . h h                          */
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
  
class XrdOucSxeq
{
public:

static const int noWait = 0x0001;
static const int Share  = 0x0002;
static const int Unlink = 0x0004;
static const int Lock   = 0x0008; // lock in constructor

int    Detach() {int lFD = lokFD; lokFD = -1; return lFD;}

int    Release();

static
int    Release(int fileD);

int    Serialize(int Opts=0);

static
int    Serialize(int fileD, int Opts);

int    lastError() {return lokRC;}

       XrdOucSxeq(int sOpts, const char *path);
       XrdOucSxeq(const char *sfx, const char *sfx1=0, const char *Dir="/tmp/");
      ~XrdOucSxeq();

private:

char *lokFN;
int   lokFD;
int   lokUL;
int   lokRC;
};
#endif
