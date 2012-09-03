/******************************************************************************/
/* XrdFfsQueue.hh  functions to run independent tasks in queue                */
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

#include <stdlib.h>
#include <pthread.h>

struct XrdFfsQueueTasks {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    short done;
    void* (*func)(void*);
    void **args;

    unsigned int id;
    struct XrdFfsQueueTasks *next;
    struct XrdFfsQueueTasks *prev;
};

struct XrdFfsQueueTasks* XrdFfsQueue_create_task(void* (*func)(void*), void **args, short initstat);
void XrdFfsQueue_free_task(struct XrdFfsQueueTasks *task);
void XrdFfsQueue_wait_task(struct XrdFfsQueueTasks *task);
unsigned int XrdFfsQueue_count_tasks();

int XrdFfsQueue_create_workers(int n);
int XrdFfsQueue_remove_workers(int n);
int XrdFfsQueue_count_workers();
 
#ifdef __cplusplus
  }
#endif

