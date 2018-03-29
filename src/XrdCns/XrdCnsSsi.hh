#ifndef __XRDCnsSSI_H_
#define __XRDCnsSSI_H_
/******************************************************************************/
/*                                                                            */
/*                          X r d C n s S s i . h h                           */
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

class  XrdCnsSsiDRec;
class  XrdCnsSsiFRec;
struct iovec;

class XrdCnsSsi
{
public:

static int List(const char *Host, const char *Path);

static int Updt(const char *Host, const char *Path);

static int Write(int xFD, struct iovec *iov, int n, int Bytes);

static int nErrs;
static int nDirs;
static int nFiles;

               XrdCnsSsi() {}
              ~XrdCnsSsi() {}

private:
static XrdCnsSsiDRec *AddDir(char *dP, char *lP);
static int            AddDel(char *pPo, char *lP);
static XrdCnsSsiFRec *AddFile(char *lfn,          char *lP);
static XrdCnsSsiFRec *AddFile(char *dP, char *fP, char *lP);
static void           AddSize(char *dP, char *fP, char *lP);
static int            ApplyLog(const char *Path);
static void           ApplyLogRec(char *Rec);
static void           FSize(char *oP, char *iP, int bsz);
static int            Write(int xFD, char *bP, int bL);
static int            Write(int xFD, int TOD, const char *Host);

};
#endif
