/*
 * XrdEcZipUtilities.hh
 *
 *  Created on: Jan 27, 2020
 *      Author: simonm
 */

#ifndef SRC_API_TEST_XRDECZIPUTILITIES_HH_
#define SRC_API_TEST_XRDECZIPUTILITIES_HH_

#include <ctime>
#include <cstdint>
#include <cstring>

#include <array>
#include <exception>

namespace XrdEc 
{
  struct dos_timestmp
  {
      inline dos_timestmp() : time( 0 ), date( 0 )
      {
        const std::time_t now = std::time( nullptr );
        const std::tm calendar_time = *std::localtime( std::addressof( now ) );

        time |= ( hour_mask & uint16_t( calendar_time.tm_hour    ) ) << hour_shift;
        time |= ( min_mask  & uint16_t( calendar_time.tm_min     ) ) << min_shift;
        time |= ( sec_mask  & uint16_t( calendar_time.tm_sec / 2 ) ) << sec_shift;

        date |= ( year_mask & uint16_t( calendar_time.tm_year - 1980 ) ) << year_shift;
        date |= ( mon_mask  & uint16_t( calendar_time.tm_mon         ) ) << mon_shift;
        date |= ( day_mask  & uint16_t( calendar_time.tm_mday        ) ) << day_shift;
      }

      uint16_t time;

      static const uint16_t sec_mask  = 0x1f;
      static const uint16_t min_mask  = 0x3f;
      static const uint16_t hour_mask = 0x1f;

      static const uint8_t sec_shift  = 0;
      static const uint8_t min_shift  = 5;
      static const uint8_t hour_shift = 11;

      uint16_t date;

      static const uint16_t day_mask  = 0x1f;
      static const uint16_t mon_mask  = 0x0f;
      static const uint16_t year_mask = 0x7f;

      static const uint8_t day_shift  = 0;
      static const uint8_t mon_shift  = 5;
      static const uint8_t year_shift = 9;
  };


  template<uint8_t SIZE>
  struct ZipMetadata
  {
      operator const void*() const
      {
        return buffer.data();
      }

      void* get_buff()
      {
        return buffer.data();
      }

      static const uint8_t size = SIZE;

    protected:

      template<typename INT>
      inline static void copy_bytes( INT value, char *buffer)
      {
        char *value_ptr = reinterpret_cast<char*>( &value );
        memcpy( buffer, value_ptr, sizeof( INT ) );
      }

      std::array<char, SIZE> buffer;
  };


  struct LFH /*Local File Header*/ : public ZipMetadata<30>
  {
      friend struct lfh_record;

      LFH( uint32_t size, uint32_t checksum, uint32_t fnlen )
      {
        char *buff = buffer.data();

        // signature
        copy_bytes( signature, buff );
        buff += sizeof( signature );

        // version
        copy_bytes( version, buff );
        buff += sizeof( version );

        // flags
        copy_bytes( flags, buff );
        buff += sizeof( flags );

        // compression method
        copy_bytes( compression, buff );
        buff += sizeof( compression );

        // modification time & date
        const dos_timestmp timestmp;
        copy_bytes( timestmp.time, buff );
        buff += sizeof( timestmp.time );
        copy_bytes( timestmp.date, buff );
        buff += sizeof( timestmp.date );

        // crc32c checksum
        copy_bytes( checksum, buff );
        buff += sizeof( checksum );

        // compressed & uncompressed size
        copy_bytes( size, buff );
        buff += sizeof( size );
        copy_bytes( size, buff );
        buff += sizeof( size );

        // file name length
        copy_bytes( fnlen, buff );
        buff += sizeof( fnlen );

        // extra field size
        copy_bytes( extra_len, buff );
        buff += sizeof( extra_len );
      }

    private:

      static const uint32_t signature   = 0x04034b50;
      static const uint16_t version     = 0;
      static const uint16_t flags       = 0;
      static const uint16_t compression = 0;
      //           uint16_t mod_time;
      //           uint16_t mod_date;
      //           uint32_t crc32c;
      //           uint32_t comp_size;
      //           uint32_t uncomp_size;
      //           uint16_t fnlen;
      static const uint16_t extra_len   = 0;
  };


  struct CDH /*Central Directory Header*/ : public ZipMetadata<46>
  {
      friend struct cdh_record;

