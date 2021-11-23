/******************************************************************************/
/*                                                                            */
/*                   X r d F r m A d m i n A u d i t . c c                    */
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

#include <cstdio>
#include <cstring>
#include <sys/param.h>

#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrc/XrdFrcUtils.hh"
#include "XrdFrm/XrdFrmAdmin.hh"
#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdFrm/XrdFrmFiles.hh"
#include "XrdOss/XrdOssPath.hh"
#include "XrdOss/XrdOssSpace.hh"
#include "XrdOuc/XrdOucNSWalk.hh"
#include "XrdOuc/XrdOucTList.hh"

using namespace XrdFrc;
using namespace XrdFrm;

/******************************************************************************/
/*                           A u d i t N a m e N B                            */
/******************************************************************************/

int XrdFrmAdmin::AuditNameNB(XrdFrmFileset *sP)
{
   char Resp, buff[80];
   int num = 0, rem;

// Report what is orphaned
//
   if (sP->lockFile())
      {num++; Msg("Orphaned lock file: ", sP->lockPath());}
   if (sP->pfnFile() )
      {num++; Msg("Orphaned pfn  file: ", sP->pfnPath());
              Msg("PFN file refers to: ", sP->pfnFile()->Link);
                 }
   if (sP->pinFile() )
      {num++; Msg("Orphaned pin  file: ", sP->pinPath());}

// Return if no fix is needed, otherwise check if we should ask before removal
//
   numProb += num;
   if (!Opt.Fix || !num) return 1;
   if (!Opt.Force)
      {Resp = XrdFrcUtils::Ask('n', "Remove orphaned files?");
       if (Resp != 'y') return Resp != 'a';
      }

// Remove the orphaned files
//
   rem = AuditRemove(sP);
   numFix += rem;

// Indicate final resolution
//
   sprintf(buff, "%d of %d orphaned files removed.", rem, num);
   Msg(buff);
   return 1;
}
  
/******************************************************************************/
/*                           A u d i t N a m e N F                            */
/******************************************************************************/

int XrdFrmAdmin::AuditNameNF(XrdFrmFileset *sP)
{
   char Resp;

// Indicate what is wrong
//
   Msg("Dangling link:  ", sP->basePath());
   Msg("Missing target: ", sP->baseFile()->Link);
   numProb++;

// Return if no fix is needed, otherwise check if we should ask before removal
//
   if (!Opt.Fix) return 1;
   if (!Opt.Force)
      {Resp = XrdFrcUtils::Ask('n', "Remove symlink?");
       if (Resp != 'y') return Resp != 'a';
      }

// Remove the symlink and associated files
//
   if (unlink(sP->basePath()))
      Emsg(errno,"remove symlink", sP->basePath());
      else if (AuditRemove(sP))
              {Msg("Symlink removed.");
               numFix++;
               return 1;
              }
   return 1;
}
  
/******************************************************************************/
/*                           A u d i t N a m e N L                            */
/******************************************************************************/

int XrdFrmAdmin::AuditNameNL(XrdFrmFileset *sP)
{
   static const char *noCPT = "No copy time for: ";
   static const char *mkCPT = "Set copy time?";
   char Resp;

// Indicate what is wrong
//
   Msg(noCPT, sP->basePath());
   numProb++;

// Return if no fix is needed, otherwise check if we should ask before removal
//
   if (!Opt.Fix) return -1;
   if (!Opt.Force)
      {Resp = XrdFrcUtils::Ask('y', mkCPT);
       if (Resp != 'y') return Resp != 'a';
      }

// Set copy time
//
   if (XrdFrcUtils::updtCpy(sP->basePath(),(Opt.MPType == 'p' ? 0 : -113)))
      {numFix++;
       Msg("Copy time set.");
      }
   return 1;
}

/******************************************************************************/
/*                            A u d i t N a m e s                             */
/******************************************************************************/
  
