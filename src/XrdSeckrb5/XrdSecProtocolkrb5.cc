/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l k r b 5 . c c                  */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

const char *XrdSecProtocolkrb5CVSID = "$Id$";

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <iostream.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>

extern "C" {
#include "krb5.h"
#include "com_err.h"
}

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSec/XrdSecInterface.hh"
  
/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/

#define krb_etxt(x) (char *)error_message(x)
  
#define XrdSecPROTOIDENT    "krb5"
#define XrdSecPROTOIDLEN    sizeof(XrdSecPROTOIDENT)
#define XrdSecNOIPCHK       0x0001
#define XrdSecDEBUG         0x1000

#define CLDBG(x) if (options & XrdSecDEBUG) cerr <<"seckrb5: " <<x <<endl;

typedef  krb5_error_code krb_rc;

/******************************************************************************/
/*              X r d S e c P r o t o c o l k r b 5   C l a s s               */
/******************************************************************************/

class XrdSecProtocolkrb5 : public XrdSecProtocol
{
public:

        int                Authenticate  (XrdSecCredentials *cred,
                                          XrdSecParameters **parms,
                                          XrdSecClientName  &client,
                                          XrdOucErrInfo     *einfo=0);

        XrdSecCredentials *getCredentials(XrdSecParameters  *parm=0,
                                          XrdOucErrInfo     *einfo=0);

        const char        *getParms(int &psz, const char *host=0)
                                   {psz = Parmsize;
                                    return (const char *)Parms;
                                   }

        int  Init(XrdOucErrInfo *einfo, char *parms);
        int  Init_Client(XrdOucErrInfo *einfo);
        int  Init_Server(XrdOucErrInfo *einfo, char *info=0);

              XrdSecProtocolkrb5(int opts)
                     {krb_keytab   = NULL;
                      lifetime = 0; options = opts;
                      Principal = (char *)"?";
                      Parms = (char *)""; Parmsize = 0;
                     }

             ~XrdSecProtocolkrb5() {} // Protocol objects are never deleted!!!

private:
static XrdOucMutex krbContext;

krb5_context       krb_context;
krb5_principal     krb_principal;
krb5_ccache        krb_ccache;
krb5_creds        *krb_creds;
krb5_keytab        krb_keytab;

unsigned long      lifetime;    // Client-side only
int                options;
char              *Principal;
char              *Parms;
int                Parmsize;
int   get_krbCreds();
int   Fatal(XrdOucErrInfo *erp, int rc, const char *msg1, char *msg2=0);
};

/******************************************************************************/
/*                           S t a t i c   D a t a                            */
/******************************************************************************/
  
XrdOucMutex XrdSecProtocolkrb5::krbContext;

/******************************************************************************/
/*       P r o t o c o l   I n i t i a l i z a t i o n   M e t h o d s        */
/******************************************************************************/
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdSecProtocolkrb5::Init(XrdOucErrInfo *erp, char *p_args)
{
    krb_rc rc;

// Create a kerberos context. There is one such context per protocol object.
//
   krbContext.Lock();
   if ((rc = krb5_init_context(&krb_context)))
      {krbContext.UnLock();
       return Fatal(erp,ENOPROTOOPT,"seckrb5: Kerberos initialization failed;",
                    krb_etxt(rc));
      }

// Obtain the default cache location
//
   if ((rc = krb5_cc_default(krb_context, &krb_ccache)))
      {krbContext.UnLock();
       return Fatal(erp,ENOPROTOOPT,"Unable to locate cred cache",krb_etxt(rc));
      }

// Make sure we have arguments
//
   if (!p_args)
      {krbContext.UnLock();
       return Fatal(erp, EPROTO, "krb5 service principal name not specified.");
      }

// Now, extract the "principal/instance@realm" from the stream
//
   if ((rc = krb5_parse_name(krb_context,(const char *)p_args,&krb_principal)))
     {krbContext.UnLock();
      return Fatal(erp,EPROTO,"Cannot parse service name",krb_etxt(rc));
     }

// Establish the correct principal to use
//
   if ((rc = krb5_unparse_name(krb_context,(krb5_const_principal)krb_principal,
                              (char **)&Principal)))
      {krbContext.UnLock();
       return Fatal(erp, EPROTO,"Unable to unparse principal;",krb_etxt(rc));
      }
   CLDBG("sname=" <<Principal);

// All done
//
   krbContext.UnLock();
   Parms    = strdup(p_args);
   Parmsize = strlen(Parms);
   return 0;
}

/******************************************************************************/
/*                           I n i t _ C l i e n t                            */
/******************************************************************************/
  
int XrdSecProtocolkrb5::Init_Client(XrdOucErrInfo *einfo)
{
   krb_rc rc;

// Get the credentials
//
   krbContext.Lock();
   rc = (krb_rc)get_krbCreds();
   krbContext.UnLock();

// Diagnose any errors
//
   if (rc) return Fatal(einfo, ESRCH, "srckrb5: Unable to get credentials;",
                        krb_etxt(rc));

// Extract out the ticket lifetime
//
   lifetime = (unsigned long)krb_creds->times.endtime;

// All done, return success
//
   return 0;
}

