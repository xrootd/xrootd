/******************************************************************************/
/*                                                                            */
/*                   X r d O s s A r c D a t a s e t . c c                    */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <sys/stat.h>
#include <unistd.h>

#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"

#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcDataset.hh"
#include "XrdOssArc/XrdOssArcRecompose.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdScheduler*   schedP;

extern XrdOssArcConfig Config;
  
extern XrdSysError     Elog;

extern XrdSysTrace     ArcTrace;
}
using namespace XrdOssArcGlobals;
  
/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
XrdSysMutex XrdOssArcDataset::dsMapMutex;

struct cmp_str
{
   bool operator()(char const *a, char const *b) const
   {
      return std::strcmp(a, b) < 0;
   }
};

std::map<const char*, XrdOssArcDataset*, cmp_str> XrdOssArcDataset::dsMap;
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class ManifestJob : public XrdJob
{
public:

virtual void DoIt() override {if (dsP->GetManifest() && dsP->Archive())
                                 dsP->Delete("Man+Archive");
                              dsP->Ref(-1);
                              delete this;
                             }

             ManifestJob(XrdOssArcDataset* dsp)
                        : XrdJob("Manifest"), dsP(dsp) {}
virtual     ~ManifestJob() {}

private:
XrdOssArcDataset* dsP;
};

class ArchiveJob : public XrdJob
{
public:

virtual void DoIt() override {if (dsP->Archive())
                                 {dsP->Delete("Archiver");
                                  dsP->Ref(-1);
                                 }
                              delete this;
                             }

             ArchiveJob(XrdOssArcDataset* dsp)
                        : XrdJob("Archiver"), dsP(dsp) {}
virtual     ~ArchiveJob() {}

private:
XrdOssArcDataset* dsP;
};
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOssArcDataset::XrdOssArcDataset(XrdOssArcRecompose& dsInfo)
{
   crTime = time(0);
   dsName = strdup(dsInfo.arcDSN);
   dsDir  = strdup(dsInfo.arcDir);
   didCnt = 0;
   Ready  = 0;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOssArcDataset::~XrdOssArcDataset()
{
   TraceInfo("DS_Destructor",0);
   
   DEBUG("Dataset entry "<<dsName<<" with "<<fsMap.size()
         <<" file entries deleted!");

   if (dsName) free(dsName);
   if (dsDir)  free(dsDir);
}

/******************************************************************************/
/*                               A d d F i l e                                */
/******************************************************************************/

bool XrdOssArcDataset::AddFile(const char* tid, const char* dsn,
                               const char* fsn, bool isCreate)
{
   TraceInfo("AddFile", tid);

// We simply emplace a file object into our set of files. Note that the file
// file onject takes ownership of the duplicated string which is also the key.
//
   fsMapMutex.Lock();
   auto it = fsMap.emplace(fsn, isCreate);
   if (!(it.second) && isCreate) it.first->second.created = true;
   fsMapMutex.UnLock();

// Trace this emplacement
//
   DEBUG("File "<<fsn<<(it.second ? " was" : " already")
                <<" added to dataset "<<dsn);

// Return whether this was added or was a duplicate
//
   return it.second;
}
  
/******************************************************************************/
/*                               A r c h i v e                                */
/******************************************************************************/
  
bool XrdOssArcDataset::Archive()
{
   TraceInfo("Archive",0);
   XrdOucStream cmdOut;
   char lclPath[MAXPATHLEN], tapPath[MAXPATHLEN];
   int rc;

// Add we need to do is launch the archive program to complete the steps:
// 1. Create the zip file of all files in the dataset <trg_dir>/<zipfn>.
// 2. Move the <trg_dir>/<zipfn> to the <tape_dir>/<zipfn>.
// 3. Do a recursvive delete starting at and including <src_dir>.
// 4. Delete file <trg_dir>/<zipfn>.

// The calling parameteris are:
// <src_dir> <tape_dir> <zipfn>
//
// <src_dir>:  The directory containing all of the files in the dataset.
// <tape_dir>: The directory to hold the zip archive destined to tape.
// <arcfn>:    The actual filename to be used for the archive.

   if ((rc = Config.GenLocalPath(dsDir, lclPath, sizeof(lclPath))))
      {Elog.Emsg("Archive", rc, "local path for dataset", dsName);
       Elog.Emsg("Archive", "Dataset", dsName, "needs manual intervention!!!");
       return false;
      }

   if ((rc = Config.GenTapePath(dsName, tapPath, sizeof(tapPath))))
      {Elog.Emsg("Archive", rc, "tape path for dataset", dsName);
       Elog.Emsg("Archive", "Dataset", dsName, "needs manual intervention!!!");
       return false;
      }

// Do some tracing
//
   DEBUG("Running "<<Config.ArchiverPath<<' '<<lclPath<<' '
                   <<tapPath<<' '<<Config.arFName);

// Run the archive script.
//
   if (!(rc = Config.ArchiverProg->Run(&cmdOut,lclPath,tapPath,Config.arFName)))
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
       zipping = false;
       return false;
      }

   return true;   
}