int XrdFrmAdmin::AuditNames()
{
   static const int fsetOpts = XrdFrmFiles::GetCpyTim | XrdFrmFiles::NoAutoDel;
   XrdFrmFileset *sP;
   XrdFrmFiles   *fP;
   char pDir[MAXPATHLEN], *lDir = Opt.Args[1];
   int opts = (Opt.Recurse ? XrdFrmFiles::Recursive : 0) | fsetOpts;
   int ec = 0, Act = 1;

// Initialization
//
   numProb = 0; numFix = 0;
   if (VerifyMP("audit", lDir) != 'y') return 0;

// Process the directory
//
   if (!Config.LocalPath(lDir, pDir, sizeof(pDir))) {finalRC = 4; return 1;}
   fP = new XrdFrmFiles(pDir, opts);
   while(Act && (sP = fP->Get(ec,1)))
        {if (!(sP->baseFile())) Act = AuditNameNB(sP);
             else {if (sP->baseFile()->Type == XrdOucNSWalk::NSEnt::isLink)
                      Act = AuditNameNF(sP);
                   if (Act && Opt.MPType && !(sP->cpyInfo.Attr.cpyTime))
                      Act = AuditNameNL(sP);
                   if (Act && sP->baseFile()->Link && isXA(sP->baseFile()))
                      Act = AuditNameXA(sP);
                  }
         delete sP;
         }
    if (ec) finalRC = 4;
    delete fP;

// All done
//
   if (!Act) Msg("Audit names aborted!");
   sprintf(pDir,"%d problem%s found; %d fixed.", numProb,
                (numProb == 1 ? "" : "s"), numFix);
   Msg(pDir);
   return !Act;
}

/******************************************************************************/
/*                           A u d i t N a m e X A                            */
/******************************************************************************/
  
int XrdFrmAdmin::AuditNameXA(XrdFrmFileset *sP)
{
   XrdOucXAttr<XrdFrcXAttrPfn> pfnInfo;
   const char *doWhat = "Recreate pfn xref?";
   char  Resp, dfltAns = 'n';
   int rc;

// Make sure there is a PFN attribute is here and references the file
//
   if ((rc = pfnInfo.Get(sP->baseFile()->Link)) > 0)
      {if (!strcmp(pfnInfo.Attr.Pfn,sP->basePath())) return 1;
       Msg("Incorrect pfn xref to ", sP->basePath());
       Msg("Data file refers to   ", pfnInfo.Attr.Pfn);
      } else {
       if (rc)  Emsg(-rc, "get pfn xattr for ",sP->basePath());
          else {Msg("Missing pfn xref to ",    sP->basePath());
                doWhat = "Create pfn xref?"; dfltAns = 'y';
               }
      }

// Check if we can fix this problem
//
   if (!Opt.Fix || rc < 0) return 1;
   if (!Opt.Force)
      {Resp = XrdFrcUtils::Ask(dfltAns, doWhat);
       if (Resp != 'y') return Resp != 'a';
      }

// Reset the pfn xattr
//
   strcpy(pfnInfo.Attr.Pfn, sP->basePath());
   if (!(rc = pfnInfo.Set(sP->baseFile()->Link)))
      {Msg("pfn xref set."); numFix++;}
      else Emsg(-rc, "set pfn xref to ", sP->basePath());


// All done.
//
   return 1;
}

/******************************************************************************/
/*                           A u d i t R e m o v e                            */
/******************************************************************************/
  
int XrdFrmAdmin::AuditRemove(XrdFrmFileset *sP)
{
   int rem = 0;

// Remove the orphaned files
//
   if (sP->lockFile())
      {if (unlink(sP->lockPath())) Emsg(errno,"remove lock file.");
          else rem++;
      }
   if (sP-> pinFile())
      {if (unlink(sP-> pinPath())) Emsg(errno,"remove pin  file.");
          else rem++;
      }
   if (sP-> pfnFile())
      {if (unlink(sP-> pfnPath())) Emsg(errno,"remove pfn  file.");
          else rem++;
      }

   return rem;
}

/******************************************************************************/
/*                            A u d i t S p a c e                             */
/******************************************************************************/
  
