#ifndef __OUC_ERRINFO_H__
#define __OUC_ERRINFO_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c E r r I n f o . h h                       */
/*                                                                            */
/* (c) 2043 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/******************************************************************************/

//        $Id$

#include <string.h>      // For strlcpy()
#include <sys/types.h>

#include "XrdOucPlatform.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/

// Maximum number of bytes of error information that can be set
//
#define OUC_MAX_ERROR_LEN 1280

/******************************************************************************/
/*                              X r d O u c E I                               */
/******************************************************************************/

struct XrdOucEI      // Err information structure
{ 
           int  code;
           char message[OUC_MAX_ERROR_LEN];

           void clear() {code = 0; message[0] = '\0';}

           XrdOucEI &operator =(const XrdOucEI &rhs)
               {code = rhs.code; strcpy(message, rhs.message); return *this;}

           XrdOucEI() {clear();}
};

/******************************************************************************/
/*                         X r d O u c E r r I n f o                          */
/******************************************************************************/
  
class XrdOucErrInfo
{
public:
      void  clear() {ErrInfo.clear();}

      int   setErrCode(const int code)
               {return ErrInfo.code = code;}
      int   setErrInfo(const int code, const char *message)
               {strlcpy(ErrInfo.message, message, sizeof(ErrInfo.message));
                return ErrInfo.code = code;
               }
      int   setErrInfo(const int code, char *txtlist[], int n)
               {int i, j = 0, k = sizeof(ErrInfo.message), l;
                for (i = 0; i < n && k > 1; i++)
                    {l = strlcpy(&ErrInfo.message[j], txtlist[i], k);
                     j += l; k -= l;
                    }
                return ErrInfo.code = code;
               }
      void  setErrUser(char *user)
               {ErrUser = user;}
      int   getErrInfo() {return ErrInfo.code;}
      int   getErrInfo(XrdOucEI &errorParm)
               {errorParm = ErrInfo; return ErrInfo.code;}
const char *getErrText() 
               {return (const char *)ErrInfo.message;}
const char *getErrText(int &ecode)
               {ecode = ErrInfo.code; return (const char *)ErrInfo.message;}
      char *getErrUser()
               {return ErrUser;}

      XrdOucErrInfo &operator =(const XrdOucErrInfo &rhs)
               {ErrInfo = rhs.ErrInfo; 
                ErrUser = rhs.ErrUser;
                return *this;
               }

      XrdOucErrInfo(char *user=0) {ErrUser = (user ? user : (char *)"?");}

protected:

XrdOucEI ErrInfo;
char    *ErrUser;
};
#endif
