#ifndef __OUC_BUFF__
#define __OUC_BUFF__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c B u f f e r . h h                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//          $Id$

#include <stdlib.h>

#include "XrdOuc/XrdOucChain.hh"
#include "XrdOuc/XrdOucPthread.hh"

class XrdOucBuffer
{
public:
       char         *data;
       int           dlen;

static XrdOucBuffer *Alloc(void);

inline int           BuffSize(void) {return size;}

       char         *Token(char **rest=0);

       void          Recycle(void);

       void          Reset(void) {dpnt = 0;}

static void          Set(int maxb);

      XrdOucBuffer();
     ~XrdOucBuffer() {if (data) free(data);}

private:

int retErr(int ecode);

static XrdOucMutex               BuffList;
static XrdOucStack<XrdOucBuffer> BuffStack;
static int                       pagsz;
static int                       alignit;
static int                       maxbuff;
static int                       numbuff;
static int                       size;

XrdOucQSItem<XrdOucBuffer> BuffLink;
char                    *dpnt;
};
#endif
