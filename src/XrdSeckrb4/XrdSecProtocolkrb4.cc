/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l k r b 4 . c c                  */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

const char *XrdSecProtocolkrb4CVSID = "$Id$";

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <iostream.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSec/XrdSecInterface.hh"

#include "kerberosIV/krb.h"
typedef  int krb_rc;

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define XrdSecPROTOIDENT    "krb4"
#define XrdSecPROTOIDLEN    sizeof(XrdSecPROTOIDENT)
#define XrdSecNOIPCHK       0x0001
#define XrdSecDEBUG         0x1000

#define CLDBG(x) if (options & XrdSecDEBUG) cerr <<"sec_krb4: " <<x <<endl;

/******************************************************************************/
/*              X r d S e c P r o t o c o l k r b 4   C l a s s               */
/******************************************************************************/

class XrdSecProtocolkrb4 : public XrdSecProtocol
{
public:

        int                Authenticate  (XrdSecCredentials *cred,
                                          XrdSecParameters **parms,
                                          XrdSecClientName  &client,
                                          XrdOucErrInfo     *einfo=0);

        XrdSecCredentials *getCredentials(XrdSecParameters  *parm=0,
                                          XrdOucErrInfo     *einfo=0);

        const char        *getParms(int &plen, const char *host=0)
                                   {plen = Parmsize;
                                    return (const char *)Parms;
                                   }

        int  Init(XrdOucErrInfo *einfo, char *parms);
        int  Init_Client(XrdOucErrInfo *einfo);
        int  Init_Server(XrdOucErrInfo *einfo, char *info=0);

              XrdSecProtocolkrb4(int opts)
                    {sname[0]    = '\0';
                     iname[0]    = '\0';
                     rname[0]    = '\0';
                     keyfile     = (char *)"";
                     Parms       = (char *)"";
                     Parmsize    = 0;
                     Principal   = (char *)"?";
                     lifetime    = 0; options = opts;
                    }

             ~XrdSecProtocolkrb4() {} // Protocol objects are never deleted!!!

private:
static XrdOucMutex  krbContext;

char  sname[SNAME_SZ+1];
char  iname[INST_SZ+1];
char  rname[REALM_SZ+1];
char *Principal;

unsigned int lifetime;     // Client-side only
char *keyfile;             // Server-side only
char *Parms;
int   Parmsize;
int   options;

char *Append(char *dst, const char *src);
int   Fatal(XrdOucErrInfo *erp, int rc, const char *msg1, const char *msg2=0);
int   get_SIR(XrdOucErrInfo *erp, const char *sh, char *sbuff, char *ibuff, 
              char *rbuff);
};
  
/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
XrdOucMutex XrdSecProtocolkrb4::krbContext;

/******************************************************************************/
/*       P r o t o c o l   I n i t i a l i z a t i o n   M e t h o d s        */
/******************************************************************************/
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdSecProtocolkrb4::Init(XrdOucErrInfo *erp, char *p_args)
{
   int plen;

// Now, extract the "Principal.instance@realm" from the stream
//
   if (!p_args) 
      return Fatal(erp, EINVAL, "krb4 service Principal name not specified.");
   if (get_SIR(erp, p_args, sname, iname, rname) < 0) return -1;
   CLDBG("sname='" <<sname <<"' iname='" <<iname <<"' rname='" <<rname <<"'");

// Construct appropriate Principal name
//
   plen = strlen(sname) + strlen(iname) + strlen(rname) + 3;
   if (!(Principal = (char *)malloc(plen)))
      {Principal = (char *)"?";
       return Fatal(erp, ENOMEM, "insufficient storage to handle",
                                 (const char *)p_args);
      }
   if (*iname) sprintf((char *)Principal, "%s.%s@%s", sname, iname, rname);
      else     sprintf((char *)Principal, "%s@%s",    sname, rname);

// All done
//
   Parms    = strdup(p_args);
   Parmsize = strlen(Parms);
   return 0;
}

/******************************************************************************/
/*                           I n i t _ C l i e n t                            */
/******************************************************************************/
  
