//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientAdmin                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A UNIX reference admin client for xrootd.                            //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRD_CLIENT_ADMIN_H
#define XRD_CLIENT_ADMIN_H


#include "XrdClientAbs.hh"
#include <string>
#include "XrdClientConn.hh"

#include <vector>

typedef std::vector<string> vecString;
typedef std::vector<bool> vecBool;


class XrdClientAdmin : public XrdClientAbs {

   XrdClientConn*               fConnModule;
   XrdClientUrlInfo             fInitialUrl;

 public:
   XrdClientAdmin(const char *url);
   virtual ~XrdClientAdmin();

   bool Connect();

   // To be called after a redirection
   bool OpenFileWhenRedirected(char *, bool &);

   // Some administration functions, see the protocol specs for details
   bool SysStatX(const char *paths_list, kXR_char *binInfo, int numPath);
   int Stat(char *fname, long &id, long &size, long &flags, long &modtime);

   //  bool ExistFile(const char *filename);
   bool ExistFiles(vecString, vecBool&);
   bool ExistDirs(vecString, vecBool&);
   bool IsFileOnline(vecString, vecBool&);

   bool Mv(const char *fileDest, const char *fileSrc);
   bool Mkdir(const char *dir, int user, int group, int other);
   bool Chmod(const char *file, int user, int group, int other);
   bool Rm(const char *file);
   bool Rmdir(const char *path);
   bool Protocol(kXR_int32 &proto, kXR_int32 &kind);
   bool Prepare(vecString vs, kXR_char opts, kXR_char prty);

   bool ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender, XrdClientMessage *unsolmsg);


};

#endif
