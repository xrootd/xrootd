/******************************************************************************/
/*                                                                            */
/*                    X r d O u c B a c k T r a c e . c c                     */
/*                                                                            */
/*(c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University   */
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

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <sys/syscall.h>

#ifdef __GNUC__
#include <execinfo.h>
#include <cxxabi.h>
#endif

// Linux and MacOS provide actual thread number, others a thread pointer.
//
#if defined(__linux__) || defined(__APPLE__)
#define TidType long long
#define TidFmt  "%lld"
#elif defined(__GNU__)
#define TidType pthread_t // int
#define TidFmt  "%d"
#else
#define TidType pthread_t
#define TidFmt  "%p"
#endif

#include "XProtocol/XProtocol.hh"
#include "XrdOuc/XrdOucBackTrace.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/
  
namespace
{
static const int     iniDepth  =15; // The default
static const int     maxDepth  =30; // The app maximum
static const int     xeqDepth  =32; // The one we use internally

static const int     ptrXORFilter = 1;

XrdSysMutex          btMutex;
std::vector<void *> *ptrFilter[2] = {0, 0};
int                  xeqPtrFilter = 0;
int                  reqFilter    = 0;
int                  rspFilter    = 0;
}
  
/******************************************************************************/
/*                                C v t R e q                                 */
/******************************************************************************/
  
namespace
{
struct XrdInfo {const char *name; int code; int mask;};

XrdInfo *CvtReq(const char *name, int rnum)
{
   static XrdInfo reqTab[] = {{"auth",     kXR_auth,    1    },
                              {"query",    kXR_query,   1<< 1},
                              {"chmod",    kXR_chmod,   1<< 2},
                              {"close",    kXR_close,   1<< 3},
                              {"dirlist",  kXR_dirlist, 1<< 4},
                              {"gpfile",   kXR_gpfile,  1<< 5},
                              {"protocol", kXR_protocol,1<< 6},
                              {"login",    kXR_login,   1<< 7},
                              {"mkdir",    kXR_mkdir,   1<< 8},
                              {"mv",       kXR_mv,      1<< 9},
                              {"open",     kXR_open,    1<<10},
                              {"ping",     kXR_ping,    1<<11},
                              {"chkpoint", kXR_chkpoint,1<<12},
                              {"read",     kXR_read,    1<<13},
                              {"rm",       kXR_rm,      1<<14},
                              {"rmdir",    kXR_rmdir,   1<<15},
                              {"sync",     kXR_sync,    1<<16},
                              {"stat",     kXR_stat,    1<<17},
                              {"set",      kXR_set,     1<<18},
                              {"write",    kXR_write,   1<<19},
                              {"fattr",    kXR_fattr,   1<<20},
                              {"prepare",  kXR_prepare, 1<<21},
                              {"statx",    kXR_statx,   1<<22},
                              {"endess",   kXR_endsess, 1<<23},
                              {"bind",     kXR_bind,    1<<24},
                              {"readv",    kXR_readv,   1<<25},
                              {"pgwrite",  kXR_pgwrite, 1<<26},
                              {"locate",   kXR_locate,  1<<27},
                              {"truncate", kXR_truncate,1<<28}
                             };

   static XrdInfo unkTab   =  {"n/a",-1,-1};
   static const int reqNum = kXR_truncate-kXR_auth+1;

// Check if we only need to translate a code to a name
//
   if (!name)
      {if (rnum < kXR_auth || rnum > kXR_truncate) return &unkTab;
       return &reqTab[rnum-kXR_auth];
      }

// Find the name in the table
//
   for (int i = 0; i < reqNum; i++)
       {if (!strcmp(name, reqTab[i].name)) return &reqTab[i];}
   return &unkTab;
}
}

/******************************************************************************/
/*                                C v t R s p                                 */
/******************************************************************************/
  
namespace
{
XrdInfo *CvtRsp(const char *name, int snum)
{
   static XrdInfo rspTab[] = {{"oksofar",  kXR_oksofar,  1<< 1},
                              {"attn",     kXR_attn,     1<< 2},
                              {"authmore", kXR_authmore, 1<< 3},
                              {"error",    kXR_error,    1<< 4},
                              {"redirect", kXR_redirect, 1<< 5},
                              {"wait",     kXR_wait,     1<< 6},
                              {"waitresp", kXR_waitresp, 1<< 7}
                             };
   static XrdInfo aokTab   =  {"ok",   0, 1};
   static XrdInfo unkTab   =  {"n/a", -1,-1};
   static const int rspNum = kXR_waitresp-kXR_oksofar+1;

// Check if we only need to translate a code to a name
//
   if (!name)
      {if (!snum) return &aokTab;
       if (snum < kXR_oksofar || snum > kXR_waitresp) return &unkTab;
       return &rspTab[snum-kXR_oksofar];
      }

// Find the name in the table
//
   for (int i = 0; i < rspNum; i++)
       {if (!strcmp(name, rspTab[i].name)) return &rspTab[i];}
   return &unkTab;
}
}
  
