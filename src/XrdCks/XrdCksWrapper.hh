#ifndef __XRDCKSWRAPPER_HH__
#define __XRDCKSWRAPPER_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d C k s W r a p p e r . h h                       */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include "XrdCks/XrdCks.hh"

class XrdCksCalc;
class XrdCksData;
class XrdOucEnv;
class XrdOucStream;
class XrdSysError;

/*! This class defines the wrapper for the checksum management interface.
    It should be used as the base class for a stacked plugin. When used that
    way, the shared library holding the plugin must define a "C" entry point
    named XrdCksAdd2() as described at the end of this include file. Note
    you pass a reference to the previous plugin-in in the plug-in chain as a
    constructor argument as supplied to the XrdCksAdd2() function.
    Override the methods you wish to wrap. Methods that are not overridden
    are forwarded to the previous plug-in.
*/
  
class XrdCksWrapper : public XrdCks
{
public:

//------------------------------------------------------------------------------
//! Calculate a new checksum for a physical file using the checksum algorithm
//! named in the Cks parameter.
//!
//! @param  Xfn       The logical or physical name of the file to be checksumed.
//! @param  Cks       For  input, it specifies the checksum algorithm to be used.
//!                   For output, the checksum value is returned upon success.
//! @param  doSet     When true, the new value must replace any existing value
//!                   in the Xfn's extended file attributes.
//! @param  pcbP      In the second form, the pointer to the callback object.
//!                   A nil pointer does not invoke any callback.
//!
//! @return Success:  zero with Cks structure holding the checksum value.
//!         Failure: -errno (see significant error numbers below).
//------------------------------------------------------------------------------
virtual
int        Calc( const char *Xfn, XrdCksData &Cks, int doSet=1)
               {return cksPI.Calc(Xfn, Cks, doSet);}

virtual
int        Calc( const char *Xfn, XrdCksData &Cks, XrdCksPCB *pcbP, int doSet=1)
               {(void)pcbP; return Calc(Xfn, Cks, doSet);}

//------------------------------------------------------------------------------
//! Delete the checksum from the Xfn's xattrs.
//!
//! @param  Xfn      The logical or physical name of the file to be checksumed.
//! @param  Cks      Specifies the checksum type to delete.
//!
//! @return Success: 0
//!         Failure: -errno (see significant error numbers below).
//------------------------------------------------------------------------------
virtual
int        Del(  const char *Xfn, XrdCksData &Cks)
              {return cksPI.Del(Xfn, Cks);}

//------------------------------------------------------------------------------
//! Retreive the checksum from the Xfn's xattrs and return it and indicate
//! whether or not it is stale (i.e. the file modification has changed or the
//! name and length are not the expected values).
//!
//! @param  Xfn      The logical or physical name of the file to be checksumed.
//! @param  Cks      For  input, it specifies the checksum type to return.
//!                  For output, the checksum value is returned upon success.
//!
//! @return Success: The length of the binary checksum in the Cks structure.
//!         Failure: -errno (see significant error numbers below).
//------------------------------------------------------------------------------
virtual
int        Get(  const char *Xfn, XrdCksData &Cks)
              {return cksPI.Get(Xfn, Cks);}

//------------------------------------------------------------------------------
//! Parse a configuration directives specific to the checksum manager.
//!
//! @param  Token    Points to the directive that triggered the call.
//! @param  Line     All the characters after the directive.
//!
//! @return Success:  1
//!         Failure:  0
//------------------------------------------------------------------------------
virtual
int        Config(const char *Token, char *Line)
                 {return cksPI.Config(Token, Line);}

//------------------------------------------------------------------------------
//! Fully initialize the manager which includes loading any plugins.
//!
//! @param  ConfigFN Points to the configuration file path.
//! @param  DfltCalc Is the default checksum and should be defaulted if NULL.
//!                  The default implementation defaults this to adler32. A
//!                  default is only needed should the checksum name in the
//!                  XrdCksData object be omitted.
//!
//!@return Success:  1
//!        Failure:  0
//------------------------------------------------------------------------------
virtual
int        Init(const char *ConfigFN, const char *DfltCalc=0)
               {return cksPI.Init(ConfigFN, DfltCalc);}

//------------------------------------------------------------------------------
//! List names of the checksums associated with a Xfn or all supported ones.
//!
//! @param  Xfn      The logical or physical file name whose checksum names are
//!                  to be returned. When Xfn is null, return all supported
//!                  checksum algorithm names.
//! @param  Buff     Points to a buffer, at least 64 bytes in length, to hold
//!                  a "Sep" separated list of checksum names.
//! @param  Blen     The length of the buffer.
//! @param  Sep      The separation character to be used between adjacent names.
//!
//! @return Success: Pointer to Buff holding at least one checksum name.
//!         Failure: A nil pointer is returned.
//------------------------------------------------------------------------------
virtual
char      *List(const char *Xfn, char *Buff, int Blen, char Sep=' ')
               {return cksPI.List(Xfn, Buff, Blen, Sep);}

//------------------------------------------------------------------------------
//! Get the name of the checksums associated with a sequence number. Note that
//! Name() may be called prior to final config to see if there are any chksums
//! to configure and avoid unintended errors.
//!
//! @param  seqNum   The sequence number. Zero signifies the default name.
//!                  Higher numbers are alternates.
//! @return Success: Pointer to the name.
//!         Failure: A nil pointer is returned (no more alternates exist).
//------------------------------------------------------------------------------
virtual const
char      *Name(int seqNum=0) {return cksPI.Name(seqNum);}

//------------------------------------------------------------------------------
//! Get a new XrdCksCalc object that can calculate the checksum corresponding to
//! the specified name or the default object if name is a null pointer. The
//! object can be used to compute checksums on the fly. The object's Recycle()
//! method must be used to delete it.
//!
//! @param  name     The name of the checksum algorithm. If null, use the
//!                  default one.
//!
//! @return Success: A pointer to the object is returned.
//!         Failure: Zero if no corresponding object exists.
//------------------------------------------------------------------------------
virtual
XrdCksCalc *Object(const char *name) {return cksPI.Object(name);}

//------------------------------------------------------------------------------
//! Get the binary length of the checksum with the corresponding name.
//!
//! @param  Name     The checksum algorithm name. If null, use the default name.
//!
//! @return Success: checksum length.
//!         Failure: Zero  if the checksum name does not exist.
//------------------------------------------------------------------------------
virtual
int        Size( const char  *Name=0) {return cksPI.Size(Name);}

//------------------------------------------------------------------------------
//! Set a file's checksum in the extended attributes along with the file's mtime
//! and the time of setting.
//!
//! @param  Xfn      The logical or physical name of the file to be set.
//! @param  Cks      Specifies the checksum name and value.
//! @param  myTime   When true then the fmTime and gmTime in the Cks structure
//!                  are to be used; as opposed to the current time.
//!
//! @return Success:  zero is returned.
//!         Failure: -errno (see significant error numbers below).
//------------------------------------------------------------------------------
virtual
int        Set(  const char *Xfn, XrdCksData &Cks, int myTime=0)
              {return cksPI.Set(Xfn, Cks, myTime);}

//------------------------------------------------------------------------------
//! Retreive the checksum from the Xfn's xattrs and compare it to the supplied
//! checksum. If the checksum is not available or is stale, a new checksum is
//! calculated and written to the extended attributes.
//!
//! @param  Xfn      The logical or physical name of the file to be verified.
//! @param  Cks      Specifies the checksum name and value.
//! @param  pcbP      In the second form, the pointer to the callback object.
//!                   A nil pointer does not invoke any callback.
//!
//! @return Success: True
//!         Failure: False (the checksums do not match) or -errno indicating
//!                  that verification could not be performed (see significant
//!                  error numbers below).
//------------------------------------------------------------------------------
virtual
int        Ver(  const char *Xfn, XrdCksData &Cks)
              {return cksPI.Ver(Xfn, Cks);}

virtual
int        Ver(  const char *Xfn, XrdCksData &Cks, XrdCksPCB *pcbP)
              {(void)pcbP; return Ver(Xfn, Cks);}

//------------------------------------------------------------------------------
//! Constructor
//!
//! @param  prevPI   Reference to the antecedent plugin.
//! @param  errP     Pointer to error message object
//------------------------------------------------------------------------------

