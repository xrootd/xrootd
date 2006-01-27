// $Id$
#ifndef __SECPWD_PLATFORM_
#define __SECPWD_PLATFORM_
/******************************************************************************/
/*                                                                            */
/*                 X r d S e c p w d P l a t f o r m. h h                     */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//
// crypt
//
#if defined(__solaris__)
#include <crypt.h>
#endif
#if defined(__osf__) || defined(__sgi) || defined(__macos__)
extern "C" char *crypt(const char *, const char *);
#endif

//
// shadow passwords
//
#include <grp.h>
// For shadow passwords
#if defined(__solaris__)
#ifndef R__SHADOWPW
#define R__SHADOWPW
#endif
#endif
#ifdef R__SHADOWPW
#include <shadow.h>
#endif

//
// groups and setresuid / setresgui
//
#if defined(__alpha) && !defined(linux) && !defined(__FreeBSD__)
extern "C" int initgroups(const char *name, int basegid);
#endif

#if defined(__sgi) && !defined(__GNUG__) && (SGI_REL<62)
extern "C" {
   int seteuid(int euid);
   int setegid(int egid);
}
#endif

#if defined(_AIX)
extern "C" {
   int seteuid(uid_t euid);
   int setegid(gid_t egid);
}
#endif

#if !defined(__hpux) && !defined(linux) && !defined(__FreeBSD__) || \
    defined(cygwingcc)
static int setresgid(gid_t r, gid_t e, gid_t)
{
   if (setgid(r) == -1)
      return -1;
   return setegid(e);
}

static int setresuid(uid_t r, uid_t e, uid_t)
{
   if (setuid(r) == -1)
      return -1;
   return seteuid(e);
}

#else

#if defined(linux) && !defined(HAS_SETRESUID)
extern "C" {
   int setresgid(gid_t r, gid_t e, gid_t s);
   int setresuid(uid_t r, uid_t e, uid_t s);
}
#endif

#endif

#endif