      CDH( uint32_t size, uint32_t checksum, uint32_t fnlen, uint32_t offset )
      {
        char *buff = buffer.data();

        // signature
        copy_bytes( signature, buff );
        buff += sizeof( signature );

        // version made by & need to extraction
        copy_bytes( version, buff );
        buff += sizeof( version );
        copy_bytes( version, buff );
        buff += sizeof( version );

        // flags
        copy_bytes( flags, buff );
        buff += sizeof( flags );

        // compression method
        copy_bytes( compression, buff );
        buff += sizeof( compression );

        // modification time & date
        const dos_timestmp timestmp;
        copy_bytes( timestmp.time, buff );
        buff += sizeof( timestmp.time );
        copy_bytes( timestmp.date, buff );
        buff += sizeof( timestmp.date );

        // crc32c checksum
        copy_bytes( checksum, buff );
        buff += sizeof( checksum );

        // compressed & uncompressed size
        copy_bytes( size, buff );
        buff += sizeof( size );
        copy_bytes( size, buff );
        buff += sizeof( size );

        // file name length
        copy_bytes( fnlen, buff );
        buff += sizeof( fnlen );

        // extra field size
        copy_bytes( extra_len, buff );
        buff += sizeof( extra_len );

        // file comment length
        copy_bytes( file_comment_len, buff );
        buff += sizeof( file_comment_len );

        // disk number
        copy_bytes( disknb, buff );
        buff += sizeof( disknb );

        // internal file attributes
        copy_bytes( intr_file_attr, buff );
        buff += sizeof( intr_file_attr );

        // external file attributes
        copy_bytes( extr_file_attr, buff );
        buff += sizeof( extr_file_attr );

        // offset of the Local File Header
        copy_bytes( offset, buff );
        buff += sizeof( offset );
      }

    private:

      static const uint32_t signature           = 0x02014b50;
      static const uint16_t version /*made by*/ = 10;
      //           uint16_t version need to     = 10
      static const uint16_t flags               = 0;
      static const uint16_t compression         = 0;
      //           uint16_t mod_time;
      //           uint16_t mod_date'
      //           uint32_t crc32c;
      //           uint32_t comp_size;
      //           uint32_t uncomp_size;
      //           uint16_t fnlen;
      static const uint16_t extra_len           = 0;
      static const uint16_t file_comment_len    = 0;
      static const uint16_t disknb              = 0;
      static const uint16_t intr_file_attr      = 0;
      static const uint16_t extr_file_attr      = 0;
      //           uint32_t lfh_offset;
  };


  struct EOCD /*End of Central Directory record*/ : public ZipMetadata<22>
  {
      friend struct eocd_record;

      EOCD( uint16_t cd_nbentries, uint32_t cd_size, uint32_t cd_offset )
      {
        char *buff = buffer.data();

        // signature
        copy_bytes( signature, buff );
        buff += sizeof( signature );

        // this $ Central Directory disk number
        copy_bytes( disknb, buff );
        buff += sizeof( disknb );
        copy_bytes( cd_disknb, buff );
        buff += sizeof( cd_disknb );

        // Number of entries in the Central Directory
        copy_bytes( cd_nbentries, buff );
        buff += sizeof( cd_nbentries );
        copy_bytes( cd_nbentries, buff );
        buff += sizeof( cd_nbentries );

        // Central Directory size
        copy_bytes( cd_size, buff );
        buff += sizeof( cd_size );

        // Central Directory offset
        copy_bytes( cd_offset, buff );
        buff += sizeof( cd_offset );

        // ZIP comment length
        copy_bytes( comment_len, buff );
        buff += sizeof( comment_len );
      }

    private:

      static const uint32_t signature = 0x06054b50;
      static const uint16_t disknb    = 0;
      static const uint16_t cd_disknb = 0;
      //           uint16_t nb_entries_disk;
      //           uint16_t nb_entries;
      //           uint32_t cd_size;
      //           uint32_t cd_offset
      static const uint16_t comment_len = 0;
  };

