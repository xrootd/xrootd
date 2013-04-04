===========================================================
:mod:`XRootD.client.enums`: Enumeration flags and constants
===========================================================

.. module:: XRootD.enums

.. attribute:: OpenFlags

  | :mod:`OpenFlags.NONE`:     Nothing
  | :mod:`OpenFlags.DELETE`:   Open a new file, deleting any existing file
  | :mod:`OpenFlags.FORCE`:    Ignore file usage rules
  | :mod:`OpenFlags.NEW`:      Open the file only if it does not already exist
  | :mod:`OpenFlags.READ`:     Open only for reading
  | :mod:`OpenFlags.UPDATE`:   Open for reading and writing
  | :mod:`OpenFlags.REFRESH`:  Refresh the cached information on file location. 
                               Voids `NoWait`.
  | :mod:`OpenFlags.MAKEPATH`: Create directory path if it doesn't already exist
  | :mod:`OpenFlags.APPEND`:   Open only for appending
  | :mod:`OpenFlags.REPLICA`:  The file is being opened for replica creation
  | :mod:`OpenFlags.POSC`:     Enable `Persist On Successful Close` processing
  | :mod:`OpenFlags.NOWAIT`:   Open the file only if it does not cause a wait. 
                               For :func:`XRootD.client.FileSystem.locate` : provide 
                               a location as soon as one becomes known. This 
                               means that not all locations are necessarily 
                               returned. If the file does not exist a wait is 
                               still imposed.
  | :mod:`OpenFlags.SEQIO`:    File will be read or written sequentially

.. attribute:: MkDirFlags

  | :mod:`MkDirFlags.NONE`:     Nothing special
  | :mod:`MkDirFlags.MAKEPATH`: Create the entire directory tree if it doesn't 
                                exist

.. attribute:: DirListFlags

  | :mod:`DirListFlags.NONE`:   Nothing special
  | :mod:`DirListFlags.STAT`:   Stat each entry
  | :mod:`DirListFlags.LOCATE`: Locate all servers hosting the directory and 
                                send the dirlist request to all of them

.. attribute:: PrepareFlags

  | :mod:`PrepareFlags.STAGE`:     Stage the file to disk if it is not online
  | :mod:`PrepareFlags.WRITEMODE`: The file will be accessed for modification
  | :mod:`PrepareFlags.COLOCATE`:  Co-locate staged files, if possible
  | :mod:`PrepareFlags.FRESH`:     Refresh file access time even if the location
                                   is known

.. attribute:: AccessMode

  | :mod:`AccessMode.NONE`: Default, no flags
  | :mod:`AccessMode.UR`:   Owner readable
  | :mod:`AccessMode.UW`:   Owner writable
  | :mod:`AccessMode.UX`:   Owner executable/browsable
  | :mod:`AccessMode.GR`:   Group readable
  | :mod:`AccessMode.GW`:   Group writable
  | :mod:`AccessMode.GX`:   Group executable/browsable
  | :mod:`AccessMode.OR`:   World readable
  | :mod:`AccessMode.OW`:   World writable
  | :mod:`AccessMode.OX`:   World executable/browsable

.. attribute:: QueryCode

  | :mod:`QueryCode.STATS`:          Query server stats
  | :mod:`QueryCode.PREPARE`:        Query prepare status
  | :mod:`QueryCode.CHECKSUM`:       Query file checksum
  | :mod:`QueryCode.XATTR`:          Query file extended attributes
  | :mod:`QueryCode.SPACE`:          Query logical space stats
  | :mod:`QueryCode.CHECKSUMCANCEL`: Query file checksum cancellation
  | :mod:`QueryCode.CONFIG`:         Query server configuration
  | :mod:`QueryCode.VISA`:           Query file visa attributes
  | :mod:`QueryCode.OPAQUE`:         Implementation dependent
  | :mod:`QueryCode.OPAQUEFILE`:     Implementation dependent

  