/******************************************************************************/
/*                                                                            */
/*                        X r d S y s E r r o r . c c                         */
/*                                                                            */
/*(c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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

#include <cctype>
#ifndef WIN32
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <sys/types.h>
#include <sys/uio.h>
#else
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include "XrdSys/XrdWin32.hh"
#endif

#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define Set_IOV_Item(x, y) {iov[iovpnt].iov_base  = (caddr_t)x;\
                            iov[iovpnt++].iov_len = y;}

#define Set_IOV_Buff(x)    {iov[iovpnt].iov_base  = (caddr_t)x;\
                            iov[iovpnt++].iov_len = strlen(x);}

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
XrdSysError_Table *XrdSysError::etab = 0;

/******************************************************************************/
/*                                b a s e F D                                 */
/******************************************************************************/
  
int XrdSysError::baseFD() {return Logger->originalFD();}

/******************************************************************************/
/*                               e c 2 t e x t                                */
/******************************************************************************/

const char *XrdSysError::ec2text(int ecode)
{
    int xcode;
    const char *etxt = 0;
    XrdSysError_Table *etp = etab;

    xcode = (ecode < 0 ? -ecode : ecode);
    while((etp != 0) && !(etxt = etp->Lookup(xcode))) etp = etp->next;
    if (!etxt) etxt = XrdSysE2T(xcode);
    return etxt;
}
  
/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

int XrdSysError::Emsg(const char *esfx, int ecode, const char *txt1, 
                                                   const char *txt2)
{
    struct iovec iov[16];
    int iovpnt = 0;
    const char *etxt = ec2text(ecode);

                         Set_IOV_Item(0,0);                          //  0
    if (epfx && epfxlen) Set_IOV_Item(epfx, epfxlen);                //  1
    if (esfx           ) Set_IOV_Buff(esfx);                         //  2
                         Set_IOV_Item(": Unable to ", 12);           //  3
                         Set_IOV_Buff(txt1);                         //  4
    if (txt2 && txt2[0]){Set_IOV_Item(" ", 1);                       //  5
                         Set_IOV_Buff(txt2); }                       //  6
                         Set_IOV_Item("; ", 2);                      //  7
                         Set_IOV_Buff(etxt);                         //  8
                         Set_IOV_Item("\n", 1);                      //  9
    Logger->Put(iovpnt, iov);

    return ecode;
}
  
void XrdSysError::Emsg(const char *esfx, const char *txt1, 
                                         const char *txt2, 
                                         const char *txt3)
{
    struct iovec iov[16];
    int iovpnt = 0;

                         Set_IOV_Item(0,0);                          //  0
    if (epfx && epfxlen) Set_IOV_Item(epfx, epfxlen);                //  1
    if (esfx           ) Set_IOV_Buff(esfx);                         //  2
                         Set_IOV_Item(": ", 2);                      //  3
                         Set_IOV_Buff(txt1);                         //  4
    if (txt2 && txt2[0]){Set_IOV_Item(" ", 1);                       //  5
                         Set_IOV_Buff(txt2);}                        //  6
    if (txt3 && txt3[0]){Set_IOV_Item(" ", 1);                       //  7
                         Set_IOV_Buff(txt3);}                        //  8
                         Set_IOV_Item("\n", 1);                      //  9
    Logger->Put(iovpnt, iov);
}

/******************************************************************************/
/*                                   S a y                                    */
/******************************************************************************/
  
void XrdSysError::Say(const char *txt1, const char *txt2, const char *txt3,
                      const char *txt4, const char *txt5, const char *txt6)
{
    struct iovec iov[9];
    int iovpnt = 0;
    if (txt1)            Set_IOV_Buff(txt1)                          //  0
       else              Set_IOV_Item(0,0);
    if (txt2 && txt2[0]) Set_IOV_Buff(txt2);                         //  1
    if (txt3 && txt3[0]) Set_IOV_Buff(txt3);                         //  2
    if (txt4 && txt4[0]) Set_IOV_Buff(txt4);                         //  3
    if (txt5 && txt5[0]) Set_IOV_Buff(txt5);                         //  4
    if (txt6 && txt6[0]) Set_IOV_Buff(txt6);                         //  5
                         Set_IOV_Item("\n", 1);                      //  6
    Logger->Put(iovpnt, iov);
}
 
/******************************************************************************/
/*                                  T b e g                                   */
/******************************************************************************/
  
void XrdSysError::TBeg(const char *txt1, const char *txt2, const char *txt3)
{
 std::cerr <<Logger->traceBeg();
 if (txt1) std::cerr <<txt1 <<' ';
 if (txt2) std::cerr <<epfx <<txt2 <<": ";
 if (txt3) std::cerr <<txt3;
}

/******************************************************************************/
/*                                  T E n d                                   */
/******************************************************************************/
  
void XrdSysError::TEnd() {std::cerr <<std::endl; Logger->traceEnd();}