  struct lfh_record
  {
      lfh_record( const void *buffer ): signature( 0 ),
                                        version( 0 ),
                                        flags( 0 ),
                                        compression( 0 ),
                                        mod_time( 0 ),
                                        mod_date( 0 ),
                                        crc32c( 0 ),
                                        comp_size( 0 ),
                                        uncomp_size( 0 ),
                                        fnlen( 0 ),
                                        extra_len( 0 )
      {
        const char *buff = reinterpret_cast<const char*>( buffer );

        // signature
        memcpy( &signature, buff, sizeof( signature ) );
        if( signature != LFH::signature ) throw std::exception();
        buff += sizeof( signature );

        // version
        memcpy( &version, buff, sizeof( version ) );
        if( version != LFH::version ) throw std::exception();
        buff += sizeof( version );

        // flags
        memcpy( &flags, buff, sizeof( flags ) );
        if( flags != LFH::flags ) throw std::exception();
        buff += sizeof( flags );

        // compression method
        memcpy( &compression, buff, sizeof( compression ) );
        if( compression != LFH::compression ) throw std::exception();
        buff += sizeof( compression );

        // modification time & date
        memcpy( &mod_time, buff, sizeof( mod_time ) );
        buff += sizeof( mod_time );
        memcpy( &mod_date, buff, sizeof( mod_date ) );
        buff += sizeof( mod_date );

        // crc32c checksum
        memcpy( &crc32c, buff, sizeof( crc32c ) );
        buff += sizeof( crc32c );

        // compressed & uncompressed size
        memcpy( &comp_size, buff, sizeof( comp_size ) );
        buff += sizeof( comp_size );
        memcpy( &uncomp_size, buff, sizeof( uncomp_size ) );
        buff += sizeof( uncomp_size );
        if( comp_size != uncomp_size ) throw std::exception();

        // file name length
        memcpy( &fnlen, buff, sizeof( fnlen ) );
        buff += sizeof( fnlen );

        // extra field size
        memcpy( &extra_len, buff, sizeof( extra_len ) );
        if( extra_len != LFH::extra_len ) throw std::exception();
        buff += sizeof( extra_len );
      }

      uint32_t signature;
      uint16_t version;
      uint16_t flags;
      uint16_t compression;
      uint16_t mod_time;
      uint16_t mod_date;
      uint32_t crc32c;
      uint32_t comp_size;
      uint32_t uncomp_size;
      uint16_t fnlen;
      uint16_t extra_len;
  };

  struct cdh_record
  {
      cdh_record( const void *buffer  ) : signature( 0 ),
                                          version_made( 0 ),
                                          version_needed( 0 ),
                                          flags( 0 ),
                                          compression( 0 ),
                                          mod_time( 0 ),
                                          mod_date( 0 ),
                                          crc32c( 0 ),
                                          comp_size( 0 ),
                                          uncomp_size( 0 ),
                                          fnlen( 0 ),
                                          extra_len( 0 ),
                                          file_comment_len( 0 ),
                                          disknb( 0 ),
                                          intr_file_attr( 0 ),
                                          extr_file_attr( 0 ),
                                          lfh_offset( 0 )
      {
        const char *buff = reinterpret_cast<const char*>( buffer );

        // signature
        memcpy( &signature, buff, sizeof( signature ) );
        if( signature != CDH::signature ) throw std::exception();
        buff += sizeof( signature );

        // version made by
        memcpy( &version_made, buff, sizeof( version_made ) );
        if( version_made != CDH::version ) throw std::exception();
        buff += sizeof( version_made );

        // version needed
        memcpy( &version_needed, buff, sizeof( version_needed ) );
        if( version_needed != CDH::version ) throw std::exception();
        buff += sizeof( version_needed );

        // flags
        memcpy( &flags, buff, sizeof( flags ) );
        if( flags != CDH::flags ) throw std::exception();
        buff += sizeof( flags );

        // compression method
        memcpy( &compression, buff, sizeof( compression ) );
        if( compression != CDH::compression ) throw std::exception();
        buff += sizeof( compression );

        // modification time
        memcpy( &mod_time, buff, sizeof( mod_time ) );
        buff += sizeof( mod_time );

        // modification date
        memcpy( &mod_date, buff, sizeof( mod_date ) );
        buff += sizeof( mod_date );

        // crc32c checksum
        memcpy( &crc32c, buff, sizeof( crc32c ) );
        buff += sizeof( crc32c );

        // compressed & uncompressed size
        memcpy( &comp_size, buff, sizeof( comp_size ) );
        buff += sizeof( comp_size );
        memcpy( &uncomp_size, buff, sizeof( uncomp_size ) );
        buff += sizeof( uncomp_size );
        if( comp_size != uncomp_size ) throw std::exception();

        // file name length
        memcpy( &fnlen, buff, sizeof( fnlen ) );
        buff += sizeof( fnlen );

        // extra field size
        memcpy( &extra_len, buff, sizeof( extra_len ) );
        if( extra_len != CDH::extra_len ) throw std::exception();
        buff += sizeof( extra_len );

        // file comment length
        memcpy( &file_comment_len, buff, sizeof( file_comment_len ) );
        if( file_comment_len != CDH::file_comment_len ) throw std::exception();
        buff += sizeof( file_comment_len );

        // disk number
        memcpy( &disknb, buff, sizeof( disknb ) );
        if( disknb != CDH::disknb ) throw std::exception();
        buff += sizeof( disknb );

        // internal file attributes
        memcpy( &intr_file_attr, buff, sizeof( intr_file_attr ) );
        if( intr_file_attr != CDH::intr_file_attr ) throw std::exception();
        buff += sizeof( intr_file_attr );

        // external file attributes
        memcpy( &extr_file_attr, buff, sizeof( extr_file_attr ) );
        if( extr_file_attr != CDH::extr_file_attr ) throw std::exception();
        buff += sizeof( extr_file_attr );

        // offset of the Local File Header
        memcpy( &lfh_offset, buff, sizeof( lfh_offset ) );
        buff += sizeof( lfh_offset );
      }