/******************************************************************************/
/*                           I n i t _ S e r v e r                            */
/******************************************************************************/
  
int XrdSecProtocolkrb5::Init_Server(XrdOucErrInfo *einfo, char *kfn)
{
   char buff[1024];
   krb_rc rc;

// Try to resolve the keyfile name
//
   if (kfn && *kfn)
      {if ((rc = krb5_kt_resolve(krb_context, kfn, &krb_keytab)))
          {snprintf(buff, sizeof(buff), "Unable to find keytab '%s';", kfn);
           return Fatal(einfo, ESRCH, buff, krb_etxt(rc));
          }
      } else {
       krb5_kt_default(krb_context, &krb_keytab);
      }

// All done
//
   return 0;
}

/******************************************************************************/
/*             C l i e n t   O r i e n t e d   F u n c t i o n s              */
/******************************************************************************/
/******************************************************************************/
/*                        g e t C r e d e n t i a l s                         */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolkrb5::getCredentials(XrdSecParameters *parm,
                                                      XrdOucErrInfo    *error)
{
   char *buff;
   int bsz;
   krb_rc rc;
   krb5_auth_context auth_context = NULL;
   krb5_data outbuf;

// Supply null credentials if so needed for this protocol
//
   if (!Principal || *Principal == '?')
      {CLDBG("Null credentials supplied.");
       return new XrdSecCredentials(0,0);
      }

// Clear outgoing ticket
//
   outbuf.length = 0; outbuf.data = 0;

// Supply kerberos-style credentials
//
   krbContext.Lock();
   rc = krb5_mk_req_extended(krb_context, &auth_context, 
             AP_OPTS_USE_SESSION_KEY, (krb5_data *)0, krb_creds, &outbuf);
   krbContext.UnLock();

// Check if all succeeded. If so, copy the ticket into the buffer. We wish
// we could place the ticket directly into the buffer but architectural
// differences won't allow us that optimization.
//
   if (!rc)
      {bsz = XrdSecPROTOIDLEN+outbuf.length;
       if (!(buff = (char *)malloc(bsz)))
          {Fatal(error, ENOMEM, "Insufficient memory to hold credentials.");
           return (XrdSecCredentials *)0;
          }
       strcpy(buff, XrdSecPROTOIDENT);
       memcpy((void *)(buff+XrdSecPROTOIDLEN),
              (const void *)outbuf.data, (size_t)outbuf.length);
       CLDBG("Returned " <<bsz <<" bytes of credentials; p=" <<Principal);
       if (outbuf.data)  free(outbuf.data);
       if (auth_context) krb5_auth_con_free(krb_context, auth_context);
       return new XrdSecCredentials(buff, bsz);
      }

// Diagnose the failure
//
   Fatal(error, EACCES, "Unable to get credetials;", krb_etxt(rc));
   if (outbuf.data)  free(outbuf.data);
   if (auth_context) krb5_auth_con_free(krb_context, auth_context);
   return (XrdSecCredentials *)0;
}

/******************************************************************************/
/*               S e r v e r   O r i e n t e d   M e t h o d s                */
/******************************************************************************/
/******************************************************************************/
/*                          A u t h e n t i c a t e                           */
/******************************************************************************/

int XrdSecProtocolkrb5::Authenticate(XrdSecCredentials *cred,
                                     XrdSecParameters **parms,
                                     XrdSecClientName  &client,
                                     XrdOucErrInfo     *error)
{
   krb5_data         inbuf;                 /* Kerberos data */
   krb5_ticket      *ticket       = NULL;
   krb5_address      ipadd;
   krb5_auth_context auth_context = NULL;
   krb_rc rc = 0;
   char *iferror = 0;

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
       Fatal(error, EPROTO, emsg);
       return -1;
      }

// Indicate who we are
//
   strncpy(client.prot, XrdSecPROTOIDENT, sizeof(client.prot));

// Create a kerberos style ticket and obtain the kerberos mutex
//
   inbuf.length = cred->size -XrdSecPROTOIDLEN;
   inbuf.data   = &cred->buffer[XrdSecPROTOIDLEN];
   krbContext.Lock();

// Check if whether the IP address in the credentials must match that of
// the incomming host.
//
   if (!(options & XrdSecNOIPCHK))
      {struct sockaddr_in *ip = (struct sockaddr_in *)&client.hostaddr;
      // The above is a hack but K5 does it this way
       ipadd.addrtype = ADDRTYPE_INET;
       ipadd.length = sizeof(ip->sin_addr);
       ipadd.contents = (krb5_octet *)&ip->sin_addr;
       iferror = (char *)"Unable to validate ip address;";
       if (!(rc=krb5_auth_con_init(krb_context, &auth_context)))
             rc=krb5_auth_con_setaddrs(krb_context, auth_context, NULL, &ipadd);
      }

