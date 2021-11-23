/******************************************************************************/
/*                                                                            */
/*                      X r d S u t B u f f e r . c c                         */
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

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <netinet/in.h>
#include <sys/types.h>

#include "XrdSec/XrdSecInterface.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSut/XrdSutBuffer.hh"
#include "XrdSut/XrdSutTrace.hh"

/******************************************************************************/
/*                                                                            */
/*  Buffer structure for managing exchanged buckets                           */
/*                                                                            */
/******************************************************************************/

//_____________________________________________________________________________
XrdSutBuffer::XrdSutBuffer(const char *buf, kXR_int32 len)
{
   // Constructor from compact form (used for exchange over the network)
   // If the buffer begins with "&P=", then only the protocol name and
   // options are extracted, assuming the format "&P=<protocol>,<options>".
   // Otherwise the format "<protocol><step><bucket_1>...<bucket_n>" is
   // assumed 
   EPNAME("Buffer::XrdSutBuffer");

   bool ok = 1;

   // Default initialization
   fOptions = "";
   fProtocol = "";
   fStep = 0;

   //
   // Check type of buffer
   if (!strncmp(buf,"&P=",3)) {
      //
      // Initial buffer format
      // Extract protocol name and options
      int cur = 3;
      int k = 0;
      while (buf[cur+k] && buf[cur+k] != ',' &&
             k < XrdSecPROTOIDSIZE && (cur+k) < len) k++;
      if (!k) {
         PRINT("no protocol name - do nothing");
      } else {
         //
         // Extract protocol name
         char proto[XrdSecPROTOIDSIZE];
         strncpy(proto,buf+cur,k);
         proto[(k >= XrdSecPROTOIDSIZE ? XrdSecPROTOIDSIZE-1:k)]=0;  // null-terminated
         fProtocol = proto;
         cur += (k+1);
         //
         // Extract options, if any
         if (cur < len) {
            k = 0;
            while ((cur+k) < len && buf[cur+k])
               k++;
            if (k) {
               char *opts = new char[k+1];
               if (opts) {
                  strncpy(opts,buf+cur,k);
                  opts[k] = 0;  // null-terminated
                  fOptions = opts;
                  delete[] opts;
               }
            }
            cur += (k+1);
         }
      }

   } else {
      //
      // Assume exchange info format
      // Check integrity
      int k = 0;
      while ( k < XrdSecPROTOIDSIZE && k < len && buf[k]) { k++; }
      if (!k || k == XrdSecPROTOIDSIZE) {
         PRINT("no protocol name: do nothing");
         ok = 0;
      }
      int cur = k+1;
      if (ok) {
         //
         // Extract protocol name
         char proto[XrdSecPROTOIDSIZE];
         strcpy(proto,buf);
         fProtocol = proto;
      
         //
         // Step/Iteration number
         kXR_int32 step;
         memcpy(&step,&buf[cur],sizeof(kXR_int32));
         fStep = ntohl(step);
         cur += sizeof(kXR_int32);
      }

      //
      // Total length of buckets (sizes+buffers) (excluded trailing 0)
      int ltot = len - sizeof(kXR_int32);
      TRACE(Dump,"ltot: " <<ltot);
      
      //
      // Now the buckets
      kXR_int32 type;
      kXR_int32 blen;
      XrdSutBucket *tmp = 0;
      char *buck = 0;
      while (ok) {
      
         //
         // Get type 
         memcpy(&type,&buf[cur],sizeof(kXR_int32));
         type = ntohl(type);
         TRACE(Dump,"type: " <<XrdSutBuckStr(type));
      
         if (type == kXRS_none) {
            //
            // We are over
            ok = 0;
         } else {
            //
            cur += sizeof(kXR_int32);
            //
            // Get length and test consistency
            memcpy(&blen,&buf[cur],sizeof(kXR_int32));
            blen = ntohl(blen);
            TRACE(Dump,"blen: " <<blen);
            //
            cur += sizeof(kXR_int32);
            TRACE(Dump,"cur: " <<cur);
            if ((cur-1+blen) > ltot)
               ok = 0;
            else {
               //
               // Store only active buckets
               if (type != kXRS_inactive){
                  //
                  // Normal active bucket: save it in the vector
                  if ((buck = new char[blen])) {
                     memcpy(buck,&buf[cur],blen);
                     if ((tmp = new XrdSutBucket(buck,blen,type))) {
                        fBuckets.PushBack(tmp);
                     } else {
                        PRINT("error creating bucket: "<<XrdSutBuckStr(type)
                              <<" (size: "<<blen<<", !buck:"<<(!buck)<<")");
                     }
                  } else {
                     PRINT("error allocating buffer for bucket: "
                           <<XrdSutBuckStr(type)<<" (size:"<<blen<<")");
                  }
               }
               cur += blen;
            }
         }
      }
   }
}