int XrdSecProtocolkrb4::Init_Client(XrdOucErrInfo *erp)
{
   XrdSecCredentials *credp;
   CREDENTIALS krb_cred;
   int rc;

// We need to obtain some credentials that we will throw away. We need
// to do this so that a ticket gets placed in the ticket cache if it's not
// there already. We then fetch the credentials to get the ticket lifetime.
//
   if (!(credp = getCredentials(0,erp))) return -1;
   delete credp;

// Now get the credentials
//
   krbContext.Lock();
   rc = krb_get_cred(sname, iname, rname, &krb_cred);
   krbContext.UnLock();

// Diagnose any errors
//
   if (rc != KSUCCESS)
      return Fatal(erp, rc, "Unable to get credentials;", krb_err_txt[rc]);

// Extract out the ticket lifetime
//
   lifetime = (unsigned int)krb_cred.lifetime;

// All done, return success
//
   return 0;
}

/******************************************************************************/
/*                           I n i t _ S e r v e r                            */
/******************************************************************************/
  
int XrdSecProtocolkrb4::Init_Server(XrdOucErrInfo *erp, char *kfn)
{

// If we have been passed a keyfile name, use it.
//
   if (kfn && *kfn) keyfile = strdup(kfn);
      else keyfile = (char *)"";

// All done
//
   return 0;
}

/******************************************************************************/
/*                               g e t _ S I R                                */
/******************************************************************************/
  
int XrdSecProtocolkrb4::get_SIR(XrdOucErrInfo *erp, const char *sh,
                                       char *sbuff, char *ibuff, char *rbuff)
{
    int h, i, j, k;

    k = strlen(sh);
    if (k > MAX_K_NAME_SZ) 
       return Fatal(erp, EINVAL, "service name is to long -", sh);

    for (j = 0; j < k && sh[j] != '@'; j++) {};
    if (j > k) j = k;
       else {if (j == k-1) 
                return Fatal(erp,EINVAL,"realm name missing after '@' in",sh);
             if (k-j > REALM_SZ) 
                return Fatal(erp, EINVAL, "realm name is to long in",sh);
            }

    for (i = 0; i < j && sh[i] != '.'; i++) {};
    if (i < j) {if (j-i >= INST_SZ) 
                   return Fatal(erp, EINVAL, "instance is too long in",sh);
                if (i+1 == j) 
                   return Fatal(erp,EINVAL,"instance name missing after '.' in",sh);
               }

    if (i == SNAME_SZ) 
       return Fatal(erp, EINVAL, "service name is too long in", sh);
    if (!i) return Fatal(erp, EINVAL, "service name not specified.");

    strncpy(sbuff, sh, i); sbuff[i] = '\0';
    if ( (h = j - i - 1) <= 0) ibuff[0] = '\0';
       else {strncpy(ibuff, &sh[i+1], h); ibuff[h] = '\0';}
    if ( (h = k - j - 1) <= 0) krb_get_lrealm(rbuff, 1);
       else {strncpy(rbuff, &sh[j+1], h); rbuff[h] = '\0';}

    return 1;
}

