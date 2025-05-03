#ifndef __XRDXROOTDREDIRPI__
#define __XRDXROOTDREDIRPI__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d R e d i r P I . h h                    */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdint>
#include <string>
  
class XrdNetAddrInfo;
class XrdOucEnv;
class XrdSysLogger;

class XrdXrootdRedirPI
{
public:

//-----------------------------------------------------------------------------
/*! Return the actual host and port to be used for a redirection.

    @param  Target  -  holds the current redirect target.The target can be a
                       a hostname, IPv4, or bracketed IPv6 address.
    @param  port     - is the numeric port the client will be using to
                       connect to the target. It may be changed to another
                       upon return (see return notes).
    @param  TCgi    -  Optional CGI information as noted in the redirect
                       protocol specification (see note below). It may also 
                       be a null string.
    @param  TNetInfo - Network address information for the target.
    @param  CNetInfo - Network address information for the client being
                       redirected.


    @return string.size() > 0 and string.front() != '!':
               A new redirect target is being returned to use for the
               redirect. The target must also include any CGI information
               this is necessary (e.g. some or all from the TCgi parameter). 
               The port argument also holds the port number that should be
               used. Leave it alone if the incommin port is correct.
            string.size() == 0:
               Use the original redirect target.
            string.size() > 0 and string.front() == '!':
               A fatal error has occured and an error message should be
               sent to the client. The error message text are the characters
               after the exclamation point.

    @note According to the protocol specification targets are of the form
          host[?[fcgi][?lcgi]] and when passed are separated and the Target
          sargument holdse host and TCgi holds the [?[fcgi][?lcgi]] which may
          be a null string. When returning a new target, it must be fully
          specified along with any relevant cgi.
*/

virtual std::string Redirect(const char* Target, uint16_t& port,
                             const char* TCgi,
                             XrdNetAddrInfo& TNetInfo,
                             XrdNetAddrInfo& CNetInfo) = 0;

//-----------------------------------------------------------------------------
/*! Return the actual URL to be used for a redirection.

    @param  urlHead -  holds the prefix to the destination spec. Typically,
                       this is "<protocol>://".
    @param  Target  -  holds the current redirect target and does not contain
                       the port. The target can be a hostname, IPv4, or
                       bracketed IPv6 address.
    @param  port     - is the character port the client will be using to
                       connect to the target. If there is no port, then
                       this is a null string.
    @param  urlTail -  Is the remaining URL (i.e. all the characters after
                       the target specification (i.e. host[:port]) it may
                       be a null string (e.g. "xroot://dest:1094" is specified.)
    @param  rdrOpts    Redirect options as bit flags. These may be changed.
                       Currently, do not touch these options.
    @param  TNetInfo - Network address information for the target.
    @param  CNetInfo - Network address information for the client being
                       redirected.

    @return string.size() > 0 and string.front() != '!':
               A new redirect URL has been returned to use for the redirect.
            string.size() == 0:
               Use the original redirect URL.
            string.size() > 0 and string.front() == '!':
               A fatal error has occured and an error message should be
               sent to the client. The error message text are the characters
               after the exclamation point.

    @note The redirect plugin should not change the original protocol.

    @note The URL type of redirect is esoteric and is used primarily to change
          protocols (e.g. xrooot to file). Hence, a default implementation
          is supplied.
*/

virtual std::string RedirectURL(const char* urlHead,
                                const char* Target,
                                const char* port,
                                const char* urlTail,
                                int&        rdrOpts,
                                XrdNetAddrInfo& TNetInfo,
                                XrdNetAddrInfo& CNetInfo
                               ) {std::string x(""); return x;}       

//-----------------------------------------------------------------------------
//! Constructor and Destructor
//-----------------------------------------------------------------------------

                       XrdXrootdRedirPI() {}
virtual               ~XrdXrootdRedirPI() {}

};

/******************************************************************************/
/*                   P l u g i n   I n s t a n t i a t o r                    */
/******************************************************************************/

//-----------------------------------------------------------------------------
/*! When building a shared library plugin, the following "C" entry point must
    exist in the library:

    @param  prevPI   - pointer tp the redirect plugin that was previously
                       loaded, nil if none. If not nil, you may return
                       this pointer if you wish to cede control to it.
                       Alternatively you can pass control to this plugin
                       as needed.
    @param  Logger   - The message logging object to be used for messages.
    @param  parms    - pointer to optional parameters passed via the redirlib
                       directive, nil if there are no parameters.
    @param  configFn - pointer to the path of the configuration file. If nil
                       there is no configuration file.
    @param  envP     - Pointer to the environment containing implementation
                       specific information.

    @return Pointer to the file system object to be used or nil if an error
            occurred.

    extern "C"
         {XrdXrootdRedirPI *XrdXrootGetdRedirPI(XrdXrootdRedirPI *prevPI,
                                                XrdSysLogger     *Logger,
                                                const char       *parms,
                                                const char       *configFn,
                                                XrdOucEnv        *envP);
         }
*/

#define XrdXrootdRedirPI_Args XrdXrootdRedirPI *prevPI,\
                              XrdSysLogger     *Logger,\
                              const char       *parms, \
                              const char       *configFn,\
                              XrdOucEnv        *envP

typedef XrdXrootdRedirPI *(*XrdXrootdRedirPI_t)(XrdXrootdRedirPI_Args);


//------------------------------------------------------------------------------
/*! Specify the compilation version.

    Additionally, you *should* declare the xrootd version you used to compile
    your plug-in. The plugin manager automatically checks for compatibility.
    Declare it as follows:

    #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdXrootGetdRedirPI,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
//------------------------------------------------------------------------------
#endif
