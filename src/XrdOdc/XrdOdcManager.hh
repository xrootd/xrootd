#ifndef __ODC_MANAGER__
#define __ODC_MANAGER__
/******************************************************************************/
/*                                                                            */
/*                      X r d O d c M a n a g e r . h h                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//          $Id$

#include <sys/uio.h>

#include "XrdOuc/XrdOucLink.hh"
#include "XrdOuc/XrdOucPthread.hh"

class XrdOucError;
class XrdOucLogger;
class XrdOucNetwork;

class XrdOdcManager
{
public:


int            isActive() {return Active;}

XrdOdcManager *nextManager() {return Next;}

char          *Name() {return Host;}

int            Send(char *msg, int mlen=0);
int            Send(const struct iovec *iov, int iovcnt);

void           setTID(pthread_t tid) {mytid = tid;}

void          *Start();

void           setNext(XrdOdcManager *np) {Next = np;}

               XrdOdcManager(XrdOucError *erp, char *host, int port, int cw);
              ~XrdOdcManager();

private:
void  Hookup();
void  Sleep(int slpsec);
char *Receive(int &msgid);

XrdOdcManager *Next;
XrdOucMutex    myData;
XrdOucNetwork *Network;
XrdOucError   *eDest;
XrdOucLink    *Link;
char          *Host;
int            Port;
pthread_t      mytid;
int            dally;
int            Active;
};
#endif
