//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientAbs                                                     // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Base class for objects handling redirections keeping open files      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef XRD_ABSCLIENTBASE_H
#define XRD_ABSCLIENTBASE_H

#include "XrdClientUnsolMsg.hh"

class XrdClientAbs: public XrdClientAbsUnsolMsgHandler {
public:

  virtual bool OpenFileWhenRedirected(char *newfhandle, 
				      bool &wasopen) = 0;
  void SetParm(const char *parm, int val);
  void SetParm(const char *parm, double val);
};

#endif
