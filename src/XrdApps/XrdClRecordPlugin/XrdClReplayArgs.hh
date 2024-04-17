//------------------------------------------------------------------------------
// Copyright (c) 2011-2021 by European Organization for Nuclear Research (CERN)
// Author: Andreas-Joachim Peters <andreas.joachim.peters@cern.ch>
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

#include <stdarg.h>
#include <getopt.h>
#include <regex>
#include <map>
#include <vector>
namespace XrdCl
{
//------------------------------------------------------------------------------
//! Args parse for XrdClReplay
//------------------------------------------------------------------------------
class ReplayArgs
{
  public:
  ReplayArgs(int argc, char* argv[])
  : option_long(false)
  , option_summary(false)
  , option_print(false)
  , option_create(false)
  , option_truncate(false)
  , option_json(false)
  , option_suppress_error(false)
  , option_verify(false)
  , option_speed(1.0)
  {
    while (1)
    {
      int                  option_index = 0;
      static struct option long_options[]
        = { { "help", no_argument, 0, 'h' },        { "print", no_argument, 0, 'p' },
            { "create", no_argument, 0, 'c' },      { "truncate", no_argument, 0, 't' },
            { "long", no_argument, 0, 'l' },        { "json", no_argument, 0, 'j' },
            { "summary", no_argument, 0, 's' },     { "replace", required_argument, 0, 'r' },
            { "suppress", no_argument, 0, 'f' },    { "verify", no_argument, 0, 'v' },
            { "speed", required_argument, 0, 'x' }, { 0, 0, 0, 0 } };

      int c = getopt_long(argc, argv, "vjpctshlfr:x:", long_options, &option_index);
      if (c == -1)
        break;

      switch (c)
      {
        case 'h':
          usage();
          break;

        case 'c':
          option_create = true;
          option_print  = true;  // create mode requires to run in simulated mode (print)
          break;

        case 't':
          option_create   = true;
          option_print    = true;  // truncate mode requires to run in simulated mode (print)
          option_truncate = true;
          break;

        case 'j':
          option_json = true;
          break;

        case 'p':
          option_print = true;
          break;

        case 's':
          option_summary = true;
          break;

        case 'l':
          option_long = true;
          break;

        case 'v':
          option_verify = true;
          break;

        case 'x':
          option_speed = std::strtod(optarg, 0);
          if (option_speed <= 0)
          {
            usage();
          }
          break;

        case 'r':
          option_regex.push_back(optarg);
          break;

        case 'f':
          option_suppress_error = true;
          break;

        default:
          usage();
      }
    }

    if (option_json && (option_long || option_summary))
      option_long = option_summary = true;

    if (option_verify)
    {
      option_print    = true;
      option_create   = false;
      option_truncate = false;
      option_json     = false;
    }

    if (optind < (argc - 1))
    {
      usage();
    }

    if (optind == (argc -1 )) {
      // we also accept to have no path and read from STDIN
      _path = argv[optind];
    }
  }

  void usage()
  {
    std::cerr
      << "usage: xrdreplay [-p|--print] [-c|--create-data] [t|--truncate-data] [-l|--long] [-s|--summary] [-h|--help] [-r|--replace <arg>:=<newarg>] [-f|--suppress] [-v|--verify] [-x|--speed <value] p<recordfilename>]\n"
      << std::endl;
    std::cerr << "                -h | --help             : show this help" << std::endl;
    std::cerr
      << "                -f | --suppress         : force to run all IO with all successful result status - suppress all others"
      << std::endl;
    std::cerr
      << "                                          - by default the player won't run with an unsuccessful recorded IO"
      << std::endl;
    std::cerr << std::endl;
    std::cerr
      << "                -p | --print            : print only mode - shows all the IO for the given replay file without actually running any IO"
      << std::endl;
    std::cerr
      << "                -s | --summary          : print summary - shows all the aggregated IO counter summed for all files"
      << std::endl;
    std::cerr
      << "                -l | --long             : print long - show all file IO counter for each individual file"
      << std::endl;
    std::cerr << "                -v | --verify           : verify the existence of all input files"
              << std::endl;
    std::cerr
      << "                -x | --speed <x>        : change playback speed by factor <x> [ <x> > 0.0 ]"
      << std::endl;
    std::cerr
      << "                -r | --replace <a>:=<b> : replace in the argument list the string <a> with <b> "
      << std::endl;
    std::cerr
      << "                                          - option is usable several times e.g. to change storage prefixes or filenames"
      << std::endl;
    std::cerr << std::endl;
    std::cerr
      << "             [recordfilename]          : if a file is given, it will be used as record input otherwise STDIN is used to read records!"
      << std::endl;
    std::cerr
      << "example:        ...  --replace file:://localhost:=root://xrootd.eu/        : redirect local file to remote"
      << std::endl;
    std::cerr << std::endl;
    exit(-1);
  }

  bool                      longformat() { return option_long; }
  bool                      summary() { return option_summary; }
  bool                      print() { return option_print; }
  bool                      create() { return option_create; }
  bool                      truncate() { return option_truncate; }
  bool                      json() { return option_json; }
  bool                      suppress_error() { return option_suppress_error; }
  bool                      verify() { return option_verify; }
  double                    speed() { return option_speed; }
  std::vector<std::string>& regex() { return option_regex; }
  std::string&              path() { return _path; }

  private:
  bool                     option_long;
  bool                     option_summary;
  bool                     option_print;
  bool                     option_create;
  bool                     option_truncate;
  bool                     option_json;
  bool                     option_suppress_error;
  bool                     option_verify;
  double                   option_speed;
  std::vector<std::string> option_regex;
  std::string              _path;
};
}
