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
/******************************************************************************/
  
//       $Id$

#include <string.h>
#include <time.h>

// The following is the packet header and is always unencrypted.
//
struct XrdSecsssRR
{
struct HDR
{
char      ProtID[4];                 // Protocol ID ("sss")
char      Pad[3];                    // Padding bytes
char      EncType;                   // Encryption type (see etxxx below)
long long KeyID;                     // Key ID for encryption
}         Hdr;

// Valid values for Hdr.EncType
//
static const char etBFish32 = '0';   // Blowfish

// The data portion of the packet is encrypted with the private shared key
// It immediately follows the header and has a maximum size (defined here).
//
union
{
unsigned char Info[2048];            // What gets encrypted

struct DAT
{
char      Rand[32];                  // 256-bit random string (avoid text attacks)
int       GenTime;                   // Time data generated   (time(0) - BaseTime)
char      Pad[3];                    // Reserved
char      Options;                   // See Snd/Usexxxx below
char      Data[2008];                // Data: (<Flag><packed full string>)+
}         Pkt;
};

// Valid option values in Pkt.Options
//
static const char UseData= 0x00;     // Use the ID data  as authenticated name
static const char SndLID = 0x01;     // Server to send login ID
static const char UseLID = 0x02;     // Use the login ID as authenticated name

// Valid flag values for packed full strings:
//
static const char theName = 0x01;
static const char theVorg = 0x02;
static const char theRole = 0x03;
static const char theGrps = 0x04;
static const char theLgid = 0x05;
static const char theRand = 0x06; // Random string (ignored)
};
#endif
