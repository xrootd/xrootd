#ifndef __SSI_SHMAT__
#define __SSI_SHMAT__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S h M a t . h h                         */
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

#include <cstdlib>
#include <cstring>

//-----------------------------------------------------------------------------
//! This class defines an abstract interface to a generic shared memory table
//! that stores key-value pairs. Since this class a pure abstract any number
//! of implementations may be supplied. The default one is named "XrdSsiShMam".
//-----------------------------------------------------------------------------

class XrdSsiShMat
{
public:

//-----------------------------------------------------------------------------
//! Add an item to the shared memory table.
//!
//! @param  newdata Pointer to the data to be added.
//! @param  olddata Pointer to the area where the replaced data, if any, is
//!                 to be placed.
//! @param  key     The key associated with the data that is to be added.
//! @param  hash    The hash of the key that is to be used to lookup the key.
//!                 If the value is zero, an internal hash is computed.
//! @param  replace When true, if the key exists, the data associated with the
//!                 key is replaced. When false, if the key exists, the addition
//!                 fails with errno set to EEXIST.
//!
//! @return true    The addition/replacement succeeded. If the key was actually
//!                 replaced errno is set to EEXIST else it is set to 0.
//! @return false   The addition/replacement failed; errno indicates reason.
//-----------------------------------------------------------------------------

virtual bool AddItem(void *newdata, void *olddata, const char *key,
                     int   hash=0,  bool  replace=false) = 0;

//-----------------------------------------------------------------------------
//! Attach this object to the shared memory associated with this object at
//! creation time (see New() method). The attach operation waits until the
//! shared memory file is available. At that time, the file is memory mapped.
//!
//! @param  tout    The maximum number of seconds to wait for the shared
//!                 memory file to become available. If tout is zero, then
//!                 the file must be immediately available. If the value is
//!                 negative then the attach waits as long as needed. When tout
//!                 is reached the attach fails with errno set to ETIMEDOUT.
//! @param  isrw    When true the file is mapped to writable memory and allows
//!                 updates to the table. If false, the shared memory is made
//!                 read/only and may be significantly faster to access.
//!
//! @return true  - The shared memory was attached, the table can be used.
//! @return false - The shared memory could not be attached, errno holds reason.
//-----------------------------------------------------------------------------

virtual bool Attach(int tout, bool isrw=false) = 0;

//-----------------------------------------------------------------------------
//! Create a new shared memory segment and associated file specified at object
//! instantiation (see New() method). Created segments must be made visible to
//! other processes using the Export() method. This allows the table to be
//! preloaded with initial values before the table is made visible.
//!
//! @param  parms   Create parameters described by CRParms. All uninitialized
//!                 members in this struct must be specified.
//!
//! @return true  - The shared memory was attached, the table can be used.
//! @return false - The shared memory could not be attached, errno holds reason.
//-----------------------------------------------------------------------------

struct CRZParms
      {int  indexSz; //!< Number of four byte hash table entries to create.
       int  maxKeys; //!< Maximum number of keys-value pairs expected in table.
       int  maxKLen; //!< The maximum acceptable key length.
       int  mode;    //!< Filemode for the newly created file.
       signed char multW;
                     //!<  1: Table can have multiple processes writing.
                     //!<  0: Table has only one process writing.
                     //!< -1: Use default or, for resize, previous setting.
       signed char reUse;
                     //!<  1: Reuse deleted objects.
                     //!<  0: Never reuse deleted objects.
                     //!< -1: Use default or, for resize, previous setting.
       char rsvd[6]; //!< Reserved for future options

