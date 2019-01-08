#ifndef __XRDSSIPACER_HH__
#define __XRDSSIPACER_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i P a c e r . h h                         */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "Xrd/XrdJob.hh"
#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiRequest.hh"

class XrdSsiPacer : public XrdJob
{
public:

void           DoIt() {Redrive();}

void           Hold(const char *reqID=0);

void           Q_Insert(XrdSsiPacer *Node)
                       {Node->next  = next;        // Chain in the item;
                        next->prev  = Node;
                        next        = Node;
                        Node->prev  = this;
                        theQ->qCnt++;
                       }

void           Q_Remove()
                       {prev->next = next;        // Unchain the item
                        next->prev = prev;
                        next       = this;
                        prev       = this;
                        theQ->qCnt--;
                       }

void           Q_PushBack(XrdSsiPacer *Node) {prev->Q_Insert(Node);}

virtual void   Redrive() {}            // Meant to be overridden

virtual
const char    *RequestID() {return 0;} // Meant to be overridden

void           Reset();

static void    Run(XrdSsiRequest::RDR_Info &rInfo,
                   XrdSsiRequest::RDR_How   rhow, const char *reqid=0);

bool           Singleton() {return next == this;}

               XrdSsiPacer() : prev(this), next(this), theQ(this),
                               qCnt(0),    aCnt(0) {}
virtual       ~XrdSsiPacer() {Reset();}

private:

static XrdSsiMutex  pMutex;
static XrdSsiPacer  glbQ;
XrdSsiPacer        *prev;
XrdSsiPacer        *next;
XrdSsiPacer        *theQ;
int                 qCnt;
int                 aCnt;
};
#endif
