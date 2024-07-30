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

#include <sys/param.h>
#include <sys/stat.h>

#include "Xrd/XrdScheduler.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOssArc/XrdOssArcBackup.hh"
#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcFSMon.hh"
#include "XrdOssArc/XrdOssArcRecompose.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdOss*         ossP;

extern XrdScheduler*   schedP;

extern XrdOssArcConfig Config;
  
extern XrdSysError     Elog;

extern XrdSysTrace     ArcTrace;

extern XrdOssArcFSMon  fsMon;
}
using namespace XrdOssArcGlobals;
  
/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdSysMutex         XrdOssArcBackup::dsBkpQMtx;
std::deque<XrdOssArcBackupTask*>
                    XrdOssArcBackup::dsBkpQ;
XrdSysCondVar2      XrdOssArcBackup::dsBkpQCV(XrdOssArcBackup::dsBkpQMtx);
int                 XrdOssArcBackup::numRunning = 0;  
int                 XrdOssArcBackup::maxRunning = 0;  


/******************************************************************************/
/*             C l a s s   X r d O s s A r c B a c k u p T a s k              */
/******************************************************************************/
/******************************************************************************/
/*              C o n s t r u c t o r   &   D e s t r u c t o r               */
/******************************************************************************/
  
XrdOssArcBackupTask::XrdOssArcBackupTask(XrdOssArcBackup& who, char* dsn)
                                 : Owner(who), theScope(who.Scope), theDSN(dsn),
                                   numBytes(0), numFiles(0), relSpace(false),
                                   btSem(0) {}

XrdOssArcBackupTask::~XrdOssArcBackupTask()
{
// Remove entry of the dataset name from the owner's set
//
   Owner.dsBkpSetMtx.Lock();
   Owner.dsBkpSet.erase(theDSN);
   Owner.dsBkpSetMtx.UnLock();

// Release the space that this request acquired
//
   if (relSpace) fsMon.Release(numBytes);

// Since theDSN was shared with the owner's set we can free the storage
//
   free(theDSN);
}
  
/******************************************************************************/
/*           X r d O s s A r c B a c k u p T a s k : : B k p X e q            */
/******************************************************************************/
  
bool XrdOssArcBackupTask::BkpXeq()
{
// The first step it to setup the staging area to create an archive. This is
// done using a scipt. The script returns the total number of files and bytes
// that will need to be backed up.

// Construct the staging path for archive creation management
//
   TraceInfo("BkpTask", 0);
   XrdOucStream cmdSup;
   char dsnDir[MAXPATHLEN];
   int n, rc;

// Compose the arena path. Note that dsetRepoPFN already ends with a slash
//
   n = snprintf(dsnDir, sizeof(dsnDir), "%s%s/%c/", Config.dsetRepoPFN,
                        XrdOssArcRecompose::DSN2Dir(theDSN).c_str(),
                        Config.mySep);
   if (n >= (int)sizeof(dsnDir))
      {Elog.Emsg("Backup",ENAMETOOLONG,"bkup staging path for dataset",theDSN);
       return false;
      }

// We now must create the directory path to the arena 
//
   if ((rc = XrdOucUtils::makePath(dsnDir, S_IRWXU|S_IRGRP|S_IXGRP)))
       {Elog.Emsg("Backup", rc, "dataset backup arena", dsnDir);
       return false;
      }

// Construct the argument list
//
   const char* supArgv[] = {"setup", Config.srcRSE, theScope, theDSN, dsnDir, 
                            Config.srcData};
   int supArgc = sizeof(supArgv)/sizeof(char*);

// Do some tracing
//
   DEBUG("Running "<<Config.BkpUtilPath<<' '<<supArgv[0]<<' '<<supArgv[1]<<
                  ' '<<supArgv[2]<<' '<<supArgv[3]<<' '<<supArgv[4]<<
                  ' '<<supArgv[5]);

// Run the setup script which prepares the dataset for archiving. It should 
// output a single line: <files> <bytes>
//
   if (!(rc = Config.BkpUtilProg->Run(&cmdSup, supArgv, supArgc)))
      {char *lp, *retStr = 0;
       size_t vb;
       int vf;
       bool isOK = false;
       while((lp = cmdSup.GetLine())) if (!retStr) retStr = strdup(lp);

       if (retStr)
          {n = sscanf(retStr, "%d %zu", &vf, &vb);
           if (n == 2) {numFiles = vf, numBytes = vb; isOK = true;}
              else {char etxt[1024];
                    snprintf(etxt, sizeof(etxt),
                             "%s setup returned bad output '",
                             Config.BkpUtilPath);
                    Elog.Emsg("Backup", etxt, retStr,"'");
                   }
           free(retStr);
          } else {
           Elog.Emsg("Backup",Config.BkpUtilPath,"setup returned no output");
           return false;
          }

       Config.BkpUtilProg->RunDone(cmdSup); // This may kill the process
       if (!isOK) return false;
      } else {
       Elog.Emsg("Backup",rc, "run setup via", Config.BkpUtilPath);
       return false;                                                     
      }

// We can only proceed if there is enough space to hold the backup
//
   while(!fsMon.Permit(this))   
      {char buff[1024];
       snprintf(buff,sizeof(buff),"Insufficient free space; defering archiving"
                     " of %s:%s", theScope, theDSN);
       Elog.Emsg("BkpXeq", buff);
       btSem.Wait();               
       snprintf(buff,sizeof(buff),"Retrying to archive %s:%s",theScope,theDSN);
      }

// We can now create the archive
//
   if (!XrdOssArcBackup::Archive(theDSN, dsnDir)) return false;

// We can now safely mark this dataset as having been backed up
//
   XrdOucStream cmdSet;
   const char* setArgv[] = {"set", Config.metaBKP, Config.doneBKP,
                                   theScope,  theDSN};
   int setArgc = sizeof(setArgv)/sizeof(char*);

// Do some tracing
//
   DEBUG("Running "<<Config.BkpUtilPath<<' '<<setArgv[0]<<' '<<setArgv[1]<<
                  ' '<<setArgv[2]<<' '<<setArgv[3]<<' '<<setArgv[4]);

// Run the setup script which sets the dataset backup metadata to completed
//
   if (!(rc = Config.BkpUtilProg->Run(&cmdSet, setArgv, setArgc)))
      {while((cmdSet.GetLine())) {}
       Config.BkpUtilProg->RunDone(cmdSet); // This may kill the process
      } else {
       Elog.Emsg("Backup",rc, "run set via", Config.BkpUtilPath);
       return false;                                                     
      }

// All done
//
   return true;
}
  
