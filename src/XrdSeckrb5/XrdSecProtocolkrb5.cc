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

#define CLDBG(x) if (options & XrdSecDEBUG) cerr <<"Seckrb5: " <<x <<endl;

typedef  krb5_error_code krb_rc;

/******************************************************************************/
/*              X r d S e c P r o t o c o l k r b 5   C l a s s               */
/******************************************************************************/

class XrdSecProtocolkrb5 : public XrdSecProtocol
{
public:

        int                Authenticate  (XrdSecCredentials *cred,
                                          XrdSecParameters **parms,
                                          XrdOucErrInfo     *einfo=0);

        XrdSecCredentials *getCredentials(XrdSecParameters  *parm=0,
                                          XrdOucErrInfo     *einfo=0);

static  char              *getPrincipal() {return Principal;}

static  int                Init(XrdOucErrInfo *einfo, char *KP=0, char *kfn=0);

static  void               setOpts(int opts) {options = opts;}

        XrdSecProtocolkrb5(const char                *KP,
                           const char                *hname,
                           const struct sockaddr     *ipadd)
                          {Service = (KP ? strdup(KP) : 0);
                           Entity.host = strdup(hname);
                           memcpy(&hostaddr, ipadd, sizeof(hostaddr));
                           CName[0] = '?'; CName[1] = '\0';
                           Entity.name = CName;
                          }

        void              Delete();

private:

       ~XrdSecProtocolkrb5() {} // Delete() does it all

static int Fatal(XrdOucErrInfo *erp,int rc,const char *msg1,char *KP=0,int krc=0);
static int get_krbCreds(char *KP, krb5_creds **krb_creds);

static XrdOucMutex        krbContext;    // Client or server
static int                options;       // Client or server
static krb5_context       krb_context;   // Client or server
static krb5_ccache        krb_ccache;    // Client or server
static krb5_keytab        krb_keytab;    // Server
static krb5_principal     krb_principal; // Server

static char              *Principal;     // Server's principal name

struct sockaddr           hostaddr;      // Client-side only
char                      CName[256];    // Kerberos limit
char                     *Service;       // Target principal for client
};

/******************************************************************************/
/*                           S t a t i c   D a t a                            */
/******************************************************************************/
  
XrdOucMutex         XrdSecProtocolkrb5::krbContext;        // Client or server

int                 XrdSecProtocolkrb5::options = 0;       // Client or Server
krb5_context        XrdSecProtocolkrb5::krb_context;       // Client or server
krb5_ccache         XrdSecProtocolkrb5::krb_ccache;        // Client or server
krb5_keytab         XrdSecProtocolkrb5::krb_keytab = NULL; // Server
krb5_principal      XrdSecProtocolkrb5::krb_principal;     // Server

char               *XrdSecProtocolkrb5::Principal = 0;     // Server

/******************************************************************************/
/*                                D e l e t e                                 */
/******************************************************************************/
  
void XrdSecProtocolkrb5::Delete()
{
     if (Entity.host) free(Entity.host);
     if (Service)     free(Service);
     delete this;
}

