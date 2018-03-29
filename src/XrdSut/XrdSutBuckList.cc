
/******************************************************************************/
/*                                                                            */
/*                     X r d S u t B u c k L i s t . c c                      */
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

#include "XrdSut/XrdSutBuckList.hh"

/******************************************************************************/
/*                                                                            */
/*  Light single-linked list for managing buckets inside the exchanged        */
/*  buffer                                                                    */
/*                                                                            */
/******************************************************************************/

//___________________________________________________________________________
XrdSutBuckList::XrdSutBuckList(XrdSutBucket *b)
{
   // Constructor

   previous = current = begin = end = 0;
   size = 0; 

   if (b) {
      XrdSutBuckListNode *f = new XrdSutBuckListNode(b,0);
      current = begin = end = f;
      size++;
   }
} 

//___________________________________________________________________________
XrdSutBuckList::~XrdSutBuckList()
{
   // Destructor

   XrdSutBuckListNode *n = 0;
   XrdSutBuckListNode *b = begin;
   while (b) {
      n = b->Next();
      delete (b);
      b = n;
   }
}

//___________________________________________________________________________
XrdSutBuckListNode *XrdSutBuckList::Find(XrdSutBucket *b)
{
   // Find node containing bucket b

   XrdSutBuckListNode *nd = begin;
   for (; nd; nd = nd->Next()) {
      if (nd->Buck() == b)
         return nd;
   }
   return (XrdSutBuckListNode *)0;
}

//___________________________________________________________________________
void XrdSutBuckList::PutInFront(XrdSutBucket *b)
{
   // Add at the beginning of the list
   // Check to avoid duplicates

   if (!Find(b)) {
      XrdSutBuckListNode *nb = new XrdSutBuckListNode(b,begin);
      begin = nb;     
      if (!end)
         end = nb;
      size++;
   }
}

//___________________________________________________________________________
void XrdSutBuckList::PushBack(XrdSutBucket *b)
{
   // Add at the end of the list
   // Check to avoid duplicates

   if (!Find(b)) {
      XrdSutBuckListNode *nb = new XrdSutBuckListNode(b,0);
      if (!begin)
         begin = nb;
      if (end)
         end->SetNext(nb);
      end = nb;
      size++;
   }
}

//___________________________________________________________________________
void XrdSutBuckList::Remove(XrdSutBucket *b)
{
   // Remove node containing bucket b

   XrdSutBuckListNode *curr = current;
   XrdSutBuckListNode *prev = previous;

   if (!curr || curr->Buck() != b || (prev && curr != prev->Next())) {
      // We need first to find the address
      curr = begin;
      prev = 0;
      for (; curr; curr = curr->Next()) {
         if (curr->Buck() == b)
            break;
         prev = curr;
      }
   }

   // The bucket is not in the list
   if (!curr)
      return;

   // Now we have all the information to remove
   if (prev) {
      current  = curr->Next();
      prev->SetNext(current);
      previous = curr;
   } else if (curr == begin) {
      // First buffer
      current  = curr->Next();
      begin = current;
      previous = 0;
   }

   // Cleanup and update size
   delete curr;      
   size--;
}

//___________________________________________________________________________
XrdSutBucket *XrdSutBuckList::Begin()
{ 
   // Iterator functionality: init

   previous = 0;
   current = begin;
   if (current)
      return current->Buck();
   return (XrdSutBucket *)0;
}

//___________________________________________________________________________
XrdSutBucket *XrdSutBuckList::Next()
{ 
   // Iterator functionality: get next

   previous = current;
   if (current) {
      current = current->Next();
      if (current)
         return current->Buck();
   }
   return (XrdSutBucket *)0;
}
