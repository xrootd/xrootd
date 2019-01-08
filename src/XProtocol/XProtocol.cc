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
/* The XRootD protocol definition, documented in this file, is distributed    */
/* under a modified BSD license and may be freely used to reimplement it.     */
/* Any references to "source" in this license refers to this file or any      */
/* other file that specifically contains the following license.               */
/*                                                                            */
/* Redistribution and use in source and binary forms, with or without         */
/* modification, are permitted provided that the following conditions         */
/* are met:                                                                   */
/*                                                                            */
/* 1. Redistributions of source code must retain the above copyright notice,  */
/*    this list of conditions and the following disclaimer.                   */
/*                                                                            */
/* 2. Redistributions in binary form must reproduce the above copyright       */
/*    notice, this list of conditions and the following disclaimer in the     */
/*    documentation and/or other materials provided with the distribution.    */
/*                                                                            */
/* 3. Neither the name of the copyright holder nor the names of its           */
/*    contributors may be used to endorse or promote products derived from    */
/*    this software without specific prior written permission.                */
/*                                                                            */
/* 4. Derived software may not use the name XRootD or cmsd (regardless of     */
/*    capitilization) in association with the derived work if the protocol    */
/*    documented in this file is changed in any way.                          */
/*                                                                            */
/*    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS     */
/*    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT       */
/*    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR   */
/*    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT    */
/*    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  */
/*    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT        */
/*    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,   */
/*    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY   */
/*    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT     */
/*    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE   */
/*    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.    */
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
