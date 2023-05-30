#ifndef __XPROTOCOL_H
#define __XPROTOCOL_H
/******************************************************************************/
/*                                                                            */
/*                          X P r o t o c o l . h h                           */
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
/* The XRoot protocol definition, documented in this file, is distributed     */
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

/******************************************************************************/
/*          P r o t o c o l   V e r s i o n   D e f i n i t i o n s           */
/******************************************************************************/
  
// The following is the binary representation of the protocol version here.
// Protocol version is repesented as three base10 digits x.y.z with x having no
// upper limit (i.e. n.9.9 + 1 -> n+1.0.0). The kXR_PROTSIGNVERSION defines the
// protocol version where request signing became available.
//
#define kXR_PROTOCOLVERSION  0x00000511
#define kXR_PROTXATTVERSION  0x00000500
#define kXR_PROTTLSVERSION   0x00000500
#define kXR_PROTPGRWVERSION  0x00000511
#define kXR_PROTSIGNVERSION  0x00000310
#define kXR_PROTOCOLVSTRING "5.1.0"

/******************************************************************************/
/*               C l i e n t - S e r v e r   H a n d s h a k e                */
/******************************************************************************/

// The fields to be sent as initial handshake
//
struct ClientInitHandShake {
   kXR_int32 first;
   kXR_int32 second;
   kXR_int32 third;
   kXR_int32 fourth;
   kXR_int32 fifth;
};

// The body received after the first handshake's header
//
struct ServerInitHandShake {
   kXR_int32 msglen;
   kXR_int32 protover;
   kXR_int32 msgval;
};
  
/******************************************************************************/
/*                       C l i e n t   R e q u e s t s                        */
/******************************************************************************/

// G.Ganis: All the following structures never need padding bytes:
//          no need of packing options like __attribute__((packed))
//
// All binary data is sent in network byte order.

// Client request codes
// 
enum XRequestTypes {
   kXR_1stRequest= 3000,
   kXR_auth    =   3000,
   kXR_query,   // 3001
   kXR_chmod,   // 3002
   kXR_close,   // 3003
   kXR_dirlist, // 3004
   kXR_gpfile,  // 3005 was kXR_getfile
   kXR_protocol,// 3006
   kXR_login,   // 3007
   kXR_mkdir,   // 3008
   kXR_mv,      // 3009
   kXR_open,    // 3010
   kXR_ping,    // 3011
   kXR_chkpoint,// 3012 was kXR_putfile
   kXR_read,    // 3013
   kXR_rm,      // 3014
   kXR_rmdir,   // 3015
   kXR_sync,    // 3016
   kXR_stat,    // 3017
   kXR_set,     // 3018
   kXR_write,   // 3019
   kXR_fattr,   // 3020 was kXR_admin
   kXR_prepare, // 3021
   kXR_statx,   // 3022
   kXR_endsess, // 3023
   kXR_bind,    // 3024
   kXR_readv,   // 3025
   kXR_pgwrite, // 3026 was kXR_verifyw
   kXR_locate,  // 3027
   kXR_truncate,// 3028
   kXR_sigver,  // 3029
   kXR_pgread,  // 3030 was kXR_decrypt
   kXR_writev,  // 3031
   kXR_REQFENCE // Always last valid request code +1
};

// Virtual client request codes
//
enum XVirtRequestTypes {
   kXR_virtReadv = 2000
};

// All client requests use a header with the following format
//
struct ClientRequestHdr {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  body[16];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                      k X R _ a u t h   R e q u e s t                       */
/******************************************************************************/
  
struct ClientAuthRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  reserved[12];
   kXR_char  credtype[4];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                      k X R _ b i n d   R e q u e s t                       */
/******************************************************************************/
  
struct ClientBindRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  sessid[16];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                     k X R _ c h m o d   R e q u e s t                      */
/******************************************************************************/
  
struct ClientChmodRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  reserved[14];
   kXR_unt16 mode;        // See XOpenRequestMode
   kXR_int32 dlen;
};

/******************************************************************************/
/*                  k X R _ c h k p o i n t   R e q u e s t                   */
/******************************************************************************/
  
struct ClientChkPointRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];    // For Create, Delete, Query, or Restore
   kXR_char  reserved[11];
   kXR_char  opcode;        // One of kXR_ckpxxxx actions
   kXR_int32 dlen;
};

// Actions
//
static const int kXR_ckpBegin   = 0;  // Begin    checkpoint
static const int kXR_ckpCommit  = 1;  // Commit   changes
static const int kXR_ckpQuery   = 2;  // Query    checkpoint limits
static const int kXR_ckpRollback= 3;  // Rollback changes
static const int kXR_ckpXeq     = 4;  // Execute trunc, write, or writev

// The minimum size of a checkpoint data limit
//
static const int kXR_ckpMinMax  = 104857604;  // 10 MB
  
/******************************************************************************/
/*                     k X R _ c l o s e   R e q u e s t                      */
/******************************************************************************/
  
struct ClientCloseRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];
   kXR_char  reserved[12];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                   k X R _ d i r l i s t   R e q u e s t                    */
/******************************************************************************/

enum XDirlistRequestOption {
   kXR_online = 1,
   kXR_dstat  = 2,
   kXR_dcksm  = 4    // dcksm implies dstat irrespective of dstat setting
};
  
struct ClientDirlistRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  reserved[15];
   kXR_char  options[1];     // See XDirlistRequestOption enum
   kXR_int32 dlen;
};

