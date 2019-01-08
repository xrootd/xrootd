/******************************************************************************/
/*                                                                            */
/*                          X r d O u c a 2 x . c c                           */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef WIN32
#include "XrdSys/XrdWin32.hh"
#endif
#include "XrdOuc/XrdOuca2x.hh"

/******************************************************************************/
/*                                   a 2 i                                    */
/******************************************************************************/

int XrdOuca2x::a2i(XrdSysError &Eroute, const char *emsg, const char *item,
                                             int *val, int minv, int maxv)
{
    char *eP;

    if (!item || !*item)
       {Eroute.Emsg("a2x", emsg, "value not specified"); return -1;}

    errno = 0;
    *val  = strtol(item, &eP, 10);
    if (errno || *eP)
       {Eroute.Emsg("a2x", emsg, item, "is not a number");
        return -1;
       }
    if (*val < minv) 
       return Emsg(Eroute, emsg, item, "may not be less than %d", minv);
    if (maxv >= 0 && *val > maxv)
       return Emsg(Eroute, emsg, item, "may not be greater than %d", maxv);
    return 0;
}
 
/******************************************************************************/
/*                                  a 2 l l                                   */
/******************************************************************************/

int XrdOuca2x::a2ll(XrdSysError &Eroute, const char *emsg, const char *item,
                                long long *val, long long minv, long long maxv)
{
    char *eP;

    if (!item || !*item)
       {Eroute.Emsg("a2x", emsg, "value not specified"); return -1;}

    errno = 0;
    *val  = strtoll(item, &eP, 10);
    if (errno || *eP)
       {Eroute.Emsg("a2x", emsg, item, "is not a number");
        return -1;
       }
    if (*val < minv) 
       return Emsg(Eroute, emsg, item, "may not be less than %lld", minv);
    if (maxv >= 0 && *val > maxv)
       return Emsg(Eroute, emsg, item, "may not be greater than %lld", maxv);
    return 0;
}

/******************************************************************************/
/*                                  a 2 f m                                   */
/******************************************************************************/

int XrdOuca2x::a2fm(XrdSysError &Eroute, const char *emsg, const char *item,
                                              int *val, int minv, int maxv)
{  int rc, num;
   if ((rc = a2fm(Eroute, emsg, item, &num, minv))) return rc;
   if ((*val | maxv) != maxv) 
      {Eroute.Emsg("a2fm", emsg, item, "is too inclusive.");
       return -1;
      }

   *val = 0;
   if (num & 0100) *val |= S_IXUSR; // execute permission: owner
   if (num & 0200) *val |= S_IWUSR; // write permission:   owner
   if (num & 0400) *val |= S_IRUSR; // read permission:    owner
   if (num & 0010) *val |= S_IXGRP; // execute permission: group
   if (num & 0020) *val |= S_IWGRP; // write permission:   group
   if (num & 0040) *val |= S_IRGRP; // read permission:    group
   if (num & 0001) *val |= S_IXOTH; // execute permission: other
   if (num & 0002) *val |= S_IWOTH; // write permission:   other
   if (num & 0004) *val |= S_IROTH; // read permission:    other
   return 0;
}

int XrdOuca2x::a2fm(XrdSysError &Eroute, const char *emsg, const char *item,
                                              int *val, int minv)
{
    if (!item || !*item)
       {Eroute.Emsg("a2x", emsg, "value not specified"); return -1;}

    errno = 0;
    *val  = strtol(item, (char **)NULL, 8);
    if (errno)
       {Eroute.Emsg("a2x", emsg, item, "is not an octal number");
        return -1;
       }
    if (!(*val & minv))
       {Eroute.Emsg("a2x", emsg, item, "is too exclusive");;
        return -1;
       }
    return 0;
}

/******************************************************************************/
/*                                  a 2 s n                                   */
/******************************************************************************/

