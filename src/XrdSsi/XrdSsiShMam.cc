/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S h M a m . c c                         */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <fcntl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <zlib.h>
#include <iostream>

#include "XrdSsi/XrdSsiShMam.hh"
#include "XrdSys/XrdSysE2T.hh"

using namespace std;

/* Gentoo removed OF from their copy of zconf.h but we need it here.
   See https://bugs.gentoo.org/show_bug.cgi?id=383179 for the sad history.
   This patch modelled after https://trac.osgeo.org/gdal/changeset/24622
*/
#ifndef OF
#define OF(args) args
#endif

/******************************************************************************/
/*   S h a r e d   M e m o r y   I n f o r m a t i o n   S t r u c t u r e    */
/******************************************************************************/

namespace
{
struct ShmInfo
      {int   verNum;        // Always 1st fout bytes
       int   index;         // Offset of index
       int   slots;         // Number of slots in index
       int   slotsUsed;     // Number of entries in use
       int   itemCount;     // Number of items in this map
       int   typeSz;        // Size of the data payload
       int   itemSz;        // Size of each item
       int   keyPos;        // Position of key in item
       int   freeItem;      // Offset to item on the free list
       int   freeCount;     // Number of items on the free list
       int   infoSz;        // Size of header also original lowFree
       int   lowFree;       // Offset to low  memory that is free
       int   highUse;       // Offset to high memory that is used
       char  reUse;         // When non-zero items can be reused (r/o locking)
       char  multW;         // When non-zero multiple writers are allowed
       char  rsvd1;
       char  rsvd2;
       int   maxKeys;       // Maximum number of keys
       int   maxKeySz;      // Longest allowed key (not including null byte)
       int   hashID;        // The name of the hash
       char  typeID[64];    // Name of the type stored here
       char  myName[64];    // Name of the implementation
      };
#define SHMINFO(x) ((ShmInfo *)shmBase)->x

#define SHMADDR(type, offs) (type *)(shmBase + offs)

#define SHMOFFS(addr)       (char *)addr - shmBase

#define ITEM_KEY(x) (char *)x + sizeof(MemItem) + keyPos

#define ITEM_VAL(x) (char *)x + sizeof(MemItem)

#define ITEM_VOF(x) (char *)x + sizeof(MemItem) - shmBase

int       PageMask = ~(sysconf(_SC_PAGESIZE)-1);
int       PageSize =   sysconf(_SC_PAGESIZE);
}

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
namespace
{
class EnumJar
{public:
char        *buff;
int          fd;
int          iNum;
             EnumJar(int xfd, int bsz)
                    : buff(new char[bsz]), fd(xfd), iNum(0) {}
            ~EnumJar() {if (fd >= 0) close(fd);
                        if (buff)    delete [] buff;
                       }
};

class FileHelper
{
public:
bool         autoClose;

             FileHelper(XrdSsiShMam *mp) : autoClose(false), shMamP(mp) {}
            ~FileHelper() {if (autoClose)
                              {int rc = errno; shMamP->Detach(); errno = rc;}
                          }
private:
XrdSsiShMam *shMamP;
};

class MutexHelper
{
public:
pthread_rwlock_t *mtxP;

                 MutexHelper(pthread_rwlock_t *mtx, XrdSsiShMam::LockType isrw)
                            : mtxP(mtx)
                            {if (mtx)
                                {if (isrw) pthread_rwlock_wrlock(mtx);
                                    else   pthread_rwlock_rdlock(mtx);
                                }
                            }

                ~MutexHelper() {if (mtxP)  pthread_rwlock_unlock(mtxP);}
};
}

/******************************************************************************/
/*              F i l e   D e s c r i p t o r   H a n d l i n g               */
/******************************************************************************/
  
