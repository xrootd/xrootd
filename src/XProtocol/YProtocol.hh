#ifndef __YPROTOCOL_H
#define __YPROTOCOL_H
/******************************************************************************/
/*                                                                            */
/*                          Y P r o t o c o l . h h                           */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/******************************************************************************/

#ifdef __CINT__
#define __attribute__(x)
#endif

#include "XProtocol/XPtypes.hh"

// We need to pack structures sent all over the net!
// __attribute__((packed)) assures no padding bytes.
//
// Note all binary values shall be in network byte order.
//
// Data is serialized as explained in XrdOucPup.
  
/******************************************************************************/
/*                C o m m o n   R e q u e s t   S e c t i o n                 */
/******************************************************************************/

namespace XrdCms
{

static const unsigned char kYR_Version = 3;

struct CmsRRHdr
{  kXR_unt32  streamid;    // Essentially opaque
   kXR_char   rrCode;      // Request or Response code
   kXR_char   modifier;    // RR dependent
   kXR_unt16  datalen;
};
  
enum CmsReqCode            // Request Codes
{    kYR_login   =  0,     // Same as kYR_data
     kYR_chmod   =  1,
     kYR_locate  =  2,
     kYR_mkdir   =  3,
     kYR_mkpath  =  4,
     kYR_mv      =  5,
     kYR_prepadd =  6,
     kYR_prepdel =  7,
     kYR_rm      =  8,
     kYR_rmdir   =  9,
     kYR_select  = 10,
     kYR_stats   = 11,
     kYR_avail   = 12,
     kYR_disc    = 13,
     kYR_gone    = 14,
     kYR_have    = 15,
     kYR_load    = 16,
     kYR_ping    = 17,
     kYR_pong    = 18,
     kYR_space   = 19,
     kYR_state   = 20,
     kYR_statfs  = 21,
     kYR_status  = 22,
     kYR_trunc   = 23,
     kYR_try     = 24,
     kYR_update  = 25,
     kYR_usage   = 26,
     kYR_xauth   = 27,
     kYR_MaxReq            // Count of request numbers (highest + 1)
};

// The hopcount is used for forwarded requests. It is incremented upon each
// forwarding until it wraps to zero. At this point the forward is not done.
// Forwarding applies to: chmod, have, mkdir, mkpath, mv, prepdel, rm, and 
// rmdir. Any other modifiers must be encoded in the low order 6 bits.
//
enum CmsFwdModifier
{    kYR_hopcount = 0xc0,
     kYR_hopincr  = 0x40
};

enum CmsReqModifier
{    kYR_raw = 0x20,     // Modifier: Unmarshalled data
     kYR_dnf = 0x10      // Modifier: mv, rm, rmdir (do not forward)
};

/******************************************************************************/
/*               C o m m o n   R e s p o n s e   S e c t i o n                */
/******************************************************************************/
  
enum CmsRspCode            // Response codes
{    kYR_data    = 0,      // Same as kYR_login
     kYR_error   = 1,
     kYR_redirect= 2,
     kYR_wait    = 3,
     kYR_waitresp= 4,
     kYR_yauth   = 5
};

enum YErrorCode
{  kYR_ENOENT = 1,        // -> ENOENT
   kYR_EPERM,             // -> ENOENT
   kYR_EACCES,            // -> EACCES
   kYR_EINVAL,            // -> EINVALO
   kYR_EIO,               // -> EIO
   kYR_ENOMEM,            // -> ENOMEM
   kYR_ENOSPC,            // -> ENOSPC
   kYR_ENAMETOOLONG,      // -> ENAMETOOLONG
   kYR_ENETUNREACH,       // -> ENETUNREACH
   kYR_ENOTBLK,           // -> ENOTBLK
   kYR_EISDIR,            // -> EISDIR
   kYR_FSError,           // -> ENODEV
   kYR_SrvError,          // -> EFAULT
   kYR_RWConflict,        // -> EEXIST
   kYR_noReplicas         // -> EADDRNOTAVAIL
};

struct CmsResponse
{      CmsRRHdr      Hdr;

enum  {kYR_async   = 128                 // Modifier: Reply to prev waitresp
      };

       kXR_unt32     Val;                // Port, Wait val, rc, asyncid
//     kXR_char      Data[Hdr.datalen-4];// Target host, more data, or emessage
};

/******************************************************************************/
/*                         a v a i l   R e q u e s t                          */
/******************************************************************************/
  
// Request: avail <diskFree> <diskUtil>
// Respond: n/a
//
struct CmsAvailRequest
{      CmsRRHdr      Hdr;
//     kXR_int32     diskFree;
//     kXR_int32     diskUtil;
};

/******************************************************************************/
/*                         c h m o d   R e q u e s t                          */
/******************************************************************************/
  
// Request: chmod <ident> <mode> <path>
// Respond: n/a
//
struct CmsChmodRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Ident;
//     kXR_string    Mode;
//     kXR_string    Path;
};