int XrdOuca2x::a2sn(XrdSysError &Eroute, const char *emsg, const char *item,
                    int *val, int nScale, int minv, int maxv)
{
    char *eP;
    int   nsVal = nScale;

    if (!item || !*item)
       {Eroute.Emsg("a2x", emsg, "value not specified"); return -1;}

    errno = 0;
    *val  = strtol(item, &eP, 10);
    if (errno || (*eP && *eP != '.'))
       {Eroute.Emsg("a2x", emsg, item, "is not a number");
        return -1;
       }

    if (*eP == '.')
       {eP++;
        while(*eP >= '0' && *eP <= '9')
             {if (nsVal > 1)
                 {*val = (*val * 10) + (*eP - int('0'));
                  nsVal /= 10;
                 }
              eP++;
             }
        if (*eP)
           {Eroute.Emsg("a2x", emsg, item, "is not a number");
            return -1;
           }
       }
    *val *= nsVal;

    if (*val < minv) 
       return Emsg(Eroute, emsg, item, "may not be less than %f",
                   double(minv)/double(nScale));
    if (maxv >= 0 && *val > maxv)
       return Emsg(Eroute, emsg, item, "may not be greater than %d",
                   double(maxv)/double(nScale));
    return 0;
}
 
/******************************************************************************/
/*                                  a 2 s p                                   */
/******************************************************************************/

int XrdOuca2x::a2sp(XrdSysError &Eroute, const char *emsg, const char *item,
                                long long *val, long long minv, long long maxv)
{
    char *pp, buff[120];
    int i;

    if (!item || !*item)
       {Eroute.Emsg("a2x", emsg, "value not specified"); return -1;}

    i = strlen(item);
    if (item[i-1] != '%') return a2sz(Eroute, emsg, item, val, minv, maxv);

    errno = 0;
    *val  = strtoll(item, &pp, 10);

    if (errno || *pp != '%')
       {Eroute.Emsg("a2x", emsg, item, "is not a number");
        return -1;
       }

    if (maxv < 0) maxv = 100;

    if (*val > maxv)
       {sprintf(buff, "may not be greater than %lld%%", maxv);
        Eroute.Emsg("a2x", emsg, item, buff);
        return -1;
       }

    if (minv < 0) minv = 0;

    if (*val > maxv)
       {sprintf(buff, "may not be less than %lld%%", minv);
        Eroute.Emsg("a2x", emsg, item, buff);
        return -1;
       }

    *val = -*val;
    return 0;
}

/******************************************************************************/
/*                                  a 2 s z                                   */
/******************************************************************************/

int XrdOuca2x::a2sz(XrdSysError &Eroute, const char *emsg, const char *item,
                                long long *val, long long minv, long long maxv)
{   long long qmult;
    char *eP, *fP = (char *)item + strlen(item) - 1;

    if (!item || !*item)
       {Eroute.Emsg("a2x", emsg, "value not specified"); return -1;}

         if (*fP == 'k' || *fP == 'K') qmult = 1024LL;
    else if (*fP == 'm' || *fP == 'M') qmult = 1024LL*1024LL;
    else if (*fP == 'g' || *fP == 'G') qmult = 1024LL*1024LL*1024LL;
    else if (*fP == 't' || *fP == 'T') qmult = 1024LL*1024LL*1024LL*1024LL;
    else                              {qmult = 1; fP++;}
    errno = 0;
    double dval  = strtod(item, &eP) * qmult;
    if (errno || eP != fP)
       {Eroute.Emsg("a2x", emsg, item, "is not a number");
        return -1;
       }
    *val = (long long)dval;
    if (*val < minv) 
       return Emsg(Eroute, emsg, item, "may not be less than %lld", minv);
    if (maxv >= 0 && *val > maxv)
       return Emsg(Eroute, emsg, item, "may not be greater than %lld", maxv);
    return 0;
}
 
/******************************************************************************/
/*                                  a 2 t m                                   */
/******************************************************************************/

