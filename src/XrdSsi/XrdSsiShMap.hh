#ifndef __SSI_SHMAP__
#define __SSI_SHMAP__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S h M a p . h h                         */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <string.h>

#include "XrdSsi/XrdSsiShMat.hh"

//-----------------------------------------------------------------------------
//! This include file defines a simple key-value store interface using shared
//! memory. This allows you to share the map with other processes in read as
//! well as read/write mode. See the XrdSsi::ShMap teplated class within.
//-----------------------------------------------------------------------------

namespace XrdSsi
{
//-----------------------------------------------------------------------------
//! The action parameter that must be passed to the Attach() method.
//-----------------------------------------------------------------------------

enum ShMap_Access         //!< Attach existing map for
    {ReadOnly  = 1,       //!< reading
     ReadWrite = 2        //!< reading & writing
    };

//-----------------------------------------------------------------------------
//! Parameters to pass to Create(). For Resize(&parms) initialize the struct
//! as: "ShMap_Parms parms(XrdSsi::ShMap_Parms::ForResize)" so that the default
//! values are appropriate for resizing instead of creation.
//-----------------------------------------------------------------------------

static const int ShMap_4Resize = -1;

struct ShMap_Parms
      {int   indexSize;   //!< Number of hash table entries to create
       int   maxKeyLen;   //!< Maximum key length
       int   maxKeys;     //!< Maximum expected keys
       int   mode;        //!< Mode setting for the newly created file.
       int   options;     //!< Bit or'd ShMop_xxxx options below
       int   reserved;    //!< Reserved for future ABI complaint use

//-----------------------------------------------------------------------------
//! Bit options that may be or'd into he options member above.
//-----------------------------------------------------------------------------

static const
       int   MultW  = 0x88000000; //!< Multiple external writers
static const
       int noMultW  = 0x08000000; //!< Opposite (default for Create)
static const
       int   ReUse  = 0x44000000; //!< Reuse map storage
static const
       int noReUse  = 0x04000000; //!< Opposite (default for Create)

//-----------------------------------------------------------------------------
//! Constructor suitable for Create()
//-----------------------------------------------------------------------------

       ShMap_Parms()        : indexSize(16381), maxKeyLen(63), maxKeys(32768),
                              mode(0640),       options(0),    reserved(0) {}

//-----------------------------------------------------------------------------
//! Constructor suitable for Resize() (use ShMap_Parms(ForResize)).
//-----------------------------------------------------------------------------
static const
       int ForResize = 0;       //!< Triggers initialization for Resize

       ShMap_Parms(int rsz) : indexSize(0),     maxKeyLen(0),  maxKeys(0),
                              mode(0640),       options(0),    reserved(rsz) {}

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