/******************************************************************************/
/*                          d i s c   R e q u e s t                           */
/******************************************************************************/
  
// Request: disc
// Respond: n/a
//
struct CmsDiscRequest
{      CmsRRHdr      Hdr;
};

/******************************************************************************/
/*                          g o n e   R e q u e s t                           */
/******************************************************************************/
  
// Request: gone <path>
// Respond: n/a
//
struct CmsGoneRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Path;
};

/******************************************************************************/
/*                          h a v e   R e q u e s t                           */
/******************************************************************************/
  
// Request: have <path>
// Respond: n/a
//
struct CmsHaveRequest
{      CmsRRHdr      Hdr;
       enum          {Online = 1, Pending = 2};  // Modifiers
//     kXR_string    Path;
};

/******************************************************************************/
/*                        l o c a t e   R e q u e s t                         */
/******************************************************************************/

struct CmsLocateRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Ident;
//     kXR_unt32     Opts;

enum  {kYR_refresh = 0x0001,
       kYR_retname = 0x0002,
       kYR_retuniq = 0x0004,
       kYR_asap    = 0x0080,
       kYR_retipv4 = 0x0000,  // Client is only IPv4
       kYR_retipv46= 0x1000,  // Client is IPv4 IPv6
       kYR_retipv6 = 0x2000,  // Client is only IPv6
       kYR_retipv64= 0x3000,  // Client is IPv6 IPv4
       kYR_retipmsk= 0x3000,  // Mask  to isolate retipcxx bits
       kYR_retipsft= 12,      // Shift to convert retipcxx bits
       kYR_listall = 0x4000,  // List everything regardless of other settings
       kYR_prvtnet = 0x8000   // Client is using a private address
      };
//     kXR_string    Path;

static const int     RHLen =266;  // Max length of each host response item
};

/******************************************************************************/
/*                         l o g i n   R e q u e s t                          */
/******************************************************************************/
  
// Request: login  <login_data>
// Respond: xauth  <auth_data>
//          login  <login_data>
//

struct CmsLoginData
{      kXR_unt16  Size;              // Temp area for packing purposes
       kXR_unt16  Version;
       kXR_unt32  Mode;              // From LoginMode
       kXR_int32  HoldTime;          // Hold time in ms(managers)
       kXR_unt32  tSpace;            // Tot  Space  GB (servers)
       kXR_unt32  fSpace;            // Free Space  MB (servers)
       kXR_unt32  mSpace;            // Minf Space  MB (servers)
       kXR_unt16  fsNum;             // File Systems   (servers /supervisors)
       kXR_unt16  fsUtil;            // FS Utilization (servers /supervisors)
       kXR_unt16  dPort;             // Data port      (servers /supervisors)
       kXR_unt16  sPort;             // Subs port      (managers/supervisors)
       kXR_char  *SID;               // Server ID      (servers/ supervisors)
       kXR_char  *Paths;             // Exported paths (servers/ supervisors)
       kXR_char  *ifList;            // Exported interfaces
       kXR_char  *envCGI;            // Exported environment

       enum       LoginMode
                 {kYR_director=   0x00000001,
                  kYR_manager =   0x00000002,
                  kYR_peer    =   0x00000004,
                  kYR_server  =   0x00000008,
                  kYR_proxy   =   0x00000010,
                  kYR_subman  =   0x00000020,
                  kYR_blredir =   0x00000040,   // Supports or is bl redir
                  kYR_suspend =   0x00000100,   // Suspended login
                  kYR_nostage =   0x00000200,   // Staging unavailable
                  kYR_trying  =   0x00000400,   // Extensive login retries
                  kYR_debug   =   0x80000000,
                  kYR_share   =   0x7f000000,   // Mask to isolate share
                  kYR_shift   =   24,           // Share shift position
                  kYR_tzone   =   0x00f80000,   // Mask to isolate time zone
                  kYR_shifttz =   19            // TZone shift position
                 };
};

struct CmsLoginRequest
{  CmsRRHdr     Hdr;
   CmsLoginData Data;
};

struct CmsLoginResponse
{  CmsRRHdr     Hdr;
   CmsLoginData Data;
};

/******************************************************************************/
/*                          l o a d   R e q u e s t                           */
/******************************************************************************/
  
// Request: load <cpu> <io> <load> <mem> <pag> <util> <dskfree>
// Respond: n/a
//
struct CmsLoadRequest
{      CmsRRHdr      Hdr;
       enum         {cpuLoad=0, netLoad, xeqLoad, memLoad, pagLoad, dskLoad,
                     numLoad};
//     kXR_char      theLoad[numload];
//     kXR_int       dskFree;
};