/******************************************************************************/
/*                   k X R _ e n d s e s s   R e q u e s t                    */
/******************************************************************************/
  
struct ClientEndsessRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  sessid[16];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                     k X R _ f a t t r   R e q u e s t                      */
/******************************************************************************/

// kXR_fattr subcodes
//
enum xfaSubCode {
   kXR_fattrDel    = 0,
   kXR_fattrGet    = 1,
   kXR_fattrList   = 2,
   kXR_fattrSet    = 3,
   kXR_fatrrMaxSC  = 3     // Highest valid subcode
};

// kXR_fattr limits
//
enum xfaLimits {
   kXR_faMaxVars = 16,     // Maximum variables per request
   kXR_faMaxNlen = 248,    // Maximum length of variable name
   kXR_faMaxVlen = 65536   // Maximum length of variable value
};
  
struct ClientFattrRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];
   kXR_char  subcode;      // See xfaSubCode enum
   kXR_char  numattr;
   kXR_char  options;      // See valid options below
   kXR_char  reserved[9];
   kXR_int32 dlen;

// Valid options:
//
   static const int isNew = 0x01; // For set,  the variable must not exist
   static const int aData = 0x10; // For list, return attribute value

// Add an attribute name to nvec (the buffer has to be sufficiently big)
//
   static char* NVecInsert( const char *name,  char *buffer );

// Add an attribute name to vvec (the buffer has to be sufficiently big)
//
   static char* VVecInsert( const char *value, char *buffer );

// Read error code from nvec
//
   static char* NVecRead( char* buffer, kXR_unt16 &rc );

// Read attribute name from nvec, should be deallocated with free()
//
   static char* NVecRead( char* buffer, char *&name );

// Read value length from vvec
//
   static char* VVecRead( char* buffer, kXR_int32 &len );

// Read attribute value from vvec, should be deallocated with free()
//
   static char* VVecRead( char* buffer, kXR_int32 len, char *&value );

};
  
/******************************************************************************/
/*                    k X R _ g p f i l e   R e q u e s t                     */
/******************************************************************************/
  
struct ClientGPfileRequest { // ??? This is all wrong; correct when implemented
   kXR_char  streamid[2];
   kXR_unt16 requestid;      // kXR_gpfile
   kXR_int32 options;
   kXR_char reserved[8];
   kXR_int32 buffsz;
   kXR_int32  dlen;
};

/******************************************************************************/
/*                    k X R _ l o c a t e   R e q u e s t                     */
/******************************************************************************/
  
struct ClientLocateRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_unt16 options;     // See XOpenRequestOption enum tagged for locate
   kXR_char  reserved[14];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                     k X R _ l o g i n   R e q u e s t                      */
/******************************************************************************/

// this is a bitmask
enum XLoginAbility {
   kXR_nothing    =   0,
   kXR_fullurl    =   1,
   kXR_multipr    =   3,
   kXR_readrdok   =   4,
   kXR_hasipv64   =   8,
   kXR_onlyprv4   =  16,
   kXR_onlyprv6   =  32,
   kXR_lclfile    =  64,
   kXR_redirflags = 128
};

// this iss a bitmask
enum XLoginAbility2 {
   kXR_empty   = 0,
   kXR_ecredir = 1
};

// this is a bitmask (note that XLoginVersion resides in lower bits)
enum XLoginCapVer {
   kXR_lcvnone = 0,
   kXR_vermask = 63,
   kXR_asyncap = 128
};

// this is a single number that is or'd into capver as the version
//
enum XLoginVersion {
   kXR_ver000 = 0,  // Old clients predating history
   kXR_ver001 = 1,  // Generally implemented 2005 protocol
   kXR_ver002 = 2,  // Same as 1 but adds asyncresp recognition
   kXR_ver003 = 3,  // The 2011-2012 rewritten client
   kXR_ver004 = 4,  // The 2016 sign-capable   client
   kXR_ver005 = 5   // The 2019 TLS-capable    client
};
  
struct ClientLoginRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_int32 pid;
   kXR_char username[8];
   kXR_char ability2;      // See XLoginAbility2 enum flags
   kXR_char ability;       // See XLoginAbility  enum flags
   kXR_char capver[1];     // See XLoginCapVer   enum flags
   kXR_char reserved2;
   kXR_int32  dlen;
};

/******************************************************************************/
/*                     k X R _ m k d i r   R e q u e s t                      */
/******************************************************************************/

enum XMkdirOptions {
   kXR_mknone  = 0,
   kXR_mkdirpath  = 1
};
  
struct ClientMkdirRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  options[1];
   kXR_char  reserved[13];
   kXR_unt16 mode;          // See XOpenRequestMode
   kXR_int32 dlen;
};

/******************************************************************************/
/*                        k X R _ m v   R e q u e s t                         */
/******************************************************************************/
  
struct ClientMvRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  reserved[14];
   kXR_int16 arg1len;
   kXR_int32 dlen;
};

/******************************************************************************/
/*                      k X R _ o p e n   R e q u e s t                       */
/******************************************************************************/

// OPEN MODE FOR A REMOTE FILE
enum XOpenRequestMode {
   kXR_ur = 0x100,
   kXR_uw = 0x080,
   kXR_ux = 0x040,
   kXR_gr = 0x020,
   kXR_gw = 0x010,
   kXR_gx = 0x008,
   kXR_or = 0x004,
   kXR_ow = 0x002,
   kXR_ox = 0x001
};