           XrdCksWrapper(XrdCks &prevPI, XrdSysError *errP)
                        : XrdCks(errP), cksPI(prevPI) {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual   ~XrdCksWrapper() {}

protected:

XrdCks &cksPI;
};

/******************************************************************************/
/*                            X r d C k s A d d 2                             */
/******************************************************************************/

#define XRDCKSADD2PARMS XrdCks &, XrdSysError *, const char *, \
                        const char *, XrdOucEnv *
  
//------------------------------------------------------------------------------
//! Obtain an instance of a stacked checksum manager.
//!
//! XrdCksAdd2() is an extern "C" function that is called to obtain an
//! instance of a stacked checksum manager object that will be used for all
//! subsequent checksums. This function is passed a refernce to the previous
//! checksum manager. Overridden methods should, if appropriate, invoke the
//! previous plug-ins corresponding method to complete the required task.
//! All the following extern symbols must be defined at file level!
//!
//! @param pPI  -> Reference to the previous checksum manager plug-in.
//! @param eDest-> The XrdSysError object for messages.
//! @param cFN  -> The name of the configuration file
//! @param Parm -> Parameters specified on the ckslib directive. If none it is
//!                zero.
//! @param pEnv -> Pointer to environmental information or nil.
//!
//! @return Success: A pointer to the checksum manager object.
//!         Failure: Null pointer which causes initialization to fail.
//------------------------------------------------------------------------------

/*! extern "C" XrdCks *XrdCksAdd2(XrdCks      &pPI,
                                  XrdSysError *eDest,
                                  const char  *cFN,
                                  const char  *Parm,
                                  XrdOucEnv   *envP
                                  );
*/
//------------------------------------------------------------------------------
//! Declare the compilation version number.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdCksAdd2,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
