/******************************************************************************/
/*                                                                            */
/*                          X r d O u c E n v . c c                           */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "string.h"
#include "stdio.h"
#include <stdlib.h>

#include "XrdOuc/XrdOucEnv.hh"
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucEnv::XrdOucEnv(const char *vardata, int varlen, 
                     const XrdSecEntity *secent)
                    : env_Hash(8,13), secEntity(secent)
{
   char *vdp, varsave, *varname, *varvalu;

   if (!vardata) {global_env = 0; global_len = 0; return;}

// Get the length of the global information (don't rely on its being correct)
//
   if (!varlen) varlen = strlen(vardata);

// We want our env copy to start with a single ampersand
//
   while(*vardata == '&' && varlen) {vardata++; varlen--;}
   if (!varlen) {global_env = 0; global_len = 0; return;}
   global_env = (char *)malloc(varlen+2);
   *global_env = '&'; vdp = global_env+1;
   memcpy((void *)vdp, (const void *)vardata, (size_t)varlen);
   *(vdp+varlen) = '\0'; global_len = varlen+1;

// scan through the string looking for '&'
//
   while(*vdp)
        {while(*vdp == '&') vdp++;
         varname = vdp;

         while(*vdp && *vdp != '=' && *vdp != '&') vdp++;  // &....=
         if (!*vdp) break;
         if (*vdp == '&') continue;
         *vdp = '\0';
         varvalu = ++vdp;

         while(*vdp && *vdp != '&') vdp++;  // &....=....&
         varsave = *vdp; *vdp = '\0';

         if (*varname && *varvalu)
            env_Hash.Rep(varname, strdup(varvalu), 0, Hash_dofree);

         *vdp = varsave; *(varvalu-1) = '=';
        }
   return;
}

/******************************************************************************/
/*                               D e l i m i t                                */
/******************************************************************************/

char *XrdOucEnv::Delimit(char *value)
{
     while(*value) if (*value == ',') {*value = '\0'; return ++value;}
                      else value++;
     return (char *)0;
}
 
/******************************************************************************/
/*                                E x p o r t                                 */
/******************************************************************************/

int XrdOucEnv::Export(const char *Var, const char *Val)
{
   int vLen = strlen(Var);
   char *eBuff;

// If this is a null value then substitute a null string
//
   if (!Val) Val = "";

// Allocate memory. Note that this memory will appear to be lost.
//
   eBuff = (char *)malloc(vLen+strlen(Val)+2); // +2 for '=' and '\0'

// Set up envar
//
   strcpy(eBuff, Var);
   *(eBuff+vLen) = '=';
   strcpy(eBuff+vLen+1, Val);
   return putenv(eBuff);
}

/******************************************************************************/

int XrdOucEnv::Export(const char *Var, int Val)
{
   char buff[32];
   sprintf(buff, "%d", Val);
   return Export(Var, buff);
}


/******************************************************************************/
/*                                I m p o r t                                 */
/******************************************************************************/
bool XrdOucEnv::Import( const char *var, char *&val )
{
  char *value = getenv( var );
  if( !value || !*value )
     return false;

  val = value;
  return true;
}

/******************************************************************************/
/*                                I m p o r t                                 */
/******************************************************************************/
bool XrdOucEnv::Import( const char *var, long  &val )
{
  char *value;
  if( !Import( var, value ) )
    return false;

  char *status;
  val = strtol( value, &status, 0 );

  if( *status != 0 )
    return false;
  return true;
}

/******************************************************************************/
/*                                G e t I n t                                 */
/******************************************************************************/

long XrdOucEnv::GetInt(const char *varname) 
{
   char *cP;

// Retrieve a char* value from the Hash table and convert it into a long.
// Return -999999999 if the varname does not exist
//
  if ((cP = env_Hash.Find(varname)) == NULL) return -999999999;
  return atol(cP);
}

/******************************************************************************/
/*                                P u t I n t                                 */
/******************************************************************************/

void XrdOucEnv::PutInt(const char *varname, long value) 
{
// Convert the long into a char* and the put it into the hash table
//
  char stringValue[24];
  sprintf(stringValue, "%ld", value);
  env_Hash.Rep(varname, strdup(stringValue), 0, Hash_dofree);
}

/******************************************************************************/
/*                                G e t P t r                                 */
/******************************************************************************/

void *XrdOucEnv::GetPtr(const char *varname)
{
   void *Valp;
   char *cP, *Value = (char *)&Valp;
   int cLen, n, i = 0, Odd = 0;

// Retrieve the variable from the hash
//
   if ((cP = env_Hash.Find(varname)) == NULL) return (void *)0;

// Verify that the string is not too long or too short
//
   if ((cLen = strlen(cP)) != (int)sizeof(void *)*2) return (void *)0;

// Now convert the hex string back to its pointer value
//
   while(cLen--)
        {     if (*cP >= '0' && *cP <= '9') n = *cP-48;
         else if (*cP >= 'a' && *cP <= 'f') n = *cP-87;
         else if (*cP >= 'A' && *cP <= 'F') n = *cP-55;
         else return (void *)0;
         if (Odd) Value[i++] |= n;
            else  Value[i  ]  = n << 4;
         cP++; Odd = ~Odd;
        }

// All done, return the actual pointer value
//
   return Valp;
}

/******************************************************************************/
/*                                P u t P t r                                 */
/******************************************************************************/

void XrdOucEnv::PutPtr(const char *varname, void *value)
{
   static char hv[] = "0123456789abcdef";
   char Buff[sizeof(void *)*2+1], *Value = (char *)&value;
   int i, j = 0;

// Convert the pointer value to a hex string
//
   if (value) for (i = 0; i <(int)sizeof(void *); i++)
                  {Buff[j++] = hv[(Value[i] >> 4) & 0x0f];
                   Buff[j++] = hv[ Value[i]       & 0x0f];
                  }
   Buff[j] = '\0';

// Replace the value in he hash
//
   env_Hash.Rep(varname, strdup(Buff), 0, Hash_dofree);
}