enum XOpenRequestOption {
   kXR_compress = 0x0001, //     1   // also locate (return unique hosts)
   kXR_delete   = 0x0002, //     2
   kXR_force    = 0x0004, //     4
   kXR_new      = 0x0008, //     8
   kXR_open_read= 0x0010, //    16
   kXR_open_updt= 0x0020, //    32
   kXR_async    = 0x0040, //    64
   kXR_refresh  = 0x0080, //   128   // also locate
   kXR_mkpath   = 0x0100, //   256
   kXR_prefname = 0x0100, //   256   // only locate
   kXR_open_apnd= 0x0200, //   512
   kXR_retstat  = 0x0400, //  1024
   kXR_4dirlist = 0x0400, //  1024   // for locate intending a dirlist
   kXR_replica  = 0x0800, //  2048
   kXR_posc     = 0x1000, //  4096
   kXR_nowait   = 0x2000, //  8192   // also locate
   kXR_seqio    = 0x4000, // 16384
   kXR_open_wrto= 0x8000  // 32768
};

enum XOpenRequestOption2 {
   kXR_dup      = 0x0001, //     1
   kXR_samefs   = 0x0002  //     2
};
  
struct ClientOpenRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_unt16 mode;
   kXR_unt16 options;
   kXR_char  reserved[12];
   kXR_int32  dlen;
};

/******************************************************************************/
/*                    k X R _ p g r e a d   R e q u e s t                     */
/******************************************************************************/
  
// The page size for pgread and pgwrite and the maximum transmission size
//
namespace XrdProto  // Always use this namespace for new additions
{
static const int kXR_pgPageSZ = 4096;     // Length of a page
static const int kXR_pgPageBL = 12;       // log2(page length)
static const int kXR_pgUnitSZ = kXR_pgPageSZ + sizeof(kXR_unt32);
static const int kXR_pgMaxEpr = 128;      // Max checksum errs per request
static const int kXR_pgMaxEos = 256;      // Max checksum errs outstanding

// kXR_pgread/write options
//
static const kXR_char kXR_AnyPath = 0xff; // In pathid
static const int      kXR_pgRetry = 0x01; // In reqflags
}
  
struct ClientPgReadRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];
   kXR_int64 offset;
   kXR_int32 rlen;
   kXR_int32 dlen;     // Request data length must be 0 unless args present
};

struct ClientPgReadReqArgs {
   kXR_char  pathid;   // Request data length must be 1
   kXR_char  reqflags; // Request data length must be 2
};

namespace
{
}

/******************************************************************************/
/*                   k X R _ p r w r i t e   R e q u e s t                    */
/******************************************************************************/
  
struct ClientPgWriteRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];
   kXR_int64 offset;
   kXR_char  pathid;
   kXR_char  reqflags;
   kXR_char  reserved[2];
   kXR_int32 dlen;
// kXR_char  data[dlen];
};

/******************************************************************************/
/*                      k X R _ p i n g   R e q u e s t                       */
/******************************************************************************/
  
struct ClientPingRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  reserved[16];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                  k X R _ p r o t o c o l   R e q u e s t                   */
/******************************************************************************/
  
struct ClientProtocolRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_int32 clientpv;      // 2.9.7 or higher
   kXR_char  flags;         // 3.1.0 or higher
   kXR_char  expect;        // 4.0.0 or higher
   kXR_char  reserved[10];
   kXR_int32 dlen;

enum RequestFlags {
   kXR_secreqs  = 0x01,  // Options: Return security requirements
   kXR_ableTLS  = 0x02,  // Options: Client is TLS capable
   kXR_wantTLS  = 0x04,  // Options: Change connection to use TLS
   kXR_bifreqs  = 0x08   // Options: Return bind interface requirements
};

enum ExpectFlags {
   kXR_ExpMask   = 0x0f, // Isolate the relevant expect enumeration value
   kXR_ExpNone   = 0x00,
   kXR_ExpBind   = 0x01,
   kXR_ExpGPF    = 0x02,
   kXR_ExpLogin  = 0x03,
   kXR_ExpTPC    = 0x04,
   kXR_ExpGPFA   = 0x08
};
};

/******************************************************************************/
/*                   k X R _ p r e p a r e   R e q u e s t                    */
/******************************************************************************/

enum XPrepRequestOption {
   kXR_cancel = 1,
   kXR_notify = 2,
   kXR_noerrs = 4,
   kXR_stage  = 8,
   kXR_wmode  = 16,
   kXR_coloc  = 32,
   kXR_fresh  = 64,
   kXR_usetcp = 128,

   kXR_evict  = 0x0001 // optionsX: file no longer useful
};
  
struct ClientPrepareRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  options;
   kXR_char  prty;
   kXR_unt16 port;          // 2.9.9 or higher
   kXR_unt16 optionX;       // Extended options
   kXR_char  reserved[10];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                     k X R _ q u e r y   R e q u e s t                      */
/******************************************************************************/

enum XQueryType {
   kXR_QStats = 1,
   kXR_QPrep  = 2,
   kXR_Qcksum = 3,
   kXR_Qxattr = 4,
   kXR_Qspace = 5,
   kXR_Qckscan= 6,
   kXR_Qconfig= 7,
   kXR_Qvisa  = 8,
   kXR_Qopaque=16,
   kXR_Qopaquf=32,
   kXR_Qopaqug=64
};
  