int XrdFrmAdmin::AuditSpace()
{
   XrdOucTList   *pP;
   char buff[256], *Path = 0, *Space = Opt.Args[1];
   int Act;

// Parse the space specification
//
   if (!(pP = ParseSpace(Space, &Path))) return 4;

// Initialize
//
   numBytes = 0; numFiles = 0; numProb = 0; numFix = 0;

// Index the space via filesets
//
   do {Act = (pP->val ? AuditSpaceXA(Space, pP->text) : AuditSpaceAX(pP->text));
       pP = pP->next;
      } while(pP && !Path && Act);

// All done
//
   sprintf(buff,"%d problem%s found; %d fixed.", numProb,
                (numProb == 1 ? "" : "s"), numFix);
   Msg(buff);
   if (!Act) Msg("Audit space aborted!");
      else {if (Path) *(--Path) = ':';
            sprintf(buff, "Space %s has %d file%s with %lld byte%s in use "
                          "(%lld unreachable).",
                    Space,
                    numFiles, (numFiles == 1 ? "" : "s"),
                    numBytes, (numBytes == 1 ? "" : "s"),
                    numBLost);
            Msg(buff);
           }
   return (Act ? 0 : 4);
}

/******************************************************************************/
/*                          A u d i t S p a c e A X                           */
/******************************************************************************/
  
int XrdFrmAdmin::AuditSpaceAX(const char *Path)
{
   XrdOucNSWalk nsWalk(&Say, Path, 0, XrdOucNSWalk::retFile
                                    | XrdOucNSWalk::retStat
                                    | XrdOucNSWalk::skpErrs);
   XrdOucNSWalk::NSEnt *nP, *pP;
   char buff[1032];
   int ec, Act = 1;

// Get the files in this directory
//
   if (!(nP = nsWalk.Index(ec))) {if (ec) finalRC = 4; return 1;}
   pP = nP;

// Now traverse through all of the files
//
   while(nP && Act)
        {Act = (XrdOssPath::genPFN(buff, sizeof(buff), nP->Path)
             ? AuditSpaceAXDC(buff, nP) : AuditSpaceAXDB(nP->Path));
         nP = nP->Next;
        }

// Delete the entries and return
//
   while(pP) {nP = pP; pP = pP->Next; delete nP;}
   return Act;
}

/******************************************************************************/
/*                        A u d i t S p a c e A X D B                         */
/******************************************************************************/
  
int XrdFrmAdmin::AuditSpaceAXDB(const char *Path)
{
   char Resp;

// Indicate the problem
//
   Msg("Invalid name for data file ", Path);
   numProb++;

// Return if no fix is needed, otherwise check if we should ask before doing it
//
   if (Opt.Fix)
      {if (!Opt.Force)
          {Resp = XrdFrcUtils::Ask('n', "Delete file?");
           if (Resp != 'y') return Resp != 'a';
          }
       if (unlink(Path)) Emsg(errno, "remove ", Path);
          else numFix++;
      }
   return 1;
}

/******************************************************************************/
/*                        A u d i t S p a c e A X D C                         */
/******************************************************************************/
  
int XrdFrmAdmin::AuditSpaceAXDC(const char *Path, XrdOucNSWalk::NSEnt *nP)
{
   struct stat buf;
   char lkbuff[1032], *Dest = nP->Path;
   int n;

// Assume we have a problem
//
   numProb++;

// Verify that the link to the file exists
//
   if (lstat(Path,&buf))
      {if (errno != ENOENT) {Emsg(errno, "stat ", Path); return -1;}
       Msg("Missing pfn data link ", Path);
       return AuditSpaceAXDL(0, Path, Dest);
      }

// Make sure the PFN file is a link
//
   if ((buf.st_mode & S_IFMT) != S_IFLNK)
      {Msg("Invalid pfn data link ", Path);
       return AuditSpaceAXDL(1, Path, Dest);
      }

// Make sure tyhe link points to the right file
//
   if ((n = readlink(Path, lkbuff, sizeof(lkbuff)-1)) < 0)
      {Emsg(errno, "read link from ", Path); return -1;}
   lkbuff[n] = '\0';
   if (strcmp(Dest, lkbuff))
      {Msg("Incorrect pfn data link ", Path);
       return AuditSpaceAXDL(1, Path, Dest);
      }

// All went well
//
   numProb--; numFiles++; numBytes += nP->Stat.st_size;
   return 1;
}

