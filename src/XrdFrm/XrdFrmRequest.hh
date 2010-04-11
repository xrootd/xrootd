#ifndef __FRMREQUEST_H__
#define __FRMREQUEST_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d F r m R e q u e s t . h h                       */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

class XrdFrmRequest
{
public:

char      LFN[2176];    // Logical File Name (optional '\0' opaque)
char      User[256];    // User trace identifier
char      ID[64];       // Request ID
char      Notify[512];  // Notification path
char      Reserved[36]; // Reserved for future
char      OPc[2];       // Operation code (debugging)
short     LFO;          // Offset to lfn in url if LFN is a url (o/w 0)
long long addTOD;       // Time added to queue
int       This;         // Offset to this request
int       Next;         // Offset to next request
int       Opaque;       // Offset to '?' in LFN if exists, 0 o/w
short     Options;      // Processing options (see class definitions)
short     Prty;         // Request priority

static const int msgFail  = 0x0001;
static const int msgSucc  = 0x0002;
static const int makeRW   = 0x0004;
static const int Migrate  = 0x0010;
static const int Purge    = 0x0020;

static const int gpfReq   = 0;
static const int migReq   = 1;
static const int pmgReq   = 2;
static const int maxPrty  = 2;
static const int maxPQE   = 3;
};
#endif
