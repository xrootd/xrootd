/******************************************************************************/
/* XrdFfsMisc.hh  Miscellanies functions                                      */
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

#ifdef __cplusplus
  extern "C" {
#endif

#define XrdFfs_MAX_NUM_NODES 4096 /* 64*64 max number of data nodes in a cluster */

char XrdFfsMisc_get_current_url(const char *oldurl, char *newurl);
int XrdFfsMisc_get_all_urls(const char *oldurl, char **newurls, const int nnodes);
int XrdFfsMisc_get_list_of_data_servers(char* list);
int XrdFfsMisc_get_number_of_data_servers();
void XrdFfsMisc_refresh_url_cache(const char* url);
void XrdFfsMisc_logging_url_cache(const char* url);

void XrdFfsMisc_xrd_init(const char *rdrurl, const char *urlcachelife, int startQueue);

void XrdFfsMisc_xrd_secsss_init();
void XrdFfsMisc_xrd_secsss_register(uid_t user_uid, gid_t user_gid, int *id);
void XrdFfsMisc_xrd_secsss_editurl(char *url, uid_t user_uid, int *id);

#ifdef __cplusplus
  }
#endif