struct ClientQueryRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_unt16 infotype;    // See XQueryType enum
   kXR_char  reserved1[2];
   kXR_char  fhandle[4];
   kXR_char  reserved2[8];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                      k X R _ r e a d   R e q u e s t                       */
/******************************************************************************/
  
struct ClientReadRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];
   kXR_int64 offset;
   kXR_int32 rlen;
   kXR_int32 dlen;
// Optionally followed by read_args
};

struct read_args {
   kXR_char  pathid;
   kXR_char  reserved[7];
// This struct may be followed by an array of readahead_list
};

struct readahead_list {
   kXR_char  fhandle[4];
   kXR_int32 rlen;
   kXR_int64 offset;
};

/******************************************************************************/
/*                     k X R _ r e a d v   R e q u e s t                      */
/******************************************************************************/
  
struct ClientReadVRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  reserved[15];
   kXR_char  pathid;
   kXR_int32 dlen;
// This struct followed by the read_list
};

namespace XrdProto  // Always use this namespace for new additions
{
struct read_list {
   kXR_char  fhandle[4];
   kXR_int32 rlen;
   kXR_int64 offset;
};
static const int rlItemLen = sizeof(read_list);
static const int maxRvecln = 16384;
static const int maxRvecsz = maxRvecln/rlItemLen;
}

/******************************************************************************/
/*                        k X R _ r m   R e q u e s t                         */
/******************************************************************************/
  
struct ClientRmRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  reserved[16];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                     k X R _ r m d i r   R e q u e s t                      */
/******************************************************************************/
  
struct ClientRmdirRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  reserved[16];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                       k X R _ s e t   R e q u e s t                        */
/******************************************************************************/
  
struct ClientSetRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char reserved[15];
   kXR_char  modifier;  // For security purposes, should be zero
   kXR_int32  dlen;
};

/******************************************************************************/
/*                    k X R _ s i g v e r   R e q u e s t                     */
/******************************************************************************/

// Cryptography used for kXR_sigver SigverRequest::crypto
enum XSecCrypto {
   kXR_SHA256   = 0x01,   // Hash used
   kXR_HashMask = 0x0f,   // Mak to extract the hash type
   kXR_rsaKey   = 0x80    // The rsa key was used
};

// Flags for kXR_sigver
enum XSecFlags {
   kXR_nodata   = 1  // Request payload was not hashed
};

// Version number
enum XSecVersion {
   kXR_Ver_00 = 0
};

struct ClientSigverRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_unt16 expectrid; // Request code of subsequent request
   kXR_char  version;   // Security version being used (see XSecVersion)
   kXR_char  flags;     // One or more flags defined in enum (see XSecFlags)
   kXR_unt64 seqno;     // Monotonically increasing number (part of hash)
   kXR_char  crypto;    // Cryptography used (see XSecCrypto)
   kXR_char  rsvd2[3];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                      k X R _ s t a t   R e q u e s t                       */
/******************************************************************************/

enum XStatRequestOption {
   kXR_vfs    = 1
};
  
struct ClientStatRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  options;    // See XStatRequestOption
   kXR_char  reserved[11];
   kXR_char  fhandle[4];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                      k X R _ s y n c   R e q u e s t                       */
/******************************************************************************/
  
struct ClientSyncRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];
   kXR_char  reserved[12];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                  k X R _ t r u n c a t e   R e q u e s t                   */
/******************************************************************************/
  
struct ClientTruncateRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];
   kXR_int64 offset;
   kXR_char  reserved[4];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                     k X R _ w r i t e   R e q u e s t                      */
/******************************************************************************/
  
struct ClientWriteRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  fhandle[4];
   kXR_int64 offset;
   kXR_char  pathid;
   kXR_char  reserved[3];
   kXR_int32 dlen;
};

/******************************************************************************/
/*                    k X R _ w r i t e v   R e q u e s t                     */
/******************************************************************************/
  
struct ClientWriteVRequest {
   kXR_char  streamid[2];
   kXR_unt16 requestid;
   kXR_char  options;  // See static const ints below
   kXR_char  reserved[15];
   kXR_int32 dlen;
// This struct followed by the write_list

   static const kXR_int32 doSync = 0x01;
};

namespace XrdProto  // Always use this namespace for new additions
{
struct write_list {
   kXR_char fhandle[4];
   kXR_int32 wlen;
   kXR_int64 offset;
};
static const int wlItemLen = sizeof(write_list);
static const int maxWvecln = 16384;
static const int maxWvecsz = maxWvecln/wlItemLen;
}

/******************************************************************************/
/*          U n i o n   o f   a l l   C l i e n t   R e q u e s t s           */
/******************************************************************************/
  
