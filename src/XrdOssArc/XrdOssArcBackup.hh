#ifndef _XRDOSSARC_BACKUP_HH_
#define _XRDOSSARC_BACKUP_HH_
/******************************************************************************/
/*                                                                            */
/*                       X r d O s s B a c k u p . h h                        */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <deque>
#include <set>
#include <string>

#include "Xrd/XrdJob.hh"

#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                   X r d O s s A r c B a c k u p T a s k                    */
/******************************************************************************/

class XrdOssArcBackup;

class XrdOssArcBackupTask
{
public:

XrdOssArcBackup&       Owner;
const char*            theScope;
      char*            theDSN;
      size_t           numBytes;
      int              numFiles;
      bool             relSpace;
      XrdSysSemaphore  btSem;

                       XrdOssArcBackupTask(XrdOssArcBackup& who, char* dsn);
                      ~XrdOssArcBackupTask();

       bool            BkpXeq();
};
  
/******************************************************************************/
/*                       X r d O s s A r c B a c k u p                        */
/******************************************************************************/
  
class XrdOssArcBackup : public XrdJob
{
friend class XrdOssArcBackupTask;
public:

static
bool  Archive(const char* dsName, const char* dsDir);

void  DoIt() override;

static
void  StartWorkers(int maxw);

const
char* theScope() {return Scope;}

      XrdOssArcBackup(const char* scp, bool& isOK);
     ~XrdOssArcBackup() {}

private:

       bool Add2Bkp(const char* dsn);
       int  GetManifest();

const char* Scope;

static XrdSysMutex                      dsBkpQMtx;
static std::deque<XrdOssArcBackupTask*> dsBkpQ;
static XrdSysCondVar2                   dsBkpQCV;

static int numRunning;
static int maxRunning;

class BkpWorker : public XrdJob
{
public:

void DoIt() override;

     BkpWorker() {}
    ~BkpWorker() {} // Never deleted
};

struct cmp_str
{
   bool operator()(char const *a, char const *b) const
   {
      return std::strcmp(a, b) < 0;
   }
};

XrdSysMutex                    dsBkpSetMtx;
std::set<const char*, cmp_str> dsBkpSet;
};
#endif
