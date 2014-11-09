#ifndef __OUC_ERRINFO_H__
#define __OUC_ERRINFO_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c E r r I n f o . h h                       */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/*                                                                            */
/******************************************************************************/

#include <string.h>      // For strlcpy()
#include <sys/types.h>

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                              X r d O u c E I                               */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! The XrdOucEI struct encapsulates error information. It uses a fixed buffer
//! for message text and tracing information. It also allows extended
//! information to be recorded in an appendage. It cannot be directly used.
//-----------------------------------------------------------------------------

struct XrdOucEI      // Err information structure
{ 
 static const size_t Max_Error_Len = 2048;
 static const int    Path_Offset   = 1024;

const      char *user;
           int   ucap;
           int   code;
           char  message[Max_Error_Len];

static const int uVMask = 0x0000ffff;  //! ucap: Extract protocol version
static const int uAsync = 0x80000000;  //! ucap: Supports async responses
static const int uUrlOK = 0x40000000;  //! ucap: Supports url   redirects
static const int uMProt = 0x20000000;  //! ucap: Supports multiple protocols
static const int uReadR = 0x10000000;  //! ucap: Supports read redirects
static const int uIPv4  = 0x08000000;  //! ucap: Supports only IPv4 info
static const int uIPv64 = 0x04000000;  //! ucap: Supports IPv6|IPv4 info and
                                       //!       uIPv4 says IPv4 is prefered
static const int uPrip  = 0x02000000;  //! ucap: Client is on a private net

inline     void clear(const char *usr=0, int uc=0)
                     {code=0; ucap = uc; message[0]='\0';
                      user = (usr ? usr : "?");
                     }

           XrdOucEI &operator =(const XrdOucEI &rhs)
               {code = rhs.code;
                user = rhs.user;
                ucap = rhs.ucap;
                strcpy(message, rhs.message); 
                return *this;
               }
           XrdOucEI(const char *usr, int uc=0) {clear(usr, uc);}
};

/******************************************************************************/
/*                         X r d O u c E r r I n f o                          */
/******************************************************************************/

class XrdOucEICB;
class XrdOucEnv;
class XrdSysSemaphore;

//-----------------------------------------------------------------------------
//! The XrdOucErrInfo class is used to communicate data, error, and callback
//! information between plug-ins.
//-----------------------------------------------------------------------------
  
class XrdOucErrInfo
{
public:

//-----------------------------------------------------------------------------
//! Reset data and error information to null. Any appenadges are released.
//-----------------------------------------------------------------------------

