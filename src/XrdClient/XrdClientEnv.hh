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

#ifndef XRD_CENV_H
#define XRD_CENV_H

#include "XrdOuc/XrdOucEnv.hh"

#include <string.h>

using namespace std;



#define EnvGetLong(x) XrdClientEnv::Instance()->GetInt(x)
#define EnvGetString(x) XrdClientEnv::Instance()->Get(x)
#define EnvPutString(name, val) XrdClientEnv::Instance()->Put(name, strdup(val))

class XrdClientEnv {
 private:

   XrdOucEnv      *fOucEnv;

   static XrdClientEnv *fgInstance;

 protected:
   XrdClientEnv();
   ~XrdClientEnv();

 public:

   char *                 Get(char *varname) {
      return fOucEnv->Get(varname);
   }

   long                   GetInt(char *varname) {
      return fOucEnv->GetInt(varname);
   }

   void                   Put(char *varname, char *value) {
      fOucEnv->Put(varname, value);
   }

   void  PutInt(char *varname, long value) {
      fOucEnv->PutInt(varname, value);
   }

   static XrdClientEnv    *Instance();

};

#endif
