#ifndef __XRDCKSLOADER_HH__
#define __XRDCKSLOADER_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d C k s L o a d e r . h h                        */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "sys/types.h"

#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksData.hh"

/* This class defines the checksum loader interface. It is intended to be used
   by xrootd clients to obtain an instance of a checksum calculation object.
   This object may be builtin or may come from a shared library.
*/

class  XrdCksCalc;
class  XrdSysError;
struct XrdVersionInfo;
  
class XrdCksLoader : public XrdCks
{
public:
virtual int         Calc( const char *Pfn, XrdCksData &Cks, int doSet=1);

virtual int         Config(const char *Token, char *Line);

virtual int         Del(  const char *Pfn, XrdCksData &Cks);

virtual int         Get(  const char *Pfn, XrdCksData &Cks);

virtual int         Init(const char *ConfigFN, const char *AddCalc=0);

virtual char       *List(const char *Pfn, char *Buff, int Blen, char Sep=' ');

virtual const char *Name(int seqNum=0);

virtual XrdCksCalc *Object(const char *name);

virtual int         Size( const char  *Name=0);

virtual int         Set(  const char *Pfn, XrdCksData &Cks, int myTime=0);

virtual int         Ver(  const char *Pfn, XrdCksData &Cks);

                    XrdCksLoader(XrdSysError *erP, int iosz,
                                  XrdVersionInfo *vInfo);
virtual            ~XrdCksLoader();

protected:

/* Calc()     returns 0 if the checksum was successfully calculated using the
              supplied CksObj and places the file's modification time in MTime.
              Otherwise, it returns -errno. The default implementation uses
              open(), fstat(), mmap(), and unmap() to calculate the results.
*/
virtual int         Calc(const char *Pfn, time_t &MTime, XrdCksCalc *CksObj);

/* ModTime()  returns 0 and places file's modification time in MTime. Otherwise,
              it return -errno. The default implementation uses stat().
*/
virtual int         ModTime(const char *Pfn, time_t &MTime);

private:

struct csInfo
      {char          Name[XrdCksData::NameSize];
       XrdCksCalc   *Obj;
       char         *Path;
       char         *Parms;
       XrdSysPlugin *Plugin;
       int           Len;
                     csInfo() : Obj(0), Path(0), Parms(0), Plugin(0), Len(0)
                                {memset(Name, 0, sizeof(Name));}
      };

int     Config(const char *cFN, csInfo &Info);
csInfo *Find(const char *Name);

static const int csMax = 4;
csInfo           csTab[csMax];
int              csLast;
int              segSize;
XrdVersionInfo  *myVersion;
};
#endif
