#ifndef __SYS_PRIV_H__
#define __SYS_PRIV_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d S y s P r i v . h h                              */
/*                                                                            */
/* (c) 2006 G. Ganis (CERN)                                                   */
/*     All Rights Reserved. See XrdInfo.cc for complete License Terms         */
/******************************************************************************/
// $Id$

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdSysPriv                                                           //
//                                                                      //
// Author: G. Ganis, CERN, 2006                                         //
//                                                                      //
// Implementation of a privileges handling API following the paper      //
//   "Setuid Demystified" by H.Chen, D.Wagner, D.Dean                   //
// also quoted in "Secure programming Cookbook" by J.Viega & M.Messier. //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#if !defined(WINDOWS)
#  include <sys/types.h>
#else
#  define uid_t unsigned int
#  define gid_t unsigned int
#endif

class XrdSysPriv
{
 public:
   XrdSysPriv();
   virtual ~XrdSysPriv() { }

   static bool fDebug;

   static int ChangeTo(uid_t uid, gid_t gid);
   static int ChangePerm(uid_t uid, gid_t gid);
   static void DumpUGID(const char *msg = 0);
   static int Restore(bool saved = 1);
};

//
// To minimize the chance of forgetting the super-privileges set
// Usage:
//
//    {  XrdSysPrivGuard priv(tempuid);
//
//       // Work as tempuid (maybe superuser)
//       ...
//
//    }
//
class XrdSysPrivGuard
{
 public:
   XrdSysPrivGuard(uid_t uid, gid_t gid);
   XrdSysPrivGuard(const char *user);
   virtual ~XrdSysPrivGuard() { if (!dum) XrdSysPriv::Restore(); }
   bool Valid() const { return valid; }
 private:
   bool dum;
   bool valid;
   void Init(uid_t uid, gid_t gid);
};

#endif
