#ifndef _XRDOSS_H
#define _XRDOSS_H
/******************************************************************************/
/*                                                                            */
/*                     X r d O s s   &   X r d O s s D F                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/******************************************************************************/
/*                                X r d O s s                                 */
/******************************************************************************/

class XrdOucEnv;
class XrdOucLogger;
  
class XrdOss
{
public:
virtual int     Create(const char *, mode_t, XrdOucEnv &)=0;
virtual int     Init(XrdOucLogger *, const char *)=0;
virtual int     Rename(const char *, const char *)=0;
virtual int     Stat(const char *, struct stat *, int resonly=0)=0;
virtual int     Unlink(const char *)=0;

                XrdOss() {}
virtual        ~XrdOss() {}
};

/******************************************************************************/
/*                              X r d O s s D F                               */
/******************************************************************************/
  
class XrdOssDF
{
public:
                // Directory oriented methods
virtual int     Opendir(const char *)                        {return -ENOTDIR;}
virtual int     Readdir(char *buff, int blen)                {return -ENOTDIR;}

                // File oriented methods
virtual int     Fstat(struct stat *)                         {return -EISDIR;}
virtual int     Fsync()                                      {return -EISDIR;}
virtual int     Ftruncate(unsigned long long)                {return -EISDIR;}
virtual int     isCompressed(char *cxidp=0)                  {return -EISDIR;}
virtual int     Open(const char *, int, mode_t, XrdOucEnv &) {return -EISDIR;}
virtual size_t  Read(off_t, size_t)                          {return (size_t)-EISDIR;}
virtual size_t  Read(void *, off_t, size_t)                  {return (size_t)-EISDIR;}
virtual size_t  ReadRaw(    void *, off_t, size_t)           {return (size_t)-EISDIR;}
virtual size_t  Write(const void *, off_t, size_t)           {return (size_t)-EISDIR;}

                // Methods common to both
virtual int     Close()=0;
inline  int     Handle() {return fd;}

                XrdOssDF() {fd = -1;}
virtual        ~XrdOssDF() {}

protected:

int     fd;      // The associated file descriptor.
};

#ifndef XrdOssOK
#define XrdOssOK 0
#endif
#endif