      uint32_t signature;
      uint16_t version_made;
      uint16_t version_needed;
      uint16_t flags;
      uint16_t compression;
      uint16_t mod_time;
      uint16_t mod_date;
      uint32_t crc32c;
      uint32_t comp_size;
      uint32_t uncomp_size;
      uint16_t fnlen;
      uint16_t extra_len;
      uint16_t file_comment_len;
      uint16_t disknb;
      uint16_t intr_file_attr;
      uint16_t extr_file_attr;
      uint32_t lfh_offset;
  };

  struct eocd_record
  {
      eocd_record( const void *buffer ): signature( 0 ),
                                         disknb( 0 ),
                                         cd_disknb( 0 ),
                                         nb_entries_disk( 0 ),
                                         nb_entries( 0 ),
                                         cd_size( 0 ),
                                         cd_offset( 0 ),
                                         comment_len( 0 )
      {
        const char *buff = reinterpret_cast<const char*>( buffer );

        // signature
        memcpy( &signature, buff, sizeof( signature ) );
        if( signature != EOCD::signature ) throw std::exception();
        buff += sizeof( signature );

        // this $ Central Directory disk number
        memcpy( &disknb, buff, sizeof( disknb ) );
        if( disknb != EOCD::disknb ) throw std::exception();
        buff += sizeof( disknb );
        memcpy( &cd_disknb, buff, sizeof( cd_disknb ) );
        if( cd_disknb != EOCD::cd_disknb ) throw std::exception();
        buff += sizeof( cd_disknb );

        // Number of entries in the Central Directory
        memcpy( &nb_entries_disk, buff, sizeof( nb_entries_disk ) );
        buff += sizeof( nb_entries_disk );
        memcpy( &nb_entries, buff, sizeof( nb_entries ) );
        buff += sizeof( nb_entries );
        if( nb_entries != nb_entries_disk ) throw std::exception();

        // Central Directory size
        memcpy( &cd_size, buff, sizeof( cd_size ) );
        buff += sizeof( cd_size );

        // Central Directory offset
        memcpy( &cd_offset, buff, sizeof( cd_offset ) );
        buff += sizeof( cd_offset );

        // ZIP comment length
        memcpy( &comment_len, buff, sizeof( comment_len ) );
        if( comment_len != EOCD::comment_len ) throw std::exception();
        buff += sizeof( comment_len );
      }

      uint32_t signature;
      uint16_t disknb;
      uint16_t cd_disknb;
      uint16_t nb_entries_disk;
      uint16_t nb_entries;
      uint32_t cd_size;
      uint32_t cd_offset;
      uint16_t comment_len;
  };
}



#endif /* SRC_API_TEST_XRDECZIPUTILITIES_HH_ */
