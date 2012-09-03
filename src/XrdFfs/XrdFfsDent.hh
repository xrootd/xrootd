/******************************************************************************/
/* XrdFfsDent.hh  help functions to merge direntries                          */
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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
  extern "C" {
#endif

struct XrdFfsDentnames {
    char *name;
    struct XrdFfsDentnames *next;
}; 

void XrdFfsDent_names_del(struct XrdFfsDentnames **p);
void XrdFfsDent_names_add(struct XrdFfsDentnames **p, char *name);
void XrdFfsDent_names_join(struct XrdFfsDentnames **p, struct XrdFfsDentnames **n);
int  XrdFfsDent_names_extract(struct XrdFfsDentnames **p, char ***dnarray);

void XrdFfsDent_cache_init();
void XrdFfsDent_cache_destroy();
int  XrdFfsDent_cache_fill(char *dname, char ***dnarray, int nents);
int  XrdFfsDent_cache_search(char *dname, char *dentname);

#ifdef __cplusplus
  }
#endif


