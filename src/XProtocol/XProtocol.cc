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

#include <cinttypes>
#include <netinet/in.h>
#include <sys/types.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#include "XProtocol/XProtocol.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace
{
const char *errNames[kXR_ERRFENCE-kXR_ArgInvalid] =
                   {"Invalid argument",           // kXR_ArgInvalid = 3000,
                    "Missing argument",           // kXR_ArgMissing
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
                    "No servers available",       // kXR_noserver
                    "Target is not a file",       // kXR_NotFile
                    "Target is a directory",      // kXR_isDirectory
                    "Request cancelled",          // kXR_Cancelled
                    "Target exists",              // kXR_ItExists
                    "Checksum is invalid",        // kXR_ChkSumErr
                    "Request in progress",        // kXR_inProgress
                    "Quota exceeded",             // kXR_overQuota
                    "Invalid signature",          // kXR_SigVerErr
                    "Decryption failed",          // kXR_DecryptErr
                    "Server is overloaded",       // kXR_Overloaded
                    "Filesystem is read only",    // kXR_fsReadOnly
                    "Invalid payload format",     // kXR_BadPayload
                    "File attribute not found",   // kXR_AttrNotFound
                    "Operation requires TLS",     // kXR_TLSRequired
                    "No new servers for replica", // kXR_noReplicas
                    "Authentication failed",      // kXR_AuthFailed
                    "Request is not possible",    // kXR_Impossible
                    "Conflicting request",        // kXR_Conflict
                    "Too many errors",            // kXR_TooManyErrs
                    "Request timed out"           // kXR_ReqTimedOut
                   };

const char *reqNames[kXR_REQFENCE-kXR_auth] =
             {"auth",        "query",       "chmod",       "close",
              "dirlist",     "gpfile",      "protocol",    "login",
              "mkdir",       "mv",          "open",        "ping",
              "chkpoint",    "read",        "rm",          "rmdir",
              "sync",        "stat",        "set",         "write",
              "fattr",       "prepare",     "statx",       "endsess",
              "bind",        "readv",       "pgwrite",     "locate",
              "truncate",    "sigver",      "pgread",      "writev"
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

/******************************************************************************/
/*                  n v e c & v v e c  o p e r a t i n s                      */
/******************************************************************************/

// Add an attribute name to nvec (the buffer has to be sufficiently big)
//
char* ClientFattrRequest::NVecInsert( const char *name,  char *buffer )
{
  // set rc to 0
  memset( buffer, 0, sizeof( kXR_unt16 ) );
  buffer += sizeof( kXR_unt16 );
  // copy attribute name including trailing null
  size_t len = strlen( name );
  memcpy( buffer, name, len + 1 );
  buffer += len + 1;

  // return memory that comes right after newly inserted nvec record
  return buffer;
}

// Add an attribute name to vvec (the buffer has to be sufficiently big)
//
char* ClientFattrRequest::VVecInsert( const char *value, char *buffer )
{
  // copy value size
  kXR_int32 len    = strlen( value );
  kXR_int32 lendat = htonl( len );
  memcpy( buffer, &lendat, sizeof( kXR_int32 ) );
  buffer += sizeof( kXR_int32 );
  // copy value itself
  memcpy( buffer, value, len );
  buffer += len;

  // return memory that comes right after newly inserted vvec entry
  return buffer;
}

// Read error code from nvec
//
char* ClientFattrRequest::NVecRead( char* buffer, kXR_unt16 &rc )
 {
   rc = *reinterpret_cast<const kXR_unt16*>( buffer );
   rc = htons( rc );
   buffer += sizeof( kXR_unt16 );
   return buffer;
 }

// Read attribute name from nvec
//
char* ClientFattrRequest::NVecRead( char* buffer, char *&name )
{
  name = strdup( buffer );
  buffer += strlen( name ) + 1;
  return buffer;
}

// Read value length from vvec
//
char* ClientFattrRequest::VVecRead( char* buffer, kXR_int32 &len )
{
  len = *reinterpret_cast<const kXR_int32*>( buffer );
  len = htonl( len );
  buffer += sizeof( kXR_int32 );
  return buffer;
}

// Read attribute value from vvec
//
char* ClientFattrRequest::VVecRead( char* buffer, kXR_int32 len, char *&value )
{
  value = reinterpret_cast<char*>( malloc( len + 1 ) );
  strncpy( value, buffer, len );
  value[len] = 0;
  buffer += len;
  return buffer;
}