namespace
{
#if ( defined(__linux__) || defined(__GNU__) ) && defined(O_CLOEXEC) && defined(F_DUPFD_CLOEXEC)
inline int  ShMam_Dup(int oldfd)
                     {return fcntl(oldfd, F_DUPFD_CLOEXEC, 0);}

inline int  ShMam_Open(const char *path, int flags)
                      {return open(path, flags|O_CLOEXEC);}

inline int  ShMam_Open(const char *path, int flags, mode_t mode)
                      {return open(path, flags|O_CLOEXEC, mode);}
#else
inline int  ShMam_Dup(int oldfd)
                     {int newfd = dup(oldfd);
                      if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                      return newfd;
                     }

inline int  ShMam_Open(const char *path, int flags)
                      {int newfd = open(path, flags);
                       if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                       return newfd;
                      }

inline int  ShMam_Open(const char *path, int flags, mode_t mode)
                      {int newfd = open(path, flags, mode);
                       if (newfd >= 0) fcntl(newfd, F_SETFD, FD_CLOEXEC);
                       return newfd;
                      }
#endif

inline bool ShMam_Flush(int fd)
{
#if  _POSIX_SYNCHRONIZED_IO > 0
   return fdatasync(fd) == 0;
#else
   return fsync(fd) == 0;
#endif
}
/*
inline bool ShMam_Flush(void *memP, int sOpt)
{
   if (msync((void *)((uintptr_t)memP & PageMask), PageSize, sOpt))
      return true;
   std::cerr <<"ShMam: msync() failed; " <<XrdSysE2T(errno) <<std::endl;
   return false;
}
*/
/*
inline bool ShMam_Flush(void *memP, int mLen, int sOpt)
{  uintptr_t memB = ((uintptr_t)memP) & PageMask;
   uintptr_t memE = ((uintptr_t)memP) + mLen;
   int rc;
   if ((rc = msync((void *)memB, memE-memB, sOpt)))
      std::cerr <<"ShMam: msync() failed; " <<XrdSysE2T(errno) <<std::endl;
   return rc == 0;
}
*/
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSsiShMam::XrdSsiShMam(XrdSsiShMat::NewParms &parms) : XrdSsiShMat(parms)
{

// Initialize common stuff
//
   shmTemp   = 0;
   shmSize   = 0;
   shmBase   = 0;
   shmFD     =-1;
   timeOut   =-1;
   lkCount   = 0;
   syncLast  = 0;
   syncOpt   = 0;
   syncQWR   = 0;
   syncQSZ   = 0;
   syncOn    = false;
   syncBase  = false;
   isRW      = false;
   lockRO    = true;
   lockRW    = true;
   reUse     = false;
   useAtomic = true;

// Initialize r/w mutexes
//
   pthread_mutex_init(&lkMutex, NULL);
   pthread_rwlock_init(&myMutex, NULL);
}
  
/******************************************************************************/
/*                               A d d I t e m                                */
/******************************************************************************/

bool XrdSsiShMam::AddItem(void *newdata, void *olddata, const char *key,
                          int   hash,    bool  replace)
{
   XLockHelper lockInfo(this, RWLock);
   MemItem  *theItem, *prvItem, *newItem;
   int hEnt, kLen, iOff, retEno = 0;

// Make sure we can allocate a new item
//
   if (!shmSize) {errno = ENOTCONN;    return false;}
   if (!isRW)    {errno = EROFS;       return false;}

// Verify key length
//
   kLen = strlen(key);
   if (kLen > maxKLen) {errno = ENAMETOOLONG; return false;}

// Check if we need to remap this memory (atomic tests is not needed here).
// We need to do this prior to file locking as the requirements may change.
//
   if (verNum != SHMINFO(verNum)) ReMap(RWLock);

// Lock the file if we have multiple writers or recycling items
//
   if (lockRW && !lockInfo.FLock()) return false;

// First try to find the item
//
   hEnt  = Find(theItem, prvItem, key, hash);

// If we found it then see if we can replace it. If so and we can reuse the
// the item, then just update the data portion. Otherwise, we need to get a
// new item and replace the existing item.
//
   if (hEnt)
      {if (olddata) memcpy(olddata, ITEM_VAL(theItem), shmTypeSz);
       if (!replace) {errno = EEXIST; return false;}
       if (reUse)
          {memcpy(ITEM_VAL(theItem), newdata, shmTypeSz);
           if (syncOn) Updated(ITEM_VOF(theItem), shmTypeSz);
           errno = EEXIST;
           return true;
          }
       retEno = EEXIST;
      }

// Get a new item
//
   if (!(newItem = NewItem())) {errno = ENOSPC; return false;}

// Construct the new item
//
   newItem->hash = hash;
   memcpy(ITEM_VAL(newItem), newdata, shmTypeSz);
   strcpy(ITEM_KEY(newItem), key);

// If we are replacing an item then We need to bridge over the item we are
// replacing in a way that doesn't make the item disappear for other readers.
// Otherwise, we can patch in the new item either on the last item in the chain
// or directly off the table. Note that releasing the lock creates a memory
// fence. To understand why this this works consider the relationship between:
// hEnt prvItem The state of the table
//    0       0 Not found because index table slot is zero
//    0      !0 Not found in a chain of items, prvItem is the last one
//   !0       0 Was found and is the first or only item in the chain
//   !0      !0 Was found and is in the middle or end of the chain
//
//
   if (hEnt) Atomic_SET(newItem->next, theItem->next);  // Atomic
      else {hEnt = (unsigned int)hash % shmSlots;
            if (hEnt == 0) hEnt = 1;
            SHMINFO(itemCount)++;
           }

   iOff = SHMOFFS(newItem);
   if (prvItem) Atomic_SET_STRICT(prvItem->next, iOff); // Atomic
      else {SHMINFO(slotsUsed)++;
                Atomic_SET_STRICT(shmIndex[hEnt],iOff); // Atomic
            if (syncOn) Updated(SHMOFFS(&shmIndex[hEnt]));
           }

// Indicate which things we changed if we have syncing
//
   if (syncOn)
      {Updated(0);
       Updated(SHMOFFS(newItem));
       if (prvItem) Updated(SHMOFFS(prvItem));
      }

// All done, return result
//
   errno = retEno;
   return true;
}

/******************************************************************************/
/*                                A t t a c h                                 */
/******************************************************************************/
  
bool XrdSsiShMam::Attach(int tout, bool isrw)
{
   FileHelper  fileHelp(this);
   XLockHelper lockInfo(this, (isrw ? RWLock : ROLock));
   struct stat Stat1, Stat2;
   int mMode, oMode;
   union {int *intP; Atomic(int) *antP;} xntP;

// Compute open and mmap options
//
   if (isrw)
      {oMode = O_RDWR;
       mMode = PROT_READ|PROT_WRITE;
       isRW  = true;
      } else {
       oMode = O_RDONLY;
       mMode = PROT_READ;
       isRW  = false;
      }

// Attempt to open the file
//
   timeOut = tout;
   if (tout < 0) tout = 0x7fffffff;
   while((shmFD = ShMam_Open(shmPath, oMode)) < 0 && tout >= 0)
        {if (errno != ENOENT) return false;
         if (!tout) break;
         Snooze(3);
         tout -= 3;
        }

// Test if we timed out
//
   if (tout <= 0) {errno = ETIMEDOUT; return false;}
   fileHelp.autoClose = true;

// Lock this file as we don't want it changing on us for now
//
   if (!lockInfo.FLock()) return false;

// Get the stat information for this file
//
   if (fstat(shmFD, &Stat1)) return false;

// The file is open, try to memory map it
//
   shmBase = (char *)mmap(0, Stat1.st_size, mMode, MAP_SHARED, shmFD, 0);
   if (shmBase == MAP_FAILED) return false;
   shmSize = Stat1.st_size;

// Make sure we have a valid hash name
//
   if (!shmHash) memcpy(&shmHash, "c32 ", sizeof(int));

// Verify tha the objects in this mapping are compatible with this object
//
   if (SHMINFO(typeSz) != shmTypeSz    || strcmp(shmType, SHMINFO(typeID))
   || strcmp(shmImpl, SHMINFO(myName)) || shmHash != SHMINFO(hashID))
      {errno = EDOM; return false;}

// Copy out the information we can use locally
//
   verNum     = SHMINFO(verNum);
   keyPos     = SHMINFO(keyPos);
   maxKLen    = SHMINFO(maxKeySz);
   xntP.intP  = SHMADDR(int, SHMINFO(index)); shmIndex = xntP.antP;
   shmSlots   = SHMINFO(slots);
   shmItemSz  = SHMINFO(itemSz);
   shmInfoSz  = SHMINFO(infoSz);

// Now, there is a loophole here as the file could have been exported while
// we were trying to attach it. If this happened, the inode would change.
// We test for this now. If it changed, tell the caller to try again.
//
   if (stat(shmPath, &Stat2)
   ||  Stat1.st_dev != Stat2.st_dev || Stat1.st_ino != Stat2.st_ino)
      {errno = EAGAIN; return false;}
   accMode = Stat2.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO);

// Set locking based on how the table was created
//
   SetLocking(isrw);
   fileHelp.autoClose = false;
   return true;
}

