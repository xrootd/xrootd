//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Authors: Cedric Caffy <ccaffy@cern.ch>, David Smith
// File Date: Aug 2023
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "XProtocol/XPtypes.hh"
#include "XrdHttpReadRangeHandler.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

//------------------------------------------------------------------------------
//! static, class method: initialise a configuraiton object. parms is currently
//! only content of environment variable XRD_READV_LIMITS, to get the specific
//! kXR_readv limits.
//------------------------------------------------------------------------------
int XrdHttpReadRangeHandler::Configure
(
    XrdSysError          &Eroute,
    const char *const     parms,
    Configuration        &cfg)
{
  if( !parms ) return 0;

  std::vector<std::string> splitArgs;
  XrdOucTUtils::splitString( splitArgs, parms, "," );
  if( splitArgs.size() < 2 ) return 0;

  //----------------------------------------------------------------------------
  // params is expected to be "<readv_ior_max>,<readv_iov_max>"
  //----------------------------------------------------------------------------
  std::string iorstr = splitArgs[0];
  std::string iovstr = splitArgs[1];
  XrdOucUtils::trim( iorstr );
  XrdOucUtils::trim( iovstr );

  int val;
  if( XrdOuca2x::a2i( Eroute, "Error reading specific value of readv_ior_max",
                      iorstr.c_str(), &val, 1, -1 ) )
  {
    return -1;
  }

  cfg.readv_ior_max = val;
  if( XrdOuca2x::a2i( Eroute, "Error reading specific value of readv_iov_max",
                      iovstr.c_str(), &val, 1, -1 ) )
  {
    return -1;
  }

  cfg.readv_iov_max = val;
  cfg.reqs_max      = RREQ_MAXSIZE;
  cfg.haveSizes     = true;

  return 0;
}

//------------------------------------------------------------------------------
//! return the Error object
//------------------------------------------------------------------------------
const XrdHttpReadRangeHandler::Error & XrdHttpReadRangeHandler::getError() const
{
  return error_;
}

//------------------------------------------------------------------------------
//! indicates when there were no valid Range head ranges supplied
//------------------------------------------------------------------------------
bool XrdHttpReadRangeHandler::isFullFile()
{
  return rawUserRanges_.empty();
}

//------------------------------------------------------------------------------
//! indicates a single range (implied whole file, or single range) or empty file
//------------------------------------------------------------------------------
bool XrdHttpReadRangeHandler::isSingleRange()
{
  if( !rangesResolved_ )
    resolveRanges();

  return( resolvedUserRanges_.size() <= 1 );
}

//------------------------------------------------------------------------------
//! return resolved (i.e. obsolute start and end) byte ranges desired
//------------------------------------------------------------------------------
const XrdHttpReadRangeHandler::UserRangeList &XrdHttpReadRangeHandler::ListResolvedRanges()
{
  static const UserRangeList emptyList;

  if( !rangesResolved_ )
    resolveRanges();

  if( error_ )
    return emptyList;

  return resolvedUserRanges_;
}

//------------------------------------------------------------------------------
//! return XrdHttpIOList for sending to read or readv
//------------------------------------------------------------------------------
const XrdHttpIOList &XrdHttpReadRangeHandler::NextReadList()
{
  static const XrdHttpIOList emptyList;

  if( !rangesResolved_ )
    resolveRanges();

  if( error_ )
    return emptyList;

  if( !splitRange_.empty() )
  {
    if( currSplitRangeIdx_ == 0 && currSplitRangeOff_ == 0 )
    {
      //------------------------------------------------------------------------
      // Nothing read: Prevent scenario where data is expected but none is
      // actually read E.g. Accessing files which return the results of a script
      //------------------------------------------------------------------------
      error_.set( 500, "Stopping request because more data is expected "
                       "but no data has been read." );
      return emptyList;
    }

    //--------------------------------------------------------------------------
    // we may have some unacknowledged portion of the last range; maybe due to a
    // short read. so remove what was received and potentially reissue.
    //--------------------------------------------------------------------------

    trimSplit();
    if( !splitRange_.empty() )
      return splitRange_;
  }

  if( splitRangeIdx_ >= resolvedUserRanges_.size() )
    return emptyList;

  splitRanges();

  return splitRange_;
}

//------------------------------------------------------------------------------
//! Force handler to enter error state
//------------------------------------------------------------------------------
void XrdHttpReadRangeHandler::NotifyError()
{
  if( error_ )
    return;

  error_.set( 500, "An error occured." );
}