            CRZParms() : indexSz(0), maxKeys(0), maxKLen(0), mode(0640),
                         multW(-1), reUse(-1)
                        {memset(rsvd, -1, sizeof(rsvd));}
           ~CRZParms() {}
      };

virtual bool Create(CRZParms &parms) = 0;

//-----------------------------------------------------------------------------
//! Export a newly created table (i.e. see Create()).
//!
//! @return true  - The table has been exported and is now visible to others.
//! @return false - The export failed, the errno value describes the reason.
//-----------------------------------------------------------------------------

virtual bool Export() = 0;

//-----------------------------------------------------------------------------
//! Delete an item from the table.
//!
//! @param  data    Pointer to the area to receive the value of the deleted key.
//!                 If the pointer is nil, then the key value is not returned.
//! @param  key     Pointer to the key of length <= MaxKLen.
//! @param  hash    The hash of the key that is to be used to lookup the key.
//!                 If the value is zero, an internal hash is computed.
//!
//! @return true  - The key and data have been deleted. This is always returned
//!                 when data is nil.
//! @return false - The key and data either not deleted or the key does not
//!                 exist and data was not nil. The errno value decribes why.
//!                 Typical reason: the key was not found (errno == ENOENT).
//-----------------------------------------------------------------------------

virtual bool DelItem(void *data, const char *key, int hash=0) = 0;

//-----------------------------------------------------------------------------
//! Detach the map from the shared memory.
//-----------------------------------------------------------------------------

virtual void Detach() = 0;

//-----------------------------------------------------------------------------
//! Enumerate the keys and assocaited values.
//!
//! @param  jar     An opaque cookie that tracks progress. It should be
//!                 initialized to zero and otherwise not touched. The same jar
//!                 must be used for all successive calls. The jar is deleted
//!                 when false is returned (also see the next Enumerate method).
//! @param  key     The pointer variable where the location of the key is
//!                 returned upon success.
//! @param  val     The pointer variable where the location f the key values
//!                 is to be returned upon success.
//!
//! @return true    A key and val pointers have been set.
//!                 Keys are returned in arbitrary order and not all keys may
//!                 be returned if the map is being actively updated.
//! @return false   Key not returned; errno holds the reason. Typically,
//!                 ENOENT       there ae no more keys.
//!                 Other errors may also be reflected. Whne false is returned
//!                 the jar is deleted and the pointer to it set to zero.
//-----------------------------------------------------------------------------

virtual bool Enumerate(void *&jar, char *&key, void *&val) = 0;

//-----------------------------------------------------------------------------
//! Terminate an active enumeration. An active enumeration is any enumeration
//! where the previous form of Enumerate() did not return false. Terminating
//! an active enumeration releases all of the enumeration resources allocated.
//!
//! @param  jar     The opaque cookie initialized by a previous call to
//!                 Enumerate() requesting the next key-value pair.
//!
//! @return true    The enumeration has been terminated and the jar was
//!                 deleted and the jar pointer is set to zero.
//!                 Keys are returned in arbitrary order and not all keys may
//!                 be returned if the map is being actively updated.
//! @return false   The jar pointer was zero; no enumeration was active.
//-----------------------------------------------------------------------------

virtual bool Enumerate(void *&jar) = 0;

//-----------------------------------------------------------------------------
//! Return information about the table.
//!
//! @param  vname   Pointer to the variable name whose value is wanted. A
//!                 particular implementation may not support all variable and
//!                 may support variables not listed here. These are for the
//!                 default implementation unless otherwise noted. They are:
//!                 hash        - name of hash being used.
//!                 impl        - The table implementation being used.
//!                 indexsz     - Number of index entries
//!                 indexused   - Number of index entries in use
//!                 keys        - Number of keys in the bale. keys/indexused is
//!                               the hash table collision factor
//!                 keysfree    - Number of keys that can still be added
//!                 maxkeylen   - Longest allowed key
//!                 multw       - If table supports multiple writers, else 0
//!                 reuse       - If table allows object reuse, else 0
//!                 type        - Name of the data type in the table.
//!                 typesz      - The number of bytes in the table's data type
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
//! @return >=0   - The variable's value or the length of the text information.
//! @return < 0   - The variable's value could not be returned; errno has the
//!                 error code describing the reason, typically ENOTSUP.
//-----------------------------------------------------------------------------

virtual int  Info(const char *vname, char *buff=0, int blen=0) = 0;

//-----------------------------------------------------------------------------
//! Get an item from the table.
//!
//! @param  data    Pointer to an area to receive the value associated with key.
//!                 If the pointer is nil, then the key value is not returned.
//! @param  key     Pointer to the key of length <= MaxKLen.
//! @param  hash    The hash of the key that is to be used to lookup the key.
//!                 If the value is zero, an internal hash is computed.
//!
//! @return true  - The key was found and if data was not nil, contains the
//!                 value associated key.
//! @return false - The key not found; errno holds the reason (typically is
//!                 ENOENT but may be some other reason).
//-----------------------------------------------------------------------------

virtual bool GetItem(void *data, const char *key, int hash=0) = 0;

//-----------------------------------------------------------------------------
//! Instantiate a shared memory object.
//!
//! @param  parms   The parameters to use when creating the table. Fields are:
//!                 impl    Pointer to the name of the implementation that is
//!                         desired. The default implementation (XrdSsiShMam)
//!                         is used if nil. All processes must specify the same
//!                         implementation that was used to create the table via
//!                         the Create() method. If specified it must not exceed
//!                         63 characters.
//!                 path    Pointer to the file that is backing the table. The
//!                         path is used to locate the table in memory.
//!                 typeID  A text name of the data type in the table. All
//!                         processes must specify the same typeID that the
//!                         table was created with using Create(). Specify text
//!                         less than 64 characters.
//!                 typesz  The number of bytes occupied by the data type in
//!                         the table.
//!                 hashID  A 4-characters text name of the hash used in the
//!                         table represented as an int. All processes must
//!                         specify the same hashID that the table was created
//!                         with using Create().
//!
//! @return !0    - Pointer to an instance of an XrdSsiShMat object.
//! @return false - The object could not instantiate because of an error;
//!                 errno holds the error code explaining why.
//-----------------------------------------------------------------------------

struct NewParms
      {const char *impl;   //!< Implementation name
       const char *path;   //!< The path to the backing file for the table
       const char *typeID; //!< The name of the type associated with the key
       int         typeSz; //!< Size of the type in bytes
       int         hashID; //!< The hash being used (0 means the default)
      };

static
XrdSsiShMat *New(NewParms &parms);

//-----------------------------------------------------------------------------
//! Resize a shared memory segment and associated file specified at object
//! instantiation (see New() method). Resizing is implementation specific but
//! may involve creating a new table and exporting it.
//!
//! @param  parms   Resize parameters. See the CRZParms struct for details. For
//!                 resize, zero values or unspecified flags use the existing
//!                 table values.
//!
//! @return true  - The shared memory was resized, the table can be used.
//! @return false - The shared memory could not be resized, errno holds reason.
//-----------------------------------------------------------------------------

virtual bool Resize(CRZParms &parms) = 0;

//-----------------------------------------------------------------------------
//! Synchronize all modified pages to the associated backing file.
//!
//! @return true  - Operation completed successfully.
//! @return false - Operation failed; errno holds the error code explaining why.
//-----------------------------------------------------------------------------

virtual bool Sync() = 0;

//-----------------------------------------------------------------------------
//! Turn memry synchronization on or off.
//!
//! @param  dosync  When true, modified table pages are written back to the
//!                 backing file. The synchronous or async nature of the
//!                 write is controlled by the second parameter. When false,
//!                 memory-file synchronization is turned off (initial setting).
//! @param  syncdo  When true, synchronization is done in the forground. That
//!                 is, a call triggering a sync will not return until complete.
//!                 When false, synchronization is done in the background.
//!
//! @return true  - Operation completed successfully.
//! @return false - Operation failed; errno holds the error code explaining why.
//-----------------------------------------------------------------------------

virtual bool Sync(bool dosync, bool syncdo=false) = 0;

//-----------------------------------------------------------------------------
//! Set the sync defer queue size.
//!
//! @param  synqsz  The maximum number of modified pages before flushing.
//!
//! @return true  - Operation completed successfully.
//! @return false - Operation failed; errno holds the error code explaining why.
//-----------------------------------------------------------------------------

virtual bool Sync(int synqsz) = 0;

//-----------------------------------------------------------------------------
//! Constructor (arguments the same as for New())
//-----------------------------------------------------------------------------

     XrdSsiShMat(NewParms &parms)
                : shmImpl(strdup(parms.impl)),   shmPath(strdup(parms.path)),
                  shmType(strdup(parms.typeID)), shmTypeSz(parms.typeSz),
                  shmHash(parms.hashID)
                {}

//-----------------------------------------------------------------------------
//! Destructor. Warning, your destructor should call your own Detach()!
//-----------------------------------------------------------------------------

virtual ~XrdSsiShMat() {if (shmImpl) free(shmImpl);
                        if (shmPath) free(shmPath);
                        if (shmType) free(shmType);
                       }

protected:

char *shmImpl;
char *shmPath;
char *shmType;
int   shmTypeSz;
int   shmHash;
};
#endif
