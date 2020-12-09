/************************************************************************
 * KineticIo - a file io interface library to kinetic devices.          *
 *                                                                      *
 * This Source Code Form is subject to the terms of the Mozilla         *
 * Public License, v. 2.0. If a copy of the MPL was not                 *
 * distributed with this file, You can obtain one at                    *
 * https://mozilla.org/MP:/2.0/.                                        *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without       *
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or         *
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public             *
 * License for more details.                                            *
 ************************************************************************/

#include "XrdEc/XrdEcRedundancyProvider.hh"

#include "isa-l/isa-l.h"
#include <string.h>
#include <sstream>
#include <algorithm>

namespace XrdEc
{

//--------------------------------------------------------------------------
//! Anything-to-string conversion, the only reason to put this in its own
//! class is to keep the stringstream parsing methods out of the public
//! namespace.
//--------------------------------------------------------------------------
class Convert{
  public:
    //--------------------------------------------------------------------------
    //! Convert an arbitrary number of arguments of variable type to a single
    //! std::string.
    //!
    //! @param args the input arguments to convert to string.
    //--------------------------------------------------------------------------
    template<typename...Args>
    static std::string toString(Args&&...args){
      std::stringstream s;
      argsToStream(s, std::forward<Args>(args)...);
      return s.str();
    }
  private:
    //--------------------------------------------------------------------------
    //! Closure of recursive parsing function
    //! @param stream string stream to store Last parameter
    //! @param last the last argument to log
    //--------------------------------------------------------------------------
    template<typename Last>
    static void argsToStream(std::stringstream& stream, Last&& last) {
      stream << last;
    }

    //--------------------------------------------------------------------------
    //! Recursive function to parse arbitrary number of variable type arguments
    //! @param stream string stream to store input parameters
    //! @param first the first of the arguments supplied to the log function
    //! @param rest the rest of the arguments should be stored in the log message
    //--------------------------------------------------------------------------
    template<typename First, typename...Rest >
    static void argsToStream(std::stringstream& stream, First&& first, Rest&&...rest) {
      stream << first;
      argsToStream(stream, std::forward<Rest>(rest)...);
    }
};



/* This function is (almost) completely ripped from the erasure_code_test.cc file
   distributed with the isa-l library. */
static int gf_gen_decode_matrix(
    unsigned char* encode_matrix, // in: encode matrix
    unsigned char* decode_matrix, // in: buffer, out: generated decode matrix
    unsigned int* decode_index,  // out: order of healthy blocks used for decoding [data#1, data#3, ..., parity#1... ]
    unsigned char* src_err_list,  // in: array of #nerrs size [index error #1, index error #2, ... ]
    unsigned char* src_in_err,    // in: array of #data size > [1,0,0,0,1,0...] -> 0 == no error, 1 == error
    unsigned int nerrs,                    // #total errors
    unsigned int nsrcerrs,                 // #data errors
    unsigned int k,                        // #data
    unsigned int m                         // #data+parity
)
{
  unsigned i, j, p;
  unsigned int r;
  unsigned char* invert_matrix, * backup, * b, s;
  int incr = 0;

  std::vector<unsigned char> memory((size_t) (m * k * 3));
  b = &memory[0];
  backup = &memory[m * k];
  invert_matrix = &memory[2 * m * k];

  // Construct matrix b by removing error rows
  for (i = 0, r = 0; i < k; i++, r++) {
    while (src_in_err[r]) {
      r++;
    }
    for (j = 0; j < k; j++) {
      b[k * i + j] = encode_matrix[k * r + j];
      backup[k * i + j] = encode_matrix[k * r + j];
    }
    decode_index[i] = r;
  }
  incr = 0;
  while (gf_invert_matrix(b, invert_matrix, k) < 0) {
    if (nerrs == (m - k)) {
      return -1;
    }
    incr++;
    memcpy(b, backup, (size_t) (m * k));
    for (i = nsrcerrs; i < nerrs - nsrcerrs; i++) {
      if (src_err_list[i] == (decode_index[k - 1] + incr)) {
        // skip the erased parity line
        incr++;
        continue;
      }
    }
    if (decode_index[k - 1] + incr >= m) {
      return -1;
    }
    decode_index[k - 1] += incr;
    for (j = 0; j < k; j++) {
      b[k * (k - 1) + j] = encode_matrix[k * decode_index[k - 1] + j];
    }

  };

  for (i = 0; i < nsrcerrs; i++) {
    for (j = 0; j < k; j++) {
      decode_matrix[k * i + j] = invert_matrix[k * src_err_list[i] + j];
    }
  }
  /* src_err_list from encode_matrix * invert of b for parity decoding */
  for (p = nsrcerrs; p < nerrs; p++) {
    for (i = 0; i < k; i++) {
      s = 0;
      for (j = 0; j < k; j++) {
        s ^= gf_mul(invert_matrix[j * k + i],
                    encode_matrix[k * src_err_list[p] + j]);
      }

      decode_matrix[k * p + i] = s;
    }
  }
  return 0;
}

RedundancyProvider::RedundancyProvider( const ObjCfg &objcfg ) :
    objcfg( objcfg ),
    encode_matrix( objcfg.nbchunks * objcfg.nbdata )
{
  // k = data
  // m = data + parity
  gf_gen_cauchy1_matrix( encode_matrix.data(), static_cast<int>( objcfg.nbchunks ), static_cast<int>( objcfg.nbdata ) );
}


std::string RedundancyProvider::getErrorPattern( stripes_t &stripes ) const
{
  std::string pattern( objcfg.nbchunks, 0 );
  for( uint8_t i = 0; i < objcfg.nbchunks; ++i )
    if( !stripes[i].valid ) pattern[i] = '\1';

  return pattern;
}


RedundancyProvider::CodingTable& RedundancyProvider::getCodingTable( const std::string& pattern )
{
  std::lock_guard<std::mutex> lock(mutex);

  /* If decode matrix is not already cached we have to construct it. */
  if( !cache.count(pattern) )
  {
    /* Expand pattern */
    int nerrs = 0, nsrcerrs = 0;
    unsigned char err_indx_list[objcfg.nbparity];
    for (std::uint8_t i = 0; i < pattern.size(); i++) {
      if (pattern[i]) {
        err_indx_list[nerrs++] = i;
        if (i < objcfg.nbdata) { nsrcerrs++; }
      }
    }

    /* Allocate Decode Object. */
    CodingTable dd;
    dd.nErrors = nerrs;
    dd.blockIndices.resize( objcfg.nbdata );
    dd.table.resize( objcfg.nbdata * objcfg.nbparity * 32);

    /* Compute decode matrix. */
    std::vector<unsigned char> decode_matrix(objcfg.nbchunks * objcfg.nbdata);

    if (gf_gen_decode_matrix( encode_matrix.data(), decode_matrix.data(), dd.blockIndices.data(),
                              err_indx_list, (unsigned char*) pattern.c_str(), nerrs, nsrcerrs,
                              static_cast<int>( objcfg.nbdata ), static_cast<int>( objcfg.nbchunks ) ) )
      throw IOError( XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errDataError, errno, "Failed computing decode matrix" ) );

    /* Compute Tables. */
    ec_init_tables( static_cast<int>( objcfg.nbdata ), nerrs, decode_matrix.data(), dd.table.data() );
    cache.insert( std::make_pair(pattern, dd) );
  }
  return cache.at(pattern);
}

