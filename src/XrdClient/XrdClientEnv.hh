//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientEnv                                                         // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Singleton used to handle the default parameter values                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef XRD_CENV_H
#define XRD_CENV_H

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdClientMutexLocker.hh"

#include <string.h>

using namespace std;



#define EnvGetLong(x) XrdClientEnv::Instance()->GetInt(x)
#define EnvGetString(x) XrdClientEnv::Instance()->Get(x)
#define EnvPutString(name, val) XrdClientEnv::Instance()->Put(name, strdup(val))
#define EnvPutInt(name, val) XrdClientEnv::Instance()->PutInt(name, val)

class XrdClientEnv {
 private:

   XrdOucEnv      *fOucEnv;
   pthread_mutex_t fMutex;
   static XrdClientEnv *fgInstance;

 protected:
   XrdClientEnv();
   ~XrdClientEnv();

 public:

   char *                 Get(char *varname) {
      char *res;
      XrdClientMutexLocker m(fMutex);

      res = fOucEnv->Get(varname);
      return res;
   }

   long                   GetInt(char *varname) {
      long res;
      XrdClientMutexLocker m(fMutex);

      res = fOucEnv->GetInt(varname);
      return res;
   }

   void                   Put(char *varname, char *value) {
      XrdClientMutexLocker m(fMutex);

      fOucEnv->Put(varname, value);
   }

   void  PutInt(char *varname, long value) {
      XrdClientMutexLocker m(fMutex);

      fOucEnv->PutInt(varname, value);
   }

   static XrdClientEnv    *Instance();

};

#endif
