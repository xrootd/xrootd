/*
 * XrdEcUtilities.hh
 *
 *  Created on: Jan 9, 2019
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECUTILITIES_HH_
#define SRC_XRDEC_XRDECUTILITIES_HH_

#include "XrdEc/XrdEcObjCfg.hh"

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClFileSystem.hh"

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClCheckSumManager.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCks/XrdCksCalc.hh"

#include <exception>
#include <memory>
#include <random>

namespace XrdEc
{
  struct stripe_t
  {
      stripe_t( char *buffer, bool valid ) : buffer( buffer ), valid( valid )
      {
      }

      char *buffer;
      bool  valid;
  };

  typedef std::vector<stripe_t> stripes_t;

  enum LocationStatus
  {
    rw,
    ro,
    drain,
    off,
  };

  LocationStatus ToLocationStatus( const std::string &str );

  typedef std::tuple<std::string,  LocationStatus> location_t;
  typedef std::vector<location_t>  placement_group;
  typedef std::vector<std::string> placement_t;

  std::string CalcChecksum( const char *buffer, uint64_t size );


  //----------------------------------------------------------------------------
  //! a buffer type
  //----------------------------------------------------------------------------
  typedef std::vector<char>  buffer_t;

  //----------------------------------------------------------------------------
  //! Generic I/O exception, wraps up XrdCl::XRootDStatus (@see XRootDStatus)
  //----------------------------------------------------------------------------
  class IOError : public std::exception
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param st : status
      //------------------------------------------------------------------------
      IOError( const XrdCl::XRootDStatus &st ) noexcept : st( st ), msg( st.ToString() )
      {
      }

      //------------------------------------------------------------------------
      //! Copy constructor
      //------------------------------------------------------------------------
      IOError( const IOError &err ) noexcept : st( err.st ), msg( err.st.ToString() )
      {
      }

      //------------------------------------------------------------------------
      //! Assigment operator
      //------------------------------------------------------------------------
      IOError& operator=( const IOError &err ) noexcept
      {
        st = err.st;
        msg = err.st.ToString();
        return *this;
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~IOError()
      {

      }

      //------------------------------------------------------------------------
      //! overloaded @see std::exception
      //------------------------------------------------------------------------
      virtual const char* what() const noexcept
      {
        return msg.c_str();
      }

      //------------------------------------------------------------------------
      //! @return : the status
      //------------------------------------------------------------------------
      const XrdCl::XRootDStatus& Status() const
      {
        return st;
      }

      enum
      {
        ioTooManyErrors
      };

    private:

      //------------------------------------------------------------------------
      //! The status object
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus st;

      //------------------------------------------------------------------------
      //! The error message
      //------------------------------------------------------------------------
      std::string msg;
  };

  //----------------------------------------------------------------------------
  //! Creates a sufix for a data chunk file
  //!
  //! @param blkid   : block ID
  //! @param chunkid : chunk ID
  //!
  //! @return        : sufix for a data chunk file
  //----------------------------------------------------------------------------
  inline std::string Sufix( uint64_t blkid, uint64_t chunkid )
  {
    return '.' + std::to_string( blkid ) + '.' + std::to_string( chunkid );
  }

  //------------------------------------------------------------------------
  //! Find a new location (host) for given chunk. TODO (update)
  //!
  //! @param chunkid   : ID of the chunk to be relocated
  //! @param relocate  : true if the chunk should be relocated even if
  //                     a placement for it already exists, false otherwise
  //------------------------------------------------------------------------
  XrdCl::OpenFlags::Flags Place( const ObjCfg                &objcfg,
                                 uint8_t                      chunkid,
                                 placement_t                 &placement,
                                 std::default_random_engine  &generator,
                                 const placement_group       &plgr,
                                 bool                         relocate );

  placement_t GeneratePlacement( const ObjCfg          &objcfg,
                                 const std::string     &objname,
                                 const placement_group &plgr,
                                 bool                   write     );

  placement_t GetSpares( const placement_group &plgr,
                         const placement_t     &placement,
                         bool                   write );

}

#endif /* SRC_XRDEC_XRDECUTILITIES_HH_ */