/******************************************************************************/
/*             C l i e n t   O r i e n t e d   F u n c t i o n s              */
/******************************************************************************/
/******************************************************************************/
/*                        g e t C r e d e n t i a l s                         */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolkrb4::getCredentials(XrdSecParameters *parm,
                                                      XrdOucErrInfo *error)
{
   const long cksum = 0L;
   struct ktext katix;       /* Kerberos data */
   krb_rc rc;
   char *buff;

// Supply null credentials if so needed for this protocol
//
   if (!sname[0])
      {CLDBG("Null credentials supplied.");
       return new XrdSecCredentials(0,0);
      }

// Supply kerberos-style credentials
//
   krbContext.Lock();
   rc = krb_mk_req(&katix, sname, iname, rname, cksum);
   krbContext.UnLock();

// Check if all succeeded. If so, copy the ticket into the buffer. We wish
// we could place the ticket directly into the buffer but architectural
// differences won't allow us that optimization.
// Because of some typedef stupidity, we are now reserving the 1st 8 bytes
// of the credentials buffer for identifying information.
//
   if (rc == KSUCCESS)
      {int bsz = XrdSecPROTOIDLEN+katix.length;
       if (!(buff = (char *)malloc(bsz)))
          {Fatal(error, ENOMEM, "Insufficient memory to hold credentials.");
           return (XrdSecCredentials *)0;
          }
       strcpy(buff, XrdSecPROTOIDENT);
       memcpy((void *)(buff+XrdSecPROTOIDLEN),
              (const void *)katix.dat, (size_t)katix.length);
       CLDBG("Returned " <<bsz <<" bytes of credentials; p=" <<Principal);
       return new XrdSecCredentials(buff, bsz);
      }

// Diagnose the failure
//
   {char ebuff[1024];
    snprintf(ebuff, sizeof(ebuff)-1, "Unable to get credentials from %s;",
             Principal);
    ebuff[sizeof(ebuff)-1] = '\0';
    Fatal(error, EACCES, (const char *)ebuff, krb_err_txt[rc]);
    return (XrdSecCredentials *)0;
   }
}

/******************************************************************************/
/*               S e r v e r   O r i e n t e d   M e t h o d s                */
/******************************************************************************/
/******************************************************************************/
/*                          A u t h e n t i c a t e                           */
/******************************************************************************/

