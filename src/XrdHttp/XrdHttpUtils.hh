//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Author: Fabrizio Furano <furano@cern.ch>
// File Date: Apr 2013
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







/** @file  XrdHttpUtils.hh
 * @brief  Utility functions for XrdHTTP
 * @author Fabrizio Furano
 * @date   April 2013
 * 
 * 
 * 
 */



#ifndef XRDHTTPUTILS_HH
#define	XRDHTTPUTILS_HH


// GetHost from URL
// Parse an URL and extract the host name and port
// Return 0 if OK
int parseURL(char *url, char *host, int &port, char **path);

// Simple itoa function
std::string itos(long i);

// Home made implementation of strchrnul
char *mystrchrnul(const char *s, int c);

void calcHashes(
        char *hash,

        const char *fn,

        kXR_int16 req,

        XrdSecEntity *secent,
        
        time_t tim,

        const char *key);


int compareHash(
        const char *h1,
        const char *h2);



// Create a new quoted string
char *quote(char *str);

// unquote a string and return a new one
char *unquote(char *str);

#endif	/* XRDHTTPUTILS_HH */

