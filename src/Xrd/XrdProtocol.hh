#ifndef __XrdProtocol_H__
#define __XrdProtocol_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P r o t o c o l . h h                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include "Xrd/XrdJob.hh"
 
/******************************************************************************/
/*                   x r d _ P r o t o c o l _ C o n f i g                    */
/******************************************************************************/
  
// The following class is passed to the XrdgetProtocol() reutine to properly
// configure the protocol. This object is not stable and the protocol must
// copyu out any values it desires to keep. It may copy the whole object using
// the supplied copy constructor.

class XrdOucError;
class XrdOucThread;
class XrdOucTrace;
class XrdBuffManager;
class XrdInet;
class XrdScheduler;
class XrdStats;

struct sockaddr;

class XrdProtocol_Config
{
public:

// The following pointers may be copied; they are stable.
//
XrdOucError    *eDest;       // Stable -> Error Message/Logging Handler
XrdInet        *NetTCP;      // Stable -> Network Object
XrdBuffManager *BPool;       // Stable -> Buffer Pool Manager
XrdScheduler   *Sched;       // Stable -> System Scheduler
XrdStats       *Stats;       // Stable -> System Statistics
XrdOucThread   *Threads;     // Stable -> The thread manager
XrdOucTrace    *Trace;       // Stable -> Trace Information

// The following information must be duplicated; it is unstable.
//
char            *ConfigFN;     // -> Configuration file
int              Format;       // Binary format of this server
int              Port;         // Port number
const char      *myInst;       // Instance name
const char      *myName;       // Host name
struct sockaddr *myAddr;       // Host address
int              ConnOptn;     // Number of connections to optimize for.
int              ConnLife;     // Life   of connections to optimize for.
int              ConnMax;      // Max       connections
int              readWait;     // Max milliseconds to wait for data
int              idleWait;     // Max milliseconds connection may be idle
int              argc;         // Number of arguments
char           **argv;         // Argument array (prescreened)
char             DebugON;      // True if started with -d option

                 XrdProtocol_Config(XrdProtocol_Config &rhs);
                 XrdProtocol_Config() {}
                ~XrdProtocol_Config() {}
};

/******************************************************************************/
/*                          x r d _ P r o t o c o l                           */
/******************************************************************************/

// This class is used by the Link object to process the input stream on a link.
// At least one protocol object exists per Link object. Specific protocols are 
// derived from this pure abstract class since a link can use one of several 
// protocols. Indeed, startup and shutdown are handled by specialized protocols.

// System configuration obtains an instance of a protocol by calling
// getProtocol(const char *protname), which may exist in a shared library.
// This instance is used as the base pointer for Alloc(), Configure(), and
// Match(). Unfortuantely, they cannot be static given the silly C++ rules.

class XrdLink;
  
class XrdProtocol : public XrdJob
{
public:

// Match()     is invoked when a new link is created and we are trying
//             to determine if this protocol can handle the link. It must
//             return a protocol object if it can and NULL (0), otherwise.
//
virtual XrdProtocol  *Match(XrdLink *lp) = 0;

// Process()   is invoked when a link has data waiting to be read
//
virtual int           Process(XrdLink *lp) = 0;

// Recycle()   is invoked when this object is no longer needed. The method is
//             passed the number of seconds the protocol was connected to the
//             link and the reason for the disconnection, if any.
//
virtual void          Recycle(XrdLink *lp=0,int consec=0,const char *reason=0)=0;

// Stats()     is invoked when we need statistics about all instances of the
//             protocol. If a buffer is supplied, it must return a null 
//             terminated string in the supplied buffer and the return value
//             is the number of bytes placed in the buffer defined by C99 for 
//             snprintf(). If no buffer is supplied, the method should return
//             the maximum number of characters that could have been returned.
//             Regardless of the buffer value, if do_sync is true, the method
//             should include any local statistics in the global data (if any)
//             prior to performing any action.
//
virtual int           Stats(char *buff, int blen, int do_sync=0) = 0;

            XrdProtocol(const char *jname): XrdJob(jname) {}
virtual    ~XrdProtocol() {}
};

/******************************************************************************/
/*                       x r d _ g e t P r o t o c o l                        */
/******************************************************************************/
  
// This extern "C" function is called to obtain an instance of a particular
// protocol. This allows protocols to live outside of XRootd (i.e., to be
// loaded at run-time).
//

extern "C"
{
extern XrdProtocol *XrdgetProtocol(const char *protocol_name, char *parms,
                                   XrdProtocol_Config *pi);
}

/******************************************************************************/
/*                   x r d _ P r o t o c o l _ S e l e c t                    */
/******************************************************************************/
  
// We also include one non-abstract class to allow for the selection of the
// appropriate link protocol. This class is used by XRootd to initiate
// protocol selection for a link.
//

#define XRD_PROTOMAX 8

class XrdProtocol_Select : public XrdProtocol
{
public:

void          DoIt() {}

static int    Load(const char *lname, const char *pname, char *parms,
                   XrdProtocol_Config *pi);

XrdProtocol  *Match(XrdLink *lp) {return 0;}

int           Process(XrdLink *lp);

void          Recycle(XrdLink *lp, int ctime, const char *txt);

int           Stats(char *buff, int blen, int do_sync=0);

              XrdProtocol_Select();
             ~XrdProtocol_Select();

private:

static XrdProtocol *getProtocol(const char *lname, const char *pname,
                                char *parms, XrdProtocol_Config *pi);

static char         *ProtName[XRD_PROTOMAX]; // ->Supported protocols
static XrdProtocol  *Protocol[XRD_PROTOMAX]; // ->Supported protocols
static int           ProtoCnt;               // Number in table (at least 1)
};
#endif