/******************************************************************************/
/*                         m k d i r   R e q u e s t                          */
/******************************************************************************/
  
// Request: mkdir <ident> <mode> <path>
// Respond: n/a
//
struct CmsMkdirRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Ident;
//     kXR_string    Mode;
//     kXR_string    Path;
};

/******************************************************************************/
/*                        m k p a t h   R e q u e s t                         */
/******************************************************************************/
  
// Request: <id> mkpath <mode> <path>
// Respond: n/a
//
struct CmsMkpathRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Ident;
//     kXR_string    Mode;
//     kXR_string    Path;
};

/******************************************************************************/
/*                            m v   R e q u e s t                             */
/******************************************************************************/
  
// Request: <id> mv <old_name> <new_name>
// Respond: n/a
//
struct CmsMvRequest {
       CmsRRHdr      Hdr;      // Subject to kYR_dnf modifier!
//     kXR_string    Ident;
//     kXR_string    Old_Path;
//     kXR_string    New_Path;
};

/******************************************************************************/
/*                          p i n g   R e q u e s t                           */
/******************************************************************************/
  
// Request: ping
// Respond: n/a
//
struct CmsPingRequest {
       CmsRRHdr      Hdr;
};

/******************************************************************************/
/*                          p o n g   R e q u e s t                           */
/******************************************************************************/
  
// Request: pong
// Respond: n/a
//
struct CmsPongRequest {
       CmsRRHdr      Hdr;
};

/******************************************************************************/
/*                       p r e p a d d   R e q u e s t                        */
/******************************************************************************/
  
// Request: <id> prepadd <reqid> <usr> <prty> <mode> <path>\n
// Respond: No response.
//
struct CmsPrepAddRequest
{      CmsRRHdr      Hdr;    // Modifier used with following options

enum  {kYR_stage   = 0x0001, // Stage   the data
       kYR_write   = 0x0002, // Prepare for writing
       kYR_coloc   = 0x0004, // Prepare for co-location
       kYR_fresh   = 0x0008, // Prepare by  time refresh
       kYR_metaman = 0x0010  // Prepare via meta-manager
      };
//     kXR_string    Ident;
//     kXR_string    reqid;
//     kXR_string    user;
//     kXR_string    prty;
//     kXR_string    mode;
//     kXR_string    Path;
//     kXR_string    Opaque; // Optional
};

/******************************************************************************/
/*                       p r e p d e l   R e q u e s t                        */
/******************************************************************************/
  
// Request: <id> prepdel <reqid>
// Respond: No response.
//
struct CmsPrepDelRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Ident;
//     kXR_string    reqid;
};

/******************************************************************************/
/*                            r m   R e q u e s t                             */
/******************************************************************************/
  
// Request: <id> rm <path>
// Respond: n/a
//
struct CmsRmRequest
{      CmsRRHdr      Hdr;    // Subject to kYR_dnf modifier!
//     kXR_string    Ident;
//     kXR_string    Path;
};

/******************************************************************************/
/*                         r m d i r   R e q u e s t                          */
/******************************************************************************/
  
// Request: <id> rmdir <path>
// Respond: n/a
//
struct CmsRmdirRequest
{      CmsRRHdr      Hdr;    // Subject to kYR_dnf modifier!
//     kXR_string    Ident;
//     kXR_string    Path;
};

/******************************************************************************/
/*                        s e l e c t   R e q u e s t                         */
/******************************************************************************/
  
// Request: <id> select[s] {c | d | m | r | w | s | t | x} <path> [-host]

// Note: selects - requests a cache refresh for <path>
// kYR_refresh   - refresh file location cache
// kYR_create  c - file will be created
// kYR_delete  d - file will be created or truncated
// kYR_metaop  m - inod will only be modified
// kYR_read    r - file will only be read
// kYR_replica   - file will replicated
// kYR_write   w - file will be read and writen
// kYR_stats   s - only stat information will be obtained
// kYR_online  x - consider only online files
//                 may be combined with kYR_stats (file must be resident)
//             - - the host failed to deliver the file.


struct CmsSelectRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Ident;
//     kXR_unt32     Opts;

enum  {kYR_refresh = 0x00000001,
       kYR_create  = 0x00000002, // May combine with trunc -> delete
       kYR_online  = 0x00000004,
       kYR_read    = 0x00000008, // Default
       kYR_trunc   = 0x00000010, // -> write
       kYR_write   = 0x00000020,
       kYR_stat    = 0x00000040, // Exclsuive
       kYR_metaop  = 0x00000080,
       kYR_replica = 0x00000100, // Only in combination with create
       kYR_mwfiles = 0x00000200, // Multiple writables files are OK
       kYR_retipv4 = 0x00000000,  // Client is only IPv4
       kYR_retipv46= 0x00001000,  // Client is IPv4 IPv6
       kYR_retipv6 = 0x00002000,  // Client is only IPv6
       kYR_retipv64= 0x00003000,  // Client is IPv6 IPv4
       kYR_retipmsk= 0x00003000,  // Mask  to isolate retipcxx bits
       kYR_retipsft= 12,          // Shift to convert retipcxx bits
       kYR_prvtnet = 0x00008000,  // Client is using a private address

