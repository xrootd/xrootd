/******************************************************************************/
/*                                                                            */
/*                       X r d S y s X S L o c k . c c                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysXSLock.hh"

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSysXSLock::~XrdSysXSLock()
{

// Prevent usage while destroying object but make sure no one else is using it
//
   LockContext.Lock();
   if (cur_count || shr_wait || exc_wait)
      {LockContext.UnLock();
       abort();
      }
   LockContext.UnLock();
}

/******************************************************************************/
/*                                  L o c k                                   */
/******************************************************************************/
  
void XrdSysXSLock::Lock(const XrdSysXS_Type usage)
{

// Serialize access to this object
//
   LockContext.Lock();

// This loop continues until we can acquire the resource. We are gauranteed
// to eventually acquire it regardless of the unblocking order.
//
   while(cur_count)
        {
        // If usage is compatible with current usage get the lock right away
        //
           if (usage == xs_Shared && cur_usage == xs_Shared && !exc_wait) break;

        // Indicate that we are waiting
        //
           if (usage == xs_Shared) shr_wait++;
              else exc_wait++;

        // Usage is not compatible. We must wait for current lock mode to end
        //
           LockContext.UnLock();
           if (usage == xs_Shared) WantShr.Wait();
              else WantExc.Wait();
           LockContext.Lock();
        }

// We obtained the right to use this object
//
   cur_usage = usage;
   cur_count++;
   LockContext.UnLock();
}

/******************************************************************************/
/*                                U n L o c k                                 */
/******************************************************************************/
  
void XrdSysXSLock::UnLock(const XrdSysXS_Type usage)
{

// Serialize access to our data
//
   LockContext.Lock();

// Make sure that the lock is currently being used
//
   if (!cur_count)
      {LockContext.UnLock();
       std::cerr << "XSLock: Attempt to unlock inactive lock." <<std::endl;
       throw "XSLock: unlocking inactive lock.";
      }

// Verify that usage is correct
//
   if (usage && cur_usage != usage)
      {LockContext.UnLock();
       std::cerr << "XSLock: Incorrect unlock usage - "
            <<  (cur_usage == xs_Shared ? "shr" : "exc") << "!="
            <<  (    usage == xs_Shared ? "shr" : "exc") << std::endl;
       throw "XSLock: invalid unlock usage specified.";
      }

// Unlock the current object. If no locks exist then check if we can let another
// thread use this object. The logic is tricky but we are trying to avoid
// starvation in an environment that has no thread ordering.
//
   cur_count--;
   if (!cur_count)
      if (exc_wait && (toggle || !shr_wait)) 
                             {toggle = 0; WantExc.Post(); exc_wait--;}
         else           {while(shr_wait) {WantShr.Post(); shr_wait--;}
                              toggle = 1;}
      else if (!toggle) {while(shr_wait) {WantShr.Post(); shr_wait--;}
                              toggle = 1;}

   LockContext.UnLock();
}
