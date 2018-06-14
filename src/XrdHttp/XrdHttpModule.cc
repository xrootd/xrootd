
#include "XrdVersion.hh"

#include "XrdHttpProtocol.hh"

/******************************************************************************/
/*                       P r o t o c o l   L o a d e r                        */
/*                        X r d g e t P r o t o c o l                         */
/******************************************************************************/

// This protocol can live in a module. The interface below is used by
// the protocol driver to obtain a copy of the protocol object that can be used
// to decide whether or not a link is talking a particular protocol.
//
XrdVERSIONINFO(XrdgetProtocol, xrdhttp);

extern "C" {

  XrdProtocol *XrdgetProtocol(const char *pname, char *parms,
          XrdProtocol_Config *pi) {
    XrdProtocol *pp = 0;
    const char *txt = "completed.";

    // Put up the banner
    //
    pi->eDest->Say("Copr. 2012 CERN IT, an HTTP implementation for the XROOTD framework.");
    pi->eDest->Say("++++++ HTTP protocol initialization started.");

    // Return the protocol object to be used if static init succeeds
    //
    if (XrdHttpProtocol::Configure(parms, pi))
      pp = (XrdProtocol *)new XrdHttpProtocol(false);
    else txt = "failed.";
    pi->eDest->Say("------ HTTP protocol initialization ", txt);
    return pp;
  }
}


/******************************************************************************/
/*                                                                            */
/*           P r o t o c o l   P o r t   D e t e r m i n a t i o n            */
/*                    X r d g e t P r o t o c o l P o r t                     */
/******************************************************************************/

// This function is called early on to determine the port we need to use. The
// default is ostensibly 1094 but can be overidden; which we allow.
//
XrdVERSIONINFO(XrdgetProtocolPort, xrdhttp);

extern "C" {

  int XrdgetProtocolPort(const char *pname, char *parms, XrdProtocol_Config *pi) {

    // Figure out what port number we should return. In practice only one port
    // number is allowed. However, we could potentially have a clustered port
    // and several unclustered ports. So, we let this practicality slide.
    //
    if (pi->Port < 0) return 1094;
    return pi->Port;
  }
}