       void  clear() {Reset(); ErrInfo.clear();}

//-----------------------------------------------------------------------------
//! Set callback argument.
//!
//! @param  cbarg   - An opaque 8-byte call-back argument.
//-----------------------------------------------------------------------------

inline void  setErrArg(unsigned long long cbarg=0) {ErrCBarg = cbarg;}

//-----------------------------------------------------------------------------
//! Set callback information.
//!
//! @param  cb      - Pointer to the object describing the callback.
//! @param  cbarg   - An opaque 8-byte call-back argument.
//-----------------------------------------------------------------------------

inline void  setErrCB(XrdOucEICB *cb, unsigned long long cbarg=0)
                     {ErrCB = cb; ErrCBarg = cbarg;}

//-----------------------------------------------------------------------------
//! Set error code. Any existing error text remains intact.
//!
//! @param  code    - The error number describing the error.
//!
//! @return code    - The error number.
//-----------------------------------------------------------------------------

inline int   setErrCode(int code) {return ErrInfo.code = code;}

//-----------------------------------------------------------------------------
//! Set error code and error text.
//!
//! @param  code    - The error number describing the error.
//! @param  emsg    - The error message text.
//!
//! @return code    - The error number.
//-----------------------------------------------------------------------------

inline int   setErrInfo(int code, const char *emsg)
                {strlcpy(ErrInfo.message, emsg, sizeof(ErrInfo.message));
                 if (dataBuff) {dataBuff->Recycle(); dataBuff = 0;}
                 return ErrInfo.code = code;
                }

//-----------------------------------------------------------------------------
//! Set error code and error text.
//!
//! @param  code    - The error number describing the error.
//! @param  txtlist - A vector of error message text segments.
//! @param  n       - The number of valid elements in txtlist.
//!
//! @return code    - The error number.
//-----------------------------------------------------------------------------

inline int   setErrInfo(int code, const char *txtlist[], int n)
                {int i, j = 0, k = sizeof(ErrInfo.message), l;
                 for (i = 0; i < n && k > 1; i++)
                     {l = strlcpy(&ErrInfo.message[j], txtlist[i], k);
                      j += l; k -= l;
                     }
                 if (dataBuff) {dataBuff->Recycle(); dataBuff = 0;}
                 return ErrInfo.code = code;
                }

//-----------------------------------------------------------------------------
//! Set error code and error text.
//!
//! @param  code    - The error number describing the error.
//! @param  buffP   - Pointer to the data buffer holding the error text, This
//!                   object takes ownership of the buffer and recycles it.
//!
//! @return code    - The error number.
//-----------------------------------------------------------------------------

inline int   setErrInfo(int code, XrdOucBuffer *buffP)
                {if (dataBuff) dataBuff->Recycle();
                 dataBuff = buffP;
                 return ErrInfo.code = code;
                }

//-----------------------------------------------------------------------------
//! Set user pointer.
//!
//! @param  user    - Pointer to a stable storage area containing the username.
//-----------------------------------------------------------------------------

inline void  setErrUser(const char *user) {ErrInfo.user = (user ? user : "?");}

//-----------------------------------------------------------------------------
//! Obtain the callback argument.
//!
//! @return The argument value currently in effect.
//-----------------------------------------------------------------------------

inline unsigned long long  getErrArg() {return ErrCBarg;}

//-----------------------------------------------------------------------------
//! Get the pointer to the internal message buffer along with its size.
//!
//! @param mblen    - Reference to where the size it to be returned.
//!
//! @return Pointer to the internal message buffer.
//-----------------------------------------------------------------------------

inline char        *getMsgBuff(int &mblen)
                       {mblen = sizeof(ErrInfo.message);
                        return ErrInfo.message;
                       }

//-----------------------------------------------------------------------------
//! Get the callback object.
//!
//! @return Pointer to the callback object.
//-----------------------------------------------------------------------------

inline XrdOucEICB  *getErrCB() {return ErrCB;}

//-----------------------------------------------------------------------------
//! Get the callback object and its argument.
//!
//! @param ap       - Reference to where the callback argument is returned.
//!
//! @return Pointer to the callback object, ap holds the argument.
//-----------------------------------------------------------------------------

inline XrdOucEICB  *getErrCB(unsigned long long &ap)
                            {ap = ErrCBarg; return ErrCB;}

//-----------------------------------------------------------------------------
//! Get the error code.
//!
//! @return The error code that was previously set.
//-----------------------------------------------------------------------------

inline int         getErrInfo() {return ErrInfo.code;}

//-----------------------------------------------------------------------------
//! Get a copy of the error information.
//!
//! @param errParm  - Reference to where error information is to be copied.
//!
//! @return The error code that was previously set.
//-----------------------------------------------------------------------------
/*
inline int          getErrInfo(XrdOucEI &errParm)
                              {errParm = ErrInfo; return ErrInfo.code;}
*/
//-----------------------------------------------------------------------------
//! Get a pointer to the error text.
//!
//! @return The pointer to the internal error text.
//-----------------------------------------------------------------------------

inline const char  *getErrText()
                       {if (dataBuff) return dataBuff->Data();
                        return (const char *)ErrInfo.message;
                       }

//-----------------------------------------------------------------------------
//! Get a pointer to the error text and the error code.
//!
//! @param ecode    - Reference to where the error code is to be returned.
//! @return The pointer to the internal error text.
//-----------------------------------------------------------------------------

inline const char  *getErrText(int &ecode)
                       {ecode = ErrInfo.code;
                        if (dataBuff) return dataBuff->Data();
                        return (const char *)ErrInfo.message;
                       }

//-----------------------------------------------------------------------------
//! Get the error text length (optimized for external buffers).
//!
//! @return The mesage length.
//-----------------------------------------------------------------------------

inline int          getErrTextLen()
                       {if (dataBuff) return dataBuff->DataLen();
                        return strlen(ErrInfo.message);
                       }

//-----------------------------------------------------------------------------
//! Get a pointer to user information.
//!
//! @return The pointer to the user string.
//-----------------------------------------------------------------------------

inline const char  *getErrUser() {return ErrInfo.user;}

//-----------------------------------------------------------------------------
//! Get a pointer to the error environment that was previously set.
//!
//! @return =0      - A callback is in effect which is mutually exclusive of
//!                   conaining an error environment (i.e. no environment).
//! @return !0      - Pointer to the error environment.
//-----------------------------------------------------------------------------

inline XrdOucEnv   *getEnv() {return (ErrCB ? 0 : ErrEnv);}

//-----------------------------------------------------------------------------
//! Set the error environment and return the previous environment. This call
//! destroys any callback information that may have existed.
//!
//! @param  newEnv  - Pointer to the new error environment.
//!
//! @return =0      - No previous envuironment existed.
//! @return !0      - Pointer to the previous error environment.
//-----------------------------------------------------------------------------

inline XrdOucEnv   *setEnv(XrdOucEnv *newEnv)
                          {XrdOucEnv *oldEnv = (ErrCB ? 0 : ErrEnv);
                           ErrEnv = newEnv;
                           ErrCB  = 0;
                           return oldEnv;
                          }

//-----------------------------------------------------------------------------
//! Get the error tracing data.
//!
//! @return =0      - No tracing data has been set.
//! @return !0      - Pointer to error tracing data.
//-----------------------------------------------------------------------------

inline const char  *getErrData() {return (dOff < 0 ? 0 : ErrInfo.message+dOff);}

//-----------------------------------------------------------------------------
//! Set the error tracing data (this is always placed in the internal buffer)
//!
//! @param  Data    - Pointer to the error tracing data.
//! @param  Offs    - Ofset into the message buffer where the data is to be set.
//-----------------------------------------------------------------------------

inline void         setErrData(const char *Data, int Offs=0)
                              {if (!Data) dOff = -1;
                                  else {strlcpy(ErrInfo.message+Offs, Data,
                                        sizeof(ErrInfo.message)-Offs);
                                        dOff = Offs;
                                       }
                              }

//-----------------------------------------------------------------------------
//! Get the monitoring identifier.
//!
//! @return The monitoring identifier.
//-----------------------------------------------------------------------------

inline int          getErrMid() {return mID;}

//-----------------------------------------------------------------------------
//! Set the monitoring identifier.
//!
//! @return The monitoring identifier.
//-----------------------------------------------------------------------------

inline void         setErrMid(int  mid) {mID = mid;}

//-----------------------------------------------------------------------------
//! Check if this object will return extended data (can optimize Reset() calls).
//!
//! @return true    - there is    extended data.
//!         false   - there is no extended data.
//-----------------------------------------------------------------------------

inline bool         extData() {return (dataBuff != 0);}

//-----------------------------------------------------------------------------
//! Reset object to no message state. Call this method to release appendages.
//-----------------------------------------------------------------------------

inline void         Reset()
                         {if (dataBuff) {dataBuff->Recycle(); dataBuff = 0;}
                          *ErrInfo.message = 0;
                           ErrInfo.code    = 0;
                         }

//-----------------------------------------------------------------------------
//! Get user capabilties.
//!
//! @return the user capabilities.
//-----------------------------------------------------------------------------

inline int          getUCap() {return ErrInfo.ucap;}

//-----------------------------------------------------------------------------
//! Set user capabilties.
//-----------------------------------------------------------------------------

inline void         setUCap(int ucval) {ErrInfo.ucap = ucval;}

//-----------------------------------------------------------------------------
//! Assignment operator
//-----------------------------------------------------------------------------