/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/
  
bool XrdSsiShMam::Create(XrdSsiShMat::CRZParms &parms)
{
   static const int minInfoSz = 256;
   static const int okMode = S_IRWXU|S_IRWXG|S_IROTH;
   static const int crMode = S_IRWXU|S_IRWXG|S_IROTH;
   FileHelper fileHelp(this);
   ShmInfo theInfo;
   int n, maxEnts, totSz, indexSz;
   union {int *intP; Atomic(int) *antP;} xntP;

// Validate parameter list values
//
   if (parms.indexSz <= 0  || parms.maxKeys <= 0 || parms.maxKLen <= 0)
      {errno = EINVAL; return false;}
   if (parms.mode & ~okMode || ((parms.mode & crMode) != crMode))
      {errno = EACCES; return false;}

// We need the reuse and multw options later so calclulate them now
//
   reUse = (parms.reUse <= 0 ? false : true);
   multW = (parms.multW <= 0 ? false : true);
  
// Clear the memory segment information we will be constructing
//
   memset(&theInfo, 0, sizeof(theInfo));

// Calculate the info header size (we round up to 1K)
//
   shmInfoSz = (sizeof(ShmInfo)+minInfoSz-1)/minInfoSz*minInfoSz;
   theInfo.lowFree = theInfo.infoSz = shmInfoSz;

// Calculate the size of each item (rounded to a doubleword)
//
   shmItemSz      = (shmTypeSz + parms.maxKLen+1 + sizeof(MemItem) + 7)/8*8;
   theInfo.itemSz = shmItemSz;

// Calculate total amount we need for the items
//
   maxEnts = parms.maxKeys;
   totSz   = shmItemSz * maxEnts;
   totSz   = (totSz+7)/8*8;

// Calculate the amount we need for the index
//
   indexSz = parms.indexSz*sizeof(int);
   indexSz = (indexSz+7)/8*8;

// Compute total size and adjust it to be a multiple of the page size
//
   totSz = totSz + indexSz + shmInfoSz;
   totSz = (totSz/PageSize+1)*PageSize;

// Generate the hashID if not specified
//
   if (!shmHash) memcpy(&shmHash, "c32 ", sizeof(int));

// Complete the shared memory segment information structure
//
   theInfo.index    = totSz-indexSz;
   theInfo.slots    = parms.indexSz;
   theInfo.typeSz   = shmTypeSz;
   theInfo.highUse  = theInfo.index;
   theInfo.reUse    = reUse;
   theInfo.multW    = multW;
   theInfo.keyPos   = keyPos = shmTypeSz + sizeof(MemItem);
   theInfo.maxKeys  = maxEnts;
   theInfo.maxKeySz = maxKLen = parms.maxKLen;
   theInfo.hashID   = shmHash;
   strncpy(theInfo.typeID, shmType, sizeof(theInfo.typeID)-1);
   strncpy(theInfo.myName, shmImpl, sizeof(theInfo.myName)-1);

// Create the new filename of the new file we will create
//
   n = strlen(shmPath);
   shmTemp = (char *)malloc(n+8);
   sprintf(shmTemp, "%s.new", shmPath);

// Open the file creaing as necessary
//
   if ((shmFD = ShMam_Open(shmTemp, O_RDWR|O_CREAT, parms.mode)) < 0)
      return false;
   accMode = parms.mode;
   fileHelp.autoClose = true;

// Verify that no one else is using this file.
//
   if (!Lock(true, true)) {errno = EADDRINUSE; return false;}

// Make the file as large as need be
//
   if (ftruncate(shmFD, 0) || ftruncate(shmFD, totSz)) return false;

// Map the file as a writable shared segment
//
   shmBase = (char *)mmap(0, totSz, PROT_READ|PROT_WRITE, MAP_SHARED, shmFD, 0);
   if (shmBase == MAP_FAILED) return false;
   shmSize = totSz;
   isRW = true;

// Copy the segment information into the segment
//
   memcpy(shmBase, &theInfo, sizeof(theInfo));
   xntP.intP  = SHMADDR(int, SHMINFO(index)); shmIndex = xntP.antP;
   shmSlots = parms.indexSz;

// A created table has, by definition, a single writer until it is exported.
// So, we simply keep the r/w lock on the file until we export the file. Other
// threads won't change that and other process will not be able to use the file.
//
   lockRO  = lockRW = false;
   fileHelp.autoClose = false;
   return true;
}

