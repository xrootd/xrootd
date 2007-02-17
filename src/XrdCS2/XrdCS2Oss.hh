#ifndef _XRDCS2OSS_H
#define _XRDCS2OSS_H
/******************************************************************************/
/*                                                                            */
/*                          X r d C S 2 O s s . h h                           */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

const char *XrdCS2Oss2csCVSID = "$Id$";

#include "XrdOss/XrdOssApi.hh"

class XrdCS2File : public XrdOssFile
{
public:
  int Open(const char *, int, mode_t, XrdOucEnv&);

  XrdCS2File(const char* tid) : XrdOssFile(tid) {};
};

class XrdCS2Oss : public XrdOssSys
{
public:
  XrdOssDF *newFile(const char *tident)
                   {return (XrdOssDF *)new XrdCS2File(tident);}

  XrdCS2Oss() {};
};
#endif