int XrdOuca2x::a2tm(XrdSysError &Eroute, const char *emsg, const char *item, int *val,
                          int minv, int maxv)
{   int qmult;
    char *eP, *fP = (char *)item + strlen(item) - 1;

    if (!item || !*item)
       {Eroute.Emsg("a2x", emsg, "value not specified"); return -1;}

         if (*fP == 's' || *fP == 'S') qmult = 1;
    else if (*fP == 'm' || *fP == 'M') qmult = 60;
    else if (*fP == 'h' || *fP == 'H') qmult = 60*60;
    else if (*fP == 'd' || *fP == 'D') qmult = 60*60*24;
    else                              {qmult = 1; fP++;}

    errno = 0;
    *val  = strtoll(item, &eP, 10) * qmult;
    if (errno || eP != fP)
       {Eroute.Emsg("a2x", emsg, item, "is not a number");
        return -1;
       }
    if (*val < minv) 
       return Emsg(Eroute, emsg, item, "may not be less than %d", minv);
    if (maxv >= 0 && *val > maxv)
       return Emsg(Eroute, emsg, item, "may not be greater than %d", maxv);
    return 0;
}

/******************************************************************************/
/*                                  a 2 v p                                   */
/******************************************************************************/

int XrdOuca2x::a2vp(XrdSysError &Eroute, const char *emsg, const char *item,
                                             int *val, int minv, int maxv)
{
    char *pp;

    if (!item || !*item)
       {Eroute.Emsg("a2x", emsg, "value not specified"); return -1;}

    errno = 0;
    *val  = strtol(item, &pp, 10);

    if (!errno && *pp == '%')
       {if (*val < 0)
           {Eroute.Emsg("a2x", emsg, item, "may not be negative.");
            return -1;
           }
        if (*val > 100)
           {Eroute.Emsg("a2x", emsg, item, "may not be greater than 100%.");
            return -1;
           }
           else {*val = -*val; return 0;}
       }

    if (*val < minv) 
       return Emsg(Eroute, emsg, item, "may not be less than %d", minv);
    if (maxv >= 0 && *val > maxv)
       return Emsg(Eroute, emsg, item, "may not be greater than %d", maxv);
    return 0;
}

/******************************************************************************/
/*                                   b 2 x                                    */
/******************************************************************************/
  
int XrdOuca2x::b2x(const unsigned char* src, int slen, char* dst, int dlen)
{
   static const char *hv = "0123456789abcdef";

// Make sure destination buffer is large enough (2*slen+1)
//
   if (dlen < slen*2+1) return 0;

// Do conversion
//
   for (int i = 0; i < slen; i++)
       {*dst++ = hv[(src[i] >> 4) & 0x0f];
        *dst++ = hv[ src[i]       & 0x0f];
       }

// End with null byte and return the full length
//
   *dst = '\0';
   return slen*2+1;
}

/******************************************************************************/
/*                                   x 2 b                                    */
/******************************************************************************/
  
int XrdOuca2x::x2b(const char* src, int slen, unsigned char* dst, int dlen,
                   bool radj)
{
   int n, len = (slen+1)/2;
   bool odd = false;

// Make sure we have enough destination bytes
//
   if (len > dlen) return 0;

// If the length is odd then the first nibble is set to zero
//
   if (radj && slen & 0x01) {*dst = 0; odd = true;}

// Perform conversion
//
    while(slen--)
         {     if (*src >= '0' && *src <= '9') n = *src-48;
          else if (*src >= 'a' && *src <= 'f') n = *src-87;
          else if (*src >= 'A' && *src <= 'F') n = *src-55;
          else return 0;
          if (odd) *dst++ |= n;
             else  *dst    = n << 4;
          src++; odd = !odd;
         }
    return len;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
  
int XrdOuca2x::Emsg(XrdSysError &Eroute, const char *etxt1, const char *item,
                                         const char *etxt2, double val)
{char buff[256];
 sprintf(buff, etxt2, val);
 Eroute.Emsg("a2x", etxt1, item, buff);
 return -1;
}
  
int XrdOuca2x::Emsg(XrdSysError &Eroute, const char *etxt1, const char *item,
                                         const char *etxt2, int val)
{char buff[256];
 sprintf(buff, etxt2, val);
 Eroute.Emsg("a2x", etxt1, item, buff);
 return -1;
}

int XrdOuca2x::Emsg(XrdSysError &Eroute, const char *etxt1, const char *item,
                                         const char *etxt2, long long val)
{char buff[256];
 sprintf(buff, etxt2, val);
 Eroute.Emsg("a2x", etxt1, item, buff);
 return -1;
}
