// $Id$
/******************************************************************************/
/*                                                                            */
/*             X r d S e c g s i G M A P F u n L D A P . c c                  */
/*                                                                            */
/* (c) 2008, G. Ganis / CERN                                                  */
/*                                                                            */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* GMAP function implementation querying a LDAP database                      */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ldapsrv    = 0;
static char *searchbase = 0;
static char *attribute  = 0;

static bool gInit = 0;
void XrdSecgsiGMAPInit();

//
// Main function
//
int XrdSecgsiGMAPFun(const char *dn, int /*now*/, char **name)
{
   // Implementation of XrdSecgsiGMAPFun querying an LDAP server
   // for the distinguished name 'dn'; the unused argument is the time at
   // which the function is called.

   // Init the relevant fields (only once)
   if (!gInit) XrdSecgsiGMAPInit();

   // Return code
   int rc = -1;

   // Prepare the command to be executed
   char cmd[4096];
   sprintf(cmd, "ldapsearch -H %s -x -b \"%s\" \"subject=%s\" %s",
                 ldapsrv, searchbase, dn, attribute);

   // Execute the command into a pipe
   FILE *fp = popen(cmd, "r");
   if (fp) {
      char line[1024], att[40], uname[256];
      sprintf(att, "%s: ", attribute);
      while (fgets(line, sizeof(line), fp)) {
         // Look for a line starting with "uid: "
         if (!strncmp(line, att, strlen(att))) {
            sscanf(line, "%s %s", att, uname);
            if (name && *name) *name = strdup(uname);
            rc = 0;
            break;
         }
      }
      pclose(fp);
   }

   // Done
   return rc;
}

//
// Init the relevant parameters from a dedicated config file
//
void XrdSecgsiGMAPInit()
{
   // Initialize the relevant parameters from the file defined by XRDGSIGMAPLDAPCF

   if (!gInit) {
      if (getenv("XRDGSIGMAPLDAPCF")) {
         FILE *fcf = fopen(getenv("XRDGSIGMAPLDAPCF"), "r");
         if (fcf) {
            char l[4096], k[20], val[4096];
            while (fgets(l, sizeof(l), fcf)) {
               int len = strlen(l);
               if (len < 2) continue;
               if (l[0] == '#') continue;
               if (l[len-1] == '\n') l[len-1] = '\0';
               sscanf(l, "%s %s", k, val);
               if (!strcmp(k, "srv:")) {
                  ldapsrv = strdup(l);
               } else if (!strcmp(k, "base:")) {
                  searchbase = strdup(l);
               } else if (!strcmp(k, "attr:")) {
                  attribute = strdup(l);
               } else {
                  fprintf(stderr, "XrdSecgsiGMAPInit (LDAP): warning: unknown key: '%s' - ignoring\n", k);
               }
            }
            // Set flag
            gInit = 1;
            fclose(fcf);
         }
      }
   }
}