/******************************************************************************/
/*                              C o m p l e t e                               */
/******************************************************************************/

int XrdOssArcDataset::Complete(const char* path, time_t& reqTime)
{
   TraceInfo("Complete",0);
   int ecode;
   XrdOssArcRecompose dsInfo(path, ecode, true);
   XrdOssArcDataset* dsP;

// Check if this complete is really for us
//
   if (ecode)
      {if (ecode != EDOM)
          Elog.Emsg("Complete", ecode, "complete", dsInfo.arcPath);
       return -ecode;                     
      }

// Find the prexisting dataset. If none, then skip this operation.
// We need to hold the global lock for this.
//
   dsMapMutex.Lock();
   auto dsIT = dsMap.find(dsInfo.arcDSN);
   if (dsIT == dsMap.end())
      {dsMapMutex.UnLock();
       Elog.Emsg("Complete", "Attempt to complete file", dsInfo.arcPath,
                             "in unknown dataset"); 
       return -EINVAL;
      }
   dsP = dsIT->second;
   dsP->Ref(1);
   reqTime = dsP->crTime;
   dsMapMutex.UnLock();

// Find the file entry
//
   dsP->fsMapMutex.Lock();
   auto it = dsP->fsMap.find(dsInfo.arcFile);
   if (it != dsP->fsMap.end())
      {dsFile* fsP = &(it->second);
       if (!(fsP->created))
          {struct stat Stat;
           if (stat(dsInfo.arcPath, &Stat) == 0)
              {Elog.Emsg("Complete", "Inconsistent create status corrected "
                         "for", dsInfo.arcPath);
               fsP->created = true;
              } else {
               Elog.Emsg("Complete", ENOENT, "complete", dsInfo.arcPath);
               fsP->created = false;
              }
          }
       if (fsP->created && !(fsP->complete))
          {dsP->Ready++;
           fsP->complete = true;
          }
       bool crVal = fsP->created, coVal = fsP->complete;
       dsP->fsMapMutex.UnLock();
       DEBUG("File "<<dsInfo.arcFile<<" Created/Complete: "<<crVal<<'/'<<coVal);
      } else {
       dsP->fsMapMutex.UnLock();
       Elog.Emsg("Complete", "Attempt to complete an unknown file",
                             dsInfo.arcFile);
      }

// If this file added to the readiness of the dataset being archived, check
// if all of the file are now ready. If so, schedule an archive.
//
   bool archiving = false;
   dsMapMutex.Lock();
   if (dsP->didCnt && dsP->didCnt <= dsP->Ready && !dsP->zipping)
      {ArchiveJob* arJob = new ArchiveJob(dsP);
       dsP->zipping = archiving = true;
       schedP->Schedule(arJob);
      }
   DEBUG("DSN " <<dsInfo.arcDSN<<" didCnt/Ready: "  
         <<dsP->didCnt<<'/'<<dsP->Ready<<(archiving ? " archiving" : ""));
   dsMapMutex.UnLock();

// Remove reference count but only if we have not launched the archiver
//
   if (!archiving) dsP->Ref(-1);

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/
  
int XrdOssArcDataset::Create(const char *tid, XrdOssArcRecompose& dsInfo)
{
   TraceInfo("Create", tid);
   XrdOssArcDataset* dsP;
   bool isNew = false;

// Find the prexisting dataset. If none, then create a pending dataset.
// We need to hold the global lock for this.
//
   dsMapMutex.Lock();
   auto dsIT = dsMap.find(dsInfo.arcDSN);
   if (dsIT != dsMap.end()) dsP = dsIT->second;
      else {dsP = new XrdOssArcDataset(dsInfo); //Warning! See above
            dsMap.insert({dsP->dsName, dsP});
            DEBUG(dsP->dsName<<" entry created"); 
            isNew = true;
           }
   dsP->Ref(1);
   dsMapMutex.UnLock();

// The dataset cannot be delete now, so we can add this path to set of paths
// that belong to the dataset. This will eventually be reconciled when we get
// a third party list of the dataset members which may take some time.
//
   dsP->AddFile(tid, dsInfo.arcDSN, dsInfo.arcFile, true);

// If this is a new dataset, then schedule the dataset to fetch its contents 
//
   if (isNew)
      {ManifestJob* gmJob = new ManifestJob(dsP);
       schedP->Schedule(gmJob);
      } else dsP->Ref(-1);

// All done
//
   return 0;
}

/******************************************************************************/
/*                                D e l e t e                                 */
/******************************************************************************/
  
void XrdOssArcDataset::Delete(const char* why)
{
   TraceInfo("Delete",0);
   bool destroy = false;

// If there are no references to us then remove us from the map.
//
   dsMapMutex.Lock(); 
   if (refCnt > 0) isDead = true;
      else {dsMap.erase(dsName);
            destroy = true;
            refCnt++;  // Keep this object alive for destruction
           }

   DEBUG("dataset map "<<dsName<<" delete"<<(destroy ? "d" : " defered")
                       <<" post "<<why);
   dsMapMutex.UnLock(); 

// If we can delete ourselves, do so, We must do this outside the scope of
// the lock because deleting the file map may be a lengthy operation.
//
   if (destroy) delete this;
}
  
/******************************************************************************/
/*                           G e t M a n i f e s t                            */
/******************************************************************************/
  
bool XrdOssArcDataset::GetManifest()
{
   TraceInfo("GetManifest",0);
   XrdOucStream cmdOut;
   int rc, fileCnt = 0, newFiles = 0;
   bool isEOF = false;

// Add we need to do is launch the manifest program that will obtain all the 
// files in the dataset that we tell it. The files themselves are what are
// unqualified did's that are applcable to any RSE. The newline separated
// names are written to stdout. Any error messages should be written to
// stderr, they will appear in the log. The final line of output must contain
// '%%%' to positively indicate the end of the list.

// The calling parameteris are:
// <dsName>
//
// <dsName>:  The dataset name.

// Do some tracing
//
   DEBUG("Running "<<Config.getManPath<<" ls "<<dsName);

// Run the manifest script.
//
   if (!(rc = Config.getManProg->Run(&cmdOut, "ls", dsName)))
      {char* lp;
       while((lp = cmdOut.GetLine()))
            {if (*lp == *Config.getManEOL && !strcmp(lp, Config.getManEOL))
                {isEOF = true;
                 break;
                }
             if (AddFile(0, dsName, lp, false)) newFiles++;
             fileCnt++;
            }
       Config.getManProg->RunDone(cmdOut); // This may kill the process
      }

// Do some tracing here
//
   DEBUG("Dataset "<<dsName<<" has "<<fileCnt<<" file entries of which "
                   <<newFiles<<" are new; rc="<<rc);

// Check if we really got an eof
//
   if (!rc && !isEOF)
      {char buff[16];
       snprintf(buff, sizeof(buff),"%d",fileCnt);
       Elog.Emsg("GetManifest", "Premature EOF when reading manifest after",
                                buff, "file entries read!");
       Elog.Emsg("Archive", "Dataset", dsName, "needs manual intervention!!!");
      }

// We need to figure out what to do if we do not have a manifest. For now
// pretend that it's OK though no archiving will likely occur. ???
//

// Update statistics. note that we may have been so slow that the whole
// dataset is already here.
//
   dsMapMutex.Lock();
   didCnt = fileCnt;
   dsMapMutex.UnLock();
   return Ready >= fileCnt; 
}

/******************************************************************************/
/*                                   R e f                                    */
/******************************************************************************/

// The dsMap lock must be held when increasing the count. It must be be held
// otherwise. This object may be deleted if it has been marked for delete and
// the ref count falls to zeo.
  
void XrdOssArcDataset::Ref(int cnt)
{
// Increase or decrease the reference counter
//
   refCnt += cnt;

// If the count is zero now and this object is marked as dead, delete it
//
   if (cnt < 0 && isDead && refCnt < 1) Delete("ref count");
}

/******************************************************************************/
/*                            R e v e r t F i l e                             */
/******************************************************************************/

bool XrdOssArcDataset::RevertFile(const char* tid, const char* fpath)
{
   TraceInfo("Revert", tid);
   XrdSysMutexHelper fsMtx(fsMapMutex);
   bool compVal = false;

// Find the file entry
//
   auto it = fsMap.find(fpath);
   if (it != fsMap.end())
      {compVal = it->second.complete;
       DEBUG("file "<<fpath<<" created="<<it->second.created
                    <<" revert complete="<<compVal<<" to false");
       it->second.complete = false;
      } else {
       Elog.Emsg("revert","file",fpath,"revert failed; not in create map");
      }

// Indicate whether the revert actually was done
//
   return compVal;
}
  
/******************************************************************************/
/*                                U n l i n k                                 */
/******************************************************************************/
  
void XrdOssArcDataset::Unlink(const char* tid, XrdOssArcRecompose& dsInfo)
{
   TraceInfo("Unlink", tid);

// Do some tracing
//
   DEBUG("Reverting file '"<<dsInfo.arcFile<<"' in dataset "<<dsInfo.arcDSN);

// Find the prexisting dataset. If none, then skip this operation.
// We need to hold the global lock for this.
//
   dsMapMutex.Lock();
   auto dsIT = dsMap.find(dsInfo.arcDSN);
   if (dsIT == dsMap.end())
      {dsMapMutex.UnLock();
       Elog.Emsg("unlink", "Attempt to unlink file", dsInfo.arcPath,
                        "in unknown dataset"); 
       return;
      }
   XrdOssArcDataset* dsP = dsIT->second;
   dsP->Ref(1);
   dsMapMutex.UnLock();

// Mark the file as incomplete
//
   if (dsP->RevertFile(tid, dsInfo.arcFile)) dsP->Ready--;   
   dsP->Ref(-1);
}