/******************************************************************************/
/*                               D e l I t e m                                */
/******************************************************************************/
  
bool XrdSsiShMam::DelItem(void *data, const char *key, int hash)
{
   XLockHelper lockInfo(this, RWLock);
   MemItem  *theItem, *prvItem;
   int hEnt, iOff;

// Make sure we can delete an item
//
   if (!shmSize) {errno = ENOTCONN;    return false;}
   if (!isRW)    {errno = EROFS;       return false;}

// Check if we need to remap this memory (atomic tests is not needed here)
//
   if (verNum != SHMINFO(verNum)) ReMap(RWLock);

// Lock the file if we have multiple writers or recycling items
// We need to do this prior to file locking as the requirements may change.
//
   if (lockRW && !lockInfo.FLock()) return false;

// First try to find the item
//
   if (!(hEnt = Find(theItem, prvItem, key, hash)))
      {if (data) {errno = ENOENT; return false;}
       return true;
      }

// Return the contents of the item if the caller wishes that
//
   if (data) memcpy(data, ITEM_VAL(theItem), shmTypeSz);

// Delete the item from the index. The update of the count need not be atomic.
// Also fetching of the next offset need not be atomic as we are the only one.
//
   iOff = theItem->next;
   SHMINFO(itemCount)--;
   if (prvItem)  Atomic_SET_STRICT(prvItem->next, iOff); // Atomic
      else {if (!iOff) SHMINFO(slotsUsed)--;
                 Atomic_SET_STRICT(shmIndex[hEnt],iOff); // Atomic
           }
   RetItem(theItem);

// Indicate the things we updated if need be
//
   if (syncOn)
      {Updated(0);
       Updated(SHMOFFS(theItem));
       if (prvItem) Updated(SHMOFFS(prvItem));
          else      Updated(SHMOFFS(&shmIndex[hEnt]));
      }

// All done
//
   return true;
}

/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/
  
void XrdSsiShMam::Detach()
{
// Clean up
//
   if (shmFD >= 0) {close(shmFD);  shmFD = -1;}
   if (shmSize)    {munmap(shmBase, shmSize); shmSize = 0;}
   if (shmTemp)    {free(shmTemp); shmTemp = 0;}
   shmIndex = 0;
}

/******************************************************************************/
/*                             E n u m e r a t e                              */
/******************************************************************************/

bool XrdSsiShMam::Enumerate(void *&jar)
{
   EnumJar  *theJar  = (EnumJar *)jar;

// Close off the enumeration
//
   if (theJar) {delete theJar; jar = 0;}
   return true;
}

/******************************************************************************/

bool XrdSsiShMam::Enumerate(void *&jar, char *&key, void *&val)
{
   XLockHelper lockInfo(this, ROLock);
   EnumJar  *theJar  = (EnumJar *)jar;
   MemItem  *theItem;
   long long iTest;
   int rc, newFD, fence, iOff, hash = 0;

// Make sure we can get an item
//
   if (!shmSize) {errno = ENOTCONN; return false;}

// If this is the first call, initialize the jar. First check if we need to
// remap the segment. We need to do this prior to file locking as the
// requirements may change. Then create a jar and a shadow copy of the segment.
//
   if (!jar)
      {if (verNum != SHMINFO(verNum)) ReMap(ROLock);
       if ((newFD = ShMam_Dup(shmFD)) < 0) return false;
       theJar = new EnumJar(newFD, shmItemSz);
       jar = theJar;
      } else if (theJar->iNum < 0)
                {Enumerate(jar);
                 errno = ENOENT;
                 return false;
                }

// Lock the file if we have multiple writers or recycling items
//
   if (lockRO && !lockInfo.FLock())
      {rc = errno; Enumerate(jar); errno = rc; return false;}

// Compute the next key we should start the search at but make sure it will not
// generate an overflow. In the process we fetch the stopping point only once.
//
   iTest = (static_cast<long long>(theJar->iNum) * shmItemSz) + shmInfoSz;
   fence = SHMINFO(lowFree); // Atomic??
   if (iTest < fence) iOff = static_cast<int>(iTest);
      else iOff = fence;

// Now start the search. Note that pread() must do a memory fence.
//
   theItem = (MemItem *)(theJar->buff);
   while(iOff < fence)
      {rc = pread(theJar->fd, theJar->buff, shmItemSz, iOff);
       if (rc < 0) return false;
       if (rc != shmItemSz) break;
       if ((hash = theItem->hash)) break; // Atomic
       iOff += shmItemSz;
      }

// Check if we found a key
//
   if (!hash) {Enumerate(jar); errno = ENOENT; return false;}

// Return the key and and the associated value
//
   key = ITEM_KEY(theItem);
   val = ITEM_VAL(theItem);

// Compute the contents of the new jar
//
   theJar->iNum = (iOff - shmInfoSz)/shmItemSz + 1;
   return true;
}