//------------------------------------------------------------------------------
//! Advance internal counters concerning received bytes
//------------------------------------------------------------------------------
int XrdHttpReadRangeHandler::NotifyReadResult
(
    const ssize_t           ret,
    const UserRange** const urp,
    bool                   &start,
    bool                   &allend
)
{
  if( error_ )
    return -1;

  if( ret == 0 )
    return 0;

  if( ret < 0 )
  {
    error_.set( 500, "Range handler read failure." );
    return -1;
  }

  if( !rangesResolved_ )
  {
    error_.set( 500, "Range handler ranges not yet resolved." );
    return -1;
  }

  if( splitRange_.empty() )
  {
    error_.set( 500, "No ranges being read." );
    return -1;
  }

  start  = false;
  allend = false;

  if( currSplitRangeIdx_ >= splitRange_.size() ||
      resolvedRangeIdx_  >= resolvedUserRanges_.size()  )
  {
    error_.set( 500, "Range handler index invalid." );
    return -1;
  }

  if( urp )
    *urp = &resolvedUserRanges_[resolvedRangeIdx_];

  if( resolvedRangeOff_ == 0 )
    start = true;

  const int   clen  = splitRange_[currSplitRangeIdx_].size;

  const off_t ulen  = resolvedUserRanges_[resolvedRangeIdx_].end
                        - resolvedUserRanges_[resolvedRangeIdx_].start + 1;

  currSplitRangeOff_ += ret;
  resolvedRangeOff_  += ret;

  if( currSplitRangeOff_  > clen || resolvedRangeOff_ > ulen )
  {
    error_.set( 500, "Range handler read crossing chunk boundary." );
    return -1;
  }

  if( currSplitRangeOff_ == clen )
  {
    currSplitRangeOff_ = 0;
    currSplitRangeIdx_++;

    if( currSplitRangeIdx_ >= splitRange_.size() )
    {
      currSplitRangeIdx_ = 0;
      splitRange_.clear();
    }
  }

  if( resolvedRangeOff_ == ulen )
  {
    resolvedRangeIdx_++;
    resolvedRangeOff_ = 0;
    if( resolvedRangeIdx_ >= resolvedUserRanges_.size() )
      allend = true;
  }

  return 0;
}

//------------------------------------------------------------------------------
//! parse the line after a "Range: " http request header
//------------------------------------------------------------------------------
void XrdHttpReadRangeHandler::ParseContentRange(const char* const line)
{
  char *str1, *saveptr1, *token;

  std::unique_ptr< char, decltype(std::free)* >
    line_copy { strdup( line ), std::free };

  //----------------------------------------------------------------------------
  // line_copy is argument of the Range header.
  //
  // e.g. "bytes=15-17,20-25"
  // We skip the unit prefix (upto first '='). We don't
  // enforce this prefix nor check what it is (e.g. 'bytes')
  //----------------------------------------------------------------------------

  str1  = line_copy.get();
  token = strchr(str1,'=');
  if (token) str1 = token + 1;

  //----------------------------------------------------------------------------
  // break up the ranges and process each
  //----------------------------------------------------------------------------

  for( ; ; str1 = NULL )
  {
    token = strtok_r( str1, " ,\n\r", &saveptr1 );
    if( token == NULL )
      break;

    if( !strlen(token) ) continue;

    const int rc = parseOneRange( token );
    if( rc )
    {
      //------------------------------------------------------------------------
      // on error we ignore the whole range header
      //------------------------------------------------------------------------
      rawUserRanges_.clear();
      return;
    }
  }
}

//------------------------------------------------------------------------------
//! resets this handler
//------------------------------------------------------------------------------
void XrdHttpReadRangeHandler::reset()
{
  error_.reset();
  rawUserRanges_.clear();
  rawUserRanges_.shrink_to_fit();
  resolvedUserRanges_.clear();
  resolvedUserRanges_.shrink_to_fit();
  splitRange_.clear();
  splitRange_.shrink_to_fit();
  rangesResolved_    = false;
  splitRangeIdx_     = 0;
  splitRangeOff_     = 0;
  currSplitRangeIdx_ = 0;
  currSplitRangeOff_ = 0;
  resolvedRangeIdx_  = 0;
  resolvedRangeOff_  = 0;
  filesize_          = 0;
}

