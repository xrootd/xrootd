/******************************************************************************/
/*                                                                            */
/*                        X r d C m s S t a t e . c c                         */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

// Original Version: 1.3 2006/04/05 02:28:09 abh

const char *XrdCmsStateCVSID = "$Id$";

#include "XProtocol/YProtocol.hh"

#include "Xrd/XrdLink.hh"

#include "XrdCms/XrdCmsManager.hh"
#include "XrdCms/XrdCmsRTable.hh"
#include "XrdCms/XrdCmsState.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdSys/XrdSysError.hh"

using namespace XrdCms;
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
XrdCmsState XrdCms::CmsState;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCmsState::XrdCmsState() : mySemaphore(0)
{
   minNodeCnt = 1;
   numActive  = 0;
   numStaging = 0;
   curState   = All_NoStage | All_Suspend;
   Changes    = 0;
   Suspended  = 1;
   Enabled    = 0;
   Disabled   = 1;
}
 
/******************************************************************************/
/*                                  C a l c                                   */
/******************************************************************************/

void XrdCmsState::Calc(int add2Activ, int add2Stage)
{
  int newState, newChanges;

// Calculate new state
//
   myMutex.Lock();
   numStaging += add2Stage;
   numActive  += add2Activ;
   newState = (numActive  < minNodeCnt ? All_Suspend : 0) |
              (numStaging ? 0 : All_NoStage);

// If any changes are noted then we must notify all our managers
//
   if ((newChanges = (newState ^ curState)))
      {curState = newState;
       Changes |= newChanges;
       mySemaphore.Post();
      }

// All done
//
   myMutex.UnLock();
}
 
/******************************************************************************/
/*                                E n a b l e                                 */
/******************************************************************************/
  
void XrdCmsState::Enable(char *theState)
{
   EPNAME("Enable");
   CmsStatusRequest myStatus = {{0, kYR_status, 0, 0}};

// Allow state changes to be refelected. By default we will not be suspended.
// Always reflect the current state when we become enabled.
//
   myMutex.Lock();
   Enabled = 1;
   Suspended = Disabled;
   if (curState & All_Suspend)
      {myStatus.Hdr.modifier  = CmsStatusRequest::kYR_Suspend;
       if (theState) strcpy(theState, "suspended ");
      } else {
       myStatus.Hdr.modifier  = CmsStatusRequest::kYR_Resume;
       if (theState) strcpy(theState, "active ");
      }
   if (curState & All_NoStage)
      {myStatus.Hdr.modifier |= CmsStatusRequest::kYR_noStage;
       if (theState) strcat(theState, "+ nostage");
      } else {
       myStatus.Hdr.modifier |= CmsStatusRequest::kYR_Stage;
       if (theState) strcat(theState, "+ stage");
      }
   DEBUG("Sending status " <<(theState ? theState : ""));
   Manager.Inform(myStatus.Hdr);
   RTable.Send((char *)&myStatus, sizeof(myStatus));
   myMutex.UnLock();
}

/******************************************************************************/
/*                               M o n i t o r                                */
/******************************************************************************/
  
void *XrdCmsState::Monitor()
{
   EPNAME("Monitor");
   CmsStatusRequest myStatus = {{0, kYR_status, 0, 0}};
   const char *SRstate, *SNstate;
   int rrModifier;

// Do this forever
//
   do {mySemaphore.Wait();
       myMutex.Lock();
       myStatus.Hdr.modifier = 0;
       if (curState & All_Suspend)
          {rrModifier = CmsStatusRequest::kYR_Suspend;
           SRstate = "suspended";
           Disabled = 1;
          } else {
           rrModifier = CmsStatusRequest::kYR_Resume;
           SRstate = "active";
           Disabled = 0;
          }

       if (Changes & All_Suspend) 
          {myStatus.Hdr.modifier = rrModifier;
           if (Enabled)
              {DEBUG("Sending redirectors status " <<SRstate);
               RTable.Send((char *)&myStatus, sizeof(myStatus));
              }
//         myStatus.Hdr.modifier = 0;
          }

       if (curState & All_NoStage)
          {rrModifier |= CmsStatusRequest::kYR_noStage;
           SNstate = "+ nostaging";
          } else {
           rrModifier |= CmsStatusRequest::kYR_Stage;
           SNstate = "+ staging";
          }

       if (Changes & All_NoStage) myStatus.Hdr.modifier |= rrModifier;

       if (myStatus.Hdr.modifier)
          { if (Enabled)
               {Suspended = Disabled;
                DEBUG("Sending managers status " <<SRstate);
                Manager.Inform(myStatus.Hdr);
               }
           Say.Emsg("State", "Status changed to", SRstate, SNstate);
          }
       Changes = 0;
       myMutex.UnLock();
      } while(1);

// All done
//
   return (void *)0;
}
  
/******************************************************************************/
/*                             s e n d S t a t e                              */
/******************************************************************************/
  
void XrdCmsState::sendState(XrdLink *lp)
{
   CmsStatusRequest myStatus = {{0, kYR_status, 0, 0}};

   myMutex.Lock();
   myStatus.Hdr.modifier  = Suspended
                          ? CmsStatusRequest::kYR_Suspend
                          : CmsStatusRequest::kYR_Resume;

   myStatus.Hdr.modifier |= (curState & All_NoStage)
                          ? CmsStatusRequest::kYR_noStage
                          : CmsStatusRequest::kYR_Stage;

   lp->Send((char *)&myStatus.Hdr, sizeof(myStatus.Hdr));
   myMutex.UnLock();
}
