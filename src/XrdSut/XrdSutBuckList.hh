#ifndef __SUT_BUCKLIST_H__
#define __SUT_BUCKLIST_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d S u t B u c k L i s t . h h                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Gerri Ganis for CERN                                         */
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

#ifndef __SUT_BUCKET_H__
#include "XrdSut/XrdSutBucket.hh"
#endif

/******************************************************************************/
/*                                                                            */
/*  Light single-linked list for managing buckets inside the exchanged        */
/*  buffer                                                                    */
/*                                                                            */
/******************************************************************************/

//
// Node definition
//
class XrdSutBuckListNode {
private:
   XrdSutBucket       *buck;
   XrdSutBuckListNode *next;
public:
   XrdSutBuckListNode(XrdSutBucket *b = 0, XrdSutBuckListNode *n = 0)
        { buck = b; next = n;}
   virtual ~XrdSutBuckListNode() { }
   
   XrdSutBucket       *Buck() const { return buck; }

   XrdSutBuckListNode *Next() const { return next; }

   void SetNext(XrdSutBuckListNode *n) { next = n; }
};

class XrdSutBuckList {

private:
   XrdSutBuckListNode *begin;
   XrdSutBuckListNode *current;
   XrdSutBuckListNode *end;
   XrdSutBuckListNode *previous;
   int                 size;

   XrdSutBuckListNode *Find(XrdSutBucket *b);

public:
   XrdSutBuckList(XrdSutBucket *b = 0);
   virtual ~XrdSutBuckList();

   // Access information
   int                 Size() const { return size; }
   XrdSutBucket       *End() const { return end->Buck(); }

   // Modifiers
   void                PutInFront(XrdSutBucket *b);
   void                PushBack(XrdSutBucket *b);
   void                Remove(XrdSutBucket *b);
   
   // Pseudo - iterator functionality
   XrdSutBucket       *Begin();
   XrdSutBucket       *Next();
};

#endif

