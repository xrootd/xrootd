//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdAbsClientBase                                                     // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Base class for objects handling redirections keeping open files      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRD_ABSCLIENTBASE_H
#define XRD_ABSCLIENTBASE_H


#include "XrdUnsolMsg.hh"

class XrdAbsClientBase: public XrdAbsUnsolicitedMsgHandler {
public:

  virtual bool OpenFileWhenRedirected(char *newfhandle, 
				      bool &wasopen) = 0;
  void SetParm(const char *parm, int val);
  void SetParm(const char *parm, double val);
};

#endif
