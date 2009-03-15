/************************************************************************/
/* Xrdadler32.cc                                                        */
/*                                                                      */
/* Auther: Wei Yang                                                     */
/* SLAC National Accelerator Laboratory / Stanford University, 2009     */
/*                                                                      */
/* Calculating Adler32 checksum of a local unix file (including stdin)  */
/* and file on a remote xrootd data server. Support using XROOTD_VMP.   */
/************************************************************************/

/*   $Id$                                                             */

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>

#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdClientAdmin.hh"
#include "XrdOuc/XrdOucString.hh"

class XrdPosixXrootPath
{
public:

void  CWD(const char *path);

char *URL(const char *path, char *buff, int blen);

      XrdPosixXrootPath();
     ~XrdPosixXrootPath();

private:

struct xpath 
       {struct xpath *next;
        const  char  *server;
               int    servln;
        const  char  *path;
               int    plen;
        const  char  *nath;
               int    nlen;

        xpath(struct xpath *cur,
              const   char *pServ,
              const   char *pPath,
              const   char *pNath) : next(cur),
                                     server(pServ),
                                     servln(strlen(pServ)),
                                     path(pPath),
                                     plen(strlen(pPath)),
                                     nath(pNath),
                                     nlen(pNath ? strlen(pNath) : 0) {}
       ~xpath() {}
       };

struct xpath *xplist;
char         *pBase;
char         *cwdPath;
int           cwdPlen;
}; /* this class is implemented in XrdPosix/XrdPosix.cc */

/* get the actual root url pointing to the data server */
char get_current_url(const char *oldurl, char *newurl)
{
    bool stat;
    long id, flags, modtime;
    long long size;
    XrdOucString url(oldurl);

    XrdClientAdmin *adm = new XrdClientAdmin(url.c_str());
    if (adm->Connect())
    {
        XrdClientUrlInfo u(url);

        stat = adm->Stat((char *)u.File.c_str(), id, size, flags, modtime);
        if (stat && adm->GetCurrentUrl().IsValid())
        {
            strcpy(newurl, adm->GetCurrentUrl().GetUrl().c_str());
            delete adm;
            return 1;
        }
    }
    delete adm;
    return 0;
}

/* the rooturl should point to the data server, not redirector */
char getchksum(const char *rooturl, char *chksum) 
{
    XrdOucString url(rooturl);
    char *sum = 0, *ptb, *pte;
    long sumlen; 

    XrdClientAdmin *adm = new XrdClientAdmin(url.c_str());
    if (adm->Connect()) 
    {
        XrdClientUrlInfo u(url);
        sumlen = adm->GetChecksum((kXR_char *)u.File.c_str(), (kXR_char**) &sum);
        pte = ptb = sum;
        if (sumlen != 0)
        {
            ptb = strchr(sum, ' ');
            ptb++;
            pte = strchr(ptb, ' ');
        }
        strncpy(chksum, ptb, pte - ptb);
        chksum[pte - ptb] = '\0';
        free(sum);
        delete adm;
        return pte - ptb;  /* 0 means sever doesn't implement a checksum */
    }
    else
        return -1;
}

#define N 64*1024  /* reading block size */

int main(int argc, char *argv[])
{
    XrdPosixXrootPath xrootpath;
    char path[2048], chksum[128], buf[N];
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
        else  
            xrootpath.URL(argv[1], path, sizeof(path));
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
        }
        else 
        {
            fd = STDIN_FILENO;
            strcpy(path, "-");
        }
        while ( (len = read(fd, buf, N)) > 0 )
            adler = adler32(adler, (const Bytef*)buf, len);

        if (fd != STDIN_FILENO) close(fd);
        printf("%08x %s\n", adler, path);
        return 0;
    }
    else
    {                       /* this is a Xrootd file */
        EnvPutInt(NAME_DEBUG, -1);
        if (!get_current_url(path, path))
        {
            printf("Error_accessing: %s\n", argv[1]);
            return 1;
        }

        if (getchksum(path, chksum) > 0) 
        {                   /* server implements checksum */
             printf("%s %s\n", chksum, argv[1]);
             return (strcmp(chksum, "Error_accessing:") ? 0 : 1);
        }
        else
        {                   /* need to read the file and calculate */
            EnvPutInt(NAME_READAHEADSIZE, N);
            EnvPutInt(NAME_READCACHESIZE, 2*N);
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
            printf("%08x %s\n", adler, argv[1]);
            return 0;
        }
    }
}
