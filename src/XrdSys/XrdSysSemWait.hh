#ifndef __SYS_SEMWAIT__
#define __SYS_SEMWAIT__

/******************************************************************************/
/*                       X r d S y s S e m W a i t                            */
/*                                                                            */
/* Author: Fabrizio Furano (INFN, 2005)                                       */
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
/*                                                                            */
/* A counting semaphore with timed out wait primitive                         */
/******************************************************************************/

#include "XrdSys/XrdSysPthread.hh"

class XrdSysSemWait {
 public:

   int  CondWait() {

      int rc = 0;
      // Wait until the sempahore value is positive. This will not be starvation
      // free is the OS implements an unfair mutex;
      // Returns 0 if signalled, non-0 if would block
      //

      semVar.Lock();
      if (semVal > 0) semVal--;
      else {
	 rc = 1;
      }

      semVar.UnLock();

      return rc;

   };
   
   void Post() {
      // Add one to the semaphore counter. If we the value is > 0 and there is a
      // thread waiting for the sempagore, signal it to get the semaphore.
      //
      semVar.Lock();

      if (semWait > 0) {
	 semVar.Signal();
	 semWait--;
      }
      else
	 semVal++;
      
      semVar.UnLock();
   };
   
   void Wait()   {
      // Wait until the sempahore value is positive. This will not be starvation
      // free is the OS implements an unfair mutex;
      //

      semVar.Lock();
      if (semVal > 0) semVal--;
      else {
	 semWait++;
	 semVar.Wait();
      }

      semVar.UnLock();

   };

   int Wait(int secs)  {
      int rc = 0;
      // Wait until the sempahore value is positive. This will not be starvation
      // free is the OS implements an unfair mutex;
      // Returns 0 if signalled, non-0 if timeout
      //

      semVar.Lock();
      if (semVal > 0) semVal--;
      else {
	 semWait++;
	 rc = semVar.Wait(secs);
	 if (rc) semWait--;
      }

      semVar.UnLock();

      return rc;
   };

   XrdSysSemWait(int semval=1,const char *cid=0) : semVar(0, cid) {
      semVal = semval; semWait = 0;
   }

   ~XrdSysSemWait() {}

private:

XrdSysCondVar semVar;
int           semVal;
int           semWait;
};



#endif