typedef union {
   struct ClientRequestHdr header;
   struct ClientAuthRequest auth;
   struct ClientBindRequest bind;
   struct ClientChkPointRequest chkpoint;
   struct ClientChmodRequest chmod;
   struct ClientCloseRequest close;
   struct ClientDirlistRequest dirlist;
   struct ClientEndsessRequest endsess;
   struct ClientFattrRequest fattr;
   struct ClientGPfileRequest gpfile;
   struct ClientLocateRequest locate;
   struct ClientLoginRequest login;
   struct ClientMkdirRequest mkdir;
   struct ClientMvRequest mv;
   struct ClientOpenRequest open;
   struct ClientPgReadRequest pgread;
   struct ClientPgWriteRequest pgwrite;
   struct ClientPingRequest ping;
   struct ClientPrepareRequest prepare;
   struct ClientProtocolRequest protocol;
   struct ClientQueryRequest query;
   struct ClientReadRequest read;
   struct ClientReadVRequest readv;
   struct ClientRmRequest rm;
   struct ClientRmdirRequest rmdir;
   struct ClientSetRequest set;
   struct ClientSigverRequest sigver;
   struct ClientStatRequest stat;
   struct ClientSyncRequest sync;
   struct ClientTruncateRequest truncate;
   struct ClientWriteRequest write;
   struct ClientWriteVRequest writev;
} ClientRequest;

typedef union {
   struct ClientRequestHdr header;
   struct ClientSigverRequest sigver;
} SecurityRequest;

/******************************************************************************/
/*                      S e r v e r   R e s p o n s e s                       */
/******************************************************************************/

// Nice header for the server response.
// Note that the protocol specifies these values to be in network
// byte order when sent
//
// G.Ganis: The following structures never need padding bytes:
//          no need of packing options

// Server response codes
//
enum XResponseType {
   kXR_ok      =   0,
   kXR_oksofar =   4000,
   kXR_attn,    // 4001
   kXR_authmore,// 4002
   kXR_error,   // 4003
   kXR_redirect,// 4004
   kXR_wait,    // 4005
   kXR_waitresp,// 4006
   kXR_status,  // 4007
   kXR_noResponsesYet = 10000
};

// All serer responses start with the same header
//
struct ServerResponseHeader {
   kXR_char  streamid[2];
   kXR_unt16 status;
   kXR_int32 dlen;
};

// This is a bit of wierdness held over from the very old days, sigh.
//
struct ServerResponseBody_Buffer {
   char data[4096];
};

/******************************************************************************/
/*                     k X R _ a t t n   R e s p o n s e                      */
/******************************************************************************/

enum XActionCode {
   kXR_asyncab =     5000, // No longer supported
   kXR_asyncdi,   // 5001     No longer supported
   kXR_asyncms =     5002,
   kXR_asyncrd,   // 5003     No longer supported
   kXR_asyncwt,   // 5004     No longer supported
   kXR_asyncav,   // 5005     No longer supported
   kXR_asynunav,  // 5006     No longer supported
   kXR_asyncgo,   // 5007     No longer supported
   kXR_asynresp=     5008
};
  
struct ServerResponseBody_Attn {
   kXR_int32 actnum;      // See XActionCode enum
   char      parms[4096]; // Should be sufficient for every use
};

struct ServerResponseBody_Attn_asyncms  {
   kXR_int32            actnum;       // XActionCode::kXR_asyncms
   char                 reserved[4];
   ServerResponseHeader resphdr;
   char                 respdata[4096];
};

struct ServerResponseBody_Attn_asynresp {
   kXR_int32            actnum;       // XActionCode::kXR_asynresp
   char                 reserved[4];
   ServerResponseHeader resphdr;
   char                 respdata[4096];
};

/******************************************************************************/
/*                 k X R _ a u t h m o r e   R e s p o n s e                  */
/******************************************************************************/
  
struct ServerResponseBody_Authmore {
   char data[4096];
};

/******************************************************************************/
/*                     k X R _ b i n d   R e s p o n s e                      */
/******************************************************************************/
  
struct ServerResponseBody_Bind {
    kXR_char substreamid;
};

/******************************************************************************/
/*                 k X R _ c h k p o i n t   R e s p o n s e                  */
/******************************************************************************/
  
struct ServerResponseBody_ChkPoint { // Only for kXR_ckpQMax
    kXR_unt32 maxCkpSize;  // Maximum number of bytes including overhead
    kXR_unt32 useCkpSize;  // The number of bytes already being used
};
  
/******************************************************************************/
/*                    k X R _ e r r o r   R e s p o n s e                     */
/******************************************************************************/

enum XErrorCode {
   kXR_ArgInvalid =        3000,
   kXR_ArgMissing,      // 3001
   kXR_ArgTooLong,      // 3002
   kXR_FileLocked,      // 3003
   kXR_FileNotOpen,     // 3004
   kXR_FSError,         // 3005
   kXR_InvalidRequest,  // 3006
   kXR_IOError,         // 3007
   kXR_NoMemory,        // 3008
   kXR_NoSpace,         // 3009
   kXR_NotAuthorized,   // 3010
   kXR_NotFound,        // 3011
   kXR_ServerError,     // 3012
   kXR_Unsupported,     // 3013
   kXR_noserver,        // 3014
   kXR_NotFile,         // 3015
   kXR_isDirectory,     // 3016
   kXR_Cancelled,       // 3017
   kXR_ItExists,        // 3018
   kXR_ChkSumErr,       // 3019
   kXR_inProgress,      // 3020
   kXR_overQuota,       // 3021
   kXR_SigVerErr,       // 3022
   kXR_DecryptErr,      // 3023
   kXR_Overloaded,      // 3024
   kXR_fsReadOnly,      // 3025
   kXR_BadPayload,      // 3026
   kXR_AttrNotFound,    // 3027
   kXR_TLSRequired,     // 3028
   kXR_noReplicas,      // 3029
   kXR_AuthFailed,      // 3030
   kXR_Impossible,      // 3031
   kXR_Conflict,        // 3032
   kXR_TooManyErrs,     // 3033
   kXR_ReqTimedOut,     // 3034
   kXR_ERRFENCE,        // Always last valid errcode + 1
   kXR_noErrorYet = 10000
};
  
