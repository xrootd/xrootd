#ifndef ___XrdOfsSECURITY_H___
#define ___XrdOfsSECURITY_H___
/******************************************************************************/
/*                                                                            */
/*                     X r d O f s S e c u r i t y . h h                      */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC03-76-SFO0515 with the Deprtment of Energy              */
/******************************************************************************/

//         $Id$

#ifdef __SECURITY__
#include "XrdAcc/XrdAccAuthorize.h"
#else
#define AOP_Chmod   0
#define AOP_Chown   0
#define AOP_Create  0
#define AOP_Delete  0
#define AOP_Insert  0
#define AOP_Mkdir   0
#define AOP_Read    0
#define AOP_Readdir 0
#define AOP_Rename  0
#define AOP_Stat    0
#define AOP_Update  0
#endif

#ifdef __SECURITY__
#define ACCESS_CHECK(u, optype, action, pathp, edata, ecode) \
    if (!XrdOfsFS.Authorization->Access(u->prot, u->name, u->host, \
                                pathp, optype)) \
       {XrdOfsFS.Emsg(epname, edata, EACCES, action, pathp); return ecode;}
#else
#define ACCESS_CHECK(usr, optype, action, pathp, edata, ecode)
#endif

#ifdef __SECURITY__
#define AUTHORIZE(usr, optype, action, pathp, edata, ecode) \
        ACCESS_CHECK(usr, optype, action, pathp, edata, ecode)
#else
#define AUTHORIZE(usr, optype, action, pathp, edata, ecode)
#endif

#ifdef __SECURITY__
#define AUTHORIZE2(usr,edata,ecode,opt1,act1,path1,opt2,act2,path2) \
       {ACCESS_CHECK(usr, opt1, act1, path1, edata, ecode); \
        ACCESS_CHECK(usr, opt2, act2, path2, edata, ecode); \
       }
#else
#define AUTHORIZE2(usr,edata,ecode,opt1,act1,path1,opt2,act2,path2)
#endif

#ifdef __SECURITY__
#define OOIDENTENV(usr, env) \
    env.Put(SEC_USER, usr->name); \
    env.Put(SEC_HOST, usr->host)
#else
#define OOIDENTENV(usr, env) env.Put(SEC_HOST, usr->host)
#endif
#endif