/******************************************************************************/
/*                        g e t C r e d e n t i a l s                         */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolkrb5::getCredentials(XrdSecParameters *noparm,
                                                      XrdOucErrInfo    *error)
{
   char *buff;
   int bsz;
   krb_rc rc;
   krb5_creds       *krb_creds    = 0;
   krb5_auth_context auth_context = NULL;
   krb5_data         outbuf;

// Supply null credentials if so needed for this protocol
//
   if (!Service)
      {CLDBG("Null credentials supplied.");
       return new XrdSecCredentials(0,0);
      }

// Clear outgoing ticket and lock the kerberos context
//
   outbuf.length = 0; outbuf.data = 0;
   krbContext.Lock();

// Get a service ticket for this principal
//
   if ((rc = (krb_rc)get_krbCreds(Service, &krb_creds)))
      {krbContext.UnLock();
       Fatal(error, ESRCH, "Unable to get credentials", Service, rc);
       return (XrdSecCredentials *)0;
      }

// Generate a kerberos-style authentication message
//
   rc = krb5_mk_req_extended(krb_context, &auth_context,
             AP_OPTS_USE_SESSION_KEY,(krb5_data *)0,krb_creds,&outbuf);

// Check if all succeeded. If so, copy the ticket into the buffer. We wish
// we could place the ticket directly into the buffer but architectural
// differences won't allow us that optimization.
//
   if (!rc)
      {bsz = XrdSecPROTOIDLEN+outbuf.length;
       if (!(buff = (char *)malloc(bsz)))
          {krbContext.UnLock();
           Fatal(error, ENOMEM, "Insufficient memory for credentials.", Service);
           return (XrdSecCredentials *)0;
          }
       strcpy(buff, XrdSecPROTOIDENT);
       memcpy((void *)(buff+XrdSecPROTOIDLEN),
              (const void *)outbuf.data, (size_t)outbuf.length);
       CLDBG("Returned " <<bsz <<" bytes of creds; p=" <<Service);
       if (outbuf.data)  free(outbuf.data);
       if (auth_context) krb5_auth_con_free(krb_context, auth_context);
       if (krb_creds)    krb5_free_creds(krb_context, krb_creds);
       krbContext.UnLock();
       return new XrdSecCredentials(buff, bsz);
      }

// Diagnose the failure
//
   if (outbuf.data)  free(outbuf.data);
   if (auth_context) krb5_auth_con_free(krb_context, auth_context);
   if (krb_creds)    krb5_free_creds(krb_context, krb_creds);
   krbContext.UnLock();
   Fatal(error, EACCES, "Unable to get credentials", Service, rc);
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
      {strncpy(Entity.prot, "host", sizeof(Entity.prot));
       return 0;
      }

// Check if this is a recognized protocol
//
   if (strcmp(cred->buffer, XrdSecPROTOIDENT))
      {char emsg[256];
       snprintf(emsg, sizeof(emsg),
                "Authentication protocol id mismatch (%.4s != %.4s).",
                XrdSecPROTOIDENT,  cred->buffer);
       Fatal(error, EINVAL, emsg, Principal);
       return -1;
      }

// Indicate who we are
//
   strncpy(Entity.prot, XrdSecPROTOIDENT, sizeof(Entity.prot));

// Create a kerberos style ticket and obtain the kerberos mutex
//
   inbuf.length = cred->size -XrdSecPROTOIDLEN;
   inbuf.data   = &cred->buffer[XrdSecPROTOIDLEN];
   krbContext.Lock();

// Check if whether the IP address in the credentials must match that of
// the incomming host.
//
   if (!(XrdSecProtocolkrb5::options & XrdSecNOIPCHK))
      {struct sockaddr_in *ip = (struct sockaddr_in *)&hostaddr;
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
                                    sizeof(CName)-1, CName)))
                  iferror = (char *)"Unable to extract client name;";

// Release any allocated storage at this point and unlock mutex
//
   CName[sizeof(CName)-1] = '\0';
   if (ticket)       krb5_free_ticket(krb_context, ticket);
   if (auth_context) krb5_auth_con_free(krb_context, auth_context);
   krbContext.UnLock();

// Diagnose any errors
//
   if (rc && iferror)
      return Fatal(error, EACCES, iferror, Principal, rc);

// All done
//
   return 0;
}

/******************************************************************************/
/*                I n i t i a l i z a t i o n   M e t h o d s                 */
/******************************************************************************/
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