//------------------------------------------------------------------------------
//! sets the filesize, used during resolving and issuing range requests
//------------------------------------------------------------------------------
int XrdHttpReadRangeHandler::SetFilesize(const off_t fs)
{
  if( error_ )
    return -1;

  if( rangesResolved_ )
  {
    error_.set( 500, "Filesize notified after ranges resolved." );
    return -1;
  }

  filesize_ = fs;
  return 0;
}

//------------------------------------------------------------------------------
//! private method: paring a single range from the header
//------------------------------------------------------------------------------
int XrdHttpReadRangeHandler::parseOneRange(char* const str)
{
  UserRange ur;
  char *sep;

  //----------------------------------------------------------------------------
  // expected input is an individual range, e.g.
  // 5-6
  // 5-
  // -2
  //----------------------------------------------------------------------------

  sep = strchr( str, '-' );
  if( !sep )
  {
    //--------------------------------------------------------------------------
    // Unexpected range format
    //--------------------------------------------------------------------------
    return -1;
  }

  *sep = '\0';
  if( rangeFig( str, ur.start_set, ur.start )<0 )
  {
    //--------------------------------------------------------------------------
    // Error in range start
    //--------------------------------------------------------------------------
    *sep = '-';
    return -1;
  }
  *sep = '-';
  if( rangeFig( sep+1, ur.end_set, ur.end )<0 )
  {
    //--------------------------------------------------------------------------
    // Error in range end
    //--------------------------------------------------------------------------
    return -1;
  }

  if( !ur.start_set && !ur.end_set )
  {
    //--------------------------------------------------------------------------
    // Unexpected range format
    //--------------------------------------------------------------------------
    return -1;
  }

  if( ur.start_set && ur.end_set && ur.start > ur.end )
  {
    //--------------------------------------------------------------------------
    // Range start is after range end
    //--------------------------------------------------------------------------
    return -1;
  }

  if( !ur.start_set && ur.end_set && ur.end == 0 )
  {
    //--------------------------------------------------------------------------
    // Request to return last 0 bytes of file
    //--------------------------------------------------------------------------
    return -1;
  }

  rawUserRanges_.push_back(ur);
  return 0;
}

//------------------------------------------------------------------------------
//! private method: decode a decimal value from range header
//------------------------------------------------------------------------------
int XrdHttpReadRangeHandler::rangeFig(const char* const s, bool &set, off_t &val)
{
  char *endptr = (char*)s;
  errno = 0;
  long long int v = strtoll( s, &endptr, 10 );
  if( (errno == ERANGE && (v == LONG_MAX || v == LONG_MIN))
        || (errno != 0 && errno != EINVAL && v == 0) )
  {
    return -1;
  }
  if( *endptr != '\0' )
  {
    return -1;
  }
  if( endptr == s )
  {
    set = false;
  }
  else
  { 
    set = true;
    val = v;
  }
  return 0;
}

//------------------------------------------------------------------------------
//! private method: turn user supplied range into absolute range using filesize
//------------------------------------------------------------------------------
void XrdHttpReadRangeHandler::resolveRanges()
{
  if( error_ )
    return;

  resolvedUserRanges_.clear();

  for( const auto &rr: rawUserRanges_ )
  {
    off_t start = 0;
    off_t end   = 0;

    if( rr.end_set )
    {
      if( rr.start_set )
      {
        //----------------------------------------------------------------------
        // end and start set
        // e.g. 5-6
        //----------------------------------------------------------------------
        start = rr.start;
        end   = rr.end;

        //----------------------------------------------------------------------
        // skip ranges outside the file
        //----------------------------------------------------------------------
        if( start >= filesize_ )
          continue;

        if( end >= filesize_ )
        {
          end = filesize_ - 1;
        }
      }
      else // !start
      {
        //----------------------------------------------------------------------
        // end is set but not start
        // e.g. -5
        //----------------------------------------------------------------------
        if( rr.end == 0 )
          continue;
        end = filesize_ -1;
        if( rr.end > filesize_ )
        {
          start = 0;
        }
        else
        {
          start = filesize_ - rr.end;
        }
      }
    }
    else // !end
    {
      //------------------------------------------------------------------------
      // end is not set
      // e.g. 5-
      //------------------------------------------------------------------------
      if( !rr.start_set ) continue;
      if( rr.start >= filesize_ )
        continue;
      start = rr.start;
      end   = filesize_ - 1;
    }
    resolvedUserRanges_.emplace_back( start, end );
  }

  if( rawUserRanges_.empty() && filesize_>0 )
  {
    //--------------------------------------------------------------------------
    // special case: no ranges: speficied, return whole file
    //--------------------------------------------------------------------------
    resolvedUserRanges_.emplace_back( 0, filesize_ - 1 );
  }

  if( !rawUserRanges_.empty() && resolvedUserRanges_.empty() )
  {
    error_.set( 416, "None of the range-specifier values in the Range "
      "request-header field overlap the current extent of the selected resource." );
  }

  rangesResolved_ = true;
}

