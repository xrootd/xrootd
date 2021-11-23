/******************************************************************************/
/* XrdFfsFsinfo.cc filesystem/xrootd oss space usage info cache               */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/* Author: Wei Yang (SLAC National Accelerator Laboratory, 2010)              */
/*         Contract DE-AC02-76-SFO0515 with the Department of Energy          */
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <pthread.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/statvfs.h>
#include "XrdOuc/XrdOucHash.hh"

#ifdef __cplusplus
  extern "C" {
#endif

struct XrdFfsFsInfo {
    time_t t;
//    unsigned long f_blocks, f_bavail, f_bfree;
    fsblkcnt_t f_blocks, f_bavail, f_bfree;
};

pthread_mutex_t XrdFfsFsinfo_cache_mutex_wr = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t XrdFfsFsinfo_cache_mutex_rd = PTHREAD_MUTEX_INITIALIZER;

XrdOucHash<struct XrdFfsFsInfo> XrdFfsFsinfoHtab;

int XrdFfsFsinfo_cache_search(int (*func)(const char*, const char*, struct statvfs*, uid_t), const char* rdrurl, const char* path, struct statvfs *stbuf, uid_t user_uid)
{
    struct XrdFfsFsInfo *s; 
    int wlock, rc = 0, dofree = 0;
    const char *p;
    char* sname;

    wlock = pthread_mutex_trylock(&XrdFfsFsinfo_cache_mutex_wr);
    pthread_mutex_lock(&XrdFfsFsinfo_cache_mutex_rd);

    p=strstr(path, "oss.cgroup=");
    if (p != NULL && p[11] != '\0')
        sname = strdup(p+11);
    else
        sname = strdup(" ");
    s = XrdFfsFsinfoHtab.Find(sname);
    if (s != NULL)
    {
        stbuf->f_blocks = s->f_blocks;
        stbuf->f_bavail = s->f_bavail;
        stbuf->f_bfree = s->f_bfree;
    }
    else
    {
        rc = (*func)(rdrurl, path, stbuf, user_uid);
        s = (struct XrdFfsFsInfo*) malloc(sizeof(struct XrdFfsFsInfo));
        s->t = 0;
        dofree = 1;
    }

    pthread_mutex_unlock(&XrdFfsFsinfo_cache_mutex_rd);
    if (wlock == 0)  // did get a lock for update
    {
        time_t curr_time = time(NULL);
        if (curr_time - s->t > 120)
        {
            if (s->t != 0)
                rc = (*func)(rdrurl, path, stbuf, user_uid);

            pthread_mutex_lock(&XrdFfsFsinfo_cache_mutex_rd);
            s->t = curr_time;
            s->f_blocks = stbuf->f_blocks;
            s->f_bavail = stbuf->f_bavail;
            s->f_bfree = stbuf->f_bfree;

            if (s->f_blocks != 0)  // if s->f_blocks is zero, then this space token probably does not exist
                XrdFfsFsinfoHtab.Rep(sname, s, 0, (XrdOucHash_Options)(Hash_default | Hash_keepdata));
               else if (dofree) free(s);
            pthread_mutex_unlock(&XrdFfsFsinfo_cache_mutex_rd);
        }   
        pthread_mutex_unlock(&XrdFfsFsinfo_cache_mutex_wr);
    }
    free(sname);
    return rc;
} 


/* for testing 

void junkfunc(const char *rdrurl, const char *path, struct statvfs *stbuf, uid_t user_uid)
{
    stbuf->f_blocks = rand();
    stbuf->f_bavail = stbuf->f_blocks;
    stbuf->f_bfree = stbuf->f_blocks;
}

main() {
     char name[100];
     struct statvfs stbuf;

     void XrdFfsFsinfo_cache_init()
     while (1) {
         printf("name = ");   // name should be /oss.cgroup=xyz
         scanf("%s", name);
         XrdFfsFsinfo_cache_search(&junkfunc, "rdr", name, &stbuf, 10);
         printf("name %s space %lld\n", name, stbuf.f_blocks);
    }
    hdestroy_r(&XrdFfsFsinfoHtab);
} 
    
*/
#ifdef __cplusplus
  }
#endif