/******************************************************************************/
/*                              D e m a n g l e                               */
/******************************************************************************/
  
namespace
{
int Demangle(char *cSym, char *buff, int blen)
{
#ifndef __GNUC__
   return -1;
#else
   int   status;
   char *plus = index(cSym, '+');
   char *brak = (plus ? index(plus, '[') : 0);
   char *cpar = (plus ? index(plus, ')') : 0);
   char *realname;

   if (*cSym != '(' || !plus || !cpar || !brak)
      return snprintf(buff, blen, "%s\n", cSym);
   *plus = 0;

   realname = abi::__cxa_demangle(cSym+1, 0, 0, &status);

   if (status) {*plus = '+'; return snprintf(buff, blen, "%s\n", cSym);}

   *cpar = 0;
   status = snprintf(buff, blen, "%s %s+%s\n", brak, realname, plus+1);
   free(realname);
   return status;
#endif
}
}

/******************************************************************************/
/*                             D u m p D e p t h                              */
/******************************************************************************/
  
namespace
{
int DumpDepth()
{
   char *theDepth = getenv("XRDBT_DEPTH");
   int   depth    = iniDepth;

   if (theDepth && (depth = atoi(theDepth)) <= 0) depth = iniDepth;

   return (depth <= maxDepth ? depth : maxDepth);
}
}

/******************************************************************************/
/*                             D u m p S t a c k                              */
/******************************************************************************/

namespace
{
void DumpStack(char *bP, int bL, TidType tid)
{
#ifndef __GNUC__
   snprintf(bP, bL, "TBT " TidFmt " No stack information available, not gnuc.", tid);
   return;
#else
   static int btDepth = DumpDepth(); // One time MT-safe call
   char **cSyms=0;
   char  *cStack[xeqDepth];
   int k, n = backtrace((void **)cStack, xeqDepth);

// Get call symbols if we have any of them here
//
   if (n > 1) cSyms = backtrace_symbols((void **)cStack, n);
      else {snprintf(bP, bL, "TBT " TidFmt " No stack information available.", tid);
            return;
           }

// Dump the stack into the buffer
//
   if (n > btDepth) n = btDepth+1;
   for (int i = 2; i < n && bL > 24; i++)
       {char *paren = index(cSyms[i], '(');
        if (!paren) k = snprintf(bP, bL, "TBT " TidFmt " %s\n", tid, cSyms[i]);
           else {k = snprintf(bP, bL, "TBT " TidFmt " ", tid);
                 bL -= k; bP += k;
                 k = Demangle(paren, bP, bL);
                }
        bL -= k; bP += k;
       }
#endif
}
}

/******************************************************************************/
/*                                S c r e e n                                 */
/******************************************************************************/

namespace
{

bool Screen(void *thisP, void *objP, bool rrOK)
{
   XrdSysMutexHelper btHelp(btMutex);
   std::vector<void *>::const_iterator it;
   std::vector<void *> *objV, *thsV;

// Filter by object pointer
//
   objV = ptrFilter[XrdOucBackTrace::isObject];
   if (objV)
      {for (it = objV->begin(); it!= objV->end(); ++it)
           if (objP == *it) return true;
      }

// Filter by this pointer
//
   thsV = ptrFilter[XrdOucBackTrace::isThis];
   if (thsV)
      {for (it = thsV->begin(); it!= thsV->end(); ++it)
           if (thisP == *it) return true;
      }

// If we something was in both lists then we have failed
//
   if ((objV && objV->size()) && (thsV && thsV->size())) return false;

// The results if the result is the req/rsp filter
//
   return rrOK;
}
}

/******************************************************************************/
/*               X r d O u c B a c k T r a c e   M e t h o d s                */
/******************************************************************************/
/******************************************************************************/
/*                                  D o B T                                   */
/******************************************************************************/
  
