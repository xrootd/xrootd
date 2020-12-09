/*
 * XrdEcObjCfg.hh
 *
 *  Created on: Nov 25, 2019
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECOBJCFG_HH_
#define SRC_XRDEC_XRDECOBJCFG_HH_

#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace XrdEc
{
  struct ObjCfg
  {
      ObjCfg() = delete;

      ObjCfg( const std::string &obj, const std::string &mtindex, uint8_t nbdata, uint8_t nbparity, uint64_t chunksize ) :
        obj( obj ),
        mtindex( mtindex ),
        nbchunks( nbdata + nbparity ),
        nbparity( nbparity ),
        nbdata( nbdata ),
        datasize( nbdata * chunksize ),
        chunksize( chunksize ),
        paritysize( nbparity * chunksize ),
        blksize( datasize + paritysize )
      {

      }

      ObjCfg( const ObjCfg &objcfg ) : obj( objcfg.obj ),
                                       mtindex( objcfg.mtindex ),
                                       nbchunks( objcfg.nbchunks ),
                                       nbparity( objcfg.nbparity ),
                                       nbdata( objcfg.nbdata ),
                                       datasize( objcfg.datasize ),
                                       chunksize( objcfg.chunksize ),
                                       paritysize( objcfg.paritysize ),
                                       blksize( objcfg.blksize ),
                                       plgr( objcfg.plgr )
      {
      }

      const std::string obj;
      const std::string mtindex;    // index of the metadata file
      const uint8_t     nbchunks;   // number of chunks in block
      const uint8_t     nbparity;   // number of chunks in parity
      const uint8_t     nbdata;     // number of chunks in data
      const uint64_t    datasize;   // size of the data in the block
      const uint64_t    chunksize;  // size of single chunk (nbchunks * chunksize = blksize)
      const uint64_t    paritysize; // size of the parity in the block
      const uint64_t    blksize;    // the whole block size (data + parity) in MB

      std::vector<std::string> plgr;
  };
}


#endif /* SRC_XRDEC_XRDECOBJCFG_HH_ */