      ~ShMap_Parms()   {}
      };

//-----------------------------------------------------------------------------
//! Options valid for the Sync() method.
//-----------------------------------------------------------------------------

enum   SyncOpt {SyncOff = 0, SyncOn, SyncAll, SyncNow, SyncQSz};

//-----------------------------------------------------------------------------
//! Typedef for the optional hash computation function (see constructor)
//!
//! @param  parms   Pointer to the key whose hash is to be returned. If nil
//!                 the function should return its 4-character name (e.g.
//!                 {int hash; memcpy(&hash, "c32 ", sizeof(int)); return hash;}
//!
//! @return Either the hash value of the key or the hash name as an int.
//-----------------------------------------------------------------------------

typedef int (*ShMap_Hash_t)(const char *key);

template<class T>
class ShMap
{
public:

//-----------------------------------------------------------------------------
//! Attach an existing shared memory map.
//!
//! @param  path    Pointer to the file that is or will represent the map.
//!
//! @param  access  How to attach the map. Specify one of the following:
//!                 ReadOnly  - Attach the map strictly for reading.
//!                 ReadWrite - Attach the map in read/write mode. New and
//!
//! @param  tmo     How many seconds to wait for the map to appear. It is
//!                 possible that a new map may have not yet been exported, so
//!                 attach will wait for the map to become visible. Specify,
//!                 <0 - wait forever.
//!                 =0 - do not wait at all.
//!                 >0 - wait the specified number seconds and then timeout.
//!
//! @return true  - The shared memory was attached, the map can be used.
//! @return false - The shared memory could not be attached, errno holds reason.
//-----------------------------------------------------------------------------

bool         Attach(const char *path, ShMap_Access access, int tmo=-1);

//-----------------------------------------------------------------------------
//! Create a new r/w shared memory map possibly replacing an existing one upon
//! export. New maps must be exported to become visible (see Export()).
//!
//! This method first creates a temporary map visible only to the creator. This
//! allows you to fill the map as needed with minimal overhead. Once this is
//! done, call Export() to make the new map visible, possibly replacing an
//! any existing version of a map with the same name.
//!
//! @param  parms   Reference to the parameters. See the ShMap_Parms struct for
//!                 for details and constructor defaults. Below is a detailed
//!                 explanation of the available options:
//!
//!                 MultW     - The map has multiple processes writing to it.
//!                             All writers must obtain an exclusive file lock
//!                             before updating the map. No file locks are needed
//!                 ReUse     - Allow reuse of storage in the map. Use this if
//!                             the map has many inserts/deletes. If set, r/o
//!                             access  will always lock the map file before
//!                             looking at it. Otherwise, there is no need for
//!                             file locks as no item is ever reused. ReUse is
//!                             good when there are few key add/delete cycles.
//!
//! @return true  - The shared memory was attached, the map can be used.
//! @return false - The shared memory could not be attached, errno holds reason.
//-----------------------------------------------------------------------------

bool         Create(const char *path, ShMap_Parms &parms);

//-----------------------------------------------------------------------------
//! Detach the map from the shared memory.
//-----------------------------------------------------------------------------

void         Detach();

//-----------------------------------------------------------------------------
//! Export a newly created map (i.e. one that was attached using ShMop_New).
//!
//! @return true  - The map has been exported and is now visible to others.
//! @return false - The export failed, the errno value describes the reason.
//-----------------------------------------------------------------------------

bool         Export();

//-----------------------------------------------------------------------------
//! Add an item to the map (see the Rep() method for key/data replacement).
//!
//! @param  key     pointer to the key of length <= MaxKeySize.
//! @param  val     The associated data to be added to the map.
//!
//! @return true  - The key and data added to the map.
//! @return false - The key and data not added, the errno value describes why.
//!                 Typical reason: the key already exists (errno == EEXIST).
//-----------------------------------------------------------------------------

bool         Add(const char *key, T &val);

//-----------------------------------------------------------------------------
//! Delete an item from the map.
//!
//! @param  key     Pointer to the key of length <= MaxKeySize.
//! @param  valP    Pointer to the area to receive the value of the deleted key.
//!                 If the pointer is nil, then the key value is not returned.
//!
//! @return true  - The key and data have been deleted. This is always returned
//!                 when valP is nil.
//! @return false - The key and data either not deleted or the key does not
//!                 exist and valP was not nil. The errno value describes why.
//!                 Typical reason: the key was not found (errno == ENOENT).
//-----------------------------------------------------------------------------

bool         Del(const char *key, T *valP=0);

//-----------------------------------------------------------------------------
//! Enumerate the keys and associated values.
//!
//! @param  jar     An opaque cookie that tracks progress. It should be
//!                 initialized to zero and otherwise not touched. The same jar
//!                 must be used for all successive calls. The jar is deleted
//!                 when false is returned (also see the next Enumerate method).
//! @param  key     The pointer variable where the location of the key is
//!                 returned upon success. The key is overwritten on the next
//!                 call to Enumerate(); so copy it if you want to keep it.
//! @param  val     The pointer variable where the location of the key value
//!                 is to be returned upon success. The value is overwritten on
//!                 the next call to Enumerate(). Copy it if you want to keep it.
//!
//! @return true    A key and val pointers have been set.
//!                 Keys are returned in arbitrary order and not all keys may
//!                 be returned if the map is being actively updated.
//! @return false   Key not returned; errno holds the reason. Typically,
//!                 ENOENT       there ae no more keys.
//!                 Other errors may also be reflected. Whene false is returned
//!                 the jar is deleted and the pointer to it set to zero.
//-----------------------------------------------------------------------------

bool         Enumerate(void *&jar, char *&key, T *&val);

//-----------------------------------------------------------------------------
//! Terminate an active enumeration. An active enumeration is any enumeration
//! where the previous form of Enumerate() did not return false. Terminating
//! an active enumeration releases all of the enumeration resources allocated.
//!
//! @param  jar     The opaque cookie initialized by a previous call to
//!                 Enumerate() whose enumeration is to be terminated.
//!
//! @return true    The enumeration has been terminated and the jar was
//!                 deleted and the jar pointer is set to zero.
//!                 Keys are returned in arbitrary order and not all keys may
//!                 be returned if the map is being actively updated.
//! @return false   The jar pointer was zero; no enumeration was active.
//-----------------------------------------------------------------------------

bool         Enumerate(void *&jar);

//-----------------------------------------------------------------------------
//! Determine whether or not a key exists in the map.
//!
//! @param  key     Pointer to the key of length <= MaxKeySize.
//!
//! @return true  - The key exists.
//! @return false - The key does not exist.
//-----------------------------------------------------------------------------

bool         Exists(const char *key);

//-----------------------------------------------------------------------------
//! Find a key in the map and return its value.
//!
//! @param  key     Pointer to the key of length <= MaxKeySize.
//! @param  val     Reference to the area to receive the value of the found key.
//!
//! @return true  - The key found and its value has been returned.
//! @return false - The key not found, the errno value describes why.
//!                 Typical reason: the key was not found (errno == ENOENT).
//-----------------------------------------------------------------------------

bool         Get(const char *key, T &val);

//-----------------------------------------------------------------------------
//! Return information about the map.
//!
//! @param  vname   Pointer to the variable name whose value is wanted. A
//!                 particular implementation may not support all variable and
//!                 may support variables not listed here. These are for the
//!                 default implementation unless otherwise noted. They are:
//!                 hash        - name of hash being used.
//!                 impl        - The table implementation being used.
//!                 indexsz     - Number of index entries
//!                 indexused   - Number of index entries in use
//!                 keys        - Number of keys in the map. keys/indexused is
//!                               the hash table collision factor
//!                 keysfree    - Number of keys that can still be added
//!                 maxkeylen   - Longest allowed key
//!                 multw       - If 1 map supports multiple writers, else 0
//!                 reuse       - If 1 map allows object reuse, else 0
//!                 type        - Name of the data type in the table.
//!                 typesz      - The number of bytes in the map's data type
//!
//! @param          buff        - Pointer to the buffer to receive text values.
//!                               Variables that return text are: hash, impl,
//!                               and type. A buffer must be supplied in any
//!                               of these variables are requested. If buff is
//!                               nill or too small a -1 is returned with errno
//!                               set to EMSGSIZE.
//!
//! @param          blen          The length of the buffer.
//!
//! @return >=0   - The variable's value.
//! @return < 0   - The variable's value could not be returned; errno has the
//!                 error code describing the reason, typically ENOSYS.
//-----------------------------------------------------------------------------

int          Info(const char *vname, char *buff=0, int blen=0);

//-----------------------------------------------------------------------------
//! Add to or replace an item in the map.
//!
//! @param  key     Pointer to the key of length <= MaxKeySize.
//! @param  val     The associated data to be added to or replaced in the map.
//! @param  valP    Pointer to the area to receive the value of a replaced key.
//!                 If the pointer is nil, then the key value is not returned.
//!
//! @return true  - The key and data added to or replaced in the map. If the
//!                 key was replaced errno is set to EEXIST else it is set to 0.
//! @return false - The key and data not added, the errno value describes why.
//!                 Typical reason: the key was too long (errno == ENAMETOOLONG).
//-----------------------------------------------------------------------------

bool         Rep(const char *key, T &val, T *valP=0);

//-----------------------------------------------------------------------------
//! Resize or change options on an existing map attached in read/write mode.
//! The map must have been exported.
//!
//! @param  parms   Pointer to the parameters. See the ShMap_Parms struct for
//!                 for details and constructor defaults. A zero value in the
//!                 parameter list uses the existing map value allowing you to
//!                 selectively change the map sizing and options. If a nil
//!                 pointer is passed, the map is simply compressed.
//!
//! @return true  - The shared memory was resized.
//! @return false - The shared memory could not be resized, errno holds reason.
//-----------------------------------------------------------------------------

bool         Resize(ShMap_Parms *parms=0);

//-----------------------------------------------------------------------------
//! Specify how the memory map is synchronized with its backing file. If sync
//! is already enabled, calling this method writes back any modified pages
//! before effecting any requested changes.
//!
//! @param  dosync  Controls how synchronization is done (see SyncOpt enum):
//!                 SyncOff - Turn synchronization off (initial setting).
//!                 SyncOn  - Turn synchronization on; pages are written in the
//!                           background (i.e. asynchronously).
//!                 SyncAll - Turn synchronization on; pages are written in the
//!                           foreground(i.e. synchronously).
//!                 SyncNow - Write back any queued pages but otherwise keep
//!                           all other settings the same.
//!                 SyncQSz - Set the queue size specified in the second
//!                           argument. This number of modified pages are
//!                           queued before being written back to disk. No
//!                           other setting in effect are altered.
//! @param  syncqsz Specifies the defer-writeback queue size. This argument
//!                 is ignored unless SyncQSz has been specified (see above).
//!
//! @return true  - Call ended successfully.
//! @return false - Call failed; the errno value describes why.
//-----------------------------------------------------------------------------

bool         Sync(SyncOpt dosync, int syncqsz=256);

//-----------------------------------------------------------------------------
//! Constructor. First allocate a ShMap object of appropriate type. Then call
//! Attach() to attach it to a shared memory segment before calling any other
//! method in this class. When through either delete the object.
//!
//! @param typeName - A text name of the type in the map. Attach() makes sure
//!                   that the map has this type. Specify text < 64 characters.
//!                   Example: XrdSsi::ShMap<int> myMap("int");
//!
//! @param hFunc    - An optional pointer to to the hash computation function
//!                   to be used. If not specified, a crc32 hash is used.
//!
//! @param implName - A text name of the map implementation desired. Zero uses
//!                   the default implementation. Currently only the default
//!                   implementation is available.
//-----------------------------------------------------------------------------

             ShMap(const char *typeName, ShMap_Hash_t hFunc=0,
                   const char *implName=0)
                  : shMat(0), hashFunc(hFunc), typeID(strdup(typeName)),
                    implID((implName ? strdup(implName) : 0)) {}

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

            ~ShMap() {Detach();
                      if (typeID) free(typeID);
                      if (implID) free(implID);
                     }

private:

XrdSsiShMat     *shMat;
ShMap_Hash_t     hashFunc;
char            *typeID;
char            *implID;
};
}

/******************************************************************************/
/*                 A c t u a l   I m p l e m e n t a t i o n                  */
/******************************************************************************/
  
#include "XrdSsi/XrdSsiShMap.icc"
#endif