       kYR_tryMISS = 0x00000000,  // Retry due to missing file (triedrc=enoent)
       kYR_tryIOER = 0x00010000,  // Retry due to I/O error    (triedrc=ioerr)
       kYR_tryFSER = 0x00020000,  // Retry due to FS error     (triedrc=fserr)
       kYR_trySVER = 0x00030000,  // Retry due to server error (triedrc=srverr)
       kYR_tryMASK = 0x00030000,  // Mask to isolate retry reason
       kYR_trySHFT = 16,          // Amount to shift right
       kYR_tryRSEL = 0x00040000,  // Retry for reselection LCL (triedrc=resel)
       kYR_tryRSEG = 0x00080000,  // Retry for reselection GBL (triedrc=resel)
       kYR_tryMSRC = 0x000C0000,  // Retry for multisource operation
       kYR_aWeak   = 0x00100000,  // Affinity: weak
       kYR_aStrong = 0x00200000,  // Affinity: strong
       kYR_aStrict = 0x00300000,  // Affinity: strict
       kYR_aNone   = 0x00400000,  // Affinity: none
       kYR_aSpec   = 0x00700000,  // Mask to test if any affinity specified
       kYR_aPack   = 0x00300000,  // Mask to test if the affinity packs choice
       kYR_aWait   = 0x00200000   // Mask to test if the affinity must wait
      };
//     kXR_string    Path;
//     kXR_string    Opaque; // Optional
//     kXR_string    Host;   // Optional
};

/******************************************************************************/
/*                         s p a c e   R e q u e s t                          */
/******************************************************************************/
  
// Request: space
//

struct CmsSpaceRequest
{      CmsRRHdr      Hdr;
};
  
/******************************************************************************/
/*                         s t a t e   R e q u e s t                          */
/******************************************************************************/
  
// Request: state <path>
//

struct CmsStateRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Path;

enum  {kYR_refresh = 0x01,   // Modifier
       kYR_noresp  = 0x02,
       kYR_metaman = 0x08
      };
};
  
/******************************************************************************/
/*                        s t a t f s   R e q u e s t                         */
/******************************************************************************/
  
// Request: statfs <path>
//

struct CmsStatfsRequest
{      CmsRRHdr      Hdr;    // Modifier used with following options
//     kXR_string    Path;

enum  {kYR_qvfs    = 0x0001  // Virtual file system query
      };
};

/******************************************************************************/
/*                         s t a t s   R e q u e s t                          */
/******************************************************************************/
  
// Request: stats or statsz (determined by modifier)
//

struct CmsStatsRequest
{      CmsRRHdr      Hdr;

enum  {kYR_size = 1  // Modifier
      };
};

/******************************************************************************/
/*                        s t a t u s   R e q u e s t                         */
/******************************************************************************/
  
// Request: status
//
struct CmsStatusRequest
{      CmsRRHdr      Hdr;

enum  {kYR_Stage  = 0x01, kYR_noStage = 0x02,  // Modifier
       kYR_Resume = 0x04, kYR_Suspend = 0x08,
       kYR_Reset  = 0x10                       // Exclusive
      };
};

/******************************************************************************/
/*                         t r u n c   R e q u e s t                          */
/******************************************************************************/
  
// Request: <id> trunc <path>
// Respond: n/a
//
struct CmsTruncRequest
{      CmsRRHdr      Hdr;
//     kXR_string    Ident;
//     kXR_string    Size;
//     kXR_string    Path;
};

/******************************************************************************/
/*                           t r y   R e q u e s t                            */
/******************************************************************************/
  
// Request: try
//
struct CmsTryRequest
{      CmsRRHdr      Hdr;
       kXR_unt16     sLen;   // This is the string length in PUP format

//     kYR_string    {ipaddr:port}[up to STMax];

enum  {kYR_permtop = 0x01    // Modifier Permanent redirect to top level
      };
};

/******************************************************************************/
/*                        u p d a t e   R e q u e s t                         */
/******************************************************************************/
  
// Request: update
//
struct CmsUpdateRequest
{      CmsRRHdr      Hdr;
};

/******************************************************************************/
/*                         u s a g e   R e q u e s t                          */
/******************************************************************************/
  
// Request: usage
//
struct CmsUsageRequest
{      CmsRRHdr      Hdr;
};

}; // namespace XrdCms
#endif
