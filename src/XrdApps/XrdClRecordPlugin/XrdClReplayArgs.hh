//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef SRC_XRDAPPS_XRDCLRECORDPLUGIN_XRDCLREPLAYARGS_HH_
#define SRC_XRDAPPS_XRDCLRECORDPLUGIN_XRDCLREPLAYARGS_HH_

#include <getopt.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Argument exception
  //----------------------------------------------------------------------------
  struct ArgsException : public std::exception
  {
    //--------------------------------------------------------------------------
    //! Constructor
    //! @param msg : the error message
    //--------------------------------------------------------------------------
    ArgsException( const std::string &msg ) : msg( msg ){ }

    //--------------------------------------------------------------------------
    //! @return : the error message
    //--------------------------------------------------------------------------
    virtual const char* what() const throw()
    {
      return msg.c_str();
    }

    private:
      std::string msg; //< the error message
  };

  //----------------------------------------------------------------------------
  //! Replay arguments parser
  //----------------------------------------------------------------------------
  class ReplayArgs
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //! @param argc : argument count
      //! @param argv : argument vector
      //------------------------------------------------------------------------
      ReplayArgs( int argc, char** argv ) :
        dumplocal( false ),
        createremote( false )
      {
        static const char *opletters = ":DC";
        static struct option opvec[] =
          {
            { "dump-local",    0, 0, 'D' },
            { "create-remote", 0, 0, 'C' },
            { 0,               0, 0,  0 }
          };

        int longindex = 0;
        int opc;
        while( ( opc = getopt_long( argc, argv, opletters, opvec, &longindex ) ) != -1 )
        {
          switch( opc )
          {
            case 'C':
            {
              createremote = true;
              break;
            }

            case 'D':
            {
              dumplocal = true;
              break;
            }

            case '?': throw ArgsException( "Invalid option." );

            case ':': throw ArgsException( "Missing argument." );

            default: throw ArgsException( "Arguments processing error." );
          }
        }

        if( optind < argc )
        {
          path = argv[optind];
          ++optind;
        }

        if( optind < argc )
          throw ArgsException( "Too many arguments." );
      }

      //------------------------------------------------------------------------
      //! @return : true if --dump-local was set, false otherwise
      //------------------------------------------------------------------------
      inline bool DumpLocal()
      {
        return dumplocal;
      }

      //------------------------------------------------------------------------
      //! @return : true if --create-remote was set, false otherwise
      //------------------------------------------------------------------------
      inline bool CreateRemote()
      {
        return createremote;
      }

      //------------------------------------------------------------------------
      //! @return : the path to csv file provider by our user
      //------------------------------------------------------------------------
      inline const std::string& GetPath()
      {
        return path;
      }

    private:
      bool dumplocal;    //< true if --dump-local was set, false otherwise
      bool createremote; //< true if --create-remote was set, false otherwise
      std::string path;  //< the path to csv file provider by our user

  };
}

#endif /* SRC_XRDAPPS_XRDCLRECORDPLUGIN_XRDCLREPLAYARGS_HH_ */
