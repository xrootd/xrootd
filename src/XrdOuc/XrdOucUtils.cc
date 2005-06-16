/******************************************************************************/
/*                                                                            */
/*                        X r d O u c U t i l s . c c                         */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <errno.h>

#include "XrdNet/XrdNetDNS.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucUtils.hh"
  
/******************************************************************************/
/*                                  d o I f                                   */
/******************************************************************************/
  
// doIf() parses "if [<hostlist>] [named <namelist>]" returning 1 if true
// (i.e., this machine is one of the named hosts in hostlist and is named
// by one of the names in namelist). Return -1 (negative truth) if an error
// occured. Otherwise, returns false (0). Either hostlist, namelist, or both
// must be specified.

int XrdOucUtils::doIf(XrdOucError *eDest, XrdOucStream &Config,
                      const char *what, const char *hname, const char *nname)
{
   char *val;
   int hostok;

// Make sure that at least one thing appears after the if
//
   if (!(val = Config.GetWord()))
      {if (eDest) eDest->Emsg("Config","Host name missing after 'if' in", what);
       return -1;
      }

// Check if we are one of the listed hosts
//
   if (strcmp(val, "named"))
      {do {hostok = XrdNetDNS::isMatch(hname, val);
           val = Config.GetWord();
          } while(!hostok && val && strcmp(val, "named"));
       if (hostok) while(val && strcmp(val, "named")) val = Config.GetWord();
       if (!val) return hostok;
      }

// Check if we need to compare net names (we are here only if we either
// passed the hostlist test or there was no hostlist present)
//
   if (!(val = Config.GetWord()))
      {if (eDest)
          eDest->Emsg("Config","Instance name missing after 'if named' in", what);
       return -1;
      }

// Check if we are one of the names
//
   if (!nname) return 0;
   while(val && strcmp(val, nname)) val = Config.GetWord();

// All done
//
   return (val != 0);
}
/******************************************************************************/
/*                               g e n P a t h                                */
/******************************************************************************/

char *XrdOucUtils::genPath(const char *p_path, const char *inst, 
                           const char *s_path)
{
   char buff[2048];
   int i = strlcpy(buff, p_path, sizeof(buff));

   if (inst)
      {if (buff[i-1] != '/') buff[i++] = '/';
       strcpy(buff+i, inst); strcat(buff, "/");
      }
   if (s_path) strcat(buff, s_path);

   i = strlen(buff);
   if (buff[i] != '/') {buff[i++] = '/'; buff[i] = '\0';}

   return strdup(buff);
}

/******************************************************************************/
  
int XrdOucUtils::genPath(char *buff, int blen, const char *path, const char *psfx)
{
    int i, j;

    i = strlen(path);
    j = (psfx ? strlen(psfx) : 0);
    if (i+j+3 > blen) return -ENAMETOOLONG;

     strcpy(buff, path);
     if (psfx)
        {if (buff[i-1] != '/') buff[i++] = '/';
         strcpy(&buff[i], psfx);
        }
    return 0;
}

/******************************************************************************/
/*                              m a k e H o m e                               */
/******************************************************************************/
  
void XrdOucUtils::makeHome(XrdOucError &eDest, const char *inst)
{
   char buff[1024];

   if (!inst || !getcwd(buff, sizeof(buff))) return;

   strcat(buff, "/"); strcat(buff, inst);
   if (mkdir(buff, pathMode) && errno != EEXIST)
      {eDest.Emsg("Config", errno, "create home directory", buff);
       return;
      }

   chdir(buff);
}

/******************************************************************************/
/*                              m a k e P a t h                               */
/******************************************************************************/
  
int XrdOucUtils::makePath(char *path, mode_t mode)
{
    char *next_path = path+1;
    struct stat buf;

// Typically, the path exists. So, do a quick check before launching into it
//
   if (!stat(path, &buf)) return 0;

// Start creating directories starting with the root
//
   while((next_path = index(next_path, int('/'))))
        {*next_path = '\0';
         if (mkdir(path, mode))
            if (errno != EEXIST) return -errno;
                  else chmod(path, mode);
         *next_path = '/';
         next_path = next_path+1;
        }

// All done
//
   return 0;
}
 
/******************************************************************************/
/*                              s u b L o g f n                               */
/******************************************************************************/
  
char *XrdOucUtils::subLogfn(XrdOucError &eDest, const char *inst, char *logfn)
{
   const mode_t lfm = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH;
   char buff[2048], *sp;
   int rc;

   if (!inst || !(sp = rindex(logfn, '/')) || (sp == logfn)) return logfn;

   *sp = '\0';
   strcpy(buff, logfn); 
   strcat(buff, "/");
   if (inst && *inst) {strcat(buff, inst); strcat(buff, "/");}

   if ((rc = XrdOucUtils::makePath(buff, lfm)))
      {eDest.Emsg("Config", rc, "create log file path", buff);
       return 0;
      }

   *sp = '/'; strcat(buff, sp+1);
   free(logfn);
   return strdup(buff);
}
