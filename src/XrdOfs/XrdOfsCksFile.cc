/******************************************************************************/
/*                                                                            */
/*                      X r d O f s C k s F i l e . c c                       */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <netinet/in.h>
#include <sys/param.h>

#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksData.hh"
#include "XrdOfs/XrdOfsCksFile.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                        s t a t i c   o b j e c t s                         */
/******************************************************************************/

extern XrdSysError  OfsEroute;
extern XrdOss*      XrdOfsOss;

namespace
{
       XrdSysError& eLog = OfsEroute;
       XrdCks*      cksP = 0;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOfsCksFile::XrdOfsCksFile(const char* tid, const char* path,
                             XrdOssDF*   df,  XrdCksCalc* cP)
                            : XrdOssWrapDF(*df),
                              tident(tid), fPath(path), ossDF(df),
                              calcP(cP), altcP(0), nextOff(0)
{
// Obtain information about the chacksum we are to use. It should have been
// pre-screened for viability, but we check it again just to make sure and
// to setup the proper execution path.
//
   int sz;
   cksName = cP->Type(sz);
   Dirty = false;

   if (cP->Combinable())
      {if (sz == (int)sizeof(uint32_t))
          {ProcessRTC = &XrdOfsCksFile::RTC_CB32;
           ProcessRTE = &XrdOfsCksFile::RTC_EB32;
           altcP = calcP->New();
          }
          else Dirty = true;
      } else {
       Dirty = true;
      }

// If we did not succeed, issue error message. The subsequent open will fail
//
   if (Dirty)
      {char eBuff[128];
       snprintf(eBuff, sizeof(eBuff), "'%s' used for", cksName);
       eLog.Emsg("ckscon", "Unsupported real-time checksum", eBuff, path);
      }
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOfsCksFile::~XrdOfsCksFile()
{
   if (ossDF) delete ossDF;
   if (calcP) calcP->Recycle();
   if (altcP) altcP->Recycle();
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

/*
  Function: Close the file associated with this object.

  Input:    None.

  Output:   Returns XrdOssOK upon success and -1 upon failure.
*/
int XrdOfsCksFile::Close(long long *retsz)
{
   char pFN[MAXPATHLEN+8];
   XrdCksData cksData;
   struct stat Stat;
   int csSize, rc;;

// Get the incomming file's pfn as we will needit
//
   const char* pfnP = XrdOfsOss->Lfn2Pfn(fPath, pFN, sizeof(pFN), rc);
   if (pfnP == 0)
      {eLog.Emsg("ckscls", rc, "determine pfn for real-time checksum");
       Dirty = true;
      }

// Process checksum if it is valid
//
   while(!Dirty) // This is not a loop but avoids deeply next if's.
        {char  eBuff[256];
         const char* eTxt = (this->*ProcessRTE)(eBuff, sizeof(eBuff));
         Dirty = true;

         // Verify that checksum was fully calculated
         //
         if (eTxt)
            {eLog.Emsg("ckcls", "Unable to get final real-time checksum", eTxt);
             break;
            }

         // Fill out the checksum information
         //
         memset((void*)&cksData, 0, sizeof(cksData));
         cksData.Set(calcP->Type(csSize));
         cksData.Length = csSize;
         memcpy(cksData.Value, calcP->Final(), csSize);

         if ((rc = ossDF->Fstat(&Stat)))
            {eLog.Emsg("clscls", rc, "get mtime for real-time checksum");
             break;
            }

         cksData.fmTime = static_cast<long long>(Stat.st_mtime);
         cksData.csTime = static_cast<int>(time(0) - Stat.st_mtime);

         if ((rc = cksP->Set(pfnP, cksData, 1)))
            eLog.Emsg("ckscls", rc, "set real-time checksum");
            else Dirty = false;

         break;
        }

// Check if all went well and issue message if not
//
   if (Dirty)
      eLog.Emsg("ckscls", cksName, "real-time checksum was not set for", fPath);

// Issue close to the underlying object
//
   Dirty = true; // Prevent re-entry processing
   return wrapDF.Close(retsz);
}

/******************************************************************************/
/*                             F t r u n c a t e                              */
/******************************************************************************/
/*
  Function: Set the length of associated file to 'flen'.

  Input:    flen      - The new size of the file.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/
int XrdOfsCksFile::Ftruncate(unsigned long long flen)
{

// Execute the truncate
//
   int rc = wrapDF.Ftruncate(flen);

// We support streaming checksum only when the truncate makes the file 0 length
//
   if (!Dirty)
      {if (rc < 0)
          {eLog.Emsg("ckstrunc", rc, "continue real-time checksum for", fPath);
           Dirty = true;
          } else {
           if (flen)
              {eLog.Emsg("ckstrunc","Unable to continue real-time checksum for",
                         fPath, "; truncate arg not 0.");
               Dirty = true;
              } else {
               nextOff = 0;
               segMap.clear();
               calcP->Init();
              }
          }
      }

// All done
//
   return rc;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

void XrdOfsCksFile::Init(XrdCks* cp, XrdOucEnv* ep)
{
// Record the checksum manager
//
   cksP = cp;
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/

int XrdOfsCksFile::Open(const char* path, int Oflag, mode_t Mode,
                        XrdOucEnv& env)
{
// Make sure we have a clean setup. If not return an error.
//
   if (Dirty) return -ENOTSUP;

// We intercept open because for non-combinable checksums we need to make
// sure the file is open in r/w mode. Since we don't support such checksums
// yet, the code below is commented out.
//
/* if (!(calcp->Combinable()))
      {Oflag &= ~O_ACCMODE;
       Oflag |= O_RDWR;
      }
*/
// Issue open and if unsuccessful, mark this as a dirty object
//
   int rc = wrapDF.Open(path, Oflag, Mode, env);
   if (rc) Dirty = true;
   return rc;
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/

ssize_t XrdOfsCksFile::pgWrite(void*     buffer,
                               off_t     offset,
                               size_t    wrlen,
                               uint32_t* csvec,
                               uint64_t  opts)
{
   const char* eText;

// We will only continue the checksum if the underlying write succeeds
//
  ssize_t retval = wrapDF.pgWrite(buffer, offset, wrlen, csvec, opts);

// Continue the streaming checksum if at all possible
//
   if (!Dirty)
      {if (retval < 0)
          {eLog.Emsg("ckspgw", retval, "continue real-time checksum for",fPath);
           Dirty = true;
          } else {
           if ((eText = (this->*ProcessRTC)(buffer, offset, wrlen)))
              {eLog.Emsg("ckspgw","unable to continue real-time checksum for",
                                 fPath, eText);
              }
          }
      }

// Return actual result
//
   return retval;
}

/******************************************************************************/

int XrdOfsCksFile::pgWrite(XrdSfsAio* aioparm, uint64_t opts)
{
   const char* eText;

// It is too complicated to do the async I/O before doing the checksum
//
   if (!Dirty)
      {if ((eText = (this->*ProcessRTC)((void *)aioparm->sfsAio.aio_buf,
                                        (off_t) aioparm->sfsAio.aio_offset,
                                        (size_t)aioparm->sfsAio.aio_nbytes)))
          {eLog.Emsg("cksaiopw", "Unable to continue real-time checksum for",
                               fPath, eText);
           Dirty = true;
          }
      }

// Now do the I/O
//
   return wrapDF.pgWrite(aioparm, opts);
}

/******************************************************************************/
/* Static:                        V i a b l e                                 */
/******************************************************************************/

bool XrdOfsCksFile::Viable(XrdCksCalc* cP)
{
// Currently we only support combinable checksums
//
   if (!(cP->Combinable())) return false;

// Of the combinable ones, we only support the ones that have 32 bits.
//
   int sz;
   cP->Type(sz);
   if (sz != (int)sizeof(uint32_t)) return false;

// We can use this checkum
//
   return true;
}

/******************************************************************************/
/*                                 w r i t e                                  */
/******************************************************************************/

/*
  Function: Write `blen' bytes to the associated file, from 'buff'
            and return the actual number of bytes written.

  Input:    buff      - Address of the buffer from which to get the data.
            offset    - The absolute 64-bit byte offset at which to write.
            blen      - The number of bytes to write from the buffer.

  Output:   Returns the number of bytes written upon success and -errno o/w.
*/

ssize_t XrdOfsCksFile::Write(const void* buff, off_t offset, size_t blen)
{
   const char* eText;

// We will only continue the checksum if the underlying write succeeds
//
  ssize_t retval = wrapDF.Write(buff, offset, blen);

// Continue the streaming checksum if at all possible
//
   if (!Dirty)
      {if (retval < 0)
          {eLog.Emsg("cksw", retval, "continue streaming checksum for", fPath);
           Dirty = true;
          } else {
           if ((eText = (this->*ProcessRTC)(buff, offset, blen)))
              {eLog.Emsg("cksw", "Unable to continue real-time checksum for",
                               fPath, eText);
               Dirty = true;
              }
          }
      }

// Return actual result
//
   return retval;
}

/******************************************************************************/

int XrdOfsCksFile::Write(XrdSfsAio* aioparm)
{
   const char* eText;

// It is too complicated to do the async I/O before doing the checksum
//
   if (!Dirty)
      {if ((eText = (this->*ProcessRTC)((void *)aioparm->sfsAio.aio_buf,
                                        (off_t) aioparm->sfsAio.aio_offset,
                                        (size_t)aioparm->sfsAio.aio_nbytes)))
          {eLog.Emsg("cksaiopw", "Unable to continue real-time checksum for",
                               fPath, eText);
           Dirty = true;
          }
      }

// Now do the I/O
//
   return wrapDF.Write(aioparm);
}

/******************************************************************************/
/*                                W r i t e V                                 */
/******************************************************************************/

ssize_t XrdOfsCksFile::WriteV(XrdOucIOVec* writeV, int n)
{

// We do not support streaming checksums when WriteV is used
//
   if (!Dirty)
      {eLog.Emsg("ckswv", "Unable to continue streaming checksum for",
                         fPath, "; WriteV() conflict.");
       Dirty = true;
      }

// We still handle the actual write
//
   return wrapDF.WriteV(writeV, n);
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                              R T C _ C B 3 2                               */
/******************************************************************************/

// This method handles combinable checkums that are 32 bits in length
//
const char* XrdOfsCksFile::RTC_CB32(const void* inBuff, off_t inOff, int inLen)
{

// Check where the incomming segment is adjacent to current segment
//
   if (inOff == nextOff)
      {calcP->Update((const char*)inBuff, inLen);
       nextOff = inOff + inLen;
       auto it = segMap.begin();
       while(it != segMap.end() && nextOff == it->second.segBeg)
            {// Combine or update base checksum
             calcP->Combine((const char*)&(it->second.segCks),it->second.segLen);
             nextOff = it->second.segBeg + it->second.segLen;
             it = segMap.erase(it);
            }

       // Verify that we end in a proper state
       //
       if (it != segMap.end() && nextOff > it->second.segBeg)
          return "; I/O segments overlap";

       return 0;
      }

// Verify that incomming segment is past the expectected segment
//
   if (inOff < nextOff) return "; ovewrite of previous data";

// Compute checksum for the incomming block
//
   char* newCS = altcP->Calc((const char*)inBuff, inLen);
   uint32_t theCS;
#ifndef Xrd_Big_Endian
   uint32_t tmp;
   memcpy(&tmp, newCS, sizeof(tmp));
   theCS = ntohl(tmp);
#else
   memcpy(theCS, newCS, sizeof(theCS);
#endif

// Create new segment and try inserting it into the map
//
   inSeg newSeg(inOff, inLen, theCS);

// Insert this element into the map
//
   auto it = segMap.insert(std::pair(inOff, newSeg));
   if (it.second == false)
      return "; duplicate write";

// All done
//
   return 0;
}

/******************************************************************************/
/*                              R T C _ E B 3 2                               */
/******************************************************************************/

// This method handles combinable checkums that are 32 bits in length
//
const char* XrdOfsCksFile::RTC_EB32(char* eBuff, int eBLen)
{

// Verify that all data has been written for this checksum
//
   if (segMap.size())
      {auto it = segMap.begin();
       snprintf(eBuff, eBLen, "; %lld bytes missing at offset %lld",
                (long long)(it->second.segBeg - nextOff),
                (long long)nextOff);
       return eBuff;
      }

// We are good to go
//
   return 0;
}