/******************************************************************************/
/*                                E x p o r t                                 */
/******************************************************************************/
  
bool XrdSsiShMam::Export()
{
   MutexHelper mtHelp(&myMutex, RWLock);

// Make sure we are attached and in R/W mode and exportable
//
   if (!shmSize) {errno = ENOTCONN;    return false;}
   if (!shmTemp) {errno = ENOPROTOOPT; return false;}
   if (!isRW)    {errno = EROFS;       return false;}

// All that is left is to export the file using the internal interface. Tell
// the exporter that we don't have the original file locked.
//
   return ExportIt(false);
}

/******************************************************************************/
/* Private:                     E x p o r t I t                               */
/******************************************************************************/
  
bool XrdSsiShMam::ExportIt(bool fLocked)
{
   int rc, oldFD;

// If data synchronization was wanted, then flush the modified pages to
// disk before we make this file visible.
//
   if (syncOn) Flush();

// Open the original file. If it exists then lock it. We will need to do this
// locally as the the Lock/Unlock() functions are cognizant of threads and that
// is not the case here. We are a singleton.
//
   if ((oldFD = ShMam_Open(shmPath, O_RDWR)) < 0)
      {if (errno != ENOENT) return false;}
      else if (!fLocked)
              {do {rc = flock(oldFD, LOCK_EX);} while(rc < 0 && errno == EINTR);
               if (rc) return false;
              }

// Rename the new file on top of the old one (the fd's remain in tact)
//
   if (rename(shmTemp, shmPath)) {if (oldFD) close(oldFD); return false;}
   free(shmTemp); shmTemp = 0;

// If there was an original file then we must indicate that a new vesion has
// been exported so current users switch to the new version. This is a lazy
// version update because we just need readers to eventually realize this.
//
   if (oldFD >= 0)
      {int vnum; bool noGo = false;
       if (pread(oldFD, &vnum, sizeof(vnum), 0) == (ssize_t)sizeof(vnum))
          {vnum++;
           if (pwrite(oldFD, &vnum, sizeof(vnum), 0) != (ssize_t)sizeof(vnum))
              noGo = true;
          } else noGo = true;
       if (noGo) std::cerr <<"SsiShMam: Unable to update version for " <<shmPath
                      <<"; " <<XrdSysE2T(errno) <<std::endl;
       close(oldFD);
      }

// We are done. However, before we return make sure the locking requirements
// are set to reflect a global view as an unexported file had relaxed locking
// requirements. The close unlocked the original file and now we must unlock
// our file as create kept the file lock until the export.
//
   SetLocking(true);
   UnLock(true);
   return true;
}

/******************************************************************************/
/* Private:                         F i n d                                   */
/******************************************************************************/
  
int XrdSsiShMam::Find(XrdSsiShMam::MemItem  *&theItem,
                      XrdSsiShMam::MemItem  *&prvItem,
                      const char *key, int   &hash)
{
   int hEnt, iOff;

// If no hash was supplied, get one
//
   if (!hash) hash = HashVal(key);

// Compute index table entry and atomically fetch the entry
//
   hEnt = (unsigned int)hash % shmSlots;
   if (hEnt == 0) hEnt = 1;
   iOff = Atomic_GET_STRICT(shmIndex[hEnt]); // Atomic?

// Find the item
//
   prvItem = 0;
   while(iOff)
      {theItem = SHMADDR(MemItem, iOff);
       if (hash == theItem->hash && !strcmp(key, ITEM_KEY(theItem)))
          return hEnt;
       prvItem = theItem;
       iOff = Atomic_GET_STRICT(theItem->next); // Atomic?
      }

// We did not find the item
//
   return 0;
}

/******************************************************************************/
/* Private:                        F l u s h                                  */
/******************************************************************************/

bool XrdSsiShMam::Flush()
{
   int rc;

// Do appropriate sync
//
#if  _POSIX_SYNCHRONIZED_IO > 0
   rc = fdatasync(shmFD) == 0;
#else
   rc = fsync(shmFD) == 0;
#endif

// If we failed, issue message
//
   if (rc)
      {rc = errno;
       std::cerr <<"ShMam: msync() failed; " <<XrdSysE2T(errno) <<std::endl;
       errno = rc; rc = -1;
      }

// Return result
//
   return rc == 0;
}
  
/******************************************************************************/
/*                               G e t I t e m                                */
/******************************************************************************/
  