// Decode the credentials and extract client's name
//
   if (!rc)
       if ((rc = krb5_rd_req(krb_context, &auth_context, &inbuf,
                            (krb5_const_principal)krb_principal,
                             krb_keytab, NULL, &ticket)))
          iferror = (char *)"Unable to authenticate credentials;";
          else if ((rc = krb5_aname_to_localname(krb_context,
                                    ticket->enc_part2->client,
                                    sizeof(client.name)-1, client.name)))
                  iferror = (char *)"Unable to extract client name;";

// Release any allocated storage at this point and unlock mutex
//
   client.name[sizeof(client.name)-1] = '\0';
   if (ticket)       krb5_free_ticket(krb_context, ticket);
   if (auth_context) krb5_auth_con_free(krb_context, auth_context);
   krbContext.UnLock();

// Diagnose any errors
//
   if (rc && iferror)
      {Fatal(error, EACCES, iferror, krb_etxt(rc));
       return -1;
      }

// All done
//
   return 0;
}
 
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                 F a t a l                                  */
/******************************************************************************/

int XrdSecProtocolkrb5::Fatal(XrdOucErrInfo *erp, int rc,
                              const char *msg1, char *msg2)
{
   char *msgv[8];
   int k, i = 0;

              msgv[i++] = (char *)"Seckrb5: ";  //0
              msgv[i++] = (char *)msg1;         //1
   if (msg2) {msgv[i++] = (char *)" ";          //2
              msgv[i++] =  msg2;                //3
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
/*                          g e t _ k r b C r e d s                           */
/******************************************************************************/

// Warning! The krbContext lock must be held prior to calling this routine
//

int XrdSecProtocolkrb5::get_krbCreds()
{
    krb_rc rc;
    krb5_creds mycreds;

// Clear my credentials
//
   memset((char *)&mycreds, 0, sizeof(mycreds));

// Copy the current target principal into the credentials
//
   if ((rc = krb5_copy_principal(krb_context, krb_principal, &mycreds.server)))
      {CLDBG("get_krbCreds: err copying principal to creds; " <<krb_etxt(rc));
       return rc;
      }

// Get our principal name
//
   if ((rc = krb5_cc_get_principal(krb_context, krb_ccache, &mycreds.client)))
      {krb5_free_cred_contents(krb_context, &mycreds);
       CLDBG("get_krbCreds: err copying client name to creds; " <<krb_etxt(rc));
       return rc;
      }

// Now get the credentials (free our local info)
//
   rc = krb5_get_credentials(krb_context, 0, krb_ccache, &mycreds,  &krb_creds);
   krb5_free_cred_contents(krb_context, &mycreds);

// Check if all went well
//
   if (rc) {CLDBG("get_krbCreds: unable to get creds; " <<krb_etxt(rc));}
   return rc;
}
 
/******************************************************************************/
/*              X r d S e c P r o t o c o l k r b 5 O b j e c t               */
/******************************************************************************/
  
extern "C"
{
XrdSecProtocol *XrdSecProtocolkrb5Object(XrdOucErrInfo *erp,
                                         const char     mode,
                                         const char    *name,
                                         const char    *parms)
{
   XrdSecProtocolkrb5 *prot;
   char *op, *KPrincipal=0, *Keytab=0;
   char parmbuff[1024], mbuff[256];
   XrdOucTokenizer inParms(parmbuff);
   int NoGo, options = XrdSecNOIPCHK;

// Verify that the name we are given corresponds to the name we should have
//
   if (strcmp(name, XrdSecPROTOIDENT))
      {sprintf(mbuff,"Seckrb5: Protocol name mismatch; %s != %.4s",
               XrdSecPROTOIDENT,name);
       if (erp) erp->setErrInfo(EPROTO, mbuff);
          else cerr <<mbuff <<endl;
       return (XrdSecProtocol *)0;
      }

// Duplicate the parms
//
   if (parms) strlcpy(parmbuff, parms, sizeof(parmbuff));
      else {char *msg = (char *)"Seckrb5: Kerberos parameters not specified.";
            if (erp) erp->setErrInfo(EPROTO, msg);
               else cerr <<msg <<endl;
            return (XrdSecProtocol *)0;
           }

// For clients, the first (and only) token must be the principal name
// For servers: [<keytab>] [-ipchk] <principal>
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
      {char *msg = (char *)"Seckrb5: Kerberos principal not specified.";
       if (erp) erp->setErrInfo(EPROTO, msg);
          else cerr <<msg <<endl;
       return (XrdSecProtocol *)0;
      }

// Set debugging mode
//
   if (getenv("XrdSecDEBUG")) options |= XrdSecDEBUG;

// Get a new protocol object
//
   if (!(prot = new XrdSecProtocolkrb5(options)))
      {char *msg = (char *)"Seckrb5: Insufficient memory for protocol.";
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

void
      __eprintf (const char *string, const char *expression,
                 unsigned int line, const char *filename)
      {
        fprintf (stderr, string, expression, line, filename);
        fflush (stderr);
        abort ();
      }
}