//_____________________________________________________________________________
XrdSutBuffer::~XrdSutBuffer()
{
   // Destructor
   // XrdSutBuffer is responsible of the buckets in the list
   EPNAME("Buffer::~XrdSutBuffer");

   XrdSutBucket *bp = fBuckets.Begin();
   while (bp) {
      TRACE(Dump,"type: " << bp->type);
      delete bp;
      // Get next bucket
      bp = fBuckets.Next();
   }
}

//_____________________________________________________________________________
int XrdSutBuffer::UpdateBucket(const char *b, int sz, int ty)
{
   // Update existing bucket (or add a new bucket to the list)
   // with sz bytes at 'b'.
   // Returns 0 or -1 if error allocating bucket
   EPNAME("Buffer::UpdateBucket");

   XrdSutBucket *bp = GetBucket(ty);
   if (!bp) {
      bp = new XrdSutBucket(0,0,ty);
      if (!bp) {
         DEBUG("Out-Of-Memory allocating bucket");
         return -1;
      }
      AddBucket(bp);
   }
   bp->SetBuf(b,sz);
   // Done
   return 0;
}

//_____________________________________________________________________________
int XrdSutBuffer::UpdateBucket(XrdOucString s, int ty)
{
   // Update existing bucket (or add a new bucket to the list)
   // with string s.
   // Returns 0 or -1 if error allocating bucket

   return UpdateBucket(s.c_str(),s.length(),ty);
}

//_____________________________________________________________________________
void XrdSutBuffer::Dump(const char *stepstr, bool all)
{
   // Dump content of buffer. If all is false, only active buckets are dumped;
   // this is the default behaviour.
   EPNAME("Buffer::Dump");

   PRINT("//-----------------------------------------------------//");
   PRINT("//                                                     //")
   PRINT("//            XrdSutBuffer DUMP                        //")
   PRINT("//                                                     //")

   int nbuck = fBuckets.Size();

   PRINT("//  Buffer        : " <<this);
   PRINT("// ");
   PRINT("//  Proto         : " <<fProtocol.c_str());
   if (fOptions.length()) {
      PRINT("//  Options       : " <<fOptions.c_str());
   } else {
      PRINT("//  Options       : none");
   }
   if (stepstr) {
      PRINT("//  Step          : " <<stepstr);
   } else {
      PRINT("//  Step          : " <<fStep);
   }
   if (!all) {
      PRINT("//  Dumping active buckets only ");
   } else {
      PRINT("//  # of buckets  : " <<nbuck);
   }
   PRINT("// ");
 
   int kb = 0;
   XrdSutBucket *bp = fBuckets.Begin();
   while (bp) {
      PRINT("// ");
      if (all || bp->type != kXRS_inactive) {
         PRINT("//  buck: " <<kb++);
         bp->Dump(0);
      }
      // Get next
      bp = fBuckets.Next();
   }
   if (!all) PRINT("//  # active buckets found: " << kb);
   PRINT("//                                                     //")
   PRINT("//-----------------------------------------------------//");
}

//_____________________________________________________________________________
void XrdSutBuffer::Message(const char *prepose)
{
   // Print content of any bucket of type kXRS_message
   // Prepose 'prepose', if defined 

   bool pripre = 0;
   if (prepose)
      pripre = 1;
 
   XrdSutBucket *bp = fBuckets.Begin();
   while (bp) {
      if (bp->type == kXRS_message) {
         if (bp->size > 0 && bp->buffer) {
            if (pripre) {
               XrdOucString premsg(prepose);
               cerr << premsg << endl;
               pripre = 0;
            }
            XrdOucString msg(bp->buffer,bp->size);
            cerr << msg << endl;
         }
      }
      // Get next
      bp = fBuckets.Next();
   }
}

//_____________________________________________________________________________
kXR_int32 XrdSutBuffer::MarshalBucket(kXR_int32 type, kXR_int32 code)
{
   // Search the vector of buckets for the first bucket of
   // type 'type'. Reset its content and fill it with 'code'
   // in network byte order. If no bucket 'type' exists, add
   // a new one.
   // Returns -1 if new bucket could be allocated; 0 otherwise .
   EPNAME("Buffer::MarshalBucket");

   // Convert to network byte order
   kXR_int32 mcod = htonl(code);

   // Get the bucket
   XrdSutBucket *bck = GetBucket(type);
   if (!bck) {
      // Allocate a new one
      bck = new XrdSutBucket(0,0,type);
      if (!bck) {
         DEBUG("could not allocate new bucket of type:"<<XrdSutBuckStr(type));
         errno = ENOMEM;
         return -1;
      }
      // Add it to the list
      AddBucket(bck);
   }

   // Set content
   bck->SetBuf((char *)(&mcod),sizeof(kXR_int32));

   // We are done
   return 0;
}

