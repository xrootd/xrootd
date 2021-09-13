/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S c a l e . c c                         */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>

#include "XrdSsi/XrdSsiScale.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
extern XrdSysError   Log;
}

using namespace XrdSsi;
  
/******************************************************************************/
/*                                g e t E n t                                 */
/******************************************************************************/
  
int XrdSsiScale::getEnt()
{
   static const int tuneMLen = 80;
   char tuneMsg[tuneMLen]; *tuneMsg = 0;

// We assign channel in a round-robbin fashion. This can be across all
// channels or only the subset that was recently added due to tuning. Note
// that we apply tuning only if we are working in the partitioned channels
// as the non-partitioned channels should no longer be getting requests.
//
   entMutex.Lock();
   uint16_t endEnt = curSpread;
   do {for (uint16_t i = nowEnt; i < endEnt; i++)
           {if (pendCnt[i] < maxPend)
               {pendCnt[i]++;
                nowEnt = i+1;
                if (!begEnt || i < begEnt) Active++;
                   else reActive++;
                entMutex.UnLock();
                if (*tuneMsg) Log.Emsg("Scale", tuneMsg);
                return int(i);
               }
           }

    // If we did a whole round and cannot autotune up, then we have failed.
    // If we didn't do the whole round or did tune, try again.
    //
       if (nowEnt == begEnt)
          {if (!autoTune || !Tune(tuneMsg, tuneMLen)) break;
           endEnt = curSpread;
          } else endEnt = nowEnt;
       nowEnt = begEnt;
      } while(true);

// We have no more stream resources left.
//
   entMutex.UnLock();
   if (*tuneMsg) Log.Emsg("Scale", tuneMsg);
   return -1;
}

/******************************************************************************/
/*                                r e t E n t                                 */
/******************************************************************************/

void XrdSsiScale::retEnt(int xEnt)
{

// Perform action only if the specified channel is valid. Retune the channels
// if necessary.
//
   if (xEnt >= 0 && xEnt < int(maxSprd))
      {entMutex.Lock();
       if (pendCnt[xEnt])
          {pendCnt[xEnt]--;
           if (!begEnt || xEnt < (int)begEnt)
              {if (Active) Active--;
               if (begEnt && needTune && Active <= (reActive + (reActive>>1)))
                  {Retune();  // Unlocks the entMutex!
                   return;
                  }
              } else if (reActive) reActive--;
          }
      }

// Unlock the mutex as we are done.
//
   entMutex.UnLock();
}
  
/******************************************************************************/
/* private:                       R e t u n e                                 */
/******************************************************************************/
  
void XrdSsiScale::Retune() // entMutex must be held is it released upon return!
{

// We only want to be called once per expansion.
//
   needTune = false;

// We combine the partioned the channel set with the previous set to increase
// to total available spread.
//
   if (begEnt)
      {uint32_t totReq = Active + reActive, spread = curSpread;
       char buff[80];
       Active   = totReq;
       reActive = 0;
       begEnt   = 0;

    // Issue message about this (don't want to hold the mutex for the msg).
    //
       entMutex.UnLock();
       snprintf(buff, sizeof(buff), "retune %u requests; spread %u",
                totReq, spread);
       Log.Emsg("Scale", buff);
      } else entMutex.UnLock();
}

/******************************************************************************/
/*                                r s v E n t                                 */
/******************************************************************************/
  
bool XrdSsiScale::rsvEnt(int xEnt)
{

// If the channel number is within range see if we can reserve a slot.
//
   if (xEnt >= 0 && xEnt < int(maxSprd))
      {entMutex.Lock();
       if (pendCnt[xEnt] < maxPend)
          {pendCnt[xEnt]++;
           entMutex.UnLock();
           return true;
          }
       entMutex.UnLock();
      }
   return false;
}

/******************************************************************************/
/*                             s e t S p r e a d                              */
/******************************************************************************/
  
void XrdSsiScale::setSpread(short sval)
{
   entMutex.Lock();

   if (sval <= 0)
      {autoTune = true;
       if (sval < 0) sval = -sval;
      } else {
       autoTune = needTune = false;
       begEnt = 0;
      }

   if (sval)
      {uint16_t newSpread;
       if (sval < short(maxSprd)) newSpread = static_cast<uint16_t>(sval);
          else newSpread = maxSprd;
       if (autoTune && newSpread < curSpread)
          {needTune = false;
           begEnt = 0;
          }
       curSpread = newSpread;
      }

   entMutex.UnLock();
}

/******************************************************************************/
/* Private:                         T u n e                                   */
/******************************************************************************/
  
bool XrdSsiScale::Tune(char *buff, int blen) // entMutex must be held!
{
   uint16_t n;

// We can only tune up the maximum allowed.
//
   if (curSpread >= maxSprd)
      {begEnt = 0;
       autoTune = needTune = false;
       return false;
      }

// Compute the number of additional channels we should have. This number
// doubles up until midTune at which point it grows linearly with one bump.
//
   if (curSpread < midTune) n = curSpread << 1;
      else if (curSpread < zipTune) n = curSpread + midTune;
              else n = curSpread + maxTune;

// If we topped out and we do not have enough new channels then turn auto
// tuning off and let it rip as it doesn't matter at this point.
//
   needTune = true;
   if (n <= maxSprd) nowEnt = begEnt = curSpread;
      else {if ((curSpread - maxSprd) < minTune)
               {begEnt = 0;
                autoTune = needTune = false;
               }
            n = maxSprd;
           }

// Adjust values to correspond to the new reality
//
   curSpread = n;
   Active   += reActive;
   reActive  = 0;

// Document what happened. The caller displays the message when mutex unlocked.
//
   snprintf(buff, blen, "tune %u requests; spread %u/%u", Active, n-begEnt, n);
   return true;
}
