#ifndef __XRDMONROLL__
#define __XRDMONROLL__
/******************************************************************************/
/*                                                                            */
/*                         X r d M o n R o l l . h h                          */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string>
#include <variant>
#include <vector>

#include "XrdSys/XrdSysRAtomic.hh"

//-----------------------------------------------------------------------------
//! XrdMonRoll
//!
//! This class is used to register counter sets to be included in the summary
//! statistics that out reported as defined by the xrootd.report directive.
//! A pointer to an instance of this object is placed in the Xrd environment
//! with a key of "XrdMonRoll*".
//-----------------------------------------------------------------------------

class XrdMonitor;
class XrdSysMutex;

class XrdMonRoll
{
public:

//-----------------------------------------------------------------------------
//! The Item class is used to define a set of variables that will be
//! registered with this class. These variables are reported in the summary.
//! The class is a more general implementation of setMember (see below).
//!
//! @param keyN    - Is the name of the key and becomes the XML tag or the
//!                  JSON key for the reported value. It may only be nil
//!                  or the empty string within the context of an array Schema;
//!                  but if specified, it is used as the XML tag for the value
//!                  and is ignored for JSON. If not specified, the XML tag
//!                  becomes "item". Hence, if you care about XML you should
//!                  specify a meaningful key. Note that outside of an array
//!                  a key is always required.
//! @param valU    - the reference to the associated variable holding the
//!                  value or, for Schema, a schema enum type.
//-----------------------------------------------------------------------------

class Item
{
public:
friend class XrdMonitor;

//-----------------------------------------------------------------------------c
//! The following enum may be passed to define a substructure, as follows:
//! enum       JSON          XML
//! begArray  "keyN" : [     <keyN>
//! endArray  ]              </keyN>
//! begObject "keyN" : {     <keyN>
//! endObject }              </varn>
//!
//! @notes
//! 1) A variable name must always begin an array or object when one appears
//!    in object context. However, elements in a JSON array do not have keys
//!    keys so the key is only used when generating XML and is otherwise
//!    ignored.
//! 2) When a key is specified for an endArray or endObject, it must match
//!    the key in the corresponding beg element. If not specicied, the key
//!    from the corresponding beg element is used.
//-----------------------------------------------------------------------------c

enum class Schema : char {unk=0, begArray, endArray, begObject, endObject};

      Item(const char* keyN, double& valU) : keyP(keyN), dblP(&valU),
                       Kind(Family::isBinary), Clan(Trait::isDouble) {}

      Item(const char* keyN, float& valU) : keyP(keyN), fltP(&valU),
                       Kind(Family::isBinary), Clan(Trait::isFloat) {}

      Item(const char* keyN, const char* valU) : keyP(keyN), chrP(valU),
                       Kind(Family::isText), Clan(Trait::isChar) {}

      Item(const char* keyN, std::string& valU) :  keyP(keyN), strP(&valU),
                       Kind(Family::isText), Clan(Trait::isString) {}

      Item(const char* keyN, Schema valU) : keyP(keyN), Plan(valU),
                       Kind(Family::isSchema) {}

      Item(bool Lock, XrdSysMutex& valU) : keyP(0), mtxP(&valU),
                      Kind(Family::isMutex), doLK(Lock) {} 

      Item(const char* keyN, RAtomic_int8_t& valU) : keyP(keyN), rbtV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isBtomic) {}

      Item(const char* keyN, RAtomic_uint8_t& valU) : keyP(keyN), rbtV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isBtomic) {}

      Item(const char* keyN, RAtomic_int16_t& valU) : keyP(keyN), rbtV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isBtomic) {}

      Item(const char* keyN, RAtomic_uint16_t& valU) : keyP(keyN), rbtV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isBtomic) {}

      Item(const char* keyN, RAtomic_int32_t& valU) : keyP(keyN), rbtV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isBtomic) {}

      Item(const char* keyN, RAtomic_uint32_t& valU) : keyP(keyN), rbtV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isBtomic) {}

      Item(const char* keyN, RAtomic_int64_t& valU) : keyP(keyN), rbtV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isBtomic) {}

      Item(const char* keyN, RAtomic_uint64_t& valU) : keyP(keyN), rbtV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isBtomic) {}

      template<typename T>  // This constructor only accepts RAtomic
      Item(const char* keyN, T& valU) : keyP(keyN), ratV(&valU),
                       Kind(Family::isBinary),  Clan(Trait::isAtomic) {}

private:
friend class XrdMonitor;

const char*  keyP;

// This variant is used for native RAtomic values. It omits the bit specific
// types as they duplicate the native types (e.g. uint64_t maps to ullong, etc)
// and the compiler cannot determine which one it should actually use. C++20
// handles this better but we need C++17 to work as well. 
//
using RATVariant = std::variant<RAtomic_ullong*,   RAtomic_llong*,
                                RAtomic_ulong*,    RAtomic_long*,
                                RAtomic_uint*,     RAtomic_int*, 
                                RAtomic_ushort*,   RAtomic_short*,
                                RAtomic_uchar*,    RAtomic_schar*,
                                RAtomic_char*,     RAtomic_bool*>;