/******************************************************************************/
/*                 c l a s s   X r d O s s A r c B a c k u p                  */
/******************************************************************************/
/******************************************************************************/
/*      X r d O s s A r c B a c k u p : : B k p W o r k e r : : D o I t       */
/******************************************************************************/
  
void XrdOssArcBackup::BkpWorker::DoIt()
{
// Get the initial lock
//
   dsBkpQMtx.Lock();

// Get a backup task and execute it
//
do{if (!dsBkpQ.empty())
      {XrdOssArcBackupTask* bTask = dsBkpQ.front(); 
       dsBkpQ.pop_front(); 
       dsBkpQMtx.UnLock();
       bool isOK = bTask->BkpXeq();
       dsBkpQMtx.Lock();

       char buff[1024];
       snprintf(buff,sizeof(buff),"%s:%s",bTask->theScope,bTask->theDSN);
       if (isOK) Elog.Emsg("BkpWorker", buff, "backed up!");
          else   Elog.Emsg("BkpWorker", buff, "backup failed; will retry later");

       delete bTask; // We may implement bTask->retries at some point???
      } else { 
       numRunning--;
       dsBkpQCV.Wait(); // This unlocks dsBkpQMtx
      }
  } while(true);
}

/******************************************************************************/
/*                 c l a s s   X r d O s s A r c B a c k u p                  */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOssArcBackup::XrdOssArcBackup(const char *scp, bool& isOK)
                : XrdJob("Backup"), Scope(scp)
{
   isOK = true;
}

/******************************************************************************/
/* Private:                      A d d 2 B k p                                */
/******************************************************************************/
  
bool XrdOssArcBackup::Add2Bkp(const char* dsn)
{
   XrdSysMutexHelper mHelp(dsBkpSetMtx);
   char* theDSN = strdup(dsn);

// Add this dataset to our backup set and return whether or not it is new.
// If the dataset is new then place a task for it on the global work queue
// and if it can be immediately serviced, signal a waiting thread to do so.
//
   auto rslt = dsBkpSet.insert(theDSN);
   if (!rslt.second) free(theDSN);
      else {XrdOssArcBackupTask* theTask = new XrdOssArcBackupTask(*this,theDSN);
            dsBkpQMtx.Lock();
            dsBkpQ.push_back(theTask);
            if (numRunning < maxRunning) dsBkpQCV.Signal();
            dsBkpQMtx.UnLock();
           }
   return rslt.second;
}
  
/******************************************************************************/
/*                               A r c h i v e                                */
/******************************************************************************/
  