bool XrdSsiShMam::GetItem(void *data, const char *key, int hash)
{
   XLockHelper lockInfo(this, ROLock);
   MemItem  *theItem, *prvItem;
   int hEnt;

// Make sure we can get an item
//
   if (!shmSize) {errno = ENOTCONN;    return false;}

// Check if we need to remap this memory (atomic tests is not needed here)
// We need to do this prior to file locking as the requirements may change.
//
   if (verNum != SHMINFO(verNum)) ReMap(ROLock);

// Lock the file if we have multiple writers or recycling items
//
   if (lockRO && !lockInfo.FLock()) return false;

// First try to find the item
//
   if (!(hEnt = Find(theItem, prvItem, key, hash)))
      {errno = ENOENT; return false;}

// Return the contents of the item if the caller wishes that
//
   if (data) memcpy(data, ITEM_VAL(theItem), shmTypeSz);

// All done
//
   return true;
}

/******************************************************************************/
/* Private:                      H a s h V a l                                */
/******************************************************************************/
  
int XrdSsiShMam::HashVal(const char *key)
{
   uLong crc;
   int hval, klen = strlen(key);

// Get initial crc value
//
   crc = crc32(0L, Z_NULL, 0);

// Compute the hash
//
   crc   = crc32(crc, (const Bytef *)key, klen);

// Cast it to an int (it's weird that zlib want to use a long for this). If the
// vaue is zero make it 1 as we need to use zero as a missing value.
//
   hval = static_cast<int>(crc);
   return (hval ? hval : 1);
}

/******************************************************************************/
/* Private:                         L o c k                                   */
/******************************************************************************/

// The caller must have obtained a mutex consistent with the argument passed.
  
bool XrdSsiShMam::Lock(bool xrw, bool nowait)
{
   int rc, act = (xrw ? LOCK_EX : LOCK_SH);

// Make sure we have a file descriptor to lock and is not already locked
//
   if (shmFD < 0) {errno = EBADF;   return false;}

// We must keep track of r/o locks as there may be many requests but we can
// only lock the file once for all of them. R/W locks are easier to handle as
// only one thread can ever have such a lock request. Atomics do not help
// for R/O locks because they suffer from an unlock control race and also
// all R/O requestors must wait if the file is locked by another process.
//
   if (xrw) lkCount = 1;
      else {pthread_mutex_lock(&lkMutex);
            if (lkCount++) {pthread_mutex_unlock(&lkMutex); return true;}
           }

// Check if we should not wait for the lock
//
   if (nowait) act |= LOCK_NB;

// Now obtain the lock
//
   do {rc = flock(shmFD, act);} while(rc < 0 && errno == EINTR);

// Decrement lock count if we failed (we were optimistic). Note that we still
// have the mutex locked if this was a T/O request.
//
   if (rc) {if (xrw) lkCount = 0;
               else  lkCount--;
           }

// Unlock the mutex if we still have it locked and return result
//
   if (!xrw) pthread_mutex_unlock(&lkMutex);
   return rc == 0;
}

/******************************************************************************/
/*                                  I n f o                                   */
/******************************************************************************/
  
int  XrdSsiShMam::Info(const char *vname, char *buff, int blen)
{
   MutexHelper mtHelp(&myMutex, ROLock);

// Make sure we can delete an item
//
   if (!shmSize) {errno = ENOTCONN; return 0;}

   if (!strcmp(vname, "atomics"))
      {int n = strlen(Atomic_IMP);
       strcpy(buff, Atomic_IMP);
       return n;
      }

   if (!strcmp(vname, "hash"))
      {if (!buff || blen < (int)(sizeof(int)+1)) {errno = EMSGSIZE; return -1;}
       memcpy(buff,  &SHMINFO(hashID), sizeof(int)); buff[sizeof(int)] = 0;
       return strlen(buff);
      }
   if (!strcmp(vname, "impl"))
      {int n = strlen(SHMINFO(myName));
       if (!buff || blen < n) {errno = EMSGSIZE; return -1;}
       strcpy(buff, SHMINFO(myName));
       return n;
      }
   if (!strcmp(vname, "flockro"))   return lockRO;
   if (!strcmp(vname, "flockrw"))   return lockRW;
   if (!strcmp(vname, "indexsz"))   return shmSlots;
   if (!strcmp(vname, "indexused")) return SHMINFO(slotsUsed);
   if (!strcmp(vname, "keys"))      return SHMINFO(itemCount); // Atomic
   if (!strcmp(vname, "keysfree"))
       return (SHMINFO(highUse) - SHMINFO(lowFree))/shmItemSz
              + SHMINFO(freeCount);
   if (!strcmp(vname, "maxkeylen")) return SHMINFO(maxKeySz);
   if (!strcmp(vname, "multw"))     return multW;
   if (!strcmp(vname, "reuse"))     return reUse;
   if (!strcmp(vname, "type"))
      {int n = strlen(SHMINFO(typeID));
       if (!buff || blen < n) {errno = EMSGSIZE; return -1;}
       strcpy(buff, SHMINFO(typeID));
       return n;
      }
   if (!strcmp(vname, "typesz"))    return SHMINFO(typeSz);

// Return variable not supported
//
   errno = ENOTSUP;
   return -1;
}

/******************************************************************************/
/* Private:                      N e w I t e m                                */
/******************************************************************************/
  
XrdSsiShMam::MemItem *XrdSsiShMam::NewItem()
{
   MemItem *itemP;
   int iOff;

// First see if we can get this from the free chain
//
   if (reUse && SHMINFO(freeItem))
      {iOff  = SHMINFO(freeItem);
       itemP = SHMADDR(MemItem, iOff);
       SHMINFO(freeItem) = itemP->next;
       SHMINFO(freeCount)--; // Atomic?
      } else {
       int newFree = SHMINFO(lowFree) + shmItemSz;
       if (newFree > SHMINFO(highUse)) itemP = 0;
          else {iOff  = SHMINFO(lowFree);
                itemP = SHMADDR(MemItem, iOff);
                SHMINFO(lowFree) = newFree;
               }
      }

// Return result
//
   return itemP;
}
  
