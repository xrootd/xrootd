/******************************************************************************/
/* XrdFfsMisc.cc  Miscellanies functions                                      */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/* Author: Wei Yang (SLAC National Accelerator Laboratory, 2009)              */
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

#define _FILE_OFFSET_BITS 64
#include <string.h>
#include <sys/types.h>
//#include <sys/xattr.h>
#include <iostream>
#include <libgen.h>
#include <unistd.h>
//#include <netdb.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <syslog.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdPosix/XrdPosixAdmin.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSecsss/XrdSecsssID.hh"
#include "XrdFfs/XrdFfsDent.hh"
#include "XrdFfs/XrdFfsFsinfo.hh"
#include "XrdFfs/XrdFfsMisc.hh"
#include "XrdFfs/XrdFfsPosix.hh"
#include "XrdFfs/XrdFfsQueue.hh"
#include "XrdPosix/XrdPosixXrootd.hh"

#ifdef __cplusplus
  extern "C" {
#endif

#define MAXROOTURLLEN 1024 // this is also defined in other files

#define nXrdConnPerUsr 8;
short iXrdConnPerUsr = 0;
pthread_mutex_t url_mlock;

char XrdFfsMisc_get_current_url(const char *oldurl, char *newurl) 
{
    struct stat stbuf;

/* if it is a directory, return oldurl */
    if (XrdFfsPosix_stat(oldurl, &stbuf) == 0 && S_ISDIR(stbuf.st_mode))
    {
        strcpy(newurl, oldurl);
        return 1;
    }

    XrdPosixAdmin adm(oldurl);
    if (adm.isOK() && adm.Stat())
    {
// We might have been redirected to a destination server. Better 
// to remember it and use only this one as output.
//      if (stat && adm->GetCurrentUrl().IsValid())
//      {
//          strcpy(newurl, adm->GetCurrentUrl().GetUrl().c_str());
//          delete adm;
            strcpy(newurl, oldurl); // This needs to change!!!
            return 1;
        }
    return 0;
}

char* XrdFfsMisc_getNameByAddr(char* ipaddr)
{
    XrdNetAddr netAddr;
    const char *theName;
    if (netAddr.Set(ipaddr,0) || !(theName = netAddr.Name())) theName = ipaddr;
    return strdup(theName);
}

int XrdFfsMisc_get_all_urls_real(const char *oldurl, char **newurls, const int nnodes)
{
    XrdPosixAdmin adm(oldurl);
    XrdCl::URL *uVec;
    int i, rval = 0;

    if (!adm.isOK() || !(uVec = adm.FanOut(rval))) return -1;

    if (rval > nnodes) rval = -1;
       else for (i = 0; i < rval; i++) 
              { strncpy(newurls[i], uVec[i].GetURL().c_str(), MAXROOTURLLEN -1); newurls[i][MAXROOTURLLEN -1] = '\0'; }

    delete [] uVec;
    return rval;
}

/*
   function XrdFfsMisc_get_all_urls() has the same interface as XrdFfsMisc_get_all_urls_real(), but 
   used a cache to reduce unnecessary queries to the redirector 
*/ 

char XrdFfsMiscCururl[1024] = "";
char *XrdFfsMiscUrlcache[XrdFfs_MAX_NUM_NODES];
int XrdFfsMiscNcachedurls = 0;
time_t XrdFfsMiscUrlcachetime = 0;
pthread_mutex_t XrdFfsMiscUrlcache_mutex = PTHREAD_MUTEX_INITIALIZER;
time_t XrdFfsMiscUrlcachelife = 60;

int XrdFfsMisc_get_number_of_data_servers()
{
    return XrdFfsMiscNcachedurls;
}

void XrdFfsMisc_set_Urlcachelife(const char *urlcachelife)
{
    int t, len;
    char *life = strdup(urlcachelife);
    char last = 's';

    if (life == NULL) return;

    len = strlen(life);
    if (! isdigit(life[len-1])) 
    {
        last = life[len-1];
        life[len-1] = '\0';
    }
    sscanf(life, "%d", &t);
    XrdFfsMiscUrlcachelife = (time_t) t;
    life[len-1] = last;
    switch (last) 
    {
        case 'm':  /* minute */
            XrdFfsMiscUrlcachelife *= 60;
            break;
        case 'h':  /* hour */
            XrdFfsMiscUrlcachelife *= 3600;
            break;
        case 'd':  /* day */
            XrdFfsMiscUrlcachelife *= 3600*24;
            break; 
        default:   /* second */
            ;
    }
    free(life);
    return;
}

int XrdFfsMisc_get_all_urls(const char *oldurl, char **newurls, const int nnodes)
{
    time_t currtime;
    int i, nurls;

    pthread_mutex_lock(&XrdFfsMiscUrlcache_mutex); 

    currtime = time(NULL);
/* setting the cache to effectively not expire will let us know if a host is down */
    if (XrdFfsMiscCururl[0] == '\0' || 
        (currtime - XrdFfsMiscUrlcachetime) > XrdFfsMiscUrlcachelife || 
        strcmp(XrdFfsMiscCururl, oldurl) != 0)
    {
        for (i = 0; i < XrdFfsMiscNcachedurls; i++)
            if (XrdFfsMiscUrlcache[i] != NULL) free(XrdFfsMiscUrlcache[i]);
        for (i = 0; i < XrdFfs_MAX_NUM_NODES; i++)
            XrdFfsMiscUrlcache[i] = (char*) malloc(MAXROOTURLLEN);
        
        XrdFfsMiscUrlcachetime = currtime;
        strcpy(XrdFfsMiscCururl, oldurl);
        XrdFfsMiscNcachedurls = XrdFfsMisc_get_all_urls_real(oldurl, XrdFfsMiscUrlcache, nnodes);
        for (i = XrdFfsMiscNcachedurls; i < XrdFfs_MAX_NUM_NODES; i++)
            if (XrdFfsMiscUrlcache[i] != NULL) free(XrdFfsMiscUrlcache[i]);
    }

    nurls = XrdFfsMiscNcachedurls;
    for (i = 0; i < nurls; i++)
    {
        newurls[i] = (char*) malloc(MAXROOTURLLEN);
        strncpy(newurls[i], XrdFfsMiscUrlcache[i], MAXROOTURLLEN -1);
        newurls[i][MAXROOTURLLEN-1] = '\0';
    }

    pthread_mutex_unlock(&XrdFfsMiscUrlcache_mutex);
    return nurls;
}

int XrdFfsMisc_get_list_of_data_servers(char* list)
{
    XrdNetAddr uAddr;
    int i, n = 0;
    const char *netName;
    const char *hName, *hNend, *hPort, *hPend;
    char *url, *rc, *hostip, hsep;
  
    rc = (char*)malloc(sizeof(char) * XrdFfs_MAX_NUM_NODES * 1024);
    rc[0] = '\0';
    pthread_mutex_lock(&XrdFfsMiscUrlcache_mutex);
    for (i = 0; i < XrdFfsMiscNcachedurls; i++)
    {
        url = strdup(XrdFfsMiscUrlcache[i]); 
        hostip = &url[7];
        if (XrdNetUtils::Parse(hostip, &hName, &hNend, &hPort, &hPend))
           {n++;
            hsep = *hNend;
            hostip[hNend-hostip] = 0;
            hostip[hPend-hostip] = 0;
            if (uAddr.Set(hName,0) || !(netName = uAddr.Name()))
               {hostip[hNend-hostip] = hsep;
                hName = hostip;
                hPend = hNend;
               }
            strcat(rc, hName);
            if (hPort != hNend)
               {strcat(rc, ":");
                strcat(rc, hPort);
               }
            strcat(rc, "\n");
           }
        free(url);
    }
    pthread_mutex_unlock(&XrdFfsMiscUrlcache_mutex);
    strcpy(list, rc);
    free(rc);
    return n;
}

void XrdFfsMisc_refresh_url_cache(const char* url)
{
    int i, nurls;
    char *surl, **turls;

    turls = (char**) malloc(sizeof(char*) * XrdFfs_MAX_NUM_NODES);

// invalid the cache first
    pthread_mutex_lock(&XrdFfsMiscUrlcache_mutex);
    XrdFfsMiscUrlcachetime = 0;
    pthread_mutex_unlock(&XrdFfsMiscUrlcache_mutex);

    if (url != NULL)
        surl = strdup(url);
    else
        surl = strdup(XrdFfsMiscCururl);

    nurls = XrdFfsMisc_get_all_urls(surl, turls, XrdFfs_MAX_NUM_NODES);

    free(surl);
    for (i = 0; i < nurls; i++) free(turls[i]);
    free(turls);
}

void XrdFfsMisc_logging_url_cache(const char* url)
{
    int i;
    char *hostlist, *p1, *p2;

    if (url != NULL) XrdFfsMisc_refresh_url_cache(url);

    hostlist = (char*) malloc(sizeof(char) * XrdFfs_MAX_NUM_NODES * 256);
    i = XrdFfsMisc_get_list_of_data_servers(hostlist);

    syslog(LOG_INFO, "INFO: use the following %d data servers :", i);
    p1 = hostlist;
    p2 = strchr(p1, '\n');
    while (p2 != NULL)
    {
        p2[0] = '\0';
        syslog(LOG_INFO, "   %s", p1);
        p1 = p2 +1;
        p2 = strchr(p1, '\n');
    }
    free(hostlist);
}

void XrdFfsMisc_xrd_init(const char *rdrurl, const char *urlcachelife, int startQueue)
{
    static int OneTimeInitDone = 0;

// Do not execute this more than once
//
   if (OneTimeInitDone) return;
   OneTimeInitDone = 1;

//    EnvPutInt(NAME_FIRSTCONNECTMAXCNT,2);
//    EnvPutInt(NAME_DATASERVERCONN_TTL, 99999999);
//    EnvPutInt(NAME_LBSERVERCONN_TTL, 99999999);
//    EnvPutInt(NAME_READAHEADSIZE,0);
//    EnvPutInt(NAME_READCACHESIZE,0);
//    EnvPutInt(NAME_REQUESTTIMEOUT, 30);
    XrdPosixXrootd::setEnv("WorkerThreads", 50);

    if (getenv("XROOTDFS_SECMOD") != NULL && !strcmp(getenv("XROOTDFS_SECMOD"), "sss"))
        XrdFfsMisc_xrd_secsss_init();

    openlog("XrootdFS", LOG_ODELAY | LOG_PID, LOG_DAEMON);

    XrdFfsMisc_set_Urlcachelife(urlcachelife);
    XrdFfsMisc_refresh_url_cache(rdrurl);
    XrdFfsMisc_logging_url_cache(NULL);

#ifndef NOUSE_QUEUE
   if (startQueue)
   {
       if (getenv("XROOTDFS_NWORKERS") != NULL)
           XrdFfsQueue_create_workers(atoi(getenv("XROOTDFS_NWORKERS")));
       else
           XrdFfsQueue_create_workers(4);

       syslog(LOG_INFO, "INFO: Starting %d workers", XrdFfsQueue_count_workers());
   }
#endif

    pthread_mutex_init(&url_mlock, NULL);
    
    XrdFfsDent_cache_init();
}

// convert an unsigned decimal int to its base24 string
 
void toChar(unsigned int r, char *d24)
{
    const char alpha[] = "0123456789abcdefghijklmn"; 
    char tmp[8];
    memcpy(tmp, d24, 8);
    memcpy(d24+1, tmp, 8);
    d24[0] = alpha[r];
    return;
}

void decTo24(unsigned int d, char *d24)
{
    unsigned int r = d % 24; 
    toChar(r, d24);
    if ((d - r) != 0) decTo24((d-r)/24, d24);
    return;
}

char* ntoa24(unsigned int d) {  // caller is repsonsible to free the memory
    char *d24 = (char*) malloc(9);
    memset(d24, 0, 9);
    decTo24(d, d24);
    return d24;
}

/*  SSS security module */

XrdSecsssID *XrdFfsMiscSssid;
bool XrdFfsMiscSecsss = false;

void XrdFfsMisc_xrd_secsss_init()
{
    XrdFfsMiscSecsss = true;
    XrdFfsMiscSssid = new XrdSecsssID(XrdSecsssID::idDynamic);

/* Enforce "sss" security */
    setenv("XrdSecPROTOCOL", "sss", 1);
}

void XrdFfsMisc_xrd_secsss_register(uid_t user_uid, gid_t user_gid, int *id)
{
    struct passwd pw, *pwp;
    struct group gr, *grp;
    char user_num[9], *pwbuf, *grbuf, *tmp;
    static size_t pwbuflen = 0;
    static size_t grbuflen = 0;

    if (pwbuflen == 0) pwbuflen = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (grbuflen == 0) grbuflen = sysconf(_SC_GETGR_R_SIZE_MAX);

    XrdSecEntity XrdFfsMiscUent("");

    tmp = ntoa24((unsigned int)user_uid);
    strncpy(user_num, tmp, 9);
    free(tmp);

    if (id != NULL) {
        pthread_mutex_lock(&url_mlock);
        *id = iXrdConnPerUsr +1;  // id = 1 to nXrdConnPerUsr+1 (8)
        iXrdConnPerUsr = (iXrdConnPerUsr +1) % nXrdConnPerUsr;
        pthread_mutex_unlock(&url_mlock);
        user_num[strlen(user_num)] = *id + 48; // this require that *id stay as single digit.
    }
    else
        user_num[strlen(user_num)] = 48;  // id = NULL

    if (XrdFfsMiscSecsss)
    {
        pwbuf = (char*) malloc(pwbuflen);
        getpwuid_r(user_uid, &pw, pwbuf, pwbuflen, &pwp);
        grbuf = (char*) malloc(grbuflen);
        getgrgid_r(user_gid, &gr, grbuf, grbuflen, &grp);

        XrdFfsMiscUent.name = pw.pw_name;
        XrdFfsMiscUent.grps = gr.gr_name;
        XrdFfsMiscSssid->Register(user_num, &XrdFfsMiscUent, 0);
        free(pwbuf);
        free(grbuf);
    }
}

void XrdFfsMisc_xrd_secsss_editurl(char *url, uid_t user_uid, int *id)
{
    char user_num[9], nurl[1024], *tmp;

// Xrootd proxy also use some of the stat()/unlink(), etc funcitons here, without user "sss". It use the default 
// connection login name. Don't change this behavior. (so for proxy, use no "sss", and always have *id = 0
    if (id != NULL || XrdFfsMiscSecsss)
    {
        tmp = ntoa24(user_uid);
        strncpy(user_num, tmp, 9);
        free(tmp);
        if (id == NULL) user_num[strlen(user_num)] = 48;
        else user_num[strlen(user_num)] = *id + 48;

        nurl[0] = '\0';
        strcat(nurl, "root://");
        strcat(nurl, user_num);
        strcat(nurl, "@");
        strcat(nurl, &(url[7])); 
        strcpy(url, nurl);
    }
}

#ifdef __cplusplus
  }
#endif
