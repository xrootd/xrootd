#ifndef __OUC_DLIST__
#define __OUC_DLIST__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c D L l i s t . h h                        */
/*                                                                            */
/*(c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*                         All Rights Reserved                                */
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
  

template<class T>
class XrdOucDLlist
{
public:

         XrdOucDLlist(T *itemval=0) {prev=this; next=this; item=itemval;}
        ~XrdOucDLlist() {if (prev != next) Remove();}

// Apply() applies the specified function to every item in the list. Apply()
//         is pointer-safe in that the current node pointers may be changed
//         without affecting the traversal of the list. An argument may be
//         passed to the function. A null pointer is returned if the list
//         was completely traversed. Otherwise, the pointer to the node on
//         which the applied function returned a non-zero value is returned.
//         An optional starting point may be passed.
//
T       *Apply(int (*func)(T *, void *), void *Arg, XrdOucDLlist *Start=0)
         {XrdOucDLlist *nextnode, *node;
          if (Start) node = Start;   // Set correct starting point
             else    node = this;

          // Iterate through the list until we hit ourselves again. We do the 
          // loop once on the current node to allow for anchorless lists.
          //
             do {nextnode = node->next;
                 if (node->item && (*func)(node->item, Arg)) return node->item;
                 node = nextnode;
                } while (node != this);

         // All done, indicate we went through the whole list
         //
         return (T *)0;
        }

// Insert() inserts the specified node immediately off itself. If an item value
//          is not given, it is not changed.
//
void Insert(XrdOucDLlist *Node, T *Item=0)
                  {Node->next  = next;        // Chain in the item;
                   next->prev  = Node;
                   next        = Node;
                   Node->prev  = this;
                   if (Item) Node->item = Item;
                  }

// Item() supplies the item value associated with itself (used with Next()).
//
T  *Item() {return item;}

// Remove() removes itself from whatever list it happens to be in.
//
void Remove()
                  {prev->next = next;                // Unchain the item
                   next->prev = prev;
                   next       = this;
                   prev       = this;
                  }

// Next() supplies the next list node.
//
XrdOucDLlist *Next() {return next;}

// Prev() supplies the prev list node.
//
XrdOucDLlist *Prev() {return prev;}

// Set the item pointer
//
void setItem(T *ival) {item = ival;}

// Singleton() indicates whether or not the node points to something
//
int          Singleton() {return next == this;}

private:
XrdOucDLlist *next;
XrdOucDLlist *prev;
T            *item;
};
#endif