/******************************************************************************/
/* Private:                        R e M a p                                  */
/******************************************************************************/
  
bool XrdSsiShMam::ReMap(LockType iHave)
{
   XrdSsiShMat::NewParms parms;

// If the caller has a read mutex then we must change it to a r/w mutex as we
// may be changing all sorts of variables. It will continue holding this mutex.
// Fortunately, remappings do not occur very often in practice.
//
   if (iHave == ROLock)
      {pthread_rwlock_unlock(&myMutex);
       pthread_rwlock_wrlock(&myMutex);
      }

// Check if the version number no longer differs, then just return. This may
// happen because a previous thread forced the remapping and everyone was
// waiting for that to happen as we hold the r/w mutex.
//
   if (verNum == SHMINFO(verNum)) return false;

// Setup parms
//
   parms.impl   = shmImpl;
   parms.path   = shmPath;
   parms.typeID = shmType;
   parms.typeSz = shmTypeSz;
   parms.hashID = shmHash;

// Attach the new segment. If we fail, then just continue
//
   XrdSsiShMam newMap(parms);
   if (!newMap.Attach(timeOut, isRW)) return false;

// Swap the new map with our map
//
   SwapMap(newMap);
   return true;
}

/******************************************************************************/
/*                                R e s i z e                                 */
/******************************************************************************/

bool XrdSsiShMam::Resize(XrdSsiShMat::CRZParms &parms)
{
   XLockHelper lockInfo(this, RWLock);
   XrdSsiShMat::NewParms newParms;
   MemItem  *theItem;
   void     *val;
   char     *key;
   int       fence, iOff, hash;

// Make sure we can delete an item
//
   if (!shmSize) {errno = ENOTCONN;    return false;}
   if (!isRW)    {errno = EROFS;       return false;}

// Validate parameter list values
//
   if (parms.indexSz < 0  || parms.maxKeys < 0 || parms.maxKLen < 0)
      {errno = EINVAL; return false;}

// A resize is not permitted on an un-exported segment
//
   if (shmTemp) {errno = EPERM; return false;}

// Check if we need to remap this memory (atomic tests is not needed here)
//
   if (verNum != SHMINFO(verNum)) ReMap(RWLock);

// Lock the source file
//
   if (!lockInfo.FLock()) return false;

// Setup parms for the segment object
//
   newParms.impl   = shmImpl;
   newParms.path   = shmPath;
   newParms.typeID = shmType;
   newParms.typeSz = shmTypeSz;
   newParms.hashID = shmHash;

// Create a new segment object (this cannot fail).
//
   XrdSsiShMam newMap(newParms);

// Set the values in the parameter list for those wanting the current setting.
//
   if (!parms.indexSz)  parms.indexSz = shmSlots;
   if (!parms.maxKeys)  parms.maxKeys = SHMINFO(maxKeys);
   if (!parms.maxKLen)  parms.maxKLen = maxKLen;
   if (parms.reUse < 0) parms.reUse   = reUse;
   if (parms.multW < 0) parms.multW   = multW;

// Create the new target file
//
   parms.mode = accMode;
   if (!newMap.Create(parms)) return false;

// Compute the offset of the first item and get the offset of the last item.
//
   fence = SHMINFO(lowFree); // Atomic??
   iOff  = shmInfoSz;

// For each item found in the current map add it to the new map
//
   while(iOff < fence)
      {theItem = SHMADDR(MemItem, iOff);
       if ((hash = theItem->hash))
          {key = ITEM_KEY(theItem);
           val = ITEM_VAL(theItem);
           if (!newMap.AddItem(val, 0, key, hash, true)) return false;
          }
       iOff += shmItemSz;
      }

// We need to drop the lock on the file otherwise the export will hang
//

// All went well, so export this the new map using the internal interface as
// we already have the source file locked and export normally tries to lock it.
//
   if (!newMap.ExportIt(true)) return false;

// All that we need to do is to swap the map with our map and we are done.
//
   SwapMap(newMap);
   return true;
}

/******************************************************************************/
/* Private:                      R e t I t e m                                */
/******************************************************************************/
  
void XrdSsiShMam::RetItem(MemItem *iP)
{

// Zorch the hash so this item cannot be found. This is problematic for
// enumerations. They may or may not include this key, but at least it will
// consistent at the time the enumeration happens.
//
   iP->hash = 0;      // Atomic?

// If reuse is allowed, place the item on the free list
//
   if (reUse)
      {iP->next = SHMINFO(freeItem);
       SHMINFO(freeItem) = SHMOFFS(iP);
       SHMINFO(freeCount)++; //Atomic??
      }
}

/******************************************************************************/
/* Private:                   S e t L o c k i n g                             */
/******************************************************************************/

void XrdSsiShMam::SetLocking(bool isrw)
{
   (void)isrw;

// If we do not have atomics then file locking is mandatory
//
#ifdef NEED_ATOMIC_MUTEX
   lockRO   = lockRW = true;
#else
// A reader must lock the file R/O if objects are being reused
//
   lockRO   = reUse =  SHMINFO(reUse);

// A writer must lock the file R/W if objects are being reused or the file may
// have multiple writers
//
   multW  = SHMINFO(multW);
   lockRW = reUse || multW;
#endif
}
  