struct ServerResponseBody_Error {
   kXR_int32 errnum;       // See XErrorCode enu
   char      errmsg[4096]; // Should be sufficient for every use
};

/******************************************************************************/
/*                    k X R _ l o g i n   R e s p o n s e                     */
/******************************************************************************/
  
struct ServerResponseBody_Login {
   kXR_char  sessid[16];
   kXR_char  sec[4096]; // Should be sufficient for every use
};

/******************************************************************************/
/*                     k X R _ o p e n   R e s p o n s e                      */
/******************************************************************************/

struct ServerResponseBody_Open {
   kXR_char  fhandle[4];
   kXR_int32 cpsize;    // cpsize & cptype returned if kXR_compress *or*
   kXR_char  cptype[4]; // kXR_retstat is specified
}; // info will follow if kXR_retstat is specified
  
/******************************************************************************/
/*                   k X R _ p g r e a d   R e s p o n s e                    */
/******************************************************************************/

struct ServerResponseBody_pgRead {
   kXR_int64 offset;    // info[]: File offset of data that follows
// kXR_char  data[dlen];
};

/******************************************************************************/
/*                  k X R _ p g w r i t e   R e s p o n s e                   */
/******************************************************************************/
  
struct ServerResponseBody_pgWrite {
   kXR_int64 offset;                // info[]: File offset of data written
};


// The following structure is appended to ServerResponseBody_pgWrite if one or
// more checksum errors occurred and need to be retransmitted.
//
struct ServerResponseBody_pgWrCSE {
   kXR_unt32 cseCRC;                // crc32c of all following bits
   kXR_int16 dlFirst;               // Data length at first offset in list
   kXR_int16 dlLast;                // Data length at last  offset in list
// kXR_int64 bof[(dlen-8)/8];       // List of offsets of pages in error
};

/******************************************************************************/
/*                 k X R _ p r o t o c o l   R e s p o n s e                  */
/******************************************************************************/

// The following information is returned in the response body when kXR_bifreqs
// is set in ClientProtocolRequest::flags. Note that the size of bifInfo is
// is variable. This response will not be returned if there are no bif's.
// Note: This structure is null byte padded to be a multiple of 8 bytes!
//
struct ServerResponseBifs_Protocol {
   kXR_char  theTag;      // Always the character 'B' to identify struct
   kXR_char  rsvd;        // Reserved for the future (always 0 for now)
   kXR_unt16 bifILen;     // Length of bifInfo including null bytes.
// kXR_char  bifInfo[bifILen];
};
  
// The following information is returned in the response body when kXR_secreqs
// is set in ClientProtocolRequest::flags. Note that the size of secvec is
// defined by secvsz and will not be present when secvsz == 0.
//
struct ServerResponseSVec_Protocol {
   kXR_char  reqindx;     // Request index
   kXR_char  reqsreq;     // Request signing requirement
};

struct ServerResponseReqs_Protocol {
   kXR_char  theTag;      // Always the character 'S' to identify struct
   kXR_char  rsvd;        // Reserved for the future (always 0 for now)
   kXR_char  secver;      // Security version
   kXR_char  secopt;      // Security options
   kXR_char  seclvl;      // Security level when secvsz == 0
   kXR_char  secvsz;      // Number of items in secvec (i.e. its length/2)
   ServerResponseSVec_Protocol secvec;
};


namespace XrdProto
{
typedef struct ServerResponseBifs_Protocol bifReqs;
typedef struct ServerResponseReqs_Protocol secReqs;
}

// Options reflected in protocol response ServerResponseReqs_Protocol::secopt
//
#define kXR_secOData 0x01
#define kXR_secOFrce 0x02

// Security level definitions (these are predefined but can be over-ridden)
//
#define kXR_secNone       0
#define kXR_secCompatible 1
#define kXR_secStandard   2
#define kXR_secIntense    3
#define kXR_secPedantic   4

// Requirements one of which set in each ServerResponseReqs_Protocol::secvec
//
#define kXR_signIgnore    0
#define kXR_signLikely    1
#define kXR_signNeeded    2

// Version used for kXR_sigver and is set in SigverRequest::version,
// ServerResponseReqs_Protocol::secver
//
#define kXR_secver_0  0
  
// KINDS of SERVERS (no longer used by new clients)
//
#define kXR_DataServer 1
#define kXR_LBalServer 0

// The below are defined for protocol version 2.9.7 or higher
// These are the flag values in the kXR_protool response
//
#define kXR_isManager     0x00000002
#define kXR_isServer      0x00000001
#define kXR_attrMeta      0x00000100
#define kXR_attrProxy     0x00000200
#define kXR_attrSuper     0x00000400
#define kXR_attrVirtRdr   0x00000800

// Virtual options set on redirect
//
#define kXR_recoverWrts   0x00001000
#define kXR_collapseRedir 0x00002000
#define kXR_ecRedir       0x00004000

// Things the server supports
//
#define kXR_anongpf       0x00800000
#define kXR_supgpf        0x00400000
#define kXR_suppgrw       0x00200000
#define kXR_supposc       0x00100000

