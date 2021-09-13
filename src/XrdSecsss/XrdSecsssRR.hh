#ifndef __SecsssRR__
#define __SecsssRR__
/******************************************************************************/
/*                                                                            */
/*                        X r d S e c s s s R R . h h                         */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <cstdint>
#include <cstring>
#include <ctime>

#include "XrdSecsss/XrdSecsssKT.hh"

// The following is the packet header and is always unencrypted.
//
struct XrdSecsssRR_Hdr
{
char      ProtID[4];                 // Protocol ID ("sss")
char      Pad[2];                    // Padding bytes
uint8_t   knSize;                    // Appended keyname size w/ null byte
char      EncType;                   // Encryption type as one of:
static const char etBFish32 = '0';   // Blowfish

long long KeyID;                     // Key ID for encryption
};

// Following this struct extends the original V1 struct with the keyname. V2
// clients send the extended header to v2 servers. It must be a multiple of
// 8 bytes and end with a null byte. Keynames have a maximum size as defined
// in XrdSecsssKT. The keyname qualifies the lookup of the KeyID.
//
struct XrdSecsssRR_Hdr2 : XrdSecsssRR_Hdr
{
char      keyName[XrdSecsssKT::ktEnt::NameSZ];
};

// The data portion of the packet is encrypted with the private shared key
// It immediately follows the header and has a maximum size (defined here).
//
struct XrdSecsssRR_DataHdr
{
char      Rand[32];                  // 256-bit random string (avoid text attacks)
int       GenTime;                   // Time data generated   (time(0) - BaseTime)
char      Pad[3];                    // Reserved
char      Options;                   // One of the following:
static const char UseData= 0x00;     // Use the ID data  as authenticated name
static const char SndLID = 0x01;     // Server to send login ID
static const char Ask4Mor= 0x02;     // Ask for additional data (future)
// Note: A variable length data portion follows the header
};

static const int  XrdSecsssRR_Data_HdrLen = sizeof(XrdSecsssRR_DataHdr);

struct XrdSecsssRR_Data : XrdSecsssRR_DataHdr
{
static const int  MaxCSz = 2048;     // Maximum size of returned credentials
static const int  MaxDSz =16344;     // Maximum size of v2 inline data
static const int  MinDSz =  128;     // Minimum size for the data segment
static const int  DataSz = 4040;     // Maximum size of V1 inline data
char      Data[DataSz];              // Optional V1 data (only for back compat)

//           (<Flag><packed null terminated string>)+
//
static const char theName = 0x01; // V1 and V2
static const char theVorg = 0x02; // V1 and V2
static const char theRole = 0x03; // V1 and V2
static const char theGrps = 0x04; // V1 and V2
static const char theEndo = 0x05; // V1 and V2
static const char theCred = 0x06; // V2: Actual credentials
static const char theRand = 0x07; // V1 and V2: Random string (ignored)

static const char theAuth = 0x08; // V2: original authentication protocol
static const char theTID  = 0x09; // V2: The trace ID
static const char theAKey = 0x0a; // V2: attribute key
static const char theAVal = 0x0b; // V2: attribute value for preceding key
static const char theUser = 0x0c; // V2: the Unix user  name (original)
static const char theGrup = 0x0d; // V2: the Unix group name (original)
static const char theCaps = 0x0e; // V2: the x509 capabilities

static const char theLgid = 0x10; // from server only
static const char theHost = 0x20; // from client only (required)
};

// Struct used to effect a short response from the server
//
struct XrdSecsssRR_DataResp : XrdSecsssRR_DataHdr
{
char      Data[XrdSecsssRR_Data::MinDSz + 16];
};
#endif