/******************************************************************************/
/*                        A u d i t S p a c e A X D L                         */
/******************************************************************************/
  
int XrdFrmAdmin::AuditSpaceAXDL(int dorm, const char *Path, const char *Dest)
{
   char Resp;

// Return if no fix is needed, otherwise check if we should ask before doing it
//
   if (!Opt.Fix) return -1;
   if (!Opt.Force)
      {if (dorm)
          Resp = XrdFrcUtils::Ask('n', "Recreate pfn symlink?");
          else
          Resp = XrdFrcUtils::Ask('y',   "Create pfn symlink?");
       if (Resp != 'y') return Resp != 'a';
      }

// Create the pfn symlink
//
   if (dorm) unlink(Path);
   if (symlink(Dest, Path))
      {Emsg(errno, "create symlink ", Path); return -1;}
   Msg("pfn symlink created.");
   numFix++;
   return 1;
}

/******************************************************************************/
/*                          A u d i t S p a c e X A                           */
/******************************************************************************/
  
int XrdFrmAdmin::AuditSpaceXA(const char *Space, const char *Path)
{
   XrdFrmFileset *sP;
   XrdFrmFiles   *fP;
   char tmpv[8], *buff;
   int ec = 0, Act = 1;

// Construct the right space path and get a files object
//
   buff = XrdOssPath::genPath(Path, Space, tmpv);
   fP = new XrdFrmFiles(buff, XrdFrmFiles::Recursive | XrdFrmFiles::NoAutoDel);

// Go and check out the files
//
   while(Act && (sP = fP->Get(ec,1)))
        {if (sP->baseFile()) Act = AuditNameNB(sP);
            else {numFiles++;
                  if ((Act = AuditSpaceXA(sP)))
                     {if (Act < 0) numFiles--;
                         else numBytes += sP->baseFile()->Stat.st_size;
                     }
                 }
         delete sP;
        }

// All done
//
   if (ec) finalRC = 4;
   free(buff);
   delete fP;
   return Act;
}

/******************************************************************************/
  
