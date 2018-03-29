#ifndef __XRDXROOTDTRANSSEND_HH_
#define __XRDXROOTDTRANSSEND_HH_
/******************************************************************************/
/*                                                                            */
/*                 X r d X r o o t d T r a n s S e n d . h h                  */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/uio.h>

#include "XProtocol/XPtypes.hh"
#include "XrdXrootd/XrdXrootdBridge.hh"

class XrdLink;
  
class XrdXrootdTransSend : public XrdXrootd::Bridge::Context
{
public:

        int   Send(const
                   struct iovec *headP, //!< pointer to leading  data array
                   int           headN, //!< array count
                   const
                   struct iovec *tailP, //!< pointer to trailing data array
                   int           tailN  //!< array count
                  );

              XrdXrootdTransSend(XrdLink *lP, kXR_char *sid, kXR_unt16 req,
                                 long long offset, int dlen, int fdnum)
                                : Context(lP, sid, req),
                                  sfOff(offset), sfLen(dlen), sfFD(fdnum) {}

              XrdXrootdTransSend(XrdLink *lP, kXR_char *sid, kXR_unt16 req,
                                 XrdOucSFVec *sfvec, int sfvnum, int dlen)
                                : Context(lP, sid, req),
                                  sfVP(sfvec), sfLen(dlen), sfFD(-sfvnum) {}

             ~XrdXrootdTransSend() {}

private:

union {long long    sfOff;
       XrdOucSFVec *sfVP;
      };
int       sfLen;
int       sfFD;
};
#endif