         XrdOucErrInfo &operator =(const XrdOucErrInfo &rhs)
                        {ErrInfo = rhs.ErrInfo;
                         ErrCB   = rhs.ErrCB;
                         ErrCBarg= rhs.ErrCBarg;
                         mID     = rhs.mID;
                         dOff    = -1;
                         if (rhs.dataBuff) dataBuff = rhs.dataBuff->Clone();
                            else dataBuff = 0;
                         return *this;
                        }

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  user    - Pointer to he user string in stable storage.
//! @param  cb      - Pointer to the callback object (zero if none).
//! @param  ca      - The callback argument.
//! @param  mid     - The monitoring identifier.
//! @param  uc      - The user capabilities.
//-----------------------------------------------------------------------------

         XrdOucErrInfo(const char *user=0,XrdOucEICB *cb=0,
                       unsigned long long ca=0, int mid=0, int uc=0)
                    : ErrInfo(user, uc), ErrCB(cb), ErrCBarg(ca), mID(mid),
                      dOff(-1), reserved(0), dataBuff(0) {}

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  user    - Pointer to he user string in stable storage.
//! @param  envp    - Pointer to the error environment.
//! @param  uc      - The user capabilities.
//-----------------------------------------------------------------------------

         XrdOucErrInfo(const char *user, XrdOucEnv *envp, int uc=0)
                    : ErrInfo(user, uc), ErrCB(0), ErrEnv(envp), mID(0),
                      dOff(-1), reserved(0), dataBuff(0) {}

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  user    - Pointer to he user string in stable storage.
//! @param  MonId   - The monitoring identifier.
//! @param  uc      - The user capabilities.
//-----------------------------------------------------------------------------

