/******************************************************************************/
/*                                                                            */
/*                          X P r o t o c o l . c c                           */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <inttypes.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "XProtocol/XProtocol.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace
{
const char *errNames[kXR_ERRFENCE-kXR_ArgInvalid] =
                   {"Invalid argument",           // kXR_ArgInvalid = 3000,
                    "Missing agument",            // kXR_ArgMissing
                    "Argument is too long",       // kXR_ArgTooLong
                    "File or object is locked",   // kXR_FileLocked
                    "File or object not open",    // kXR_FileNotOpen
                    "Filesystem error",           // kXR_FSError
                    "Invalid request",            // kXR_InvalidRequest
                    "I/O error",                  // kXR_IOError
                    "Insufficient memory",        // kXR_NoMemory
                    "Insufficient space",         // kXR_NoSpace
                    "Request not authorized",     // kXR_NotAuthorized
                    "File or object not found",   // kXR_NotFound
                    "Internal server error",      // kXR_ServerError
                    "Unsupported request",        // kXR_Unsupported
                    "No serves available",        // kXR_noserver
                    "Target is not a file",       // kXR_NotFile
                    "Target is a directory",      // kXR_isDirectory
                    "Request cancelled",          // kXR_Cancelled
                    "Checksum length error",      // kXR_ChkLenErr
                    "Checksum is invalid",        // kXR_ChkSumErr
                    "Request in progress",        // kXR_inProgress
                    "Quota exceeded",             // kXR_overQuota
                    "Invalid signature",          // kXR_SigVerErr
                    "Decryption failed"           // kXR_DecryptErr
                   };

const char *reqNames[kXR_REQFENCE-kXR_auth] =
             {"auth",        "query",       "chmod",       "close",
              "dirlist",     "getfile",     "protocol",    "login",
              "mkdir",       "mv",          "open",        "ping",
              "putfile",     "read",        "rm",          "rmdir",
              "sync",        "stat",        "set",         "write",
              "admin",       "prepare",     "statx",       "endsess",
              "bind",        "readv",       "verifyw",     "locate",
              "truncate",    "sigver",      "decrypt",     "writev"
             };

// Following value is used to determine if the error or request code is
// host byte or network byte order making use of the fact each starts at 3000
//
union Endianness {kXR_unt16 xyz; unsigned char Endian[2];} little = {1};
}

/******************************************************************************/
/*                               e r r N a m e                                */
/******************************************************************************/
  
const char *XProtocol::errName(kXR_int32 errCode)
{
// Mangle request code if the byte orderdoesn't match our host order
//
   if ((errCode < 0 || errCode > kXR_ERRFENCE) && little.Endian[0])
      errCode = ntohl(errCode);

// Validate the request code
//
   if (errCode < kXR_ArgInvalid || errCode >= kXR_ERRFENCE)
      return "!undefined error";

// Return the proper table
//
   return errNames[errCode - kXR_ArgInvalid];
}

/******************************************************************************/
/*                               r e q N a m e                                */
/******************************************************************************/
  
const char *XProtocol::reqName(kXR_unt16 reqCode)
{
// Mangle request code if the byte orderdoesn't match our host order
//
   if (reqCode > kXR_REQFENCE && little.Endian[0]) reqCode = ntohs(reqCode);

// Validate the request code
//
   if (reqCode < kXR_auth || reqCode >= kXR_REQFENCE) return "!unknown";

// Return the proper table
//
   return reqNames[reqCode - kXR_auth];
}