int XrdFrmAdmin::AuditSpaceXA(XrdFrmFileset *sP)
{
   XrdOucXAttr<XrdFrcXAttrPfn> pfnInfo;
   struct stat buf;
   const char *Plug;
   char Resp = 0, tempPath[1032], lkbuff[1032], *Pfn = pfnInfo.Attr.Pfn;
   int n;

// First step is to get the pfn extended attribute (Get will issue a msg)
//
   if ((n = pfnInfo.Get(sP->basePath()) <= 0))
      {if (!n) Msg("Missing pfn xref for data file ", sP->basePath());
       numProb++;
       return 1;
      }

// If there is no PFN file then recreate symlink if possible
//
   if (lstat(Pfn,&buf))
      {numProb++;
       if (errno != ENOENT) {Emsg(errno, "stat ", Pfn); return 1;}
       Msg("Data file xrefs missing pfn ", Pfn);
       if (Opt.Fix)
          {if (Opt.Force) Resp = 'y';
              else Resp = XrdFrcUtils::Ask('y',"Create pfn symlink?");
           if (Resp == 'y')
              {if (!symlink(sP->basePath(), Pfn))
                  {Msg("pfn symlink created."); numFix++; return 1;}
               Emsg(errno, "create symlink ", Pfn);
              }
          }
       numBLost += sP->baseFile()->Stat.st_size;
       return Resp != 'a';
      }

// If the PFN file is not a link, the see if we should remove the data file
//
   if ((buf.st_mode & S_IFMT) != S_IFLNK)
      {numProb++;
       Msg("Data file xrefs non-symlink pfn ", Pfn);
       if (Opt.Fix)
          {if (Opt.Force) Resp = 'n';
              else Resp = XrdFrcUtils::Ask('n',"Remove data file?");
           if (Resp == 'y')
              {if (unlink(sP->basePath())) Emsg(errno,"remove ",sP->basePath());
                  else {Msg("Data file removed."); numFix++; return -1;}
              }
          }
       numBLost += sP->baseFile()->Stat.st_size;
       return Resp != 'a';
      }

// Check if xrefs are consistent.
//
   if ((n = readlink(Pfn, lkbuff, sizeof(lkbuff)-1)) < 0)
      {Emsg(errno, "read link from ", Pfn); numProb++; return 1;}
   lkbuff[n] = '\0';
   if (!strcmp(sP->basePath(), lkbuff)) return 1;

// Issue first message (there is value in seeing the data file path)
//
   Msg("Inconsistent data file: ", sP->basePath());
   numProb++;

// Diagnose the problem and check if fix is possible
//
   if (!stat(lkbuff, &buf))     Plug = "exists.";
      else if (errno == ENOENT) Plug = "is missing.";
              else {Emsg(errno, "stat ", lkbuff); return 1;}
   Msg("Data file xrefs pfn ", Pfn);
   Msg("Pfn points to a different data file that ", Plug);
   if (!Opt.Fix) return 1;

// If the data file is orphaned then check if we can remove it otherwise
// see if we can simply change the symlink to point to this file
//
   if (*Plug == 'e')
      {if (Opt.Force) Resp = 'n';
           else Resp = XrdFrcUtils::Ask('n',"Remove unreferenced data file?");
       if (Resp == 'y')
          {if (unlink(sP->basePath())) Emsg(errno,"remove ",sP->basePath());
              else {Msg("Data file removed."); numFix++; return -1;}
          }
      } else {
       if (Opt.Force) Resp = 'n';
          else Resp = XrdFrcUtils::Ask('n',"Change pfn symlink?");
       if (Resp == 'y')
          {*tempPath = ' '; strcpy(tempPath+1, Pfn); unlink(tempPath);
           if (symlink(sP->basePath(), tempPath) || rename(tempPath, Pfn))
              {Emsg(errno, "create symlink ", Pfn); unlink(tempPath);}
              else {Msg("pfn symlink changed."); numFix++; return 1;}
          }
      }

// Space for this file is definitely lost
//
   numBLost += sP->baseFile()->Stat.st_size;
   return Resp != 'a';
}
  
/******************************************************************************/
/*                            A u d i t U s a g e                             */
/******************************************************************************/
  
int XrdFrmAdmin::AuditUsage()
{
   XrdFrmConfig::VPInfo *vP = Config.VPList;
   char Sbuff[1024];
   int retval, rc;

// Check if we have a space or we should do all spaces
//
   if (Opt.Args[1]) return AuditUsage(Opt.Args[1]);

// If no cache configured say so
//
   if (!vP) {Emsg("No outplace space has been configured."); return -1;}

// Audit usage for each space
//
   retval = 1;
   while(vP) 
        {strcpy(Sbuff, vP->Name);
         if (!(rc = AuditUsage(Sbuff))) return 0;
         if (rc < 0) retval = rc;
         vP = vP->Next;
        }
   return retval;
}
  
/******************************************************************************/
  