// TLS requirements
//
#define kXR_haveTLS       0x80000000
#define kXR_gotoTLS       0x40000000
#define kXR_tlsAny        0x1f000000
#define kXR_tlsData       0x01000000
#define kXR_tlsGPF        0x02000000
#define kXR_tlsLogin      0x04000000
#define kXR_tlsSess       0x08000000
#define kXR_tlsTPC        0x10000000
#define kXR_tlsGPFA       0x20000000

// Body for the kXR_protocol response... useful
//
struct ServerResponseBody_Protocol {
   kXR_int32 pval;
   kXR_int32 flags;
   ServerResponseReqs_Protocol secreq; // Only for V3.1.0+ && if requested
};

// Handy definition of the size of the protocol response when the security
// information is not present.
//
#define kXR_ShortProtRespLen sizeof(ServerResponseBody_Protocol)-\
                             sizeof(ServerResponseReqs_Protocol)

/******************************************************************************/
/*                 k X R _ r e d i r e c t   R e s p o n s e                  */
/******************************************************************************/
  
struct ServerResponseBody_Redirect {
   kXR_int32 port;
   char host[4096]; // Should be sufficient for every use
};

/******************************************************************************/
/*                     k X R _ s t a t   R e s p o n s e                      */
/******************************************************************************/

// The following bits are encoded in the "flags" token in the response
//
enum XStatRespFlags {
   kXR_file    = 0,
   kXR_xset    = 1,
   kXR_isDir   = 2,
   kXR_other   = 4,
   kXR_offline = 8,
   kXR_readable=16,
   kXR_writable=32,
   kXR_poscpend=64,
   kXR_bkpexist=128
};
  
/******************************************************************************/
/*                   k X R _ s t a t u s   R e s p o n s e                    */
/******************************************************************************/

struct ServerResponseBody_Status { // Always preceeded by ServerResponseHeader
   kXR_unt32 crc32c;      // IETF RFC 7143 standard
   kXR_char  streamID[2]; // Identical to streamid[2]  in ServerResponseHeader
   kXR_char  requestid;   // requestcode - kXR_1stRequest
   kXR_char  resptype;    // See RespType enum below
   kXR_char  reserved[4];
   kXR_int32 dlen;
// kXR_char  info[ServerResponseHeader::dlen-sizeof(ServerResponseBody_Status)];
// kXR_char  data[dlen];
};

namespace XrdProto
{
enum RespType {

   kXR_FinalResult   = 0x00,
   kXR_PartialResult = 0x01,
   kXR_ProgressInfo  = 0x02
};

   // This is the minimum size of ServerResponseHeader::dlen for kXR_status
   //
   static const int kXR_statusBodyLen = sizeof(ServerResponseBody_Status);
}

struct ServerResponseStatus {
   struct ServerResponseHeader      hdr;
   struct ServerResponseBody_Status bdy;
};
  
/******************************************************************************/
/*                     k X R _ w a i t   R e s p o n s e                      */
/******************************************************************************/
  
struct ServerResponseBody_Wait {
   kXR_int32 seconds;
   char infomsg[4096]; // Should be sufficient for every use
};

/******************************************************************************/
/*                 k X R _ w a i t r e s p   R e s p o n s e                  */
/******************************************************************************/
  
struct ServerResponseBody_Waitresp {
   kXR_int32 seconds;
};

/******************************************************************************/
/*         U n i o n   o f   a l l   S e r v e r   R e s p o n s e s          */
/******************************************************************************/
  
struct ServerResponse
{
  ServerResponseHeader hdr;
  union
  {
    ServerResponseBody_Attn     attn;
    ServerResponseBody_Authmore authmore;
    ServerResponseBody_Bind     bind;
    ServerResponseBody_Buffer   buffer;
    ServerResponseBody_Error    error;
    ServerResponseBody_Login    login;
    ServerResponseBody_Protocol protocol;
    ServerResponseBody_Redirect redirect;
    ServerResponseBody_Status   status;
    ServerResponseBody_Wait     wait;
    ServerResponseBody_Waitresp waitresp;
  } body;
};

// The pgread and pgwrite do not fit the union above because they are composed
// of three structs not two as all the above. So, we define the exceptions here.
//
struct ServerResponseV2
{
  ServerResponseStatus status; // status.bdy and status.hdr
  union
  {
    ServerResponseBody_pgRead   pgread;
    ServerResponseBody_pgWrite  pgwrite;
  } info;
};

struct ALIGN_CHECK {char chkszreq[25-sizeof(ClientRequest)];
   char chkszrsp[ 9-sizeof(ServerResponseHeader)];
};

/******************************************************************************/
/*                   X P r o t o c o l   U t i l i t i e s                    */
/******************************************************************************/

#include <cerrno>
#if defined(WIN32)
#if !defined(ENOTBLK)
#  define ENOTBLK 15
#endif
#if !defined(ETXTBSY)
#define ETXTBSY 26
#endif
#if !defined(ENOBUFS)
#define ENOBUFS 105
#endif
#if !defined(ENETUNREACH)
#define ENETUNREACH 114
#endif
#endif

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#ifndef EBADRQC
#define EBADRQC EBADRPC
#endif

#ifndef EAUTH
#define EAUTH EBADE
#endif

struct stat;
  
