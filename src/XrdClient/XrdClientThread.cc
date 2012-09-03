/******************************************************************************/
/*                                                                            */
/*                  X r d C l i e n t T h r e a d . c c                       */
/*                                                                            */
/* Author: F.Furano (INFN, 2005)                                              */
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// An user friendly thread wrapper                                      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include <pthread.h>
#include <signal.h>

#include "XrdClient/XrdClientThread.hh"

//_____________________________________________________________________________
void * XrdClientThreadDispatcher(void * arg)
{
   // This function is launched by the thread implementation. Its purpose
   // is to call the actual thread body, passing to it the original arg and
   // a pointer to the thread object which launched it.

   XrdClientThread::XrdClientThreadArgs *args = (XrdClientThread::XrdClientThreadArgs *)arg;

   args->threadobj->SetCancelDeferred();
   args->threadobj->SetCancelOn();

   if (args->threadobj->ThreadFunc)
      return args->threadobj->ThreadFunc(args->arg, args->threadobj);

   return 0;

}

//_____________________________________________________________________________
int XrdClientThread::MaskSignal(int snum, bool block)
{
   // Modify masking for signal snum: if block is true the signal is blocked,
   // else is unblocked. If snum <= 0 (default) all the allowed signals are
   // blocked / unblocked.
#ifndef WIN32
   sigset_t mask;
   int how = block ? SIG_BLOCK : SIG_UNBLOCK;
   if (snum <= 0)
      sigfillset(&mask);
      else sigaddset(&mask, snum);
   return pthread_sigmask(how, &mask, 0);
#else
   return 0;
#endif
}


