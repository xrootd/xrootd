#ifndef _XROOTD_FILE_H_
#define _XROOTD_FILE_H_
/******************************************************************************/
/*                                                                            */
/*                      X r d X r o o t d F i l e . h h                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <set>
#include <vector>

#include "XProtocol/XPtypes.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdFileStats.hh"

/******************************************************************************/
/*                       X r d X r o o t d F i l e H P                        */
/******************************************************************************/

class XrdXrootdFileHP
{
public:

void Avail(int fHandle) {fhMutex.Lock();
                         bool done = (1 == refs--);
                         if (noMore)
                            {fhMutex.UnLock();
                             if (done) delete this;
                            } else {
                             fhAvail.push_back(fHandle);
                             fhMutex.UnLock();
                            }
                        }

void Delete() {fhMutex.Lock();
               if (!refs) {fhMutex.UnLock(); delete this;}
                  else {noMore = true; fhMutex.UnLock();}
              }

int  Get() {int fh;
            fhMutex.Lock();
            if (fhAvail.empty()) fh = -1;
               else {fh = fhAvail.back();
                     fhAvail.pop_back();
                    }
            fhMutex.UnLock();
            return fh;
           }

void Ref() {fhMutex.Lock(); refs++; fhMutex.UnLock();}

     XrdXrootdFileHP(int rsv=2) : refs(1), noMore(false)
                                {fhAvail.reserve(rsv);}

private:

    ~XrdXrootdFileHP() {}

XrdSysMutex      fhMutex;
std::vector<int> fhAvail;
int              refs;
bool             noMore;
};

  
/******************************************************************************/
/*                         X r d X r o o t d F i l e                          */
/******************************************************************************/

class XrdSfsFile;
class XrdXrootdFileLock;
class XrdXrootdAioFob;
class XrdXrootdMonitor;
class XrdXrootdPgwFob;

class XrdXrootdFile
{
public:

XrdSfsFile        *XrdSfsp;      // -> Actual file object
union {char       *mmAddr;       // Memory mapped location, if any
       unsigned
       long long   cbArg;        // Callback argument upon close()
      };
char              *FileKey;      // -> File hash name (actual file name now)
char               FileMode;     // 'r' or 'w'
bool               AsyncMode;    // 1 -> if file in async r/w mode
bool               isMMapped;    // 1 -> file is memory mapped
bool               sfEnabled;    // 1 -> file is sendfile enabled
union {int         fdNum;        // File descriptor number if regular file
       int         fHandle;      // The file handle upon close()
      };
XrdXrootdAioFob   *aioFob;       // Aio freight pointer for reads
XrdXrootdPgwFob   *pgwFob;       // Pgw freight pointer for writes
XrdXrootdFileHP   *fhProc;       // File handle processor (set at close time)
const char        *ID;           // File user

XrdXrootdFileStats Stats;        // File access statistics

static void Init(XrdXrootdFileLock *lp, XrdSysError *erP, bool sfok);

       void Ref(int num);

       void Serialize();

           XrdXrootdFile(const char *id, const char *path, XrdSfsFile *fp,
                         char mode='r', bool async=false, struct stat *sP=0);
          ~XrdXrootdFile();

private:
int bin2hex(char *outbuff, char *inbuff, int inlen);
static XrdXrootdFileLock *Locker;
static int                sfOK;
static const char        *TraceID;

int                       refCount;     // Reference counter
int                       reserved;
XrdSysSemaphore          *syncWait;
XrdSysMutex               fileMutex;
};
 
/******************************************************************************/
/*                    X r d X r o o t d F i l e T a b l e                     */
/******************************************************************************/

// The before define the structure of the file table. We will have FTABSIZE
// internal table entries. We will then provide an external linear table
// that increases by FTABSIZE entries. There is one file table per link and
// it is owned by the base protocol object.
//
#define XRD_FTABSIZE   16
  
// WARNING! Manipulation (i.e., Add/Del/delete) of this object must be
//          externally serialized at the link level. Only one thread
//          may be active w.r.t this object during manipulation!
//
class XrdXrootdFileTable
{
public:

       int            Add(XrdXrootdFile *fp);

       XrdXrootdFile *Del(XrdXrootdMonitor *monP, int fnum, bool dodel=true);

inline XrdXrootdFile *Get(int fnum)
                         {if (fnum >= 0)
                             {if (fnum < XRD_FTABSIZE) return FTab[fnum];
                              if (XTab && (fnum-XRD_FTABSIZE)<XTnum)
                                 return XTab[fnum-XRD_FTABSIZE];
                             }
                          return (XrdXrootdFile *)0;
                         }

       void           Recycle(XrdXrootdMonitor *monP);

       XrdXrootdFileTable(unsigned int mid=0) : fhProc(0), FTfree(0), monID(mid),
                                                XTab(0), XTnum(0), XTfree(0)
                         {memset((void *)FTab, 0, sizeof(FTab));}

private:

      ~XrdXrootdFileTable() {} // Always use Recycle() to delete this object!

static const char *TraceID;
static const char *ID;
XrdXrootdFileHP   *fhProc;

XrdXrootdFile *FTab[XRD_FTABSIZE];
int            FTfree;
unsigned int   monID;

XrdXrootdFile **XTab;
int             XTnum;
int             XTfree;
};
#endif
