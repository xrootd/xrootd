==================================================
:mod:`XRootD.client.Client`: Filesystem operations
==================================================

The client object is used to perform filesystem operations.

.. warning::
  TODO: Rewrite this whole page

The `status` return dictionary
------------------------------

With each request, even asynchronous requests, you get one of these bad boys:

  +---------------------------------------------+
  | The `status` return dictionary              |
  +==============+==============================+
  | `status`     | The status                   |
  +--------------+------------------------------+
  | `code`       | The code                     |
  +--------------+------------------------------+
  | `errNo`      | Is the node a manager        |
  +--------------+------------------------------+
  | `message`    | The message                  |
  +--------------+------------------------------+
  | `isOK`       | Is the request OK            |
  +--------------+------------------------------+
  | `isError`    | Is the request an error      |
  +--------------+------------------------------+
  | `isFatal`    | Is the request a fatal error |
  +--------------+------------------------------+
  | `shellCode`  | The shell code               |
  +--------------+------------------------------+
    
The `host info` return dictionary
---------------------------------

With asynchronous requests, you even get one of these:

  +------------------------------------------------------+
  | The `host info` return dictionary                    |
  +================+=====================================+
  | `url`          | todo (see :mod:`XRootD.client.URL`) |
  +----------------+-------------------------------------+
  | `protocol`     | todo                                |
  +----------------+-------------------------------------+
  | `flags`        | todo                                |
  +----------------+-------------------------------------+
  | `loadBalancer` | todo                                |
  +----------------+-------------------------------------+

.. note::
  All methods accept keyword arguments.
  
.. note::
  All methods accept an optional callback argument. This will make the function
  call happen asynchronously. All methods also accept optional timeout argument.
  
.. note::
  **For all synchronous function calls**, you get a tuple in return. The tuple 
  contains the status dictionary and the response dictionary, if any. If the 
  function has no return dictionary, the second item of the tuple will be None.
  
  **For all asynchronous requests**, you will get the initial status of the 
  request when the call returns, i.e. you can discover network problems etc, 
  but not the server response. When your callback is invoked, you will get 
  another status dict and the response dict (if any, otherwise None). You also
  get the host info
  
Method reference
----------------

.. module:: XRootD.client

.. autoclass:: XRootD.client.Client
  
  .. automethod:: locate(path[, flags[, timeout[, callback]]])
 
    +---------------------------------------------+
    | The `location info` return dictionary       |
    +==============+==============================+
    | `address`    | The address                  |
    +--------------+------------------------------+
    | `isServer`   | Is the node a server         |
    +--------------+------------------------------+
    | `isManager`  | Is the node a manager        |
    +--------------+------------------------------+
    | `accessType` | The access type              |
    +--------------+------------------------------+
    
  .. automethod:: deeplocate(path[, flags[, timeout[, callback]]])
  
  .. automethod:: mv(source, dest[, timeout[, callback]])
  
  .. automethod:: query(querycode, arg[, timeout[, callback]])
  
    .. note::
      For more information about XRootD query codes and arguments, see 
      `the relevant section in the protocol reference 
      <http://xrootd.slac.stanford.edu/doc/prod/XRdv299.htm#_Toc337053385>`_.
  
  .. automethod:: truncate(path, size[, timeout[, callback]])
  
  .. automethod:: rm(path[, timeout[, callback]])
  
  .. automethod:: mkdir(path[, flags[, timeout[, callback]]])
  
  .. automethod:: rmdir(path[, timeout[, callback]])
  
  .. automethod:: chmod(path[, mode[, timeout[, callback]]])
  
  .. automethod:: ping([timeout[, callback]])
  
  .. automethod:: stat(path[, timeout[, callback]])
  
    +-------------------------------------------+
    | The `stat info` return dictionary         |
    +===================+=======================+
    | `id`              | The id                |
    +-------------------+-----------------------+
    | `size`            | The size              |
    +-------------------+-----------------------+
    | `flags`           | Le flags              |
    +-------------------+-----------------------+
    | `modTime`         | Le mod time           |
    +-------------------+-----------------------+
    | `modTimeAsString` | Le mod time as string |
    +-------------------+-----------------------+
  
  .. automethod:: statvfs(path[, timeout[, callback]])
  
    +----------------------------------------------+
    | The `statvfs info` return dictionary         |
    +======================+=======================+
    | `nodesRW`            | todo                  |
    +----------------------+-----------------------+
    | `nodesStaging`       | todo                  |
    +----------------------+-----------------------+
    | `freeRW`             | todo                  |
    +----------------------+-----------------------+
    | `freeStaging`        | todo                  |
    +----------------------+-----------------------+
    | `utilizationRW`      | todo                  |
    +----------------------+-----------------------+
    | `utilizationStaging` | todo                  |
    +----------------------+-----------------------+
  
  .. automethod:: protocol([timeout[, callback]])
  
    +---------------------------------------+
    | The `protocol info` return dictionary |
    +===================+===================+
    | `version`         | todo              |
    +-------------------+-------------------+
    | `hostInfo`        | todo              |
    +-------------------+-------------------+
  
  .. automethod:: dirlist(path[, flags[, timeout[, callback]]])
  
    +-------------------------------------------------------------+
    | The `directory list info` return dictionary                 |
    +===============+=============================================+
    | `name`        | todo                                        |
    +---------------+---------------------------------------------+
    | `hostAddress` | todo                                        |
    +---------------+---------------------------------------------+
    | `statInfo`    | todo (see :mod:`XRootD.client.Client.stat`) |
    +---------------+---------------------------------------------+
  
  .. automethod:: sendinfo(info[, timeout[, callback]])
  
  .. automethod:: prepare(file, flags[, priority[, timeout[, callback]]])
  
  