//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdInputBuffer                                                       // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Buffer for incoming messages (responses)                             //
//  Handles the waiting (with timeout) for a message to come            //
//   belonging to a logical streamid                                    //
//  Multithread friendly                                                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_INPUTBUFFER_H
#define XRC_INPUTBUFFER_H

#include <pthread.h>
#include "XrdMessage.hh"

#include <list>
#include <map>

using namespace std;

typedef map<int, pthread_cond_t *> StreamidCondition;

class XrdInputBuffer {

private:

   list<XrdMessage*>            fMsgQue;      // queue for incoming messages
   list<XrdMessage*>::iterator  fMsgIter;     // an iterator on it

   pthread_mutex_t             fMutex;       // mutex to protect data structures
   pthread_mutex_t             fCndMutex;    // mutex to protect the condition variables

   StreamidCondition           fSyncobjRepo; // each streamid counts on a condition
                                             // variable to make the caller wait
                                             // until some data is available

   pthread_cond_t  *GetSyncObjOrMakeOne(int streamid);
   int             MsgForStreamidCnt(int streamid);

public:
   XrdInputBuffer();
  ~XrdInputBuffer();

   inline bool     IsMexEmpty() { return (MexSize() == 0); }
   inline bool     IsSemEmpty() { return (SemSize() == 0); }
   inline int      MexSize() { return fMsgQue.size(); }
   int             PutMsg(XrdMessage *msg);
   inline int      SemSize() { return fSyncobjRepo.size(); }
   XrdMessage      *GetMsg(int streamid, int secstimeout);
};



#endif
