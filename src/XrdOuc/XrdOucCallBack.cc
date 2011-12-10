/******************************************************************************/
/*                                                                            */
/*                     X r d O u c C a l l B a c k . c c                      */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdOuc/XrdOucCallBack.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                                C a n c e l                                 */
/******************************************************************************/
  
void XrdOucCallBack::Cancel()
{

// If a callback is outstanding, send a reply indicating that the operation
// should be retried.
//
   if (cbObj) Reply(1, 0, "");
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdOucCallBack::Init(XrdOucErrInfo *eInfo)
{
   const char *theUser;


// Make sure we can do a callback here
//
   if (cbObj || eInfo->getErrCB() == 0) return 0;

// Copy relevant data
//
   if ((theUser = eInfo->getErrUser())) strlcpy(UserID,theUser,sizeof(UserID));
      else strcpy(UserID, "???");
   cbObj = eInfo->getErrCB(cbArg);

// Now set the callback object in the input ErrInfo object to be ours so
// that we can make sure that the wait for callback response was sent
// before we actually effect a reply.
//
   eInfo->setErrCB(this, cbArg);

// All done
//
   return 1;
}

/******************************************************************************/
/*                                 R e p l y                                  */
/******************************************************************************/
  
int XrdOucCallBack::Reply(int retVal, int eValue, const char *eText)
{
   XrdOucErrInfo cbInfo(UserID, this, cbArg);
   XrdOucEICB *objCB;

// Verify that we can actually do a callback
//
   if (!(objCB = cbObj)) return 0;
   cbObj = 0;

// Wait for the semaphore to make sure the "wait for callback" response was
// actually sent to preserve time causality.
//
   cbSync.Wait();

// Send the reply using the constructed ErrInfo object and then wait until we
// know that the response was actually sent to allow this object to be deleted.
//
   objCB->Done(retVal, &cbInfo);
   cbSync.Wait();

// All done
//
   return 1;
}
