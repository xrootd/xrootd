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
  PyDoc_STRVAR(filesystem_type_doc, "FileSystem object");

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
     :type  priority: integer                                              \n\
     :returns:        tuple containing status dictionary and None          \n");

  //----------------------------------------------------------------------------
  //! Documentation strings for PyXRootD::File
  //----------------------------------------------------------------------------
  PyDoc_STRVAR(file_type_doc, "File object");

  PyDoc_STRVAR(file_open_doc,
    "Open the file pointed to by the given URL.                          \n\n\
     :param url: url of the file to be opened                              \n\
     :type  url: string                                                    \n\
     :param flags: An `ORed` combination of :mod:`XRootD.enums.OpenFlags`  \n\
                   where the default is `OpenFlags.NONE`                   \n\
     :param  mode: access mode for new files, an `ORed` combination of     \n\
                  :mod:`XRootD.enums.AccessMode` where the default is      \n\
                  `AccessMode.NONE`                                        \n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(file_close_doc,
    "Close the file.                                                     \n\n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(file_stat_doc,
    "Obtain status information for this file.                            \n\n\
     :param force: do not use the cached information, force re-stating     \n\
     :type  force: boolean                                                 \n\
     :returns:     tuple containing status dictionary and None             \n");

  PyDoc_STRVAR(file_read_doc,
    "Read a data chunk from a given offset.                              \n\n\
     :param offset: offset from the beginning of the file                  \n\
     :type  offset: integer                                                \n\
     :param   size: number of bytes to be read                             \n\
     :type    size: integer                                                \n\
     :returns:      tuple containing status dictionary and None            \n");

  PyDoc_STRVAR(file_readline_doc,
    "Read a data chunk from a given offset, until the first newline          \
     encountered or a maximum of `size` bytes are read.                  \n\n\
     :param offset: offset from the beginning of the file                  \n\
     :type  offset: integer                                                \n\
     :param   size: maximum number of bytes to be read                     \n\
     :type    size: integer                                                \n\
     :returns:      data that was read, including the trailing newline     \n\
     :rtype:        string                                                 \n");

  PyDoc_STRVAR(file_readlines_doc,
    "Read lines from a given offset until EOF encountered. Return list of    \
     lines read.                                                         \n\n\
     :param offset: offset from the beginning of the file                  \n\
     :type  offset: integer                                                \n\
     :param   size: maximum number of bytes to be read                     \n\
     :type    size: integer                                                \n\
     :returns:      data that was read, including trailing newlines        \n\
     :rtype:        list of strings                                        \n");

  PyDoc_STRVAR(file_readchunks_doc,
    "Read data chunks from a given offset of the given size until EOF.       \
     Return list of chunks read.                                         \n\n\
     :param    offset: offset from the beginning of the file               \n\
     :type     offset: integer                                             \n\
     :param blocksize: maximum number of bytes to be read                  \n\
     :type  blocksize: integer                                             \n\
     :returns:         chunks that were read                               \n\
     :rtype:           list of strings                                     \n");

  PyDoc_STRVAR(file_write_doc,
    "Write a data chunk at a given offset.                               \n\n\
     :param offset: offset from the beginning of the file                  \n\
     :type  offset: integer                                                \n\
     :param   size: number of bytes to be written                          \n\
     :type    size: integer                                                \n\
     :returns:      tuple containing status dictionary and None            \n");

  PyDoc_STRVAR(file_sync_doc,
    "Commit all pending disk writes.                                     \n\n\
     :returns:      tuple containing status dictionary and None            \n");

  PyDoc_STRVAR(file_truncate_doc,
    "Truncate the file to a particular size.                             \n\n\
     :param size: desired size of the file                                 \n\
     :type  size: integer                                                  \n\
     :returns:    tuple containing status dictionary and None              \n");

  PyDoc_STRVAR(file_vector_read_doc,
    "Read scattered data chunks in one operation.                        \n\n\
     :param chunks: list of the chunks to be read. The default maximum     \n\
                    chunk size is 2097136 bytes and the default maximum    \n\
                    number of chunks per request is 1024. The server may   \n\
                    be queried using :mod:`query` for the actual settings. \n\
     :type  chunks: list of 2-tuples of the form (offset, size)            \n\
     :returns:      tuple containing status dictionary and vector read     \n\
                    info dictionary (see below)                            \n");

  PyDoc_STRVAR(file_is_open_doc,
    "Check if the file is open.                                          \n\n\
     :rtype: boolean                                                       \n");

  PyDoc_STRVAR(file_enable_read_recovery_doc,
    "Enable/disable state recovery procedures while the file is open for     \
     reading.                                                            \n\n\
     :param enable: is read recovery enabled                               \n\
     :type  enable: boolean                                                \n");

  PyDoc_STRVAR(file_enable_write_recovery_doc,
    "Enable/disable state recovery procedures while the file is open for     \
     writing or read/write.                                              \n\n\
     :param enable: is write recovery enabled                              \n\
     :type  enable: boolean                                                \n");

  PyDoc_STRVAR(file_get_data_server_doc,
    "Get the data server the file is accessed at.                        \n\n\
     :returns: the address of the data server                              \n\
     :rtype:   string                                                      \n");
}


#endif /* PYXROOTD_DOCUMENTATION_HH_ */
