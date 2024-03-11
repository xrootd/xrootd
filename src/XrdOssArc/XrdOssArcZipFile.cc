/******************************************************************************/
/*                                                                            */
/*                   X r d O s s A r c Z i p F i l e . h h                    */
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <zip.h>

#include "XrdOss/XrdOss.hh"
#include "XrdOssArc/XrdOssArcZipFile.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

namespace XrdOssArcGlobals
{
extern XrdSysError     Elog;
}
using namespace XrdOssArcGlobals;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOssArcZipFile::XrdOssArcZipFile(XrdOssDF& df, const char* path, int &rc)
                                  : ossDF(df)
{
   XrdOucEnv zipEnv;
   int zFD, zrc;

// Try to open the file. We only support read mode. We need to get a duplicate
// file descriptor as attaching the FD to a zipfile destroys the original FD.
//
   if ((rc = ossDF.Open(path, O_RDONLY, 0, zipEnv)) < 0) return;
   if ((zFD = ossDF.getFD()) < 0) {rc = -ENOTBLK; return;}
   if ((zFD = XrdSysFD_Dup(zFD)) < 0) 
      {rc = errno;
       Elog.Emsg("ZipFile", rc, "dup FD for", path);
       return;
      }

// Record path
//
   zPath = strdup(path);

// Convert open to archive open
//
   if ((zFile = zip_fdopen(zFD, ZIP_CHECKCONS, &zrc)) == 0)
      {rc = zip2syserr("fdopen", zrc);
       return;
      }
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOssArcZipFile::~XrdOssArcZipFile()
{
// If we have an open subfile, close it
//
   if (zSubFile) Close();

// Close the archive itself
//
   if (zFile)
      {if (zip_close(zFile))
          {zipEmsg("close", zip_get_error(zFile));
           zip_discard(zFile);
          }
       zFile = 0;
      }

// Free up any storage
//
   if (zPath) free(zPath);
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/
  
int XrdOssArcZipFile::Close()
{
   int zrc = 0;

// Close the member subfile if it is open
//
   if (zSubFile)
      {if ((zrc = zip_fclose(zSubFile))) zrc = zip2syserr("close member", zrc);
       zSubFile = 0;
      }

// Close the underlying file
//
   ossDF.Close();

// Remove all vestigaes of this subfile
//
   if (zMember) free(zMember);

// All done
//
   return zrc;
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int XrdOssArcZipFile::Open(const char* member)
{
   int rc; 

// Make sure we have an open archive here
//
   if (zFile == 0) return -EBADF;

// If an archive member is alreaddy open then close it
//
   if (zSubFile)
      {if ((rc = zip_fclose(zSubFile))) {} //???
       free(zMember);
       zMember  = 0;
       zSubFile = 0;
      }

// Set member name we are handling
//
   if (zMember) free(zMember);
   zMember = strdup(member);

// Open the archive member
//
   zSubFile = zip_fopen(zFile, zMember, 0);   
   if (zSubFile == 0) return zip2syserr("open", zip_get_error(zFile));

// We should check if this is a compressed archive as ther can onl be read
// sequentially. However, that is not supported until v 10.1 and the current
// rpms available at 1.7. So, we punt on this and assume it not compressed and
// is  seekable. Note that compressed files are not seekable.
//
// zSeek = zip_file_is_seekable(zSubFile) == 1;
   zSeek = true;
   zOffset = 0;

// All done
//
   return 0;
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

ssize_t XrdOssArcZipFile::Read(void *buff, off_t offset, size_t blen)
{
// Make sure this file is actually open
//
   if (zSubFile == 0) return -EBADF;
   if (!blen) return 0;

// If this file does not support seeks, return a seek error if a seek wanted
//
   if (offset != zOffset)
      {if (!zSeek) return -ESPIPE;
       if (zip_fseek(zSubFile, offset, SEEK_SET))
          return zip2syserr("seek into", zip_file_get_error(zSubFile));
       zEOF = false;
      }

// Check if we have reached EOF
//
   if (zEOF) return 0;

// Perform the read
//
   zip_int64_t ret = zip_fread(zSubFile, buff, blen);  
   if (ret < 0)
      return zip2syserr("read", zip_file_get_error(zSubFile));

// Update offset and check for EOF
//
   zOffset += ret;
   if (ret < (zip_int64_t)blen) zEOF = true;
   return ret;
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/

int XrdOssArcZipFile::Stat(struct stat& buf)
{
   zip_stat_t zStat;
   int rc;

// Make sure this file is actually open
//
   if (zSubFile == 0) return -EBADF;

// Get the stat of the base file
//
  if ((rc = ossDF.Fstat(&buf))) return rc;

// Clear the stat structures
//
   zip_stat_init(&zStat);

// Get information
//
   if (zip_stat(zFile, zMember, 0, &zStat) < 0)
      return zip2syserr("close", zip_get_error(zFile));

// Copy the relevant information
//
   if (zStat.valid & ZIP_STAT_INDEX) buf.st_ino  = zStat.index;
   if (zStat.valid & ZIP_STAT_SIZE)  buf.st_size = zStat.size;

// All done
//
   return 0;
}

/******************************************************************************/
/*                               z i p E m s g                                */
/******************************************************************************/
  
void XrdOssArcZipFile::zipEmsg(const char *what, zip_error_t* zerr)
{
   XrdOucString target(zPath, 280);

// Create target name
//
   target += '[';
   if (zMember) target += zMember;
   target += "];";

// Create starting error message
//
   char eText[80];
   snprintf(eText, sizeof(eText), "unable to %s", what);

// Issue message
//
   Elog.Emsg("ZipFile", eText, target.c_str(), zip_error_strerror(zerr));
}
  
/******************************************************************************/
/* Private:                   z i p 2 s y s e r r                             */
/******************************************************************************/
  
int XrdOssArcZipFile::zip2syserr(const char *what, zip_error_t* zerr, bool msg)
{
   int retc = zip_error_code_zip(zerr);
   if (retc != ZIP_ER_OK)
      {switch(retc)
             {case ZIP_ER_CRC:       retc = EILSEQ;         break; 
              case ZIP_ER_EXISTS:    retc = EEXIST;         break; 
              case ZIP_ER_INCONS:    retc = ENOEXEC;        break; 
              case ZIP_ER_INUSE:     retc = EBUSY;          break; 
              case ZIP_ER_INVAL:     retc = EINVAL;         break; 
              case ZIP_ER_MEMORY:    retc = ENOMEM;         break; 
              case ZIP_ER_NOENT:     retc = ENOENT;         break; 
              case ZIP_ER_NOZIP:     retc = ENOTBLK;        break; 
              case ZIP_ER_OPNOTSUPP: retc = EOPNOTSUPP;     break; 
              case ZIP_ER_RDONLY:    retc = EROFS;          break; 
              case ZIP_ER_READ:      retc = EIO;            break; 
              case ZIP_ER_SEEK:      retc = ESPIPE;         break; 
              case ZIP_ER_WRITE:     retc = EIO;            break; 
              default:               retc = EDOM;           break; 
             }
      } else {
       retc = zip_error_code_system(zerr);
       if (retc == 0) retc = ENOMSG;
          else if (retc < 0) retc = -retc;
      }
   if (msg) zipEmsg(what, zerr);
   zip_error_fini(zerr);
   return -retc;
}

int XrdOssArcZipFile::zip2syserr(const char *what, int zrc, bool msg)
{
   zip_error_t zerr;
   zip_error_init_with_code(&zerr, zrc);
   return zip2syserr(what, &zerr, msg); 
}