//_____________________________________________________________________________
kXR_int32 XrdSutBuffer::UnmarshalBucket(kXR_int32 type, kXR_int32 &code)
{
   // Search the vector of buckets for the first bucket of
   // type 'type'. Unmarshalled its content to host byte order
   // and fill it in code.
   // Returns 0 if ok.
   // Returns -1 if no bucket of requested 'type' could be
   // found; -2 if the bucket size is inconsistent.
   EPNAME("Buffer::UnmarshalBucket");

   code = 0;
   // Get the bucket
   XrdSutBucket *bck = GetBucket(type);
   if (!bck) {
      DEBUG("could not find a bucket of type:"<<XrdSutBuckStr(type));
      errno = ENOENT;
      return -1;
   }
   if (bck->size != sizeof(kXR_int32)) {
      DEBUG("Wrong size: type:"<<XrdSutBuckStr(type)
            <<", size:"<<bck->size<<", expected:"<<sizeof(kXR_int32));
      errno = EINVAL;
      return -2;
   }
   //
   // Get the content
   memcpy(&code,bck->buffer,sizeof(kXR_int32));
   //
   // Unmarshal
   code = ntohl(code);

   // We are done
   return 0;
}

//_____________________________________________________________________________
XrdSutBucket *XrdSutBuffer::GetBucket(kXR_int32 type, const char *tag)
{
   // Search the vector of buckets for the first bucket of
   // type 'type'.
   // If tag is defined, search buckets whose buffer contains tag
   // in the form <tag>'\0'<rest_of_buffer>.
   // Returns the pointer to the buffer; 0 if the no bucket
   // is found

   //
   // Check tag, if any
   int ltag = (tag) ? strlen(tag) : 0;
   //
   // Loop over buckets
   XrdSutBucket *bp = fBuckets.Begin();
   while (bp) {
      if (type == bp->type && (!tag || (ltag < bp->size && 
                                        !strncmp(bp->buffer,tag,ltag) && 
                                        (bp->buffer)[ltag] == '\0')))
         return bp;
      // Get next
      bp = fBuckets.Next();
   }

   // Nothing found
   return 0;
}

//_____________________________________________________________________________
void XrdSutBuffer::Deactivate(kXR_int32 type)
{
   // Deactivate first bucket of type 'type', if any
   // If type == -1, deactivate all buckets (cleanup)

   //
   // Loop over buckets
   XrdSutBucket *bp = fBuckets.Begin();
   while (bp) {
      if (type == bp->type) {
         bp->type = kXRS_inactive;
         break;
      } else if (type == -1) {
         bp->type = kXRS_inactive;
      }
      // Get next
      bp = fBuckets.Next();
   }
}

//_____________________________________________________________________________
int XrdSutBuffer::Serialized(char **buffer, char opt)
{
   // Serialize the content in a form suited for exchange
   // over the net; the result is saved in '*buffer', which
   // must be deleted (opt = 'n', default) or freed (opt == 'm') by the caller.
   // Returns the length of the buffer in case of success.
   // Returns -1 in case of problems allocating the buffer.
   EPNAME("Buffer::Serialized");

   //
   // Check that we got a valid argument
   if (!buffer) {
      DEBUG("invalid input argument");
      errno = EINVAL;
      return -1;
   }

   //
   // Calculate the length of the buffer
   int blen = fProtocol.length() + 1 + 2*sizeof(kXR_int32);
   // buckets
   XrdSutBucket *bp = fBuckets.Begin();
   while (bp) {
      if (bp->type != kXRS_inactive) {
         blen += 2*sizeof(kXR_int32);
         blen += bp->size;
      }
      // Get next
      bp = fBuckets.Next();
   }

   //
   // Allocate the buffer
   *buffer = (opt == 'n') ? (new char[blen]) : (char *)malloc(blen);
   if (!(*buffer))
      return -1;
   char *tbuf = *buffer;
   int cur = 0;

   //
   // Add protocol
   memcpy(tbuf,fProtocol.c_str(),fProtocol.length());
   tbuf[fProtocol.length()] = 0;
   cur += fProtocol.length() + 1;

   //
   // Add step number
   kXR_int32 step = htonl(fStep);
   memcpy(tbuf+cur,&step,sizeof(kXR_int32));
   cur += sizeof(kXR_int32);

   //
   // Add buckets
   bp = fBuckets.Begin();
   while (bp) {
      if (bp->type != kXRS_inactive) {
         kXR_int32 type = htonl(bp->type);
         memcpy(tbuf+cur,&type,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         kXR_int32 size = htonl(bp->size);
         memcpy(tbuf+cur,&size,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         memcpy(tbuf+cur,bp->buffer,bp->size);
         cur += bp->size;
      }
      // Get next bucket
      bp = fBuckets.Next();
   }

   //
   // Add 0 termination
   kXR_int32 ltmp = htonl(kXRS_none);
   memcpy(tbuf+cur,&ltmp,sizeof(kXR_int32));

   // Return total length
   return blen;
}
