/******************************************************************************/
/*                                                                            */
/*                        o o u c _ H a s h V a l . C                         */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$ 

const char *XrdOucHashValCVSID = "$Id$";

#include "Experiment/Experiment.hh"

#include "string.h"
#include "strings.h"

unsigned long XrdOucHashVal(const char *KeyVal)
         {extern unsigned long XrdOucHashVal2(const char *, int);
          return XrdOucHashVal2(KeyVal, strlen(KeyVal));
         }

unsigned long XrdOucHashVal2(const char *KeyVal, int KeyLen)
{  int j;
   unsigned long *lp, lword, hval = 0;
   int hl = sizeof(hval);

// If name is shorter than the hash length, use the name.
//
   if (KeyLen <= hl)
      {memcpy(&hval, KeyVal, (size_t)KeyLen);
       return hval;
      }

// Compute the length of the name and develop starting hash.
//
   hval = KeyLen;
   j = KeyLen % hl; KeyLen /= hl;
   if (j) 
      {memcpy(&lword, KeyVal, (size_t)hl);
       hval ^= lword;
      }
   lp = (unsigned long *)&KeyVal[j];

// Compute and return the full hash.
//
   while(KeyLen--)
        {memcpy(&lword, lp++, (size_t)hl);
         hval ^= lword;
        }
   return (hval ? hval : 1);
}