int XrdSecProtocolkrb4::Authenticate(XrdSecCredentials *cred,
                                     XrdSecParameters **parms,
                                     XrdSecClientName  &client,
                                     XrdOucErrInfo     *error)
{
   struct ktext katix;       /* Kerberos data */
   struct auth_dat pid;
   krb_rc rc;
   char *idp;
   unsigned int ipaddr;  // Should be 32 bits in all supported data models

// Check if we have any credentials or if no credentials really needed.
// In either case, use host name as client name
//
   if (cred->size <= (int)XrdSecPROTOIDLEN || !cred->buffer)
      {strncpy(client.prot, "host", sizeof(client.prot));
       client.name[0] = '?'; client.name[1] = '\0';
       return 0;
      }

// Check if this is a recognized protocol
//
   if (strcmp(cred->buffer, XrdSecPROTOIDENT))
      {char emsg[256];
       snprintf(emsg, sizeof(emsg),
                "Authentication protocol id mismatch (%.4s != %.4s).",
                XrdSecPROTOIDENT,  cred->buffer);
       Fatal(error, EINVAL, (const char *)emsg);
       return -1;
      }

// Indicate who we are
//
   strncpy(client.prot, XrdSecPROTOIDENT, sizeof(client.prot));

// Create a kerberos style ticket (need to do that, unfortunately)
//
   katix.length = cred->size-XrdSecPROTOIDLEN;
   memcpy((void *)katix.dat, (const void *)&cred->buffer[XrdSecPROTOIDLEN],
                                   (size_t) katix.length);

// Prepare to check the ip address. This is rather poor since K4 "knows"
// that IP addresses are 4 bytes. Well, by the time IPV6 comes along, K4
// will be history (it's almost there now :-).
//
   if (options & XrdSecNOIPCHK) ipaddr = 0;
      else memcpy((void *)&ipaddr, (void *)&client.hostaddr, sizeof(ipaddr));

// Decode the credentials
//
   krbContext.Lock();
   rc = krb_rd_req(&katix, sname, iname, ipaddr, &pid, keyfile);
   krbContext.UnLock();

// Diagnose any errors
//
   if (rc != KSUCCESS)
      {Fatal(error, rc, "Unable to authenticate credentials;", krb_err_txt[rc]);
       return -1;
      }

// Construct the user's name
//
   idp = Append(client.name, pid.pname);
   if (pid.pinst[0])
      {*idp = '.'; idp++; idp = Append(idp, pid.pinst);}
   if (pid.prealm[0] && strcasecmp(pid.prealm, rname))
      {*idp = '@'; idp++; idp = Append(idp, pid.prealm);}

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                A p p e n d                                 */
/******************************************************************************/
  
 char *XrdSecProtocolkrb4::Append(char *dst, const char *src)
{
      while(*src) {*dst = *src; dst++; src++;}
      *dst = '\0';
      return dst;
}

/******************************************************************************/
/*                                 F a t a l                                  */
/******************************************************************************/

int XrdSecProtocolkrb4::Fatal(XrdOucErrInfo *erp, int rc, 
                              const char *msg1, const char *msg2)
{
   char *msgv[8];
   int k, i = 0;

              msgv[i++] = (char *)"Seckrb4: ";  //0
              msgv[i++] = (char *)msg1;         //1
   if (msg2) {msgv[i++] = (char *)" ";          //2
              msgv[i++] = (char *)msg2;         //3
             }
              msgv[i++] = (char *)" (p=";       //4
              msgv[i++] = Principal;            //5
              msgv[i++] = (char *)").";         //6

   if (erp) erp->setErrInfo(rc, msgv, i);
      else {for (k = 0; k < i; k++) cerr <<msgv[k];
            cerr <<endl;
           }

   return -1;
}
 
/******************************************************************************/
/*              X r d S e c P r o t o c o l k r b 4 O b j e c t               */
/******************************************************************************/
  
extern "C"
{
XrdSecProtocol *XrdSecProtocolkrb4Object(XrdOucErrInfo *erp,
                                         const char     mode,
                                         const char    *name,
                                         const char    *parms)
{
   XrdSecProtocolkrb4 *prot;
   char *op, *KPrincipal=0, *Keytab=0;
   char parmbuff[1024], mbuff[256];
   XrdOucTokenizer inParms(parmbuff);
   int NoGo, options = XrdSecNOIPCHK;

// Verify that the name we are given corresponds to the name we should have
//
   if (strcmp(name, XrdSecPROTOIDENT))
      {sprintf(mbuff, "Seckrb4: Protocol name mismatch; %s != %.4s",
                      XrdSecPROTOIDENT, name);
       if (erp) erp->setErrInfo(EINVAL, mbuff);
          else cerr <<mbuff <<endl;
       return (XrdSecProtocol *)0;
      }

// Duplicate the parms
//
   if (parms) strlcpy(parmbuff, parms, sizeof(parmbuff));
      else {char *msg = (char *)"Seckrb4: Kerberos parameters not specified.";
            if (erp) erp->setErrInfo(EINVAL, msg);
               else cerr <<msg <<endl;
            return (XrdSecProtocol *)0;
           }

// For clients, the first (and only) token must be the Principal name
// For servers: [<keytab>] [-ipchk] <Principal>
//
   if (inParms.GetLine())
      if (mode == 'c') KPrincipal = inParms.GetToken();
         else {if ((op = inParms.GetToken()) && *op == '/')
                  {Keytab = op; op = inParms.GetToken();}
               if (op && !strcmp(op, "-ipchk"))
                  {options &= ~XrdSecNOIPCHK;
                   op = inParms.GetToken();
                  }
               KPrincipal = op;
              }

// Now make sure that we have all the right info
//
   if (!KPrincipal)
      {char *msg = (char *)"Seckrb4: Kerberos Principal not specified.";
       if (erp) erp->setErrInfo(EINVAL, msg);
          else cerr <<msg <<endl;
       return (XrdSecProtocol *)0;
      }

// Get a new protocol object
//
   if (!(prot = new XrdSecProtocolkrb4(options)))
      {char *msg = (char *)"Seckrb4: Insufficient memory for protocol.";
       if (erp) erp->setErrInfo(ENOMEM, msg);
          else cerr <<msg <<endl;
       return (XrdSecProtocol *)0;
      }

// Initialize this protocol
//
   if (0 == prot->Init(erp, KPrincipal))
      if (mode == 'c') NoGo = prot->Init_Client(erp);
         else          NoGo = prot->Init_Server(erp, Keytab);
      else NoGo = 1;

// Check if all went well
//
   if (NoGo) {delete prot; prot = 0;}
   return prot;
}
}
