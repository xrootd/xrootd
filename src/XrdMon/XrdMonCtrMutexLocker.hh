/*****************************************************************************/
/*                                                                           */
/*                         XrdMonCtrMutexLocker.hh                           */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONCTRMUTEXLOCKER_HH
#define XRDMONCTRMUTEXLOCKER_HH

#include <pthread.h>

class XrdMonCtrMutexLocker {
public:
    inline XrdMonCtrMutexLocker(pthread_mutex_t m) : _mutex(m) {
        pthread_mutex_lock(&_mutex);
    }
    inline ~XrdMonCtrMutexLocker() {
        pthread_mutex_unlock(&_mutex);
    }
private:
   pthread_mutex_t _mutex;
};

#endif /* XRDMONCTRMUTEXLOCKER_HH */
