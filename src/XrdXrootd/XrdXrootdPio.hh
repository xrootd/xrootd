#ifndef __XRDXROOTDPIO__
#define __XRDXROOTDPIO__
/******************************************************************************/
/*                                                                            */
/*                       X r d X r o o t d P i o . h h                        */
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
  
#include "XProtocol/XPtypes.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"

class XrdXrootdFile;

class XrdXrootdPio
{
public:

       XrdXrootdPio       *Next;
       int                (XrdXrootdProtocol::*ResumePio)();
       XrdXrootd::IOParms  IO;
       kXR_char            StreamID[2];

static XrdXrootdPio      *Alloc(int n=1);

inline XrdXrootdPio      *Clear(XrdXrootdPio *np=0)
                               {ResumePio = 0;
                                memset(&IO, 0, sizeof(IO));
                                StreamID[0] = 0; StreamID[1] = 0;
                                Next = np; return this;
                               }

       void               Recycle();

inline void               Set(int (XrdXrootdProtocol::*Invoke)(),
                              XrdXrootd::IOParms &io, const kXR_char *theSID)
                             {ResumePio = Invoke;
                              IO = io;
                              StreamID[0] = theSID[0]; StreamID[1] = theSID[1];
                             }

                          XrdXrootdPio(XrdXrootdPio *np=0) {Clear(np);}
                         ~XrdXrootdPio() {}

private:

static const int          FreeMax = 256;
static XrdSysMutex        myMutex;
static XrdXrootdPio      *Free;
static int                FreeNum;
};
#endif
