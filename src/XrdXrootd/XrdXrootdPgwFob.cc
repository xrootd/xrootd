/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d P g w F o b . c c                     */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>

#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdPgwFob.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdXrootd
{
extern XrdSysError  eLog;
}
using namespace XrdXrootd;

extern XrdSysTrace  XrdXrootdTrace;
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
#define TRACELINK fileP
  
XrdXrootdPgwFob::~XrdXrootdPgwFob()
{
   int len, n = badOffs.size();

// Write an error message if this file has any outstanding checksum errors
//
   if (n)
      {char buff[128];
       snprintf(buff, sizeof(buff), "Warning! %d checksum error(s) in", n);
       eLog.Emsg("PgwFob", buff, fileP->FileKey);
      }

// Check if we have anything to do and if we do, dump the list of bad checksums.
//
   if (TRACING(TRACE_PGCS))
      {const char *TraceID = "FileFob", *fname = fileP->FileKey;
       if (n)
          {XrdOucString lolist((1+4+1+13)*n);
           char item[128];
           kXR_int64 val;

           for (std::set<kXR_int64>::iterator it =  badOffs.begin();
                                              it != badOffs.end(); ++it)
               {val = *it;
                len = val & (XrdProto::kXR_pgPageSZ-1);
                if (!len) len = XrdProto::kXR_pgPageSZ;
                val = val >> XrdProto::kXR_pgPageBL;
                snprintf(item, sizeof(item), " %d@%lld", len, val);
                lolist += item;
               }
           TRACEI(PGCS,fname<<" had "<<numErrs<<" cksum errs and "<<numFixd
                            <<" fixes"<<"; areas in error:"<<lolist.c_str());
          } else if (numErrs)
                    {TRACEI(PGCS,fname<<" had "<<numErrs<<" cksum errs and "
                                      <<numFixd<<" fixes.");
                    }
      }
}
