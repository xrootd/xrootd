#ifndef __XRDPOSIXPREPIO_HH__
#define __XRDPOSIXPREPIO_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d P o s i x P r e p I O . h h                      */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "Xrd/XrdJob.hh"
#include "XrdPosix/XrdPosixFile.hh"

class XrdPosixPrepIO : public XrdOucCacheIO2
{
public:

XrdOucCacheIO *Base()   {return this;} // Already defined

XrdOucCacheIO *Detach() {return this;} // Already defined

void        Disable();

long long   FSize() {return (Init() ? fileP->FSize() : openRC);}

int         Fstat(struct stat &buf)
                 {return (Init() ? fileP->Fstat(buf) : openRC);}

bool        ioActive() { return false; } // Already defined

int         Open() {Init(); return openRC;}

const char *Path()  {return fileP->Path();}

int         Read (char *Buffer, long long Offset, int Length)
                 {return (Init() ? fileP->Read(Buffer, Offset, Length) : openRC);}

void        Read (XrdOucCacheIOCB &iocb, char *buff, long long offs, int rlen)
                 {if (Init(&iocb)) fileP->Read(iocb, buff, offs, rlen);
                     else iocb.Done(openRC);
                 }

int         ReadV(const XrdOucIOVec *readV, int n)
                 {return (Init() ? fileP->ReadV(readV, n) : openRC);}

void        ReadV(XrdOucCacheIOCB &iocb, const XrdOucIOVec *readV, int rnum)
                 {if (Init(&iocb)) fileP->ReadV(iocb, readV, rnum);
                     else iocb.Done(openRC);
                 }

int         Sync() {return (Init() ? fileP->Sync() : openRC);}

void        Sync(XrdOucCacheIOCB &iocb)
                 {if (Init(&iocb)) fileP->Sync(iocb);
                     else iocb.Done(openRC);
                 }

int         Trunc(long long Offset)
                 {return (Init() ? fileP->Trunc(Offset) : openRC);}

int         Write(char *Buffer, long long Offset, int Length)
                 {return (Init() ? fileP->Write(Buffer,Offset,Length) : openRC);}

void        Write(XrdOucCacheIOCB &iocb, char *buff, long long offs, int wlen)
                 {if (Init(&iocb)) fileP->Write(iocb, buff, offs, wlen);
                     else iocb.Done(openRC);
                 }

            XrdPosixPrepIO(XrdPosixFile *fP, XrdCl::OpenFlags::Flags clflags,
                            XrdCl::Access::Mode clmode)
                          : fileP(fP), openRC(0), iCalls(0),
                            clFlags(clflags), clMode(clmode) {}
virtual    ~XrdPosixPrepIO() {}

private:
bool          Init(XrdOucCacheIOCB *iocbP=0);

XrdPosixFile *fileP;
int           openRC;
int           iCalls;

XrdCl::OpenFlags::Flags clFlags;
XrdCl::Access::Mode     clMode;
};
#endif
