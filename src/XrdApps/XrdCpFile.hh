#ifndef __XRDCPFILE_HH__
#define __XRDCPFILE_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d C p F i l e . h h                           */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
#include <stdlib.h>
#include <string.h>

class XrdCpFile
{
public:

enum PType {isOther = 0, isDir, isFile, isXroot, isHttp, isHttps};

XrdCpFile        *Next;         // -> Next file in list
char             *Path;         // -> Absolute path to the file
short             Doff;         //    Offset to directory extension in Path
short             Dlen;         //    Length of directory extension (0 if none)
PType             Protocol;     //    Protocol type
char              ProtName[8];  //    Protocol name
long long         fSize;        //    Size of file

int               Extend(XrdCpFile **pLast, int &nFile, long long &nBytes);

int               Resolve();

                  XrdCpFile() : Next(0), Path(0), Doff(0), Dlen(0),
                                Protocol(isOther), fSize(0) {*ProtName = 0;}

                  XrdCpFile(const char *FSpec, int &badURL);

                  XrdCpFile(      char *FSpec, struct stat &Stat,
                                  short doff,         short dlen);

                 ~XrdCpFile() {if (Path) free(Path);}
};
#endif