bool XrdOssArcBackup::Archive(const char* dsName, const char* dsDir)
{
   TraceInfo("Archive",0);
   XrdOucStream cmdOut;
   char tapPath[MAXPATHLEN];
   int rc;

// Add we need to do is launch the archive program to complete the steps:
// 1. Create the zip file of all files in the dataset.
// 2. Move the zip file to the <tape_dir>.
// 3. Do a recursvive delete starting at and including <src_dir>.
// 4. Delete file <trg_dir>/<zipfn>.

// The calling parameters are:
// <src_dir> <tape_dir> <arcfn>
//
// <src_dir>:  The directory containing all of the files in the dataset.
//             This is apssed as a PFN via dsDir parameter.
// <tape_dir>: The directory to hold the zip archive destined to tape.
//             We need to build this using the dsName parameter.
// <arcfn>:    The actual filename to be used for the archive. By convention
//             the archive is created as '<src_dir>/../<arcfn>'.
//
   if ((rc = Config.GenTapePath(dsName,tapPath,sizeof(tapPath))))
      {Elog.Emsg("Archive", rc, "tape path for dataset", dsName);
       Elog.Emsg("Archive", "Dataset", dsName, "needs manual intervention!!!");
       return false;
      }

// Do some tracing
//
   DEBUG("Running "<<Config.ArchiverPath<<' '<<dsDir<<' '
                   <<tapPath<<' '<<Config.arFName);

// Run the archive script.
//
   if (!(rc = Config.ArchiverProg->Run(&cmdOut,dsDir,tapPath,Config.arFName)))
      {char* lp;
       while((lp = cmdOut.GetLine())) {} // Throw away stdout
       rc = Config.ArchiverProg->RunDone(cmdOut);
      }

// Check for any failures
//
   if (rc)
      {char rcVal[32];
       snprintf(rcVal, sizeof(rcVal),"%d",rc);
       Elog.Emsg("Archive", "Archive script failed with rc=", rcVal);
       Elog.Emsg("Archive", "Dataset", dsName, "needs manual intervention!!!");
       return false;
      }

   return true;   
}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdOssArcBackup::DoIt()
{
  // Do a backup round and then reschedule for the next one
  //
  GetManifest();

  schedP->Schedule(this, time(0)+Config.bkpPoll);
}
  
/******************************************************************************/
/*                           G e t M a n i f e s t                            */
/******************************************************************************/
  
int XrdOssArcBackup::GetManifest()
{
   static const char* manEOL = "%%%";
   static const char* lsbArgv[] = {"list", Config.metaBKP, Config.needBKP,
                                   Scope,  manEOL};
   static int lsbArgc = sizeof(lsbArgv)/sizeof(char*);
   static XrdSysMutex manMutex;

   TraceInfo("GetManifest",0);
   XrdOucStream cmdOut;
   int rc, dsCnt, dsNew = 0;
   bool isEOF = false;

// Here we launch the BkpUtils program to tell us the list of datasets that 
// need to be backed up by this RSE. The BkpUtils program writes newline
// deparated dataset did's to stdout. Error messages are written to stderr.
// The final line conatins '%%%' indicating the actual end of output. If we
// don't get that we warn that the list is incomplete but use what we have.

// The calling parameteris are:
// lsb <scope> <metavar> <metaval> <eolmarker>

// Do some tracing
//
   DEBUG("Running "<<Config.BkpUtilPath<<' '<<lsbArgv[0]<<' '<<lsbArgv[1]<<
                  ' '<<lsbArgv[2]<<' '<<lsbArgv[3]<<' '<<lsbArgv[4]);

// To avoid placing a huge load on the dataset we will be querying, only one
// manifest request can run at a time.
//
   manMutex.Lock();

// Run the manifest script.
//
   if (!(rc = Config.BkpUtilProg->Run(&cmdOut, lsbArgv, lsbArgc)))
      {char* lp;
       while((lp = cmdOut.GetLine()))
            {if (*lp == *manEOL && !strcmp(lp, manEOL))
                {isEOF = true;
                 break;
                }
             if (Add2Bkp(lp)) dsNew++;
            }
       Config.BkpUtilProg->RunDone(cmdOut); // This may kill the process
      }

// We are done running this program
//
   manMutex.UnLock();

// Check if we really got an eof
//
   if (!isEOF)
      {char buff[16];
       snprintf(buff, sizeof(buff),"%d",rc);
       Elog.Emsg("GetManifest","Premature EOF when reading manifest; rc=",buff);
      }

// Get the number of entries in the backup set
//
   dsBkpSetMtx.Lock();
   dsCnt = dsBkpSet.size();
   dsBkpSetMtx.UnLock();

// Do some tracing here
//
   DEBUG("Scope "<<Scope<<" has "<<dsCnt
                 <<" dataset(s) needing backup of which "<<dsNew<<" are new");

// Return the number of datasets in the backup list
//
   return dsCnt;
}

/******************************************************************************/
/*                          S t a r t W o r k e r s                           */
/******************************************************************************/
  
void XrdOssArcBackup::StartWorkers(int maxw)
{
   TraceInfo("StartWorkers",0);
   numRunning = maxRunning = maxw;

// Start all of the workers, they will immediately go idle.
// This is a one time call from config.
//
   for (int i = 0; i < maxw; i++)
       {XrdJob* bJob = new BkpWorker();
        schedP->Schedule(bJob);
       }

// Do some tracing
//
   DEBUG("Started "<<maxw<<" backup workers.");
}