int XrdFrmAdmin::AuditUsage(char *Space)
{
   XrdOucTList   *pP;
   const char *Sfx;
   char Resp, buff[256], *Path = 0;
   long long theClaim, theDiff;
   int haveUsage, Probs = 0;

// Parse the space specification
//
   if (!(pP = ParseSpace(Space, &Path))) return -1;
   if (Path) {Emsg("Path not allowed for audit usage."); return -1;}

// Initialize
//
   numBytes = 0; numFiles = 0; numProb = 0;
   haveUsage = XrdOssSpace::Init();

// Index the space via filesets
//
   do {Probs |= (pP->val ? AuditUsageXA(pP->text, Space)
                         : AuditUsageAX(pP->text));
       pP = pP->next;
      } while(pP);

// Print ending condition
//
   sprintf(buff, "Audit of %d file%s in %s space completed with %serrors.",
                 numFiles, (numFiles == 1 ? "" : "s"), Space,
                 (Probs ? "" : "no "));
   Msg(buff);

// Print what is in the usage file
//
   if (haveUsage)
      {XrdOssSpace::uEnt myEnt;
       XrdOssSpace::Usage(Space, myEnt);
       theClaim = myEnt.Bytes[XrdOssSpace::Serv]
                + myEnt.Bytes[XrdOssSpace::Pstg]
                - myEnt.Bytes[XrdOssSpace::Purg]
                + myEnt.Bytes[XrdOssSpace::Admin];
       sprintf(buff, "%12lld", theClaim);
       Msg("Claimed: ", buff);
      } else theClaim = numBytes;

// Print what we came up with
//
   sprintf(buff, "%12lld", numBytes);
   Msg("Actual:  ", buff);

// Check if fix is required and wanted
//
   if (numBytes == theClaim || !Opt.Fix) return 1;
   if (!haveUsage)
      {Emsg(0, "No usage file present to fix!"); return -1;}

// Compute difference
//
   if (theClaim < numBytes) theDiff = numBytes - theClaim;
      else                  theDiff = theClaim - numBytes;

// See if we should fix this
//
   if (!Opt.Force)
      {if (theDiff < 500000) Sfx = "byte";
          {theDiff = (theDiff+512)/1024; Sfx = "KB";}
       sprintf(buff, "Fix %lld %s difference?", theDiff, Sfx);
       Resp = XrdFrcUtils::Ask('n', "Fix usage information?");
       if (Resp != 'y') return Resp != 'a';
      }

// Fix the problem
//
   XrdOssSpace::Adjust(Space, numBytes-theClaim, XrdOssSpace::Admin);
   return 1;
}

/******************************************************************************/
/*                          A u d i t U s a g e A X                           */
/******************************************************************************/
  
int XrdFrmAdmin::AuditUsageAX(const char *Path)
{
   XrdOucNSWalk nsWalk(&Say, Path, 0, XrdOucNSWalk::retFile
                                    | XrdOucNSWalk::retStat
                                    | XrdOucNSWalk::skpErrs);
   XrdOucNSWalk::NSEnt *nP, *pP;
   int ec;

// Get the files in this directory
//
   if (!(nP = nsWalk.Index(ec))) {if (ec) finalRC = 4; return 1;}

// Now traverse through all of the files
//
   while(nP)
        {numBytes += nP->Stat.st_size;
         numFiles++;
         pP = nP;
         nP = nP->Next;
         delete pP;
        }

// All done
//
   return 0;
}

/******************************************************************************/
/*                          A u d i t U s a g e X A                           */
/******************************************************************************/
  
int XrdFrmAdmin::AuditUsageXA(const char *Path, const char *Space)
{
   XrdFrmFileset *sP;
   XrdFrmFiles   *fP;
   char tmpv[8], *buff;
   int ec = 0;

// Construct the right space path and get a files object
//
   buff = XrdOssPath::genPath(Path, Space, tmpv);
   fP = new XrdFrmFiles(buff, XrdFrmFiles::Recursive | XrdFrmFiles::NoAutoDel);

// Go and check out the files
//
   while((sP = fP->Get(ec)))
        {if ((sP->baseFile()))
            {numFiles++; numBytes += sP->baseFile()->Stat.st_size;}
         delete sP;
        }

// All done
//
   free(buff);
   delete fP;
   return ec;
}
  
/******************************************************************************/
/*                                  i s X A                                   */
/******************************************************************************/
  
int XrdFrmAdmin::isXA(XrdOucNSWalk::NSEnt *nP)
{
   char *lP;

   if (!(nP->Link)) return 0;
   lP = nP->Link + nP->Lksz -1;
   return (*lP == XrdOssPath::xChar);
}