void XrdOucBackTrace::DoBT(const char *head,  void *thisP, void *objP,
                           const char *tail,  bool  force)
{
   TidType tid;
   int      k;
   char     btBuff[4096];

// Apply any necessary filters
//
   if (!force)
      {if (AtomicGet(xeqPtrFilter) && !Screen(thisP, objP, false)) return;}

// Prepare for formatting
//
   if (!head) head = "";
   if (!tail) tail = "";
#if defined(__linux__) || defined(__APPLE__)
   tid     = syscall(SYS_gettid);
#else
   tid     = XrdSysThread::ID();
#endif

// Format the header
//
   k = snprintf(btBuff,sizeof(btBuff),"\nTBT " TidFmt " %p %s obj %p %s\n",
                tid, thisP, head, objP, tail);

// Now dump the stack
//
   DumpStack(btBuff+k, sizeof(btBuff)-k-8, tid);

// Output the information
//
   std::cerr <<btBuff <<std::flush;
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
bool XrdOucBackTrace::Init(const char *reqs, const char *rsps)
{
   XrdOucTokenizer tokLine(0);
   XrdInfo *infoP;
   char *line, *token;
   bool aOK = true;

// Check if we have a request filter
//
   if (reqs || (reqs = getenv("XRDBT_REQFILTER")))
      {line = strdup(reqs);
       tokLine.Attach(line);
       token = tokLine.GetLine();
       while((token = tokLine.GetToken()))
            {infoP = CvtReq(token, 0);
             if (infoP->code > 0) reqFilter |= infoP->mask;
                else aOK = false;
            }
       free(line);
      }

// Check if we have a response filter
//
   if (rsps || (rsps = getenv("XRDBT_RSPFILTER")))
      {line = strdup(rsps);
       tokLine.Attach(line);
       token = tokLine.GetLine();
       while((token = tokLine.GetToken()))
            {infoP = CvtRsp(token, 0);
             if (infoP->code > 0) rspFilter |= infoP->mask;
                else aOK = false;
            }
       free(line);
      }

// All done
//
   return aOK;
}
  
/******************************************************************************/
/*                                F i l t e r                                 */
/******************************************************************************/
  
void XrdOucBackTrace::Filter(void *ptr, XrdOucBackTrace::PtrType pType,
                                        XrdOucBackTrace::Action  how)
{
   XrdSysMutexHelper btHelp(btMutex);
   std::vector<void *>::iterator it;
   std::vector<void *> *filtP;

// Get the filter, we have the mutex so no need to atomically fetch it
//
   filtP = ptrFilter[pType];

// Perfome action when we don't already have a filter
//
   if (!filtP)
      {if (how != XrdOucBackTrace::clrIt && how != XrdOucBackTrace::delIt)
          {filtP = new std::vector<void *>();
           filtP->push_back(ptr);
           ptrFilter[pType] = filtP;
           AtomicInc(xeqPtrFilter); // This forces the above to complete
          }
       return;
      }

// We have a filter, see it we need to clear it
//
   if (how == XrdOucBackTrace::clrIt)
      {int i = pType ^ ptrXORFilter;
       filtP->clear();
       if (!ptrFilter[i] || ptrFilter[i]->size() == 0) AtomicZAP(xeqPtrFilter);
       return;
      }

// We have a filter, see it we need to replace it
//
   if (how == XrdOucBackTrace::repIt)
      {filtP->clear();
       filtP->push_back(ptr);
       AtomicInc(xeqPtrFilter);
       return;
      }

// We only have add and delete left and these require us to find the pointer
//
   for (it = filtP->begin(); it!= filtP->end(); ++it) if (ptr == *it) break;

// Handle the case where we found the element
//
   if (it != filtP->end())
      {if (how == XrdOucBackTrace::delIt)
          {int i = pType ^ ptrXORFilter;
           filtP->erase(it);
           if (filtP->size() == 0 && (!ptrFilter[i] || !(ptrFilter[i]->size())))
              AtomicZAP(xeqPtrFilter);
std::cerr <<"delIt: " <<xeqPtrFilter <<std::endl;
          }
       return;
      }

// We did not find the element, add it if we must
//
   if (how == XrdOucBackTrace::addIt)
      {filtP->push_back(ptr);
       AtomicInc(xeqPtrFilter);
      }
}
  
/******************************************************************************/
/*                                 X r d B T                                  */
/******************************************************************************/
  
void XrdOucBackTrace::XrdBT(const char *head,  void *thisP, void *objP,
                                  int   rspN,  int   reqN,
                            const char *tail,  bool  force)
{
   XrdInfo *infoP, *reqInfo, *rspInfo;
   TidType  tid;
   int      k;
   char     btBuff[4096];
   bool     rrOK;

// Apply any necessary filters
//
   if (!force)
      {     if (!reqFilter && !rspFilter)                rrOK = false;
       else if (reqFilter && (infoP=CvtReq(0, reqN))
                          && !(reqFilter & infoP->mask)) rrOK = false;
       else if (rspFilter && (infoP=CvtRsp(0, rspN))
                          && !(rspFilter & infoP->mask)) rrOK = false;
       else rrOK = true;
       if (AtomicGet(xeqPtrFilter)) {if (!Screen(thisP, objP, rrOK)) return;}
          else if (!rrOK) return;
      }

// Prepare for formatting
//
   if (!head) head = "";
   if (!tail) tail = "";
   reqInfo = CvtReq(0, reqN);
   rspInfo = CvtRsp(0, rspN);
#if defined(__linux__) || defined(__APPLE__)
   tid     = syscall(SYS_gettid);
#else
   tid     = XrdSysThread::ID();
#endif

// Format the header
//
   k = snprintf(btBuff, sizeof(btBuff),
                "\nTBT " TidFmt " %p %s obj %p req %s rsp %s %s\n",
                tid, thisP, head, objP, reqInfo->name, rspInfo->name, tail);

// Now dump the stack
//
   DumpStack(btBuff+k, sizeof(btBuff)-k-8, tid);

// Output the information
//
   std::cerr <<btBuff <<std::flush;
}
