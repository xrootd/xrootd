#ifndef __XRDFRMCNS_HH__
#define __XRDFRMCNS_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d F r m C n s . h h                           */
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
#include <sys/uio.h>
  
class XrdFrmCns
{
public:

static void  Add(const char *tID,const char *Path,long long Size,mode_t Mode);

static
const  int   cnsAuto    = -1;  // Init Opts: Use cnsd if there, ignore o/w
static
const  int   cnsIgnore  =  0;  // Init Opts: Always ignore the cnsd
static
const  int   cnsRequire =  1;  // Init Opts: Wait for cnsd if not present

static int   Init(const char *aPath, int Opts);

static int   Init(const char *myID, const char *aPath, const char *iName);

static void  Rm (const char *Path, int islfn=0) 
                {if (cnsMode) Del(Path, HdrRmf, islfn);}

static void  Rmd(const char *Path, int islfn=0)
                {if (cnsMode) Del(Path, HdrRmd, islfn);}

             XrdFrmCns() {}
            ~XrdFrmCns() {}

private:

static const int HdrRmd = 0;
static const int HdrRmf = 1;

static void  Del(const char *Path, int HdrType, int islfn=0);
static int   Init();
static int   Retry(int eNum, int &pMsg);
static int   Send2Cnsd(struct iovec *iov, int iovn);
static int   setPath(const char *aPath, const char *iName);

static char       *cnsPath;
static char       *cnsHdr[2];
static int         cnsHdrLen;
static int         cnsInit;
static int         cnsFD;
static int         cnsMode;
};
#endif