int XrdSecProtocolkrb5::Init(XrdOucErrInfo *erp, char *KP, char *kfn)
{
   krb_rc rc;
   char buff[1024];

// Create a kerberos context. There is one such context per protocol object.
//
   if ((rc = krb5_init_context(&krb_context)))
      return Fatal(erp, ENOPROTOOPT, "Kerberos initialization failed", KP, rc);

// Obtain the default cache location
//
   if ((rc = krb5_cc_default(krb_context, &krb_ccache)))
      return Fatal(erp, ENOPROTOOPT, "Unable to locate cred cache", KP, rc);

// If we have no principal then this is a client-side call
//
   if (!KP) return 0;

// Try to resolve the keyfile name
//
   if (kfn && *kfn)
      {if ((rc = krb5_kt_resolve(krb_context, kfn, &krb_keytab)))
          {snprintf(buff, sizeof(buff), "Unable to find keytab '%s';", kfn);
           return Fatal(erp, ESRCH, buff, Principal, rc);
          }
      } else {
       krb5_kt_default(krb_context, &krb_keytab);
      }

// Now, extract the "principal/instance@realm" from the stream
//
   if ((rc = krb5_parse_name(krb_context,(const char *)KP,&krb_principal)))
     return Fatal(erp, EINVAL, "Cannot parse service name", KP, rc);

// Establish the correct principal to use
//
   if ((rc = krb5_unparse_name(krb_context,(krb5_const_principal)krb_principal,
                              (char **)&Principal)))
      return Fatal(erp, EINVAL, "Unable to unparse principal;", KP, rc);

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

int XrdSecProtocolkrb5::Fatal(XrdOucErrInfo *erp, int rc, const char *msg,
                             char *KP, int krc)
{
   char *msgv[8];
   int k, i = 0;

              msgv[i++] = (char *)"Seckrb5: ";  //0
              msgv[i++] = (char *)msg;          //1
   if (krc)  {msgv[i++] = (char *)"; ";         //2
              msgv[i++] =  krb_etxt(krc);        //3
             }
   if (KP)   {msgv[i++] = (char *)" (p=";       //4
              msgv[i++] = KP;                   //5
              msgv[i++] = (char *)").";         //6
             }
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

int XrdSecProtocolkrb5::get_krbCreds(char *KP, krb5_creds **krb_creds)
{
    krb_rc rc;
    krb5_principal the_principal;
    krb5_creds mycreds;

// Clear my credentials
//
   memset((char *)&mycreds, 0, sizeof(mycreds));

// Setup the "principal/instance@realm"
//
   if ((rc = krb5_parse_name(krb_context,(const char *)KP,&the_principal)))
      {CLDBG("get_krbCreds: Cannot parse service name;" <<krb_etxt(rc));
       return rc;
      }

// Copy the current target principal into the credentials
//
   if ((rc = krb5_copy_principal(krb_context, the_principal, &mycreds.server)))
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
   rc = krb5_get_credentials(krb_context, 0, krb_ccache, &mycreds,  krb_creds);
   krb5_free_cred_contents(krb_context, &mycreds);

// Check if all went well
//
   if (rc) {CLDBG("get_krbCreds: unable to get creds; " <<krb_etxt(rc));}
   return rc;
}
 
/******************************************************************************/
/*                X r d S e c p r o t o c o l k r b 5 I n i t                 */
/******************************************************************************/
  
extern "C"
{
char  *XrdSecProtocolkrb5Init(const char     mode,
                              const char    *parms,
                              XrdOucErrInfo *erp)
{
   char *op, *KPrincipal=0, *Keytab=0;
   char parmbuff[1024];
   XrdOucTokenizer inParms(parmbuff);
   int options = XrdSecNOIPCHK;

// For client-side one-time initialization, we only need to set debug flag and
// initialize the kerberos context and cache location.
//
   if (mode == 'c')
      {if (getenv("XrdSecDEBUG")) XrdSecProtocolkrb5::setOpts(XrdSecDEBUG);
       return (XrdSecProtocolkrb5::Init(erp) ? (char *)0 : (char *)"");
      }

// Duplicate the parms
//
   if (parms) strlcpy(parmbuff, parms, sizeof(parmbuff));
      else {char *msg = (char *)"Seckrb5: Kerberos parameters not specified.";
            if (erp) erp->setErrInfo(EINVAL, msg);
               else cerr <<msg <<endl;
            return (char *)0;
           }

// Expected parameters: [<keytab>] [-ipchk] <principal>
//
   if (inParms.GetLine())
      {if ((op = inParms.GetToken()) && *op == '/')
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
       if (erp) erp->setErrInfo(EINVAL, msg);
          else cerr <<msg <<endl;
       return (char *)0;
      }

// Now initialize the server
//
   XrdSecProtocolkrb5::setOpts(options);
   return (XrdSecProtocolkrb5::Init(erp, KPrincipal, Keytab)
           ? (char *)0 : XrdSecProtocolkrb5::getPrincipal());
}
}

/******************************************************************************/
/*              X r d S e c P r o t o c o l k r b 5 O b j e c t               */
/******************************************************************************/
  
extern "C"
{
XrdSecProtocol *XrdSecProtocolkrb5Object(const char              mode,
                                         const char             *hostname,
                                         const struct sockaddr  &netaddr,
                                         const char             *parms,
                                               XrdOucErrInfo    *erp)
{
   XrdSecProtocolkrb5 *prot;
   char *KPrincipal=0;

// If this is a client call, then we need to get the target principal from the
// parms (which must be the first and only token). For servers, we use the
// context we established at initialization time.
//
   if (mode == 'c')
      {if ((KPrincipal = (char *)parms)) while(*KPrincipal == ' ') KPrincipal++;
       if (!KPrincipal || !*KPrincipal)
          {char *msg = (char *)"Seckrb5: Kerberos principal not specified.";
           if (erp) erp->setErrInfo(EINVAL, msg);
              else cerr <<msg <<endl;
           return (XrdSecProtocol *)0;
          }
      }

// Get a new protocol object
//
   if (!(prot = new XrdSecProtocolkrb5(KPrincipal, hostname, &netaddr)))
      {char *msg = (char *)"Seckrb5: Insufficient memory for protocol.";
       if (erp) erp->setErrInfo(ENOMEM, msg);
          else cerr <<msg <<endl;
       return (XrdSecProtocol *)0;
      }

// All done
//
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
