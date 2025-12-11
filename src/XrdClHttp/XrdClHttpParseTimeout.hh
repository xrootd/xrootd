/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#ifndef XRDCLHTTP_PARSETIMEOUT_HH
#define XRDCLHTTP_PARSETIMEOUT_HH

#include <string>

#include <time.h>

namespace XrdClHttp {

// Parse a given string as a duration, returning the parsed value as a timespec.
//
// The implementation is based on the Go duration format which is a signed sequence of
// decimal numbers with a unit suffix.
//
// Examples:
// - 30ms
// - 1h5m
// Valid time units are "ns", "us", "ms", "s", "m", "h".  Unlike go, UTF-8 for microsecond is not accepted
//
// If an invalid value is given, false is returned and the errmsg is set.
bool ParseTimeout(const std::string &duration, struct timespec &, std::string &errmsg);

// Given a time value, marshal it to a string (based on the Go duration format)
//
// Result will be of the form XYZsABCms (or 1s500ms for 1.5 seconds).
std::string MarshalDuration(const struct timespec &timeout);

}

#endif // XRDCLHTTP_PARSETIMEOUT_HH
