#ifndef __SUT_BUFFER_H__
#define __SUT_BUFFER_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S u t B u f f e r . h h                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Geri Ganis for CERN                                          */
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

#ifndef __SUT_BUCKLIST_H__
#include "XrdSut/XrdSutBuckList.hh"
#endif

/******************************************************************************/
/*                                                                            */
/*  Buffer structure for managing exchanged buckets                           */
/*                                                                            */
/******************************************************************************/

class XrdOucString;

class XrdSutBuffer {

private:
   
   XrdSutBuckList         fBuckets;
   XrdOucString           fOptions;
   XrdOucString           fProtocol;
   kXR_int32              fStep;

public:
   XrdSutBuffer(const char *prot, const char *opts = 0)
                 {fOptions = opts; fProtocol = prot; fStep = 0;}
   XrdSutBuffer(const char *buffer, kXR_int32 length);

   virtual ~XrdSutBuffer();

   int         AddBucket(char *bp=0, int sz=0, int ty=0) 
                 { XrdSutBucket *b = new XrdSutBucket(bp,sz,ty);
                   if (b) { fBuckets.PushBack(b); return 0;} return -1; }
   int         AddBucket(XrdOucString s, int ty=0) 
                 { XrdSutBucket *b = new XrdSutBucket(s,ty);
                   if (b) { fBuckets.PushBack(b); return 0;} return -1; }
   int         AddBucket(XrdSutBucket *b) 
                 { if (b) { fBuckets.PushBack(b); return 0;} return -1; }

   int         UpdateBucket(const char *bp, int sz, int ty); 
   int         UpdateBucket(XrdOucString s, int ty);

   // Remove from the list, to avoid destroy by ~XrdSutBuffer
   void        Remove(XrdSutBucket *b) { fBuckets.Remove(b); }

   void        Dump(const char *stepstr = 0, bool all = false);
   void        Message(const char *prepose = 0);
   int         Serialized(char **buffer, char opt = 'n');

   void        Deactivate(kXR_int32 type);  // Deactivate bucket (type=-1 for cleanup)

   // To fill / access buckets containing 4-byte integers (status codes, versions ...)
   kXR_int32   MarshalBucket(kXR_int32 type, kXR_int32 code);
   kXR_int32   UnmarshalBucket(kXR_int32 type, kXR_int32 &code);

   XrdSutBucket *GetBucket(kXR_int32 type, const char *tag = 0);
   XrdSutBuckList *GetBuckList() const { return (XrdSutBuckList *)&fBuckets; }
   int         GetNBuckets() const     { return fBuckets.Size(); }
   const char *GetOptions() const      { return fOptions.c_str(); }
   const char *GetProtocol() const     { return fProtocol.c_str(); }
   int         GetStep() const         { return (int)fStep; }
   void        SetStep(int s)   { fStep = (kXR_int32)s; }
   void        IncrementStep()  { fStep++; }
};

#endif