void RedundancyProvider::replication( stripes_t &stripes )
{
  // get index of a valid block
  void *healthy = nullptr;
  for( auto itr = stripes.begin(); itr != stripes.end(); ++itr )
  {
    if( itr->valid )
      healthy = itr->buffer;
  }

  if( !healthy ) throw IOError( XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errDataError ) );

  // now replicate, by now 'buffers' should contain all chunks
  for( uint8_t i = 0; i < objcfg.nbchunks; ++i )
  {
    if( !stripes[i].valid )
      memcpy( stripes[i].buffer, healthy, objcfg.chunksize );
  }
}

void RedundancyProvider::compute( stripes_t &stripes )
{
  /* throws if stripe is not recoverable */
  std::string pattern = getErrorPattern( stripes );

  /* nothing to do if there are no parity blocks. */
  if ( !objcfg.nbparity ) return;

  /* in case of a single data block use replication */
  if ( objcfg.nbdata == 1 )
    return replication( stripes );

  /* normal operation: erasure coding */
  CodingTable& dd = getCodingTable(pattern);

  unsigned char* inbuf[objcfg.nbdata];
  for( uint8_t i = 0; i < objcfg.nbdata; i++ )
    inbuf[i] = reinterpret_cast<unsigned char*>( stripes[dd.blockIndices[i]].buffer );

  std::vector<unsigned char> memory( dd.nErrors * objcfg.chunksize );

  unsigned char* outbuf[dd.nErrors];
  for (int i = 0; i < dd.nErrors; i++)
  {
    outbuf[i] = &memory[i * objcfg.chunksize];
  }

  ec_encode_data(
      static_cast<int>( objcfg.chunksize ), // Length of each block of data (vector) of source or destination data.
      static_cast<int>( objcfg.nbdata ),     // The number of vector sources in the generator matrix for coding.
      dd.nErrors,     // The number of output vectors to concurrently encode/decode.
      dd.table.data(), // Pointer to array of input tables
      inbuf,          // Array of pointers to source input buffers
      outbuf          // Array of pointers to coded output buffers
  );

  int e = 0;
  for (size_t i = 0; i < objcfg.nbchunks; i++)
  {
    if( pattern[i] )
    {
      memcpy( stripes[i].buffer, outbuf[e], objcfg.chunksize );
      e++;
    }
  }
}


};
