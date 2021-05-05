//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
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

#ifndef PYXROOTDENV_HH_
#define PYXROOTDENV_HH_

#include "PyXRootD.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdVersion.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Sets the given key in the xrootd client environment to the given value.
  //! @return : false if there is already a shell-imported setting for this key,
  // true otherwise
  //----------------------------------------------------------------------------
  PyObject* EnvPutString_cpp( PyObject *self, PyObject *args )
  {
    char *key = 0, *value = 0;
    // parse arguments
    if( !PyArg_ParseTuple( args, "ss", &key, &value ) )
    {
      return NULL;
    }

    return PyBool_FromLong( XrdCl::DefaultEnv::GetEnv()->PutString( key, value ) );
  }

  //----------------------------------------------------------------------------
  //! Gets the given key from the xrootd client environment. If key does not
  //! exist in the environment returns None.
  //----------------------------------------------------------------------------
  PyObject* EnvGetString_cpp( PyObject *self, PyObject *args )
  {
    char *key = 0;
    // parse arguments
    if( !PyArg_ParseTuple( args, "s", &key) )
    {
      return NULL;
    }

    std::string value;
    if( !XrdCl::DefaultEnv::GetEnv()->GetString( key, value ) )
      Py_RETURN_NONE;

    return Py_BuildValue( "s", value.c_str() );
  }

  //----------------------------------------------------------------------------
   //! Sets the given key in the xrootd client environment to the given value.
   //! @return : false if there is already a shell-imported setting for this key,
   // true otherwise
   //----------------------------------------------------------------------------
  PyObject* EnvPutInt_cpp( PyObject *self, PyObject *args )
  {
    char *key = 0;
    int   value = 0;
    // parse arguments
    if( !PyArg_ParseTuple( args, "si", &key, &value ) )
    {
      return NULL;
    }

    return PyBool_FromLong( XrdCl::DefaultEnv::GetEnv()->PutInt( key, value ) );
  }

  //----------------------------------------------------------------------------
  //! Gets the given key from the xrootd client environment. If key does not
  //! exist in the environment returns None.
  //----------------------------------------------------------------------------
  PyObject* EnvGetInt_cpp( PyObject *self, PyObject *args )
  {
    char *key = 0;
    // parse arguments
    if( !PyArg_ParseTuple( args, "s", &key) )
    {
      return NULL;
    }

    int value = 0;
    if( !XrdCl::DefaultEnv::GetEnv()->GetInt( key, value ) )
      Py_RETURN_NONE;

    return Py_BuildValue( "i", value );
  }

  //----------------------------------------------------------------------------
  //! Gets default parameter value for given key
  //----------------------------------------------------------------------------
  PyObject* EnvGetDefault_cpp( PyObject *self, PyObject *args )
  {
    char *key = 0;
    // parse arguments
    if( !PyArg_ParseTuple( args, "s", &key) )
    {
      return NULL;
    }

    std::string value;
    if( XrdCl::DefaultEnv::GetEnv()->GetDefaultStringValue( key, value ) )
      return Py_BuildValue( "s", value.c_str() );

    int val;
    if( XrdCl::DefaultEnv::GetEnv()->GetDefaultIntValue( key, val ) )
      return Py_BuildValue( "s", std::to_string( val ).c_str() );

    Py_RETURN_NONE;
  }

  //----------------------------------------------------------------------------
  //! @return : client version, e.g.: v4.10.0
  //----------------------------------------------------------------------------
  PyObject* XrdVersion_cpp( PyObject *self, PyObject *args )
  {
    static std::string verstr( XrdVERSION[0] == 'v' ? XrdVERSION + 1 : XrdVERSION );
    return Py_BuildValue( "s", verstr.c_str() );
  }

}

#endif /* PYENV_HH_ */
