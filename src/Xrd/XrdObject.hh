#ifndef __XRD_OBJECT_H__
#define __XRD_OBJECT_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d O b j e c t . h h                           */
/*                                                                            */
/*(c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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

#include <cstring>
#include <strings.h>
#include <ctime>
#include <sys/types.h>

#include "Xrd/XrdJob.hh"

// The classes here are templates for singly linked list handling that allows
// elements to be added to either end but be removed only from the front. Most
// objects in this package are managed in queues of this type.
  
/******************************************************************************/
/*                            x r d _ O b j e c t                             */
/******************************************************************************/

template <class T>
class XrdObjectQ;
  
template <class T>
class XrdObject
{
public:
friend class XrdObjectQ<T>;


// Item() supplies the item value associated with itself (used with Next()).
//
T             *objectItem() {return Item;}

// Next() supplies the next list node.
//
XrdObject<T> *nextObject() {return Next;}

// Set the item pointer
//
void           setItem(T *ival) {Item = ival;}

               XrdObject(T *ival=0) {Next = 0; Item = ival; QTime = 0;}
              ~XrdObject() {}

private:
XrdObject<T> *Next;
T             *Item;
time_t         QTime;  // Only used for time-managed objects
};

/******************************************************************************/
/*                           x r d _ O b j e c t Q                            */
/******************************************************************************/
  
// Note to properly cleanup this type of queue you must call Set() at least
// once to cause the time element to be sceduled.

class XrdSysTrace;
class XrdScheduler;
  
template <class T>
class XrdObjectQ : public XrdJob
{
public:

inline T      *Pop() {XrdObject<T> *Node;
                      QMutex.Lock();
                      if ((Node = First)) {First = First->Next; Count--;}
                      QMutex.UnLock();
                      if (Node) return Node->Item;
                      return (T *)0;
                     }

inline void    Push(XrdObject<T> *Node)
                     {Node->QTime = Curage;
                      QMutex.Lock();
                      if (Count >= MaxinQ) delete Node->Item;
                         else {Node->Next = First;
                               First = Node;
                               Count++;
                              }
                      QMutex.UnLock();
                     }

       void    Set(int inQMax, time_t agemax=1800);

       void    Set(XrdScheduler *sp, XrdSysTrace *tp, int TraceChk=0)
                      {Sched = sp; Trace = tp; TraceON = TraceChk;}

       void    DoIt();

       XrdObjectQ(const char *id, const char *desc) : XrdJob(desc)
                  {Curage = Count = 0; Maxage = 0; TraceID = id;
                   MaxinQ = 32; MininQ = 16; First = 0;
                  }

      ~XrdObjectQ() {}

private:

XrdSysMutex    QMutex;
XrdObject<T>  *First;
int            Count;
int            Curage;
int            MininQ;
int            MaxinQ;
time_t         Maxage;
XrdScheduler  *Sched;
XrdSysTrace   *Trace;
int            TraceON;
const char    *TraceID;
};

#include "Xrd/XrdObject.icc"
#endif