class XProtocol
{
public:

// mapError() is the official mapping from errno to xroot protocol error.
//
static int mapError(int rc)
      {if (rc < 0) rc = -rc;
       switch(rc)
          {case ENOENT:        return kXR_NotFound;
           case EINVAL:        return kXR_ArgInvalid;
           case EPERM:         return kXR_NotAuthorized;
           case EACCES:        return kXR_NotAuthorized;
           case EIO:           return kXR_IOError;
           case ENOMEM:        return kXR_NoMemory;
           case ENOBUFS:       return kXR_NoMemory;
           case ENOSPC:        return kXR_NoSpace;
           case ENAMETOOLONG:  return kXR_ArgTooLong;
           case ENETUNREACH:   return kXR_noserver;
           case ENOTBLK:       return kXR_NotFile;
           case ENOTSUP:       return kXR_Unsupported;
           case EISDIR:        return kXR_isDirectory;
           case ENOTEMPTY: [[fallthrough]];
           // In the case one tries to delete a non-empty directory
           // we have decided that until the next major release
           // the kXR_ItExists flag will be returned
           case EEXIST:
                return kXR_ItExists;
           case EBADRQC:       return kXR_InvalidRequest;
           case ETXTBSY:       return kXR_inProgress;
           case ENODEV:        return kXR_FSError;
           case EFAULT:        return kXR_ServerError;
           case EDOM:          return kXR_ChkSumErr;
           case EDQUOT:        return kXR_overQuota;
           case EILSEQ:        return kXR_SigVerErr;
           case ERANGE:        return kXR_DecryptErr;
           case EUSERS:        return kXR_Overloaded;
           case EROFS:         return kXR_fsReadOnly;
           case ENOATTR:       return kXR_AttrNotFound;
           case EPROTOTYPE:    return kXR_TLSRequired;
           case EADDRNOTAVAIL: return kXR_noReplicas;
           case EAUTH:         return kXR_AuthFailed;
           case EIDRM:         return kXR_Impossible;
           case ENOTTY:        return kXR_Conflict;
           case ETOOMANYREFS:  return kXR_TooManyErrs;
           case ETIMEDOUT:     return kXR_ReqTimedOut;
           case EBADF:         return kXR_FileNotOpen;
           case ECANCELED:     return kXR_Cancelled;
           default:            return kXR_FSError;
          }
      }
  
static int toErrno( int xerr )
{
    switch(xerr)
       {case kXR_ArgInvalid:    return EINVAL;
        case kXR_ArgMissing:    return EINVAL;
        case kXR_ArgTooLong:    return ENAMETOOLONG;
        case kXR_FileLocked:    return EDEADLK;
        case kXR_FileNotOpen:   return EBADF;
        case kXR_FSError:       return ENODEV;
        case kXR_InvalidRequest:return EBADRQC;
        case kXR_IOError:       return EIO;
        case kXR_NoMemory:      return ENOMEM;
        case kXR_NoSpace:       return ENOSPC;
        case kXR_NotAuthorized: return EACCES;
        case kXR_NotFound:      return ENOENT;
        case kXR_ServerError:   return EFAULT;
        case kXR_Unsupported:   return ENOTSUP;
        case kXR_noserver:      return EHOSTUNREACH;
        case kXR_NotFile:       return ENOTBLK;
        case kXR_isDirectory:   return EISDIR;
        case kXR_Cancelled:     return ECANCELED;
        case kXR_ItExists:      return EEXIST;
        case kXR_ChkSumErr:     return EDOM;
        case kXR_inProgress:    return EINPROGRESS;
        case kXR_overQuota:     return EDQUOT;
        case kXR_SigVerErr:     return EILSEQ;
        case kXR_DecryptErr:    return ERANGE;
        case kXR_Overloaded:    return EUSERS;
        case kXR_fsReadOnly:    return EROFS;
        case kXR_BadPayload:    return EINVAL;
        case kXR_AttrNotFound:  return ENOATTR;
        case kXR_TLSRequired:   return EPROTOTYPE;
        case kXR_noReplicas:    return EADDRNOTAVAIL;
        case kXR_AuthFailed:    return EAUTH;
        case kXR_Impossible:    return EIDRM;
        case kXR_Conflict:      return ENOTTY;
        case kXR_TooManyErrs:   return ETOOMANYREFS;
        case kXR_ReqTimedOut:   return ETIMEDOUT;
        default:                return ENOMSG;
       }
}

static const char *errName(kXR_int32 errCode);

static const char *reqName(kXR_unt16 reqCode);

/******************************************************************************/
/*                  O b s o l e t e   D e f i n i t i o n s                   */
/******************************************************************************/

struct ServerResponseBody_Attn_asyncdi { // No longer supported
   kXR_int32 actnum;
   kXR_int32 wsec;
   kXR_int32 msec;
};

struct ServerResponseBody_Attn_asyncrd { // No longer supported
   kXR_int32 actnum;
   kXR_int32 port;
   char host[4092];
};

struct ServerResponseBody_Attn_asyncwt { // No longer supported
   kXR_int32 actnum;
   kXR_int32 wsec;
};

// Kind of error inside a XTNetFile's routine (temporary)
//
enum XReqErrorType {
   kGENERICERR = 0,    // Generic error
   kREAD,              // Error while reading from stream
   kWRITE,             // Error while writing to stream
   kREDIRCONNECT,      // Error redirecting to a given host
   kOK,                // Everything seems ok
   kNOMORESTREAMS      // No more available stream IDs for
                       // async processing
};

typedef kXR_int32 ServerResponseType;

#define kXR_maxReqRetry 10

}; // XProtocol
#endif