// This variant is used for bit specific RAtomic values. It is an exceptional
// variant that we use when handling bit specific RAtomics. Until both variants
// can be combined, some time in the future, we need to special case these.
//
using RBTVariant = std::variant<RAtomic_int8_t*,   RAtomic_uint8_t*,
                                RAtomic_int16_t*,  RAtomic_uint16_t*,
                                RAtomic_int32_t*,  RAtomic_uint32_t*,
                                RAtomic_int64_t*,  RAtomic_uint64_t*>;

// We use an anonymous union where Family::Kind tells us what to use. We
// can't include the variant in the union as it has a constructor/destructor.
//
       RATVariant   ratV;
       RBTVariant   rbtV; // Future: variant will be combined with ratV
union {float*       fltP;
       double*      dblP;
       const char*  chrP;
       std::string* strP;
       XrdSysMutex* mtxP;
       Schema       Plan;
      };

enum class Family : char {isBinary = 0, isMutex, isSchema, isText}; 

enum class Trait  : char {isNone=0, isAtomic, isBtomic, isFloat, isDouble,
                                    isChar,   isString};

short   iNum   = 0;           // Filled in by Monitor
Family  Kind;
Trait   Clan   = Trait::isNone;
bool    Array  = false;       // Reset by Monitor to true if item is in a list
bool    doLK   = false;       // Reset by constructor for mutex item
};

static const int ItemSize = sizeof(Item);

//-----------------------------------------------------------------------------c
//! Misc mapped to AddOn and Protocol is mapped to Plugin since Misc and
//! Protocol are now deprecated. Use AddOn or Plugin!
//-----------------------------------------------------------------------------c

enum rollType {Misc, Protocol, AddOn, Plugin};

//-----------------------------------------------------------------------------c
//! Register the list of counters to be reported.
//!
//! @param setType - Is the type of set being defined:
//!                  AddOn   - counters for miscellaneous activities.
//!                  Plugin  - counters for a plugin.
//! @param setName - Is the name of the set of counter variables. The name
//!                  must not already be registered. The name is reported in
//!                  stats xml tag or JSON stats key value.
//! @param setVec  - Is a vector of class Item objects that define the set of 
//!                  variables for the summary report. The vector must
//!                  reside in allocated storage until execution ends.
//!                  Typically, it is declared static.
//!
//! @return true when the set has been registered and false if the set is
//!         already registered (i.e. setName is in use) or if the setVec
//!         cannot be represented in JSON or CML.
//!
//! @note When false is returned messages will appear in the log desscribing
//!       why registration failed.
//-----------------------------------------------------------------------------c

bool Register(rollType setType, const char* setName, std::vector<Item>& iVec);

//-----------------------------------------------------------------------------c
// The following are deprecated and should not be used in new code. The above
// definition should be used as they are a superset of what is below.
//-----------------------------------------------------------------------------c

//-----------------------------------------------------------------------------c
//! The setMember structure is used to define a set of counters that will be
//! registered with this class. These counters are reported in the summary.
//!
//! @param varName - Is the name of the variable and becomes the xml tag or
//!                  of JSON key for the reported value.
//! @param varValu - Is the reference to the associated counter variable
//!                  holding the value.
//-----------------------------------------------------------------------------

struct setMember {const char* varName; RAtomic_uint& varValu;}; 

//-----------------------------------------------------------------------------
//! Register a set of counters to be reported.
//!
//! @param setType - Is the type of set being defined:
//!                  AddOn   - counters for miscellaneous activities.
//!                  Plugin  - counters for a plugin.
//! @param setName - Is the name of the set of counter variables. The name
//!                  must not already be registered. The name is reported in
//!                  stats xml tag or JSON stats key value.
//! @param setVec  - Is a vector of setMember items that define the set of 
//!                  variables for the summary report. The last element of
//!                  the vector must be initialized to {0, EOV}. The vector
//!                  must reside in allocated storage until execution ends.
//!
//! @return true when the set has been registered and false if the set is
//!         already registered (i.e. setName is in use).
//-----------------------------------------------------------------------------

static RAtomic_uint EOV; // Variable at the end of the setVec.

bool Register(rollType setType, const char* setName, setMember setVec[]);

             XrdMonRoll(XrdMonitor& xMon);
            ~XrdMonRoll() {}

private:
XrdMonitor& XrdMon;
void*       rsvd[3];
};

/******************************************************************************/
/*                              E x a m p l e s                               */
/******************************************************************************/

/* Lets say you have RAtomic_int variable cntr1 and cntr2 and wish to export
   them in the summary stream as: {"keys":{"key1":cntr1, "key2":cntr2}} the
   following Item list would produce the export:
  
   Item iList[] = {Item("keys", Schema::begObject),
                        Item("key1", cntr1), Item("key2", cntr2),
                   Item("keys", Schema::endObject)
                  };

   Alternatively, assume you want to export the variables as
   {"keyvals" : [cntr1, cntr2]} then the following Item list would suffice:

   Item iList[] = {Item("keyvals", Schema::begArray),
                        Item(0, cntr1), Item(0, cntr2),
                   Item("keyvals", Schema::endArray)
                  |;

   The XML version of each would be:
   a) <keys><key1>cntr1</key1><key2>cntr2</key2></keys>
   b) <keyvals><item>cntr1</item><item> cntr2</item></keyvals>
*/
#endif
