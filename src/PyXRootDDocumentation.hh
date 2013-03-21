//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
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

#ifndef PYXROOTD_DOCUMENTATION_HH_
#define PYXROOTD_DOCUMENTATION_HH_

#include "PyXRootD.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Main client module documentation string
  //----------------------------------------------------------------------------
  PyDoc_STRVAR(client_module_doc, "XRootD Client extension module.");

  //----------------------------------------------------------------------------
  //! Documentation strings for PyXRootD::FileSystem
  //----------------------------------------------------------------------------
  PyDoc_STRVAR(client_type_doc, "Client object");

  PyDoc_STRVAR(filesystem_locate_doc,
    "Locate a file.                                                      \n\n\
     :param  path: path to the file to be located                          \n\
     :type   path: string                                                  \n\
     :param flags: An `ORed` combination of :mod:`XRootD.enums.OpenFlags`  \n\
     :returns:     tuple containing status dictionary and location info    \n\
                   dictionary (see below)                                  \n");

  PyDoc_STRVAR(filesystem_deeplocate_doc,
    "Locate a file, recursively locate all disk servers.                 \n\n\
     :param  path: path to the file to be located                          \n\
     :type   path: string                                                  \n\
     :param flags: An `ORed` combination of :mod:`XRootD.enums.OpenFlags`  \n\
     :returns:     tuple containing status and location info (see above)   \n");

  PyDoc_STRVAR(filesystem_mv_doc,
    "Move a directory or a file.                                         \n\n\
     :param source: the file or directory to be moved                      \n\
     :type  source: string                                                 \n\
     :param   dest: the new name                                           \n\
     :type    dest: string                                                 \n\
     :returns:      tuple containing status dictionary and None            \n");

  PyDoc_STRVAR(filesystem_query_doc,
    "Obtain server information.                                          \n\n\
     :param querycode: the query code as specified in                      \n\
                       :mod:`XRootD.enums.QueryCode`                       \n\
     :param       arg: query argument                                      \n\
     :type        arg: string                                              \n\
     :returns:         the query response or None if there was an error    \n\
     :rtype:           string                                              \n");

  PyDoc_STRVAR(filesystem_truncate_doc,
    "Truncate a file.                                                    \n\n\
     :param path: path to the file to be truncated                         \n\
     :type  path: string                                                   \n\
     :param size: file size                                                \n\
     :type  size: integer                                                  \n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(filesystem_rm_doc,
    "Remove a file.                                                      \n\n\
     :param path: path to the file to be removed                           \n\
     :type  path: string                                                   \n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(filesystem_mkdir_doc,
    "Create a directory.                                                 \n\n\
    :param  path: path to the directory to create                          \n\
    :type   path: string                                                   \n\
    :param flags: An `ORed` combination of :mod:`XRootD.enums.MkDirFlags`  \n\
                  where the default is `MkDirFlags.NONE`                   \n\
    :param  mode: the initial file access mode, an `ORed` combination of   \n\
                  :mod:`XRootD.enums.AccessMode` where the default is      \n\
                  `AccessMode.NONE`                                        \n\
    :returns:     tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(filesystem_rmdir_doc,
    "Remove a directory.                                                 \n\n\
     :param path: path to the directory to remove                          \n\
     :type  path: string                                                   \n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(filesystem_chmod_doc,
    "Change access mode on a directory or a file.                        \n\n\
     :param path: path to the file/directory to change access mode         \n\
     :type  path: string                                                   \n\
     :param mode: An `OR`ed` combination of :mod:`XRootD.enums.AccessMode` \n\
                  where the default is `AccessMode.NONE`                   \n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(filesystem_ping_doc,
    "Check if the server is alive.                                       \n\n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(filesystem_stat_doc,
    "Obtain status information for a path.                               \n\n\
     :param path: path to the file/directory to stat                       \n\
     :type  path: string                                                   \n\
     :returns:    tuple containing status dictionary and stat info         \n\
                  dictionary (see below)                                   \n");

  PyDoc_STRVAR(filesystem_statvfs_doc,
    "Obtain status information for a Virtual File System.                \n\n\
     :param path: path to the file/directory to stat                       \n\
     :type  path: string                                                   \n\
     :returns:    tuple containing status dictionary and statvfs info      \n\
                  dictionary (see below)                                   \n");

  PyDoc_STRVAR(filesystem_protocol_doc,
    "Obtain server protocol information.                                 \n\n\
     :returns: tuple containing status dictionary and protocol info        \n\
               dictionary (see below)                                      \n");

  PyDoc_STRVAR(filesystem_dirlist_doc,
    "List entries of a directory.                                        \n\n\
     :param  path: path to the directory to list                           \n\
     :type   path: string                                                  \n\
     :param flags: An `ORed` combination of :mod:`XRootD.enums.DirListFlags` \
                   where the default is `DirListFlags.NONE`                \n\
     :returns:     tuple containing status dictionary and directory        \n\
                   list info dictionary (see below)                        \n");

  PyDoc_STRVAR(filesystem_sendinfo_doc,
    "Send info to the server (up to 1024 characters).                    \n\n\
     :param info: the info string to be sent                               \n\
     :type  info: string                                                   \n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(filesystem_prepare_doc,
    "Prepare one or more files for access.                               \n\n\
     :param    files: list of files to be prepared                         \n\
     :type     files: list                                                 \n\
     :param    flags: An `ORed` combination of                             \n\
                      :mod:`XRootD.enums.PrepareFlags`                     \n\
     :param priority: priority of the request 0 (lowest) - 3 (highest)     \n\
     :type  priority: integer                                              \n");

  //----------------------------------------------------------------------------
  //! Documentation strings for PyXRootD::File
  //----------------------------------------------------------------------------
  PyDoc_STRVAR(file_type_doc, "File object");

  PyDoc_STRVAR(file_open_doc,
    "Client object");

  PyDoc_STRVAR(file_close_doc,
    "Client object");

  PyDoc_STRVAR(file_stat_doc,
    "Client object");

  PyDoc_STRVAR(file_read_doc,
    "Client object");

  PyDoc_STRVAR(file_readline_doc,
    "Read a data chunk at a given offset, until "
        "the first newline encountered");

  PyDoc_STRVAR(file_readlines_doc,
    "Read data chunks from a given offset, separated "
        "by newlines, until EOF encountered. Return list "
        "of lines read.");

  PyDoc_STRVAR(file_readchunks_doc,
    "Read data chunks from a given offset of the "
        "given size, until EOF encountered. Return list "
        "of chunks read.");

  PyDoc_STRVAR(file_write_doc,
    "Client object");

  PyDoc_STRVAR(file_sync_doc,
    "Client object");

  PyDoc_STRVAR(file_truncate_doc,
    "Client object");

  PyDoc_STRVAR(file_vector_read_doc,
    "Client object");

  PyDoc_STRVAR(file_is_open_doc,
    "Client object");

  PyDoc_STRVAR(file_enable_read_recovery_doc,
    "Client object");

  PyDoc_STRVAR(file_enable_write_recovery_doc,
    "Client object");

  PyDoc_STRVAR(file_get_data_server_doc,
    "Client object");
}


#endif /* PYXROOTD_DOCUMENTATION_HH_ */