/******************************************************************************/
/*                                S n o o z e                                 */
/******************************************************************************/

void XrdSsiShMam::Snooze(int sec)
{
 struct timespec naptime, waketime;

// Calculate nano sleep time
//
   naptime.tv_sec  =  sec;
   naptime.tv_nsec =  0;

// Wait for a number of seconds
//
   while(nanosleep(&naptime, &waketime) && EINTR == errno)
        {naptime.tv_sec  =  waketime.tv_sec;
         naptime.tv_nsec =  waketime.tv_nsec;
        }
}
  
/******************************************************************************/
/* Private:                      S w a p M a p                                */
/******************************************************************************/
  
void XrdSsiShMam::SwapMap(XrdSsiShMam &newMap)
{

// Detach the old map
//
   Detach();

// Swap the maps
//
   shmFD           = newMap.shmFD;
   newMap.shmFD    = -1;
   shmSize         = newMap.shmSize;
   newMap.shmSize  =  0;
   shmBase         = newMap.shmBase;
   newMap.shmBase  =  0;
   shmIndex        = newMap.shmIndex;
   newMap.shmIndex =  0;
   lockRO          = newMap.lockRO;
   lockRW          = newMap.lockRW;
   reUse           = newMap.reUse;
   multW           = newMap.multW;
   verNum          = newMap.verNum;
}

/******************************************************************************/
/*                                  S y n c                                   */
/******************************************************************************/

bool XrdSsiShMam::Sync()
{
   MutexHelper mtHelp(&myMutex, RWLock);

// Make sure we are attached and in R/W mode
//
   if (!shmSize) {errno = ENOTCONN;    return false;}
   if (!isRW)    {errno = EROFS;       return false;}

// For now do a flush as this works in Linux. We may need to generalize this
// for all platforms using msync, sigh.
//
   if (!Flush()) return false;

// Reset counters
//
   syncBase = false;
   syncLast = 0;
   syncQWR  = 0;
   return true;
}

/******************************************************************************/

bool XrdSsiShMam::Sync(int syncqsz)
{
   MutexHelper mtHelp(&myMutex, RWLock);

// Make sure we are attached and in R/W mode
//
   if (!shmSize)    {errno = ENOTCONN;    return false;}
   if (!isRW)       {errno = EROFS;       return false;}
   if (syncqsz < 0) {errno = EINVAL;      return false;}

// Flush out pages if sync it turned on
//
   if (syncOn && !Flush()) return false;

// Set new queue size
//
   syncQSZ = syncqsz;
   return true;
}

/******************************************************************************/

bool XrdSsiShMam::Sync(bool dosync, bool syncdo)
{
   MutexHelper mtHelp(&myMutex, RWLock);

// Make sure we are attached and in R/W mode
//
   if (!shmSize)    {errno = ENOTCONN;    return false;}
   if (!isRW)       {errno = EROFS;       return false;}

// Flush out pages if sync it turned on
//
   if (syncOn && !Flush()) return false;

// Set new options
//
   syncOn  = dosync;
   syncOpt = (syncdo ? MS_SYNC : MS_ASYNC);
   return true;
}

/******************************************************************************/
/* Private:                       U n L o c k                                 */
/******************************************************************************/

// The caller must have obtained a mutex consistent with the argument passed.
  
void XrdSsiShMam::UnLock(bool isrw)
{
   int rc;

// Make sure we have a file descriptor to unlock
//
   if (shmFD < 0) return;

// If this is a R/W type of lock then we can immediate release it as there
// could have been only one writer. Otherwise, we will need to keep track
// of the number of R/O locks has dropped to zero before unlocking the file.
// Atomics do not help here because of possible thread inversion.
//
   if (isrw) lkCount = 0;
      else {pthread_mutex_lock(&lkMutex);
            lkCount--;
            if (lkCount) {pthread_mutex_unlock(&lkMutex); return;}
           }

// Now release the lock
//
   do {rc = flock(shmFD, LOCK_UN);} while(rc < 0 && errno == EINTR);

// If this was a r/o unlock then we have kept the mutex and must unlock it
// We kept the mutex to prevent a control race condition.
//
   if (!isrw) pthread_mutex_unlock(&lkMutex);
}

/******************************************************************************/
/* Private:                      U p d a t e d                                */
/******************************************************************************/

void XrdSsiShMam::Updated(int mOff)
{
// Check if this refers to the info struct
//
   if (!mOff)
      {if (!syncBase) {syncBase = true; syncQWR++;}
      } else {
       if (syncLast != (mOff & PageMask))
          {syncLast  = (mOff & PageMask); syncQWR++;}
      }

// Check if we need to flush now
//
   if (syncQWR >= syncQSZ) {ShMam_Flush(shmFD); syncQWR = 0;}
}

/******************************************************************************/

void XrdSsiShMam::Updated(int mOff, int  mLen)
{
    int memB = mOff & PageMask;
    int memE = mOff + mLen;

// This is a range update. This is not very precise if update the same page
// and the we cross the page boundary. But it should be good enough.
//
    if (memB != syncLast)
       {syncQWR++;
        if (memB != (memE & PageMask)) syncQWR++;
        syncLast = memB;
       }

// Check if we need to flush now
//
   if (syncQWR >= syncQSZ) {ShMam_Flush(shmFD); syncQWR = 0;}
}
