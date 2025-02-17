#ifndef _XRDOSSACZIPFILE_H
#define _XRDOSSACZIPFILE_H
/******************************************************************************/
/*                                                                            */
/*                   X r d O s s A r c Z i p F i l e . h h                    */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <string.h>
#include <sys/types.h>

struct  stat;
struct  zip;
typedef zip zip_t;
struct  zip_file;
typedef zip_file  zip_file_t;
struct  zip_error;
typedef zip_error zip_error_t;
class   XrdOssDF;

class XrdOssArcZipFile 
{
public:

int     Close();

int     Open(const char* member);

ssize_t Read(void *buff, off_t offset, size_t blen);

int     Stat(struct stat& buf);

        XrdOssArcZipFile(const char* path, int &rc);

       ~XrdOssArcZipFile();

private:

void zipEmsg(const char *what, zip_error_t* zerr);
int  zip2syserr(const char *what, zip_error_t* zerr, bool msg=true);
int  zip2syserr(const char *what, int zrc, bool msg=true);

struct stat zFStat;
char*       zPath     = 0;
char*       zMember   = 0;
zip_t*      zFile     = 0;
zip_file_t* zSubFile  = 0;
off_t       zOffset   = 0;
bool        zSeek     = false;
bool        zEOF      = false;
};
#endif
