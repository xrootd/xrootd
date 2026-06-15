/******************************************************************************/
/*                                                                            */
/*                         X r d C l i . c c                                  */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
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
/******************************************************************************/

#include "XrdVersion.hh"

#include <iostream>

#include <CLI/CLI.hpp>

int main(int argc, char **argv)
{
  CLI::App app{"XRootD command-line client."};
  app.name("xrd");

  bool showVersion = false;
  app.add_flag("--version", showVersion, "Show version information and exit");

  CLI11_PARSE(app, argc, argv);

  if(showVersion)
  {
    std::cout << "xrd " << XrdVERSION << '\n';
  }
  return 0;
}
