/******************************************************************************/
/*                                                                            */
/*                   X r d O s s C k s M a n a g e r . c c                    */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
  
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksCalcadler32.hh"
#include "XrdCks/XrdCksCalccrc32.hh"
#include "XrdCks/XrdCksCalcmd5.hh"
#include "XrdCks/XrdCksLoader.hh"
#include "XrdCks/XrdCksManager.hh"
#include "XrdCks/XrdCksXAttr.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucXAttr.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFAttr.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCksManager::XrdCksManager(XrdSysError *erP, int rdsz, XrdVersionInfo &vInfo,
                             bool autoload)
              : XrdCks(erP), myVersion(vInfo)
{

// Get a dynamic loader if so wanted
//
   if (autoload) cksLoader = new XrdCksLoader(vInfo);
      else       cksLoader = 0;

// Prefill the native digests we support
//
   strcpy(csTab[0].Name, "adler32");
   strcpy(csTab[1].Name, "crc32");
   strcpy(csTab[2].Name, "md5");
   csLast = 2;

// Compute the i/o size
//
   if (rdsz <= 65536) segSize = 67108864;
      else segSize = ((rdsz/65536) + (rdsz%65536 != 0)) * 65536;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdCksManager::~XrdCksManager()
{
   int i;
   for (i = 0; i <= csLast; i++)
       {if (csTab[i].Obj && csTab[i].doDel) csTab[i].Obj->Recycle();
        if (csTab[i].Path)   free(  csTab[i].Path);
        if (csTab[i].Parms)  free(  csTab[i].Parms);
        if (csTab[i].Plugin) delete csTab[i].Plugin;
       }
   if (cksLoader) delete cksLoader;
}

/******************************************************************************/
/*                                  C a l c                                   */
/******************************************************************************/
  
int XrdCksManager::Calc(const char *Pfn, XrdCksData &Cks, int doSet)
{
   XrdCksCalc *csP;
   csInfo *csIP = &csTab[0];
   time_t MTime;
   int rc;

// Determine which checksum to get
//
   if (csLast < 0) return -ENOTSUP;
   if (!(*Cks.Name)) Cks.Set(csIP->Name);
      else if (!(csIP = Find(Cks.Name))) return -ENOTSUP;

// If we need not set the checksum then see if we can get it from the
// extended attributes.

// Obtain a new checksum object
//
   if (!(csP = csIP->Obj->New())) return -ENOMEM;

// Use the calculator to get and possibly set the checksum
//
   if (!(rc = Calc(Pfn, MTime, csP)))
      {memcpy(Cks.Value, csP->Final(), csIP->Len);
       Cks.fmTime = static_cast<long long>(MTime);
       Cks.csTime = static_cast<int>(time(0) - MTime);
       Cks.Length = csIP->Len;
       csP->Recycle();
       if (doSet)
          {XrdOucXAttr<XrdCksXAttr> xCS;
           memcpy(&xCS.Attr.Cks, &Cks, sizeof(xCS.Attr.Cks));
           if ((rc = xCS.Set(Pfn))) return -rc;
          }
      }

// All done
//
   return rc;
}

/******************************************************************************/
  
int XrdCksManager::Calc(const char *Pfn, time_t &MTime, XrdCksCalc *csP)
{
   class ioFD
        {public:
         int FD;
             ioFD() : FD(-1) {}
            ~ioFD() {if (FD >= 0) close(FD);}
        } In;
   struct stat Stat;
   char *inBuff;
   off_t  Offset=0, fileSize;
   size_t ioSize, calcSize;
   int rc;

// Open the input file
//
   if ((In.FD = open(Pfn, O_RDONLY)) < 0) return -errno;

// Get the file characteristics
//
   if (fstat(In.FD, &Stat)) return -errno;
   if (!(Stat.st_mode & S_IFREG)) return -EPERM;
   calcSize = fileSize = Stat.st_size;
   MTime = Stat.st_mtime;

// We now compute checksum 64MB at a time using mmap I/O
//
   ioSize = (fileSize < (off_t)segSize ? fileSize : segSize); rc = 0;
   while(calcSize)
        {if ((inBuff = (char *)mmap(0, ioSize, PROT_READ, 
                       MAP_NORESERVE|MAP_PRIVATE, In.FD, Offset)) == MAP_FAILED)
            {rc = errno; eDest->Emsg("Cks", rc, "memory map", Pfn); break;}
         madvise(inBuff, ioSize, MADV_SEQUENTIAL);
         csP->Update(inBuff, ioSize);
         calcSize -= ioSize; Offset += ioSize;
         if (munmap(inBuff, ioSize) < 0)
            {rc = errno; eDest->Emsg("Cks",rc,"unmap memory for",Pfn); break;}
         if (calcSize < (size_t)segSize) ioSize = calcSize;
        }

// Return if we failed
//
   if (calcSize) return (rc ? -rc : -EIO);
   return 0;
}

/******************************************************************************/
/*                                C o n f i g                                 */
/******************************************************************************/
/*
   Purpose:  To parse the directive: ckslib <digest> <path> [<parms>]

             <digest>  the name of the checksum.
             <path>    the path of the checksum library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/
int XrdCksManager::Config(const char *Token, char *Line)
{
   XrdOucTokenizer Cfg(Line);
   char *val, *path = 0, name[XrdCksData::NameSize], *parms;
   int i;

// Get the the checksum name
//
   Cfg.GetLine();
   if (!(val = Cfg.GetToken()) || !val[0])
      {eDest->Emsg("Config", "checksum name not specified"); return 1;}
   if (int(strlen(val)) >= XrdCksData::NameSize)
      {eDest->Emsg("Config", "checksum name too long"); return 1;}
   strcpy(name, val);

// Get the path and optional parameters
//
   val = Cfg.GetToken(&parms);
   if (val && val[0]) path = strdup(val);
      else {eDest->Emsg("Config","library path missing for ckslib digest",name);
            return 1;
           }

// Check if this replaces an existing checksum
//
   for (i = 0; i < csMax; i++)
       if (!(*csTab[i].Name) || !strcmp(csTab[i].Name, name)) break;

// See if we can insert a new checksum (or replace one)
//
   if (i >= csMax)
      {eDest->Emsg("Config", "too many checksums specified");
       if (path) free(path);
       return 1;
      } else if (!(*csTab[i].Name)) csLast = i;

// Insert the new checksum
//
   strcpy(csTab[i].Name, name);
   if (csTab[i].Path) free(csTab[i].Path);
   csTab[i].Path = path;
   if (csTab[i].Parms) free(csTab[i].Parms);
   csTab[i].Parms = (parms && *parms ? strdup(parms) : 0);

// All done
//
   return 0;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

int XrdCksManager::Init(const char *ConfigFN, const char *DfltCalc)
{
   int i;

// See if we need to set the default calculation
//
   if (DfltCalc)
      {for (i = 0; i < csLast; i++) if (!strcmp(csTab[i].Name, DfltCalc)) break;
       if (i >= csMax)
          {eDest->Emsg("Config", DfltCalc, "cannot be made the default; "
                                           "not supported.");
           return 0;
          }
       if (i) {csInfo Temp = csTab[i]; csTab[i] = csTab[0]; csTab[0] = Temp;}
      }

// See if there are any chacksums to configure
//
   if (csLast < 0)
      {eDest->Emsg("Config", "No checksums defined; cannot configure!");
       return 0;
      }

// Complete the checksum table
//
   for (i = 0; i <= csLast; i++)
       {if (csTab[i].Path) {if (!(Config(ConfigFN, csTab[i]))) return 0;}
           else {     if (!strcmp("adler32", csTab[i].Name))
                         csTab[i].Obj = new XrdCksCalcadler32;
                 else if (!strcmp("crc32",   csTab[i].Name))
                         csTab[i].Obj = new XrdCksCalccrc32;
                 else if (!strcmp("md5",     csTab[i].Name))
                         csTab[i].Obj = new XrdCksCalcmd5;
                 else {eDest->Emsg("Config", "Invalid native checksum -",
                                             csTab[i].Name);
                       return 0;
                      }
                 csTab[i].Obj->Type(csTab[i].Len);
                }
       }

// All done
//
   return 1;
}

/******************************************************************************/

#define XRDOSSCKSLIBARGS XrdSysError *, const char *, const char *, const char *

int XrdCksManager::Config(const char *cFN, csInfo &Info)
{
   XrdOucPinLoader myPin(eDest, &myVersion, "ckslib", Info.Path);
   XrdCksCalc *(*ep)(XRDOSSCKSLIBARGS);
   int n;

// Find the entry point
//
   Info.Plugin = 0;
   if (!(ep = (XrdCksCalc *(*)(XRDOSSCKSLIBARGS))
              (myPin.Resolve("XrdCksCalcInit"))))
      {eDest->Emsg("Config", "Unable to configure cksum", Info.Name);
       myPin.Unload();
       return 0;
      }

// Get the initial object
//
   if (!(Info.Obj = ep(eDest,cFN,Info.Name,(Info.Parms ? Info.Parms : ""))))
      {eDest->Emsg("Config", Info.Name, "checksum initialization failed");
       myPin.Unload();
       return 0;
      }

// Verify the object
//
   if (strcmp(Info.Name, Info.Obj->Type(n)))
      {eDest->Emsg("Config",Info.Name,"cksum plugin returned wrong name -",
                            Info.Obj->Type(n));
       myPin.Unload();
       return 0;
      }
   if (n > XrdCksData::ValuSize || n <= 0)
      {eDest->Emsg("Config",Info.Name,"cksum plugin has an unsupported "
                                "checksum length");
       myPin.Unload();
       return 0;
      }

// All is well
//
   Info.Plugin = myPin.Export();
   Info.Len = n;
   return 1;
}

/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/
  
XrdCksManager::csInfo *XrdCksManager::Find(const char *Name)
{
   static XrdSysMutex myMutex;
   XrdCksCalc *myCalc;
   int i;

// Find the pre-loaded checksum
//
   for (i = 0; i <= csLast; i++)
       if (!strcmp(Name, csTab[i].Name)) return &csTab[i];

// If we have loader see if we can auto-load this object
//
   if (!cksLoader) return 0;
   myMutex.Lock();

// An entry could have been added as we were running unlocked
//
   for (i = 0; i <= csLast; i++)
       if (!strcmp(Name, csTab[i].Name))
          {myMutex.UnLock();
           return &csTab[i];
          }

// Check if we have room in the table
//
   if (csLast >= csMax)
      {myMutex.UnLock();
       eDest->Emsg("CksMan","Unable to load",Name,"; checksum limit reached.");
       return 0;
      }

// Attempte to dynamically load this object
//
{  char buff[2048];
   *buff = 0;
   if (!(myCalc = cksLoader->Load(Name, 0, buff, sizeof(buff), true)))
      {myMutex.UnLock();
       eDest->Emsg("CksMan", "Unable to load", Name);
       if (*buff) eDest->Emsg("CksMan", buff);
       return 0;
      }
}

// Fill out the table
//
   i = csLast + 1;
   strncpy(csTab[i].Name, Name, XrdCksData::NameSize);
   csTab[i].Obj    = myCalc;
   csTab[i].Path   = 0;
   csTab[i].Parms  = 0;
   csTab[i].Plugin = 0;
   csTab[i].doDel  = false;
   myCalc->Type(csTab[i].Len);

// Return the result
//
   csLast = i;
   myMutex.UnLock();
   return &csTab[i];
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/

int XrdCksManager::Del(const char *Pfn, XrdCksData &Cks)
{
   XrdOucXAttr<XrdCksXAttr> xCS;

// Set the checksum name
//
   xCS.Attr.Cks.Set(Cks.Name);

// Delete the attribute and return the result
//
   return xCS.Del(Pfn);
}

/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/

int XrdCksManager::Get(const char *Pfn, XrdCksData &Cks)
{
   XrdOucXAttr<XrdCksXAttr> xCS;
   time_t MTime;
   int rc, nFault;

// Determine which checksum to get (we will accept unsupported ones as well)
//
   if (csLast < 0) return -ENOTSUP;
   if (!*Cks.Name) Cks.Set(csTab[0].Name);
   if (!xCS.Attr.Cks.Set(Cks.Name)) return -ENOTSUP;

// Retreive the attribute
//
   if ((rc = xCS.Get(Pfn)) <= 0) return (rc ? rc : -ESRCH);

// Mark state of the name and copy the attribute over
//
   nFault = strcmp(xCS.Attr.Cks.Name, Cks.Name);
   Cks = xCS.Attr.Cks;

// Verify the file
//
   if ((rc = ModTime(Pfn, MTime))) return rc;

// Return result
//
   return (Cks.fmTime != MTime || nFault
       ||  Cks.Length > XrdCksData::ValuSize || Cks.Length <= 0
        ? -ESTALE : int(Cks.Length));
}

/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/
  
char *XrdCksManager::List(const char *Pfn, char *Buff, int Blen, char Sep)
{
   static const char *vPfx = "XrdCks.";
   static const int   vPln = strlen(vPfx);
   XrdSysFAttr::AList *vP, *axP = 0;
   char *bP = Buff;
   int i, n;

// Verify that the buffer is large enough
//
   if (Blen < 2) return 0;

// Check if the default list is wanted
//
   if (!Pfn)
      {if (csLast < 0) return 0;
       i = 0;
       while(i <= csLast && Blen > 1)
            {n = strlen(csTab[i].Name);
             if (n >= Blen) break;
             if (bP != Buff) *bP++ = Sep;
             strcpy(bP, csTab[i].Name); bP += n; *bP = 0;
            }
       return (bP == Buff ? 0 : Buff);
      }

// Get a list of attributes for this file
//
   if (XrdSysFAttr::Xat->List(&axP, Pfn) < 0 || !axP) return 0;

// Go through the list extracting what we are looking for
//
   vP = axP;
   while(vP)
        {if (vP->Nlen > vPln && !strncmp(vP->Name, vPfx, vPln))
            {n  = vP->Nlen - vPln;
             if (n >= Blen) break;
             if (bP != Buff) *bP++ = Sep;
             strcpy(bP, vP->Name + vPln); bP += n; *bP = 0;
            }
         vP = vP->Next;
        }

// All done
//
   XrdSysFAttr::Xat->Free(axP);
   return (bP == Buff ? 0 : Buff);
}

/******************************************************************************/
/*                               M o d T i m e                                */
/******************************************************************************/
  
int XrdCksManager::ModTime(const char *Pfn, time_t &MTime)
{
   struct stat Stat;

   if (stat(Pfn, &Stat)) return -errno;

   MTime = Stat.st_mtime;
   return 0;
}

/******************************************************************************/
/*                                  N a m e                                   */
/******************************************************************************/
  
const char *XrdCksManager::Name(int seqNum)
{

   return (seqNum < 0 || seqNum > csLast ? 0 : csTab[seqNum].Name);
}

/******************************************************************************/
/*                                O b j e c t                                 */
/******************************************************************************/
  
XrdCksCalc *XrdCksManager::Object(const char *name)
{
   csInfo *csIP = &csTab[0];

// Return an object it at all possible
//
   if (name && !(csIP = Find(name))) return 0;
   return csIP->Obj->New();
}
  
/******************************************************************************/
/*                                  S i z e                                   */
/******************************************************************************/
  
int XrdCksManager::Size(const char *Name)
{
   csInfo *iP = (Name != 0 ? Find(Name) : &csTab[0]);
   return (iP != 0 ? iP->Len : 0);
}

/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
int XrdCksManager::Set(const char *Pfn, XrdCksData &Cks, int myTime)
{
   XrdOucXAttr<XrdCksXAttr> xCS;
   csInfo *csIP = &csTab[0];

// Verify the incomming checksum for correctness
//
   if (csLast < 0 || (*Cks.Name && !(csIP = Find(Cks.Name)))) return -ENOTSUP;
   if (Cks.Length != csIP->Len)  return -EDOM;
   memcpy(&xCS.Attr.Cks, &Cks, sizeof(xCS.Attr.Cks));

// Set correct times if need be
//
   if (!myTime)
      {time_t MTime;
       int rc = ModTime(Pfn, MTime);
       if (rc) return rc;
       xCS.Attr.Cks.fmTime = static_cast<long long>(MTime);
       xCS.Attr.Cks.csTime = static_cast<int>(time(0) - MTime);
      }

// Now set the checksum information in the extended attribute object
//
   return xCS.Set(Pfn);
}

/******************************************************************************/
/*                                   V e r                                    */
/******************************************************************************/

int XrdCksManager::Ver(const char *Pfn, XrdCksData &Cks)
{
   XrdOucXAttr<XrdCksXAttr> xCS;
   time_t MTime;
   csInfo *csIP = &csTab[0];
   int rc;

// Determine which checksum to get
//
   if (csLast < 0 || (*Cks.Name && !(csIP = Find(Cks.Name)))) return -ENOTSUP;
   xCS.Attr.Cks.Set(csIP->Name);

// Verify the file
//
   if ((rc = ModTime(Pfn, MTime))) return rc;

// Retreive the attribute. Return upon fatal error.
//
   if ((rc = xCS.Get(Pfn)) < 0) return rc;

// Verify the checksum and see if we need to recalculate it
//
   if (!rc || xCS.Attr.Cks.fmTime != MTime
   ||  strcmp(xCS.Attr.Cks.Name, csIP->Name)
   ||  xCS.Attr.Cks.Length != csIP->Len)
      {strcpy(xCS.Attr.Cks.Name, Cks.Name);
       if ((rc = Calc(Pfn, xCS.Attr.Cks, 1)) < 0) return rc;
      }

// Compare the checksums
//
   return (xCS.Attr.Cks.Length == Cks.Length 
       &&  !memcmp(xCS.Attr.Cks.Value, Cks.Value, csIP->Len));
}
