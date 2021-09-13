/******************************************************************************/
/*                                                                            */
/*                         X r d a d l e r 3 2 . c c                          */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*        Produced by Wei Yang for Stanford University under contract         */
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
  
/************************************************************************/
/* Calculating Adler32 checksum of a local unix file (including stdin)  */
/* and file on a remote xrootd data server. Support using XROOTD_VMP.   */
/************************************************************************/

#define _FILE_OFFSET_BITS 64

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
  #include <sys/xattr.h>
#endif
#include <zlib.h>

#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdPosix/XrdPosixXrootdPath.hh"
#include "XrdOuc/XrdOucString.hh"

#include "XrdCks/XrdCksXAttr.hh"
#include "XrdOuc/XrdOucXAttr.hh"

void fSetXattrAdler32(const char *path, int fd, const char* attr, char *value)
{
    XrdOucXAttr<XrdCksXAttr> xCS;
    struct stat st;
    
    if (fstat(fd, &st) || strlen(value) != 8) return;

    if (!xCS.Attr.Cks.Set("adler32") || !xCS.Attr.Cks.Set(value,8)) return;

    xCS.Attr.Cks.fmTime = static_cast<long long>(st.st_mtime);
    xCS.Attr.Cks.csTime = static_cast<int>(time(0) - st.st_mtime);

    xCS.Set("", fd);

// Remove any old attribute at this point
//
#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
    fremovexattr(fd, attr);
#elif defined(__solaris__)
    int attrfd;
    attrfd = openat(fd, attr, O_XATTR|O_RDONLY);
    if (attrfd >= 0)
       {unlinkat(attrfd, attr, 0); close(attrfd);}
#endif
}

int fGetXattrAdler32(int fd, const char* attr, char *value)
{
    struct stat st;
    char mtime[12], attr_val[25], *p;
    int rc;

    if (fstat(fd, &st)) return 0;
    sprintf(mtime, "%ld", st.st_mtime);

#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
    rc = fgetxattr(fd, attr, attr_val, 25);
#elif defined(__solaris__)
    int attrfd;
    attrfd = openat(fd, attr, O_XATTR|O_RDONLY);
    if (attrfd < 0) return(0);

    rc = read(attrfd, attr_val, 25);
    close(attrfd);
#else
    return(0);
#endif

    if (rc == -1 || attr_val[8] != ':') return(0);
    attr_val[8] = '\0';
    attr_val[rc] = '\0';
    p = attr_val + 9;
     
    if (strcmp(p, mtime)) return(0);

    strcpy(value, attr_val);

    return(strlen(value));
}

int fGetXattrAdler32(const char *path, int fd, const char* attr, char *value)
{
    XrdOucXAttr<XrdCksXAttr> xCS;
    struct stat st;

    if (!xCS.Attr.Cks.Set("adler32") || xCS.Get(path, fd) <= 0
    || strcmp(xCS.Attr.Cks.Name, "adler32"))
       {int rc = fGetXattrAdler32(fd, attr, value);
        if (rc == 8) fSetXattrAdler32(path, fd, attr, value);
        return rc;
       }

    if (fstat(fd, &st)
    ||  xCS.Attr.Cks.fmTime != static_cast<long long>(st.st_mtime)) return 0;

    xCS.Attr.Cks.Get(value, 9);
    return 8;
}

/* the rooturl should point to the data server, not redirector */
char getchksum(const char *rooturl, char *chksum) 
{
    char csBuff[256];
    int  csLen;

// Obtain the checksum (this is the default checksum)
//
   csLen = XrdPosixXrootd::Getxattr(rooturl, "xroot.cksum",
                                    csBuff,  sizeof(csBuff));
   if (csLen <  0) return -1;
   if (csLen == 0) return  0; // Server doesn't have the checksum

// Verify that the checksum returned is "adler32"
//
   if (strncmp("adler32 ", csBuff, 8)) return 0;

// Return the checksum value (this is really bad code)
//
   strcpy(chksum, csBuff+8);
   return strlen(csBuff+8);
}

#define N 1024*1024  /* reading block size */

int main(int argc, char *argv[])
{
    char path[2048], chksum[128], buf[N], adler_str[9];
    const char attr[] = "user.checksum.adler32";
    struct stat stbuf;
    int fd, len, rc;
    uLong adler;
    adler = adler32(0L, Z_NULL, 0);

    if (argc == 2 && ! strcmp(argv[1], "-h"))
    {
        printf("Usage: %s file. Calculating adler32 checksum of a given file.\n", argv[0]);
        printf("A file can be local file, stdin (if omitted), or root URL (including via XROOTD_VMP)\n");
        return 0;
    }

    path[0] = '\0';
    if (argc > 1)  /* trying to convert to root URL */
    {
        if (!strncmp(argv[1], "root://", 7))
            strcpy(path, argv[1]);
        else {XrdPosixXrootPath xrdPath;
              xrdPath.URL(argv[1], path, sizeof(path));
             }
    }
    if (argc == 1 || path[0] == '\0')
    {                        /* this is a local file */
        if (argc > 1) 
        {
            strcpy(path, argv[1]);
            rc = stat(path, &stbuf);        
            if (rc != 0 || ! S_ISREG(stbuf.st_mode) ||
                (fd = open(path,O_RDONLY)) < 0) 
            {
                printf("Error_accessing %s\n", path);
                return 1;
            }
            else  /* see if the adler32 is saved in attribute already */
                if (fGetXattrAdler32(path, fd, attr, adler_str) == 8)
                {
                    printf("%s %s\n", adler_str, path);
                    return 0;
                }
        }
        else 
        {
            fd = STDIN_FILENO;
            strcpy(path, "-");
        }
        while ( (len = read(fd, buf, N)) > 0 )
            adler = adler32(adler, (const Bytef*)buf, len);

        if (fd != STDIN_FILENO) 
        {   /* try saving adler32 to attribute before close() */
            sprintf(adler_str, "%08lx", adler);
            fSetXattrAdler32(path, fd, attr, adler_str);
            close(fd);
        }
        printf("%08lx %s\n", adler, path);
        return 0;
    }
    else
    {                       /* this is a Xrootd file */
        if (getchksum(path, chksum) > 0) 
        {                   /* server implements checksum */
             printf("%s %s\n", chksum, argv[1]);
             return (strcmp(chksum, "Error_accessing:") ? 0 : 1);
        }
        else
        {                   /* need to read the file and calculate */
            XrdPosixXrootd myPFS(-8, 8, 1);
            rc = XrdPosixXrootd::Stat(path, &stbuf);
            if (rc != 0 || ! S_ISREG(stbuf.st_mode) ||
                (fd = XrdPosixXrootd::Open(path, O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
            {
                printf("Error_accessing: %s\n", argv[1]);
                return 1;
            }
            while ( (len = XrdPosixXrootd::Read(fd, buf, N)) > 0 )
                adler = adler32(adler, (const Bytef*)buf, len);

            XrdPosixXrootd::Close(fd);
            printf("%08lx %s\n", adler, argv[1]);
            return 0;
        }
    }
}
