/******************************************************************************/
/*                                                                            */
/*                        X r d O u c E r r o r . c c                         */
/*                                                                            */
/*(c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC03-76-SFO0515 with the Deprtment of Energy                  */
/******************************************************************************/
 
//         $Id$

const char *XrdOucErrorCVSID = "$Id$";

#include <unistd.h>
#include <errno.h>
#include <iostream.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stropts.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"

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
  
XrdOucError_Table *XrdOucError::etab = 0;

/******************************************************************************/
/*                                b a s e F D                                 */
/******************************************************************************/
  
int XrdOucError::baseFD() {return Logger->originalFD();}

/******************************************************************************/
/*                               e c 2 t e x t                                */
/******************************************************************************/

char *XrdOucError::ec2text(int ecode)
{
    int xcode;
    char *etxt = 0;
    XrdOucError_Table *etp = etab;

    xcode = (ecode < 0 ? -ecode : ecode);
    while((etp != 0) && !(etxt = etp->Lookup(xcode))) etp = etp->next;
    if (!etxt) etxt = strerror(xcode);
    return etxt;
}
  
/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

int XrdOucError::Emsg(const char *esfx, int ecode, const char *txt1, char *txt2)
{
    struct iovec iov[16];
    int iovpnt = 0;
    char ebuff[16], tbuff[24], *etxt = 0;
    XrdOucError_Table *etp = etab;

    if (!(etxt = ec2text(ecode)))
       {snprintf(ebuff, sizeof(ebuff), "Error %d", ecode); etxt = ebuff;}

                         Set_IOV_Item(0,0);                          //  0
    if (epfx && epfxlen) Set_IOV_Item(epfx, epfxlen);                //  1
    if (esfx           ) Set_IOV_Buff(esfx);                         //  2
                         Set_IOV_Item(": ", 2);                      //  3
                         Set_IOV_Buff(etxt);                         //  4
                         Set_IOV_Item("; ", 2);                      //  5
                         Set_IOV_Buff(txt1);                         //  6
                         Set_IOV_Item(" ", 1);                       //  7
    if (txt2 && txt2[0]) Set_IOV_Buff(txt2);                         //  8
                         Set_IOV_Item("\n", 1);                      //  9
    Logger->Put(iovpnt, iov);
    Logger->Flush();

    return ecode;
}
  
void XrdOucError::Emsg(const char *esfx, const char *txt1, char *txt2, char *txt3)
{
    struct iovec iov[16];
    int iovpnt = 0;
    char tbuff[24];

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
    Logger->Flush();
}
  
void XrdOucError::Emsg(const char *esfx, const char *txt1, char *txt2)
{
    struct iovec iov[8];
    int iovpnt = 0;
    char tbuff[24];

                         Set_IOV_Item(0,0);                          //  0
    if (epfx && epfxlen) Set_IOV_Item(epfx, epfxlen);                //  1
    if (esfx           ) Set_IOV_Buff(esfx);                         //  2
                         Set_IOV_Item(": ", 2);                      //  3
                         Set_IOV_Buff(txt1);                         //  4
                         Set_IOV_Item(" ", 1);                       //  5
    if (txt2 && txt2[0]) Set_IOV_Buff(txt2);                         //  6
                         Set_IOV_Item("\n", 1);                      //  7
    Logger->Put(iovpnt, iov);
    Logger->Flush();
}
  
void XrdOucError::Emsg(const char *esfx, const char *txt1)
{
    struct iovec iov[8];
    int iovpnt = 0;
    char tbuff[24];

                         Set_IOV_Item(0,0);                          //  0
    if (epfx && epfxlen) Set_IOV_Item(epfx, epfxlen);                //  1
    if (esfx           ) Set_IOV_Buff(esfx);                         //  2
                         Set_IOV_Item(": ", 2);                      //  3
                         Set_IOV_Buff(txt1);                         //  4
                         Set_IOV_Item("\n", 1);                      //  5
    Logger->Put(iovpnt, iov);
    Logger->Flush();
}

/******************************************************************************/
/*                                   S a y                                    */
/******************************************************************************/
  
void XrdOucError::Say(const char *txt1, char *txt2, char *txt3)
{
    struct iovec iov[5];
    int iovpnt = 0;
    char *Odata[5];
    int   Odlen[5];
    if (txt1)            Set_IOV_Buff(txt1)                          //  0
       else              Set_IOV_Item(0,0);
    if (txt2 && txt2[0]) Set_IOV_Buff(txt2);                         //  1
    if (txt3 && txt3[0]) Set_IOV_Buff(txt3);                         //  2
                         Set_IOV_Item("\n", 1);                      //  3
    Logger->Put(iovpnt, iov);
}
 
/******************************************************************************/
/*                                  T b e g                                   */
/******************************************************************************/
  
void XrdOucError::TBeg(const char *txt1, const char *txt2, const char *txt3)
{
 cerr <<Logger->traceBeg();
 if (txt1) cerr <<txt1 <<' ';
 if (txt2) cerr <<epfx <<txt2 <<": ";
 if (txt3) cerr <<txt3;
}

/******************************************************************************/
/*                                  T E n d                                   */
/******************************************************************************/
  
void XrdOucError::TEnd() {cerr <<endl; Logger->traceEnd();}