//------------------------------------------------------------------------------
//! private method: proceed through the resolved ranges, splitting into ranges
//! suitable for read or readv. This method is called repeatedly until we've
//! gone though all the resolved ranges.
//------------------------------------------------------------------------------
void XrdHttpReadRangeHandler::splitRanges()
{
  splitRange_.clear();
  currSplitRangeIdx_ = 0;
  currSplitRangeOff_ = 0;
  resolvedRangeIdx_  = splitRangeIdx_;
  resolvedRangeOff_  = splitRangeOff_;

  //----------------------------------------------------------------------------
  // If we make a list of just one range XrdHttpReq will issue kXR_read,
  // otherwise kXR_readv.
  //
  // If this is a full file read, or single user range, we'll fetch only one
  // range at a time, so it is sent as a series of kXR_read requests.
  //
  // For multi range requests we pack a number of suitably sized ranges, thereby
  // using kXR_readv. However, if there's a long user range we can we try to
  // proceed by issuing single range requests and thereby using kXR_read.
  //
  // We don't merge user ranges in a single chunk as we always expect to be
  // able to notify at boundaries with the output bools of NotifyReadResult.
  //----------------------------------------------------------------------------

  size_t maxch  = vectorReadMaxChunks_;
  size_t maxchs = vectorReadMaxChunkSize_;
  if( isSingleRange() )
  {
    maxchs =  rRequestMaxBytes_;
    maxch  =  1;
  }

  splitRange_.reserve( maxch );

  //----------------------------------------------------------------------------
  // Start/continue splitting the resolvedUserRanges_ into a XrdHttpIOList.
  //----------------------------------------------------------------------------

  const size_t cs  = resolvedUserRanges_.size();
  size_t       nc  = 0;
  size_t       rsr = rRequestMaxBytes_;
  UserRange    tmpur;

  while( ( splitRangeIdx_ < cs ) && ( rsr > 0 ) )
  {
    //--------------------------------------------------------------------------
    // Check if we've readed the maximum number of allowed chunks.
    //--------------------------------------------------------------------------
    if( nc >= maxch )
      break;

    if( !tmpur.start_set )
    {
        tmpur         = resolvedUserRanges_[splitRangeIdx_];
        tmpur.start  += splitRangeOff_;
    }

    const off_t l = tmpur.end - tmpur.start + 1;
    size_t maxsize = std::min( rsr, maxchs );

    //--------------------------------------------------------------------------
    // If we're starting a new set of chunks and we have enough data available
    // in the current user range we allow a kXR_read of the max request size.
    //--------------------------------------------------------------------------
    if( nc == 0 && l >= (off_t)rRequestMaxBytes_ )
      maxsize = rRequestMaxBytes_;

    if( l > (off_t)maxsize )
    {
      splitRange_.emplace_back( nullptr, tmpur.start, maxsize );
      tmpur.start    += maxsize;
      splitRangeOff_ += maxsize;
      rsr            -= maxsize;
    }
    else
    {
      splitRange_.emplace_back(	nullptr, tmpur.start, l );
      rsr            -= l;
      tmpur           = UserRange();
      splitRangeOff_  = 0;
      splitRangeIdx_++;
    }
    nc++;
  }
}

//------------------------------------------------------------------------------
//! private method: remove partially received request
//------------------------------------------------------------------------------
void XrdHttpReadRangeHandler::trimSplit()
{
  if( currSplitRangeIdx_ < splitRange_.size() )
  {
    splitRange_.erase( splitRange_.begin(),
                       splitRange_.begin() + currSplitRangeIdx_ );
  }
  else
    splitRange_.clear();

  if( splitRange_.size() > 0 )
  {
    if( currSplitRangeOff_ < splitRange_[0].size )
    {
      splitRange_[0].offset += currSplitRangeOff_;
      splitRange_[0].size   -= currSplitRangeOff_;
    }
    else
      splitRange_.clear();
  }

  currSplitRangeIdx_ = 0;
  currSplitRangeOff_ = 0;
}