         XrdOucErrInfo(const char *user, int MonID, int uc=0)
                    : ErrInfo(user, uc), ErrCB(0), ErrCBarg(0), mID(MonID),
                      dOff(-1), reserved(0), dataBuff(0) {}

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

virtual ~XrdOucErrInfo() {Reset();}

protected:

XrdOucEI            ErrInfo;
XrdOucEICB         *ErrCB;
union {
unsigned long long  ErrCBarg;
XrdOucEnv          *ErrEnv;
      };
int                 mID;
short               dOff;
short               reserved;
XrdOucBuffer       *dataBuff;
};

/******************************************************************************/
/*                            X r d O u c E I C B                             */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! The XrdOucEICB is the object that instantiates a callback. This abstract
//! class is used to define the callback interface. It is normally handled by
//! classes that know how to deal with this object in a user friendly way
//! (e.g. XrdOucCallBack).
//-----------------------------------------------------------------------------

class XrdOucEICB
{
public:

//-----------------------------------------------------------------------------
//! Invoke a callback after an operation completes.
//!
//! @param Result - the original function's result (may be changed).
//! @param eInfo  - Associated error information. The eInfo object may not be
//!                 modified until it's own callback Done() method is called, if
//!                 supplied. If the callback function in eInfo is zero, then
//!                 the eInfo object is deleted by the invoked callback.
//!                 Otherwise, that method must be invoked by this callback
//!                 function after the actual callback message is sent. This
//!                 allows the callback requestor to do post-processing and be
//!                 asynchronous being assured that the callback completed.
//!        Path   - Optionally, the path related to thid request. It is used
//!                 for tracing and detailed monitoring purposes.
//-----------------------------------------------------------------------------

virtual void        Done(int           &Result,   //I/O: Function result
                         XrdOucErrInfo *eInfo,    // In: Error Info
                         const char    *Path=0)=0;// In: Relevant path

//-----------------------------------------------------------------------------
//! Determine if two callback arguments refer to the same client.
//!
//! @param  arg1  - The first  callback argument.
//! @param  arg2  - The second callback argument.
//!
//! @return !0    - The arguments refer to the same client.
//! @return =0    - The arguments refer to the different clients.
//-----------------------------------------------------------------------------

virtual int         Same(unsigned long long arg1, unsigned long long arg2)=0;

//-----------------------------------------------------------------------------
//! Constructor and destructor
//-----------------------------------------------------------------------------

                    XrdOucEICB() {}
virtual            ~XrdOucEICB() {}
};
#endif
